#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>
#include <cassert>
#include <utility>

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

struct GraphOrdering {
	GraphOrdering(IOResult &io_result)
	: io_result(std::move(io_result)), last_row(0), last_column(0) {
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

		auto lower_column = std::lower_bound(io_result.tuples.begin(), io_result.tuples.end(), Tuple{0, last_column * CROSSBAR_COLS, 0}, [] (auto a, auto b) {
			return a.j < b.j;
		});

		auto upper_column = std::upper_bound(io_result.tuples.begin(), io_result.tuples.end(), Tuple{0, (last_column + 1) * CROSSBAR_COLS, 0}, [] (auto a, auto b) {
			return a.j < b.j;
		});

		auto lower_row = std::lower_bound(lower_column, upper_column, Tuple{last_row * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		auto upper_row = std::upper_bound(lower_column, upper_column, Tuple{(last_row + 1) * CROSSBAR_ROWS, 0, 0}, [] (auto a, auto b) {
			return a.i < b.i;
		});

		last_row++;
		if (last_row * CROSSBAR_ROWS >= io_result.dimensions) {
			last_row = 0;
			last_column++;
		}

		return std::vector<Tuple>(lower_row, upper_row);
	}

	bool has_next_subgraph() const {
		return last_row * CROSSBAR_ROWS < io_result.dimensions && last_column * CROSSBAR_COLS < io_result.dimensions;
	}

	void reset_position() {
		last_row = 0;
		last_column = 0;
	}
private:
	IOResult io_result;
	size_t last_row;
	size_t last_column;
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
	}

	auto max = std::max_element(result.tuples.begin(), result.tuples.end(), [] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});
	result.dimensions = std::max(max->i, max->j);

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
static void expand_to_crossbar(const std::vector<Tuple> &tuples) {
	for (auto &tuple : tuples)
		crossbar[tuple.i * CROSSBAR_COLS + tuple.j] = tuple.weight;
}

static void reset_crossbar() {
	for (auto &val : crossbar)
		val = std::numeric_limits<int>::max();
}

// This function runs the actual SSSP algorithm, taking as parameter the start node,
// and outputting the "d" vector. We maintain a vector of active nodes,
// and we iterate and go through the crossbar row for which there is an active node.
// The function is pretty much exactly what GraphR also implements in their paper.
static std::vector<int> run_algorithm(size_t start_node, GraphOrdering &graph) {
	std::vector<int> d(CROSSBAR_COLS, std::numeric_limits<int>::max());
	std::vector<bool> active_nodes(CROSSBAR_COLS);

	assert(start_node < CROSSBAR_COLS);
	active_nodes[start_node] = true;

	bool is_active = true;
	while (is_active) {
		while (graph.has_next_subgraph()) {
			reset_crossbar();

			auto tuples = graph.next_subgraph();
			expand_to_crossbar(tuples);

			for (size_t i = 0; i < active_nodes.size(); i++) {
				if (!active_nodes[i])
					continue;

				is_active = true;
				for (size_t j = 0; j < CROSSBAR_COLS; j++) {
					auto sum = crossbar[i * CROSSBAR_COLS + j] + d[i];
					auto old_d = d[j];
					d[j] = std::min(old_d, sum);
					if (d[j] != old_d)
						active_nodes[j] = true;
				}

				active_nodes[i] = false;

			}
		}
	}

	return d;
}


int main(int argc, char **argv) {
	auto io_result = read_graph(argv[1]);
	crossbar.resize(CROSSBAR_ROWS * CROSSBAR_COLS, std::numeric_limits<int>::max());

	GraphOrdering ordering(io_result);
	auto d = run_algorithm(0, ordering);
	return 0;
}
