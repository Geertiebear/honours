#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <utility>
#include <iostream>

#include "crossbar.hpp"

struct Tuple {
	Tuple(size_t i, size_t j ,int weight)
	: i(i), j(j), weight(weight) {}

	size_t i, j;
	int weight;
};

struct IOResult {
	std::vector<Tuple> tuples;
	size_t dimensions;
};

struct Statistics {
	uint64_t write_ops;
	uint64_t read_ops;
	uint64_t additions;
	uint64_t mins;
} stats;

struct Pair {
	Pair()
	: weight(std::numeric_limits<int>::max()), dest(0)
	{}
	Pair(int weight, size_t dest)
	: weight(weight), dest(dest)
	{}

	Pair operator+(int b) {
		return Pair{weight + b, dest};
	}

	int weight;
	size_t dest;
};

constexpr auto round_up(const auto a, const auto b) {
	return ((a + b - 1) / b)*b;
}

constexpr unsigned int ITERATIONS = 10;
constexpr unsigned int CROSSBAR_ROWS = 2048;
constexpr unsigned int CROSSBAR_COLS = 2048;
Crossbar<Pair> crossbar;
std::vector<double> efficiences;


struct GraphOrdering {
	GraphOrdering(IOResult &pio_result)
	: io_result(std::move(pio_result)), current_row(0), current_column(0), next_row(0),
	next_column(0) {
		// Sort the edges by increasing column.
		std::sort(io_result.tuples.begin(), io_result.tuples.end(), [] (auto a, auto b) {
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

		std::vector<Tuple> intermediate(lower_column, upper_column);
		std::sort(intermediate.begin(), intermediate.end(), [] (auto a, auto b) {
			if (a.i == b.i)
				return a.j < b.j;
			return a.i < b.i;
		});

		auto lower_row = std::lower_bound(intermediate.begin(), intermediate.end(), Tuple{current_row * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		auto upper_row = std::upper_bound(intermediate.begin(), intermediate.end(), Tuple{(current_row + 1) * CROSSBAR_ROWS - 1, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		next_column++;
		if (next_column * CROSSBAR_ROWS >= io_result.dimensions) {
			next_column = 0;
			next_row++;
		}

		std::vector<Tuple> result(lower_row, upper_row);

		// Sort the edges by increasing row.
		std::sort(result.begin(), result.end(), [] (auto a, auto b) {
			if (a.i == b.i)
				return a.j < b.j;
			return a.i < b.i;
		});

		return result;
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
		result.tuples.emplace_back(col, row, 1);
	}

	auto max = std::max_element(result.tuples.begin(), result.tuples.end(), [] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});
	result.dimensions = std::max(max->i, max->j) + 1;

	fclose(fp);
	return result;
}

// This function takes the "compressed" tuple representation
// of the graph, and expands it into the crossbar. The crossbar
// is initially filled with all "M" values, so we only need
// to fill in the edges that we read from the graph and nothing more.
// Also checks if we are not trying to insert a graph larger than the
// crossbar dimensions. The GraphR paper allows for this, but we don't
// implemented that yet.
static void expand_to_crossbar(const std::vector<Tuple> &tuples, size_t row_offset, size_t col_offset) {
	size_t row = 0;
	std::vector<Pair> vals(CROSSBAR_COLS);
	for (auto &tuple : tuples) {
		if ((tuple.i - row_offset) != row) {
			crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
			row = tuple.i - row_offset;
			vals.clear();
			vals.resize(CROSSBAR_COLS);
		}
		vals[tuple.j - col_offset] = Pair{tuple.weight, tuple.j - col_offset};
	}

	crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
	efficiences.push_back(crossbar.space_efficiency([] (Pair &val) {
		return val.weight != std::numeric_limits<int>::max();
	}));
}

static void reset_crossbar() {
	crossbar.clear();
}

// This function runs the actual SSSP algorithm, taking as parameter the start node,
// and outputting the "d" vector. We maintain a vector of active nodes,
// and we iterate and go through the crossbar row for which there is an active node.
// The function is pretty much exactly what GraphR also implements in their paper.
static void run_algorithm(std::vector<int> &d, size_t start_node, GraphOrdering &graph) {
	std::vector<bool> active_nodes(((graph.get_dimensions() + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS);
	std::vector<bool> changed_nodes(((graph.get_dimensions() + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS);

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
			reset_crossbar();

			auto tuples = graph.next_subgraph();
			expand_to_crossbar(tuples, graph.get_row_offset(), graph.get_col_offset());
			stats.write_ops++;

			for (size_t i = 0; i < CROSSBAR_ROWS; i++) {
				auto real_row = i + graph.get_row_offset();

				if (!active_nodes[real_row]) {
					continue;
				}

				is_active = true;
				auto array = crossbar.readWithInput(i, 0, CROSSBAR_COLS, d[real_row]);
				int j = 0;
				for (auto pair : array) {
					stats.read_ops++;
					auto real_col = j + graph.get_col_offset();
					stats.additions++;
					if (pair.weight < 0)
						pair.weight = std::numeric_limits<int>::max();

					auto old_d = d[real_col];
					d[real_col] = std::min(old_d, pair.weight);
					stats.mins++;
					if (d[real_col] != old_d) {
						changed_nodes[real_col] = true;
					}
					j++;
				}

			}
		}
		active_nodes = changed_nodes;
		changed_nodes.clear();
		changed_nodes.resize(((graph.get_dimensions() + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS);
		std::cout << "completed one iteration!" << std::endl;
		iterations++;
	}
}


int main(int argc, char **argv) {
	CrossbarOptions opts;
	opts.rows = CROSSBAR_ROWS;
	opts.cols = CROSSBAR_COLS;
	opts.input_resolution = 16;
	opts.cols_per_adc = 32;
	opts.adc = true;
	
	std::string graph_path(argv[1]);
	std::string graph = [&graph_path] {
		auto tmp = graph_path.substr(graph_path.find_last_of("/\\") + 1);
		return tmp.substr(0, tmp.find_first_of("."));
	}();

	std::ofstream log(graph + "-graphr.log");
	crossbar.set_logfile(&log);
	crossbar.set_options(opts);
	crossbar.init();

	stats = Statistics{};
	auto io_result = read_graph(graph_path);
	std::cout << "dimensions: " << io_result.dimensions << "\n";

	GraphOrdering ordering(io_result);
	std::vector<int> d(round_up(io_result.dimensions, CROSSBAR_COLS), std::numeric_limits<int>::max());
	run_algorithm(d, 5, ordering);
	for (auto d_val : d)
		std::cout << "d_val: " << d_val << std::endl;

	double efficiency = 0;
	for (auto val : efficiences)
		efficiency += val;
	efficiency /= efficiences.size();

	log << "efficiency " << efficiency << std::endl;

	return 0;
}
