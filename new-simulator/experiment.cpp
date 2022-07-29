#include "experiment.hpp"

#include <iostream>
#include <assert.h>

// GraphR

Stats Graphr::expand_to_crossbar(const SubGraph &sub_graph) {
	Stats stats;
	if (sub_graph.tuples.empty())
		return stats;

	_row_offset = sub_graph.row_offset;
	_col_offset = sub_graph.col_offset;

	const auto &tuples = sub_graph.tuples;
	const auto max_rows = _crossbar.get_num_rows();
	const auto max_cols = _crossbar.get_num_cols();

	size_t row = 0;
	std::vector<Data> vals(max_cols);
	for (auto &tuple : tuples) {
		if ((tuple.i - _row_offset) != row) {
			stats += _crossbar.writeRow(row, 0, max_cols, vals);
			row = tuple.i - _row_offset;
			vals.clear();
			vals.resize(max_cols);
		}
		vals[tuple.j - _col_offset] = Data{1};
	}

	stats += _crossbar.writeRow(row, 0, max_rows, vals);
	stats.efficiency += _crossbar.space_efficiency([] (Data &val) {
		return val.weight != std::numeric_limits<short>::max();
	});
	populated = true;
	return stats;
}

Stats Graphr::clear() {
	populated = false;
	return _crossbar.clear();
}

// SparseMEM

Stats SparseMEM::expand_to_crossbar(const SubGraph &sub_graph) {
	Stats stats;
	if (sub_graph.tuples.empty())
		return stats;

	_row_offset = sub_graph.row_offset;
	_col_offset = sub_graph.col_offset;

	const auto &tuples = sub_graph.tuples;
	const auto max_rows = _data_crossbar.get_num_rows();
	const auto max_cols = _data_crossbar.get_num_cols();

	if (tuples.size() >= max_rows * max_cols)
		throw std::runtime_error("graph too large to fit into crossbar!");

	size_t row = 0, column = 0;
	std::vector<Data> vals(max_rows);
	std::vector<Offset> offset_array(max_rows);

	std::vector<unsigned int> degrees(max_rows);
	for (auto &tuple : tuples)
		degrees[tuple.i - _row_offset]++;

	for (auto &tuple : tuples) {
		auto degree = degrees[tuple.i - _row_offset];
		if (degree >= max_rows)
			std::cout << "too large degree: " << degree << std::endl;
		assert(degree < max_rows);

		auto offset = row * max_rows + column;
		if (offset_array[tuple.i - _row_offset].start == std::numeric_limits<size_t>::max()) {
			if (degree > max_rows - column) {
				stats += _data_crossbar.writeRow(row, 0, max_rows, vals);
				row++;
				column = 0;
				vals.clear();
				vals.resize(max_rows);
			}
			offset = row * max_rows + column;
			offset_array[tuple.i - _row_offset] = Offset{offset, offset + degree};
		} else {
			assert(offset_array[tuple.i - _row_offset].stop >= offset);
		}
		assert(tuple.i - _row_offset < offset_array.size());

		assert(tuple.j - _col_offset < max_rows);
		vals[column] = Data{static_cast<unsigned short>(tuple.j - _col_offset)};
		column++;
	}
	stats += _data_crossbar.writeRow(row, 0, max_rows, vals);
	stats += _offset_crossbar.writeRow(0, 0, max_rows, offset_array);

	populated = true;

	stats.efficiency += _data_crossbar.space_efficiency([] (Data &val) {
		return val.dest != std::numeric_limits<unsigned short>::max();
	});
	stats.num_efficiencies++;
	return stats;
}

Stats SparseMEM::clear() {
	populated = false;

	Stats stats;
	stats += _data_crossbar.clear();
	stats += _offset_crossbar.clear();
	return stats;
}
