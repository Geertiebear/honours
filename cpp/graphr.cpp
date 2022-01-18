#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <utility>
#include <iostream>

std::vector<int> crossbar;
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
	size_t dimensions;
};

struct Statistics {
	uint64_t write_ops;
	uint64_t read_ops;
	uint64_t additions;
	uint64_t mins;
} stats;

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
private:
	IOResult io_result;
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
	for (auto &tuple : tuples) {
		crossbar[(tuple.i - row_offset) * CROSSBAR_COLS + (tuple.j - col_offset)] = tuple.weight;
	}
}

static void reset_crossbar() {
	for (auto &val : crossbar)
		val = std::numeric_limits<int>::max();
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

	bool is_active = true;
	while (is_active) {
		is_active = false;
		graph.reset_position();
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
				for (size_t j = 0; j < CROSSBAR_COLS; j++) {
					stats.read_ops++;
					auto real_col = j + graph.get_col_offset();
					auto sum = crossbar[i * CROSSBAR_COLS + j] + d[real_row];
					stats.additions++;
					if (sum < 0)
						sum = std::numeric_limits<int>::max();
					auto old_d = d[real_col];
					d[real_col] = std::min(old_d, sum);
					stats.mins++;
					if (d[real_col] != old_d) {
						changed_nodes[real_col] = true;
					}
				}

			}
		}
		active_nodes = changed_nodes;
		changed_nodes.clear();
		changed_nodes.resize(((graph.get_dimensions() + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS);
	}
}


int main(int argc, char **argv) {
	stats = Statistics{};
	auto io_result = read_graph(argv[1]);
	std::cout << "dimensions: " << io_result.dimensions << std::endl;
	crossbar.resize(CROSSBAR_ROWS * CROSSBAR_COLS, std::numeric_limits<int>::max());

	GraphOrdering ordering(io_result);
	std::vector<int> d(((io_result.dimensions + CROSSBAR_COLS - 1)/CROSSBAR_COLS)*CROSSBAR_COLS, std::numeric_limits<int>::max());
	run_algorithm(d, 0, ordering);
	for (auto d_val : d)
		std::cout << "d_val: " << d_val << std::endl;

	std::cout << "Statistics:" << std::endl;
	std::cout << "writes: " << stats.write_ops << "\n"
		<< "reads: " << stats.read_ops << "\n"
		<< "additions: " << stats.additions << "\n"
		<< "mins: " << stats.mins << "\n";
	return 0;
}
