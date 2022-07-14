#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <chrono>
#include <iostream>

#include "crossbar.hpp"

struct Pair {
	Pair()
	: dest(std::numeric_limits<unsigned short>::max())
	{}
	Pair(size_t dest)
	: dest(dest)
	{}

	unsigned short dest;
};
static_assert(sizeof(Pair) == sizeof(unsigned short));

struct Offset {
	Offset() : start(std::numeric_limits<int>::max()),
	stop(std::numeric_limits<int>::max())
	{}
	Offset(size_t start, size_t stop)
	: start(start), stop(stop)
	{}

	Offset operator+(int b) {
		return Offset{start, stop};
	}

	size_t start, stop;
};

std::vector<size_t> edge_counts;
constexpr unsigned int ITERATIONS = 20000;
constexpr unsigned int CROSSBAR_ROWS = 256;
constexpr unsigned int CROSSBAR_COLS = 256;
Crossbar<Pair> crossbar;
Crossbar<Offset> offsets;
std::vector<double> efficiences;

struct Tuple {
	Tuple(size_t i, size_t j ,int weight)
	: i(i), j(j), weight(weight) {}

	size_t i, j;
	int weight;
};

struct Statistics {
	uint64_t write_ops;
	uint64_t read_ops;
	uint64_t additions;
	uint64_t mins;
} stats;

struct IOResult {
	std::vector<Tuple> tuples;
	std::vector<size_t> degrees;
	size_t dimensions;
};

constexpr auto round_up(const auto a, const auto b) {
	return ((a + b - 1) / b)*b;
}

struct GraphOrdering {
	GraphOrdering(IOResult &io_result)
	: io_result(std::move(io_result)), current_row(0), current_column(0), next_row(0),
	next_column(0) {
		// Sort the edges by increasing column.
		std::sort(this->io_result.tuples.begin(), this->io_result.tuples.end(), [] (auto a, auto b) {
			if (a.j == b.j)
				return a.i < b.i;
			return a.j < b.j;
		});
	}

	std::vector<Tuple> next_subgraph() {
		// Two steps: first find lower and upper bound for columns.
		// Then find lower and upper bound for rows.

		current_row = next_row;
		current_column = next_column;
		auto lower_column = std::lower_bound(io_result.tuples.begin(), io_result.tuples.end(), Tuple{0, current_column * CROSSBAR_COLS, 0}, [] (auto a, auto b) {
			return a.j < b.j;
		});

		auto upper_column = std::upper_bound(io_result.tuples.begin(), io_result.tuples.end(), Tuple{0, (current_column + 1) * CROSSBAR_COLS - 1, 0}, [] (auto a, auto b) {
			return a.j < b.j;
		});

		std::vector<Tuple> temp(lower_column, upper_column);
		std::sort(temp.begin(), temp.end(), [] (auto a, auto b) {
			if (a.i == b.i)
				return a.j < b.j;
			return a.i < b.i;
		});

		auto lower_row = std::lower_bound(temp.begin(), temp.end(), Tuple{current_row * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		auto upper_row = std::upper_bound(temp.begin(), temp.end(), Tuple{(current_row + 1) * CROSSBAR_ROWS - 1, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		next_column++;
		if (next_column * CROSSBAR_ROWS >= io_result.dimensions) {
			next_column = 0;
			next_row++;
		}

		return std::vector<Tuple>(lower_row, upper_row);
	}

	bool has_next_subgraph() const {
		// We are done when we are finished with the last column.
		return (next_row) * CROSSBAR_COLS < ((io_result.dimensions + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS;
	}

	bool last_column() const {
		return (current_row + 1) * CROSSBAR_COLS < ((io_result.dimensions + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS;
	}

	void reset_position() {
		current_row = 0;
		current_column = 0;
		next_row = 0;
		next_column = 0;
	}

	size_t get_col_offset() const {
		return current_column * CROSSBAR_COLS;
	}

	size_t get_row_offset() const {
		return current_row * CROSSBAR_ROWS;
	}

	size_t get_dimensions() const {
		return io_result.dimensions;
	}

	IOResult io_result;
private:
	size_t current_row;
	size_t current_column;
	size_t next_row;
	size_t next_column;
};

// This function reads the graph from a file and
// turns it into a set of tuples (u, v, 1). We also
// determine the size of the graph by the maximum
// vertex index referenced in the edge list.
static IOResult read_graph(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");
	IOResult result{};

	char line[256];
	while (fgets(line, 256, fp)) {
		if (!line[0] || line[0] == '%' || line[0] == '#')
			continue;

		size_t row, col;
		sscanf(line, "%ld %ld", &row, &col);

		result.tuples.emplace_back(row, col, 1);
		//result.tuples.emplace_back(col, row, 1);

		auto max = std::max(row, col);

		result.degrees.resize(std::max(max, result.degrees.size()) + 1);
		result.degrees[row] += 1;
		//result.degrees[col] += 1;
	}

	auto max = std::max_element(result.tuples.begin(), result.tuples.end(), [] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});
	result.dimensions = std::max(max->i, max->j) + 1;
	std::sort(result.tuples.begin(), result.tuples.end(), [&] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});

	fclose(fp);
	return result;
}

static void expand_to_crossbar(IOResult &io_result, const std::vector<Tuple> &tuples, size_t row_offset,
		size_t col_offset) {
	if (tuples.size() >= CROSSBAR_ROWS * CROSSBAR_COLS)
		throw std::runtime_error("graph too large to fit into crossbar!");

	size_t row = 0, column = 0;
	std::vector<Pair> vals(CROSSBAR_COLS);
	std::vector<Offset> offset_array(CROSSBAR_COLS);

	std::vector<unsigned int> degrees(CROSSBAR_COLS);
	for (auto &tuple : tuples)
		degrees[tuple.i - row_offset]++;

	for (auto &tuple : tuples) {
		auto degree = degrees[tuple.i - row_offset];
		if (degree > CROSSBAR_COLS)
			std::cout << "too large degree: " << degree << std::endl;
		assert(degree <= CROSSBAR_COLS);

		auto offset = row * CROSSBAR_COLS + column;
		if (offset_array[tuple.i - row_offset].start == std::numeric_limits<int>::max()) {
			if (degree > CROSSBAR_COLS - column) {
				crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
				row++;
				column = 0;
				vals.clear();
				vals.resize(CROSSBAR_COLS);
			}
			offset = row * CROSSBAR_COLS + column;
			offset_array[tuple.i - row_offset] = Offset{offset, offset + degree};
		} else {
			assert(offset_array[tuple.i - row_offset].stop >= offset);
		}
		assert(tuple.i - row_offset < offset_array.size());

		assert(tuple.j - col_offset < CROSSBAR_COLS);
		vals[column] = Pair{tuple.j - col_offset};
		column++;
	}
	crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
	offsets.writeRow(0, 0, CROSSBAR_COLS, offset_array);

	efficiences.push_back(crossbar.space_efficiency([] (Pair &val) {
		return val.dest != std::numeric_limits<unsigned short>::max();
	}));
}

static void reset_crossbar() {
	crossbar.clear();
}

// This function runs the actual SSSP algorithm, taking as parameter the start node,
// and outputting the "d" vector. We maintain a vector of active nodes,
// and we iterate and go through the crossbar row for which there is an active node.
// The function is pretty much exactly what GraphR also implements in their paper.
static std::vector<int> run_algorithm(size_t start_node, GraphOrdering &graph) {
	std::vector<int> d(round_up(graph.get_dimensions(), CROSSBAR_COLS), std::numeric_limits<int>::max());
	std::vector<bool> active_nodes(round_up(graph.get_dimensions(), CROSSBAR_COLS));
	std::vector<bool> changed_nodes(round_up(graph.get_dimensions(), CROSSBAR_COLS));

	assert(start_node < CROSSBAR_COLS);
	active_nodes[start_node] = true;
	d[start_node] = 0;

	unsigned int iterations = 0;

	bool is_active = true;
	while (is_active) {
		is_active = false;
		graph.reset_position();

		if (iterations == ITERATIONS)
			break;

		while (graph.has_next_subgraph()) {
			auto start = std::chrono::system_clock::now();
			reset_crossbar();

			auto end = std::chrono::system_clock::now();
			std::chrono::duration<double> duration = end - start;
			//std::cout << "cleared crossbar! " << duration.count() << std::endl;
			start = std::chrono::system_clock::now();

			auto tuples = graph.next_subgraph();

			end = std::chrono::system_clock::now();
			duration = end - start;
			//std::cout << "got tuples! " << duration.count() << std::endl;
			start = std::chrono::system_clock::now();

			if (tuples.empty())
				continue;
			expand_to_crossbar(graph.io_result, tuples, graph.get_row_offset(),
					graph.get_col_offset());
			stats.write_ops++;

			end = std::chrono::system_clock::now();
			duration = end - start;
			//std::cout << "expanded to crossbar! " << duration.count() << std::endl;
			start = std::chrono::system_clock::now();

			for (size_t i = 0; i < CROSSBAR_COLS; i++) {
				auto real_row = i + graph.get_row_offset();
				if (!active_nodes[real_row])
					continue;

				is_active = true;
				auto offset = offsets.readRow(0, i, 1)[0];
				if (offset.start == std::numeric_limits<int>::max())
					continue;

				auto num_edges = offset.stop - offset.start;
				auto offset_i = offset.start / CROSSBAR_COLS;
				auto offset_j = offset.start % CROSSBAR_COLS;
				auto read_result = crossbar.readRow(offset_i, offset_j, num_edges);
				crossbar.logElements(read_result.size());
				for (auto pair : read_result) {
					auto j = pair.dest + graph.get_col_offset();
					auto sum = 1 + d[real_row];

					if (sum < 0)
						sum = std::numeric_limits<int>::max();

					auto old_d = d[j];
					d[j] = std::min(old_d, sum);
					if (d[j] != old_d)
						changed_nodes[j] = true;
				}
			}
			end = std::chrono::system_clock::now();
			duration = end - start;
			//std::cout << "processed subgraph! " << duration.count() << std::endl;
		}

		std::cout << "completed iteration!" << std::endl;
		active_nodes = changed_nodes;
		changed_nodes.clear();
		changed_nodes.resize(round_up(graph.get_dimensions(), CROSSBAR_COLS));
		iterations++;
	}

	return d;
}


int main(int argc, char **argv) {
	stats = Statistics{};

	CrossbarOptions opts;
	opts.cols = CROSSBAR_COLS;
	opts.rows = CROSSBAR_ROWS;
	opts.input_resolution = 16;
	opts.cols_per_adc = 4;
	opts.adc = false;
	std::string graph_path(argv[1]);
	std::string graph = [&graph_path] {
		auto tmp = graph_path.substr(graph_path.find_last_of("/\\") + 1);
		return tmp.substr(0, tmp.find_first_of("."));
	}();

	std::ofstream log(graph + "-ours.log");
	crossbar.set_logfile(&log);
	crossbar.set_options(opts);
	crossbar.init();
	std::ofstream other_log(graph + "-ours-offsets.log");
	offsets.set_logfile(&other_log);
	offsets.set_options(opts);
	offsets.init();

	auto io_result = read_graph(graph_path);
	std::cout << "dimensions: " << io_result.dimensions << std::endl;
	GraphOrdering ordering(io_result);
	auto d = run_algorithm(5, ordering);
	for (auto val : d)
		std::cout << "d_val: " << val << std::endl;

	double efficiency = 0;
	for (auto val : efficiences)
		efficiency += val;
	efficiency /= efficiences.size();

	log << "efficiency " << efficiency << std::endl;
	return 0;
}
