#ifndef GRAPH_HPP
#define GRAPH_HPP

#include <stddef.h>
#include <string>
#include <vector>

struct Tuple {
	size_t i, j;
	int weight;
};

struct SubGraph {
	size_t row_offset, col_offset;
	std::vector<Tuple> tuples;
};

class Graph {
public:
	Graph(const std::string &filepath, size_t max_row, size_t max_col);

	Graph(const Graph &) = delete;
	Graph(Graph &&) = default;

	Graph operator= (const Graph &) = delete;
	Graph &operator= (Graph &&) = default;

	size_t get_dimensions() const;
	size_t get_num_subgraphs() const;
	size_t get_subgraph_row(size_t subgraph) const;
	size_t get_subgraph_col(size_t subgraph) const;
	SubGraph get_subgraph_at(size_t subgraph) const;
private:
	size_t _max_row, _max_col;
	size_t _dimensions;
	std::vector<Tuple> _tuples;
};

#endif // GRAPH_HPP
