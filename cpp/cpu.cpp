#include <vector>
#include <string.h>
#include <stdio.h>
#include <stddef.h>
#include <iostream>
#include <limits>
#include <algorithm>

struct Pair {
	size_t j;
	int weight;
};

std::vector<std::vector<Pair>> adjacency_list;
std::vector<int> d;

static void read_graph(const std::string &path) {
	FILE *fp = fopen(path.c_str(), "r");

	char line[256];
	while (fgets(line, 256, fp)) {
		if (!line[0] || line[0] == '%' || line[0] == '#')
			continue;

		size_t row, col;
		sscanf(line, "%ld %ld", &row, &col);

		auto max = std::max(row, col);
		adjacency_list.resize(std::max(max + 1, adjacency_list.size()));
		adjacency_list[row].emplace_back(col, 1);
		adjacency_list[col].emplace_back(row, 1);
	}

	d.resize(adjacency_list.size(), std::numeric_limits<int>::max());
	fclose(fp);
}

static void run_algorithm(int start) {
	std::vector<bool> active_nodes(adjacency_list.size());
	std::vector<bool> changed_nodes(adjacency_list.size());

	active_nodes[start] = true;
	d[start] = 0;

	bool is_active = true;
	while (is_active) {
		is_active = false;

		for (size_t i = 0; i < adjacency_list.size(); i++) {
			if (!active_nodes[i])
				continue;

			is_active = true;

			for (auto pair : adjacency_list[i]) {
				auto j = pair.j;
				auto sum = pair.weight + d[i];

				if (sum < 0)
					sum = std::numeric_limits<int>::max();

				auto old_d = d[j];
				d[j] = std::min(old_d, sum);
				if (d[j] != old_d)
					changed_nodes[j] = true;
			}
		}
		active_nodes = changed_nodes;
		changed_nodes.clear();
		changed_nodes.resize(active_nodes.size());
	}
}

int main(int argc, char **argv) {
	read_graph(argv[1]);	
	std::cout << "dimensions: " << adjacency_list.size() << std::endl;
	run_algorithm(5);
	for (auto val : d)
		std::cout << "d_val: " << val << std::endl;
	return 0;
}
