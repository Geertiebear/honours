#include "graph.hpp"
#include "util.hpp"

#include <stdio.h>
#include <algorithm>
#include <iostream>

Graph::Graph(const std::string &filepath, size_t max_row, size_t max_col)
	: _max_row(max_row), _max_col(max_col) {
	FILE *fp = fopen(filepath.c_str(), "r");

	char line[256];
	while(fgets(line, 256, fp)) {
		if (!line[0] || line[0] == '%' || line[0] == '#')
			continue;

		size_t row, col;
		sscanf(line, "%ld %ld", &row, &col);

		_tuples.emplace_back(row, col, 1);
	}

	auto max = std::max_element(_tuples.begin(), _tuples.end(),
			[] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});
	_dimensions = std::max(max->i, max->j) + 1;
	std::sort(_tuples.begin(), _tuples.end(), [] (auto a, auto b) {
		if (a.j == b.j)
			return a.i < b.i;
		return a.j < b.j;
	});
	fclose(fp);
}

SubGraph Graph::next_subgraph() {
	_current_row = _next_row;
	_current_col = _next_col;

	auto col_comp = [] (auto a, auto b) -> bool {
		return a.j < b.j;
	};

	auto lower_col = std::lower_bound(_tuples.begin(), _tuples.end(),
			Tuple{0, _current_col * _max_col, 0}, col_comp);
	auto upper_col = std::upper_bound(_tuples.begin(), _tuples.end(),
			Tuple{0, (_current_col + 1) * _max_col - 1, 0}, col_comp);

	std::vector<Tuple> temp(lower_col, upper_col);
	std::sort(temp.begin(), temp.end(), [] (auto a, auto b) {
		if (a.i == b.i)
			return a.j < b.j;
		return a.i < b.i;
	});

	auto row_comp = [] (auto a, auto b) -> bool {
		return a.i < b.i;
	};

	auto lower_row = std::lower_bound(temp.begin(), temp.end(),
			Tuple{_current_row * _max_row, 0, 0}, row_comp);
	auto upper_row = std::upper_bound(temp.begin(), temp.end(),
			Tuple{(_current_row + 1) * _max_row - 1, 0, 0}, row_comp);

	_next_col++;
	if (_next_col * _max_col >= _dimensions) {
		_next_col = 0;
		_next_row++;
	}

	return SubGraph{_current_row * _max_row, _current_col * _max_col,
		std::vector<Tuple>(lower_row, upper_row)};
}

bool Graph::has_next_subgraph() const {
	auto limit = round_up(_dimensions, _max_row);
	return _next_row * _max_row < limit;
}

bool Graph::last_column() const {
	auto limit = round_up(_dimensions, _max_col);
	return (_current_row + 1) * _max_col < limit;
}

void Graph::reset_position() {
	_current_row = _current_col = _next_row = _next_col = 0;
}

size_t Graph::get_dimensions() const {
	return _dimensions;
}
