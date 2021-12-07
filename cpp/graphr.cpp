#include <stdio.h>
#include <string>
#include <vector>
#include <algorithm>
#include <limits>
#include <stdexcept>

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

static void expand_to_crossbar(IOResult &io_result) {
	if (io_result.dimensions >= CROSSBAR_ROWS)
		throw std::runtime_error("graph too large to fit into crossbar!");

	for (auto &tuple : io_result.tuples)
		crossbar[tuple.i * CROSSBAR_COLS + tuple.j] = tuple.weight;
}

int main(int argc, char **argv) {
	auto io_result = read_graph(argv[1]);
	crossbar.resize(CROSSBAR_ROWS * CROSSBAR_COLS, std::numeric_limits<int>::max());
	expand_to_crossbar(io_result);
	return 0;
}
