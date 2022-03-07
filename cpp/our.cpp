#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <iostream>

#include "crossbar.hpp"

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

std::vector<size_t> edge_counts;
constexpr unsigned int CROSSBAR_ROWS = 2048;
constexpr unsigned int CROSSBAR_COLS = 2048;
Crossbar<Pair, CROSSBAR_ROWS, CROSSBAR_COLS, 16, 4> crossbar("ours.log");

struct Tuple {
	Tuple(size_t i, size_t j ,int weight)
	: i(i), j(j), weight(weight) {}

	size_t i, j;
	int weight;
};

std::vector<Tuple> offsets;

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

		auto upper_column = std::upper_bound(io_result.tuples.begin(), io_result.tuples.end(), Tuple{0, (current_column + 1) * CROSSBAR_COLS, 0}, [] (auto a, auto b) {
			return a.j < b.j;
		});

		auto lower_row = std::lower_bound(lower_column, upper_column, Tuple{current_row * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		auto upper_row = std::upper_bound(lower_column, upper_column, Tuple{(current_row + 1) * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
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
		result.tuples.emplace_back(col, row, 1);

		auto max = std::max(row, col);

		result.degrees.resize(std::max(max, result.degrees.size()) + 1);
		result.degrees[row] += 1;
		result.degrees[col] += 1;
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
	std::cout << "result num_tuples: " << result.tuples.size() << std::endl;

	fclose(fp);
	return result;
}

static void expand_to_crossbar(IOResult &io_result, const std::vector<Tuple> &tuples) {
	if (tuples.size() >= CROSSBAR_ROWS * CROSSBAR_COLS)
		throw std::runtime_error("graph too large to fit into crossbar!");

	size_t row = 0, column = 0;
	std::array<Pair, CROSSBAR_COLS> vals;
	for (auto &tuple : tuples) {
		auto degree = io_result.degrees[tuple.i];
		if (degree > CROSSBAR_COLS)
			std::cout << "too large degree: " << degree << std::endl;
		assert(degree <= CROSSBAR_COLS);
		if (degree > CROSSBAR_COLS - column) {
			crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
			row++;
			column = 0;
			vals.fill(Pair{});
		}
		if (offsets.size() <= tuple.i) {
			offsets.resize(tuple.i + 1, Tuple{0, 0, std::numeric_limits<int>::max()});
			edge_counts.resize(tuple.i + 1);
		}

		auto offset = row * CROSSBAR_COLS + column;
		if (offsets[tuple.i].weight == std::numeric_limits<int>::max())
			offsets[tuple.i] = Tuple{row, column, 0};
		auto old_offset = offsets[tuple.i].i * CROSSBAR_COLS + offsets[tuple.i].j;
		if ((offset - old_offset) > degree)
			offsets[tuple.i] = Tuple{row, offsets[tuple.i].j, 0};
		edge_counts[tuple.i] = degree;
		vals[column] = Pair{tuple.weight, tuple.j};
		column++;
	}
	crossbar.writeRow(row, 0, CROSSBAR_COLS, vals);
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

	bool is_active = true;
	while (is_active) {
		is_active = false;
		graph.reset_position();

		while (graph.has_next_subgraph()) {
			reset_crossbar();

			auto tuples = graph.next_subgraph();
			if (tuples.empty())
				continue;
			expand_to_crossbar(graph.io_result, tuples);
			stats.write_ops++;

			for (size_t i = 0; i < CROSSBAR_COLS; i++) {
				auto real_row = i + graph.get_row_offset();
				if (!active_nodes[i])
					continue;

				is_active = true;
				auto num_edges = edge_counts[real_row];
				auto offset = offsets[real_row];
				auto read_result = crossbar.readWithInput(offset.i, offset.j, num_edges, d[real_row]);
				for (auto pair : read_result) {
					auto j = pair.dest;
					auto sum = pair.weight;

					if (sum < 0)
						sum = std::numeric_limits<int>::max();

					auto old_d = d[j];
					d[j] = std::min(old_d, sum);
					if (d[j] != old_d)
						changed_nodes[j] = true;
				}
			}
		}

		active_nodes = changed_nodes;
		changed_nodes.clear();
		changed_nodes.resize(round_up(graph.get_dimensions(), CROSSBAR_COLS));
	}

	return d;
}


int main(int argc, char **argv) {
	stats = Statistics{};
	auto io_result = read_graph(argv[1]);
	std::cout << "dimensions: " << io_result.dimensions << std::endl;
	GraphOrdering ordering(io_result);
	auto d = run_algorithm(5, ordering);
	for (auto val : d)
		std::cout << "d val: " << val << std::endl;

	std::cout << "Statistics:" << std::endl;
	std::cout << "writes: " << stats.write_ops << "\n"
		<< "reads: " << stats.read_ops << "\n"
		<< "additions: " << stats.additions << "\n"
		<< "mins: " << stats.mins << "\n";
	return 0;
}
