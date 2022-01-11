#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <iostream>

struct Pair {
	Pair(int weight, size_t dest)
	: weight(weight), dest(dest) {}
	int weight;
	size_t dest;
};

std::vector<size_t> offsets;
std::vector<size_t> edge_counts;
std::vector<Pair> crossbar;
constexpr unsigned int CROSSBAR_ROWS = 512;
constexpr unsigned int CROSSBAR_COLS = 512;

struct Tuple {
	Tuple(size_t i, size_t j ,int weight)
	: i(i), j(j), weight(weight) {}

	size_t i, j;
	int weight;
};

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

		next_row++;
		if (next_row * CROSSBAR_ROWS >= io_result.dimensions) {
			next_row = 0;
			next_column++;
		}

		return std::vector<Tuple>(lower_row, upper_row);
	}

	bool has_next_subgraph() const {
		// We are done when we are finished with the last column.
		return (next_column) * CROSSBAR_COLS < ((io_result.dimensions + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS;
	}

	bool last_column() const {
		return (current_column + 1) * CROSSBAR_COLS < ((io_result.dimensions + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS;
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

	char line[128];
	while (fgets(line, 128, fp)) {
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
	std::sort(result.tuples.begin(), result.tuples.end(), [&] (auto a, auto b) {
		return result.degrees[a.i] < result.degrees[b.i];
	});
	result.dimensions = std::max(max->i, max->j) + 1;

	fclose(fp);
	return result;
}

static void expand_to_crossbar(IOResult &io_result, const std::vector<Tuple> &tuples) {
	if (tuples.size() >= CROSSBAR_ROWS * CROSSBAR_COLS)
		throw std::runtime_error("graph too large to fit into crossbar!");

	size_t row = 0, column = 0;
	for (auto &tuple : tuples) {
		std::cout << "adding " << tuple.i << std::endl;
		auto degree = io_result.degrees[tuple.i];
		assert(degree <= CROSSBAR_COLS);
		if (degree > CROSSBAR_COLS - column) {
			row++;
			column = 0;
		}
		if (offsets.size() <= tuple.i) {
			offsets.resize(tuple.i + 1);
			offsets[tuple.i] = row * CROSSBAR_COLS + column;

			edge_counts.resize(tuple.i + 1);
			edge_counts[tuple.i] = degree;
		}

		crossbar[row * CROSSBAR_COLS + column] = Pair{tuple.weight, tuple.j};
		column++;
	}
}

static void reset_crossbar() {
	for (auto &val : crossbar)
		val = Pair{std::numeric_limits<int>::max(), 0};
}

// This function runs the actual SSSP algorithm, taking as parameter the start node,
// and outputting the "d" vector. We maintain a vector of active nodes,
// and we iterate and go through the crossbar row for which there is an active node.
// The function is pretty much exactly what GraphR also implements in their paper.
static std::vector<int> run_algorithm(size_t start_node, GraphOrdering &graph) {
	std::vector<int> d(CROSSBAR_COLS, std::numeric_limits<int>::max());
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
			expand_to_crossbar(graph.io_result, tuples);

			for (size_t i = 0; i < active_nodes.size(); i++) {
				auto real_row = i + graph.get_row_offset();
				if (!active_nodes[i])
					continue;

				std::cout << "active node: " << real_row << std::endl;

				is_active = true;
				auto num_edges = edge_counts[real_row];
				std::cout << "num_edges: " << num_edges << std::endl;
				auto offset = offsets[real_row];
				for (size_t n = 0; n < num_edges; n++) {
					std::cout << "summing for active node " << real_row << std::endl;
					auto j = crossbar[offset + n].dest;
					auto sum = crossbar[offset + n].weight + d[real_row];

					if (sum < 0)
						sum = std::numeric_limits<int>::max();

					auto old_d = d[j];
					d[j] = std::min(old_d, sum);
					if (d[j] != old_d) {
						changed_nodes[j] = true;
						std::cout << "activated node: " << j << std::endl;
					}
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
	auto io_result = read_graph(argv[1]);
	GraphOrdering ordering(io_result);
	crossbar.resize(CROSSBAR_ROWS * CROSSBAR_COLS, Pair{std::numeric_limits<int>::max(), 0});
	auto d = run_algorithm(0, ordering);
	for (auto val : d)
		std::cout << "d val: " << val << std::endl;

	return 0;
}
