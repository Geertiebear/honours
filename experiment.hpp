#ifndef EXPERIMENTS_HPP
#define EXPERIMENTS_HPP

#include <vector>
#include <memory>
#include <omp.h>
#include <optional>
#include <limits>
#include <stdint.h>

#include "stats.hpp"
#include "crossbar.hpp"
#include "graph.hpp"

template <bool PageRank = false>
class Graphr {
public:
	struct Data {
		Data()
		{
			if constexpr (PageRank) {
				weight = 0;
			} else {
				weight = std::numeric_limits<float>::max();
			}
		}

		explicit Data(float weight)
		: weight(weight)
		{}

		Data operator+(float other_weight) {
			return Data{weight + other_weight};
		}

		Data operator+(Data other) {
			return Data{weight + other.weight};
		}

		float weight;
	};

	Graphr(CrossbarOptions crossbar_options)
	: _crossbar(crossbar_options)
	{}

	template<typename RowFunc, typename ElementFunc, typename Data,
		bool MultiRow = std::is_invocable_v<RowFunc, Data&>>
	Stats run_kernel(RowFunc row_func, ElementFunc element_func, Data &data) {
		Stats stats;
		if (!populated)
			return stats;

		if constexpr (!MultiRow) {
			for (size_t i = 0; i < _crossbar.get_num_rows(); i++) {
				auto real_row = i + _row_offset;

				auto row_input = row_func(data, real_row);
				if (!row_input)
					continue;

				auto [read_stats, array] = _crossbar.readWithInput(
						i, 0, _crossbar.get_num_cols(),
						*row_input);
				stats += read_stats;

				size_t j = _col_offset;
				for (auto elem : array) {
					if (elem.weight < 0)
						elem.weight = std::numeric_limits<float>::max();

					element_func(data, elem, j);
					j++;
				}
			}
		} else {
			const auto row_input = row_func(data);
			if (!row_input)
				return stats;

			auto [read_stats, array] = _crossbar.multiReadWithInput(
					0, _crossbar.get_num_rows(),
					0, _crossbar.get_num_cols(),
					*row_input);
			stats += read_stats;

			size_t j = _col_offset;
			for (auto elem : array) {
				element_func(data, elem, j);
				j++;
			}
		}

		return stats;
	}

	Stats expand_to_crossbar(const SubGraph &sub_graph) {
		Stats stats;
		const auto max_rows = _crossbar.get_num_rows();
		const auto max_cols = _crossbar.get_num_cols();

		// Optimisation
		if (sub_graph.tuples.empty())
			return stats;

		std::vector<Data> vals(max_cols);
		if (sub_graph.tuples.empty()) {
			for (size_t i = 0; i < max_rows; i++)
				stats += _crossbar.writeRow(i, 0, max_cols, vals);
			return stats;
		}

		_row_offset = sub_graph.row_offset;
		_col_offset = sub_graph.col_offset;

		const auto &tuples = sub_graph.tuples;

		size_t row = 0;
		for (auto &tuple : tuples) {
			if ((tuple.i - _row_offset) != row) {
				stats += _crossbar.writeRow(row, 0, max_cols, vals);
				row = tuple.i - _row_offset;
				vals.clear();
				vals.resize(max_cols);
			}
			vals[tuple.j - _col_offset] = Data{tuple.weight};
		}

		stats += _crossbar.writeRow(row, 0, max_cols, vals);

		if (row != max_rows) {
			vals.clear();
			vals.resize(max_cols);
			for (size_t i = row + 1; i < max_rows; i++)
				stats += _crossbar.writeRow(i, 0, max_cols, vals);
		}

		stats.efficiency += _crossbar.space_efficiency([] (Data &val) {
			return val.weight != std::numeric_limits<float>::max();
		});
		stats.num_efficiencies++;
		populated = true;
		return stats;
	}

	Stats clear() {
		populated = false;
		return _crossbar.clear();
	}
private:
	Crossbar<Data> _crossbar;
	size_t _row_offset = 0, _col_offset = 0;
	bool populated = false;
};

template <bool PageRank = false>
class SparseMEM {
public:
	struct Data {
		Data()
		: dest(std::numeric_limits<unsigned short>::max())
		{
			if constexpr (PageRank) {
				weight = 0;
			} else {
				weight = std::numeric_limits<float>::max();
			}
		}

		Data(unsigned short dest)
		: dest(dest), weight(1)
		{}

		Data(unsigned short dest, float weight)
		: dest(dest), weight(weight)
		{}

		unsigned short dest;
		float weight;
	};

	struct Offset {
		Offset()
		: start(std::numeric_limits<size_t>::max()),
		stop(std::numeric_limits<size_t>::max())
		{}

		Offset(size_t start, size_t stop)
		: start(start), stop(stop)
		{}

		size_t start, stop;
	};

	SparseMEM(CrossbarOptions options)
	: _options(options), _data_crossbar(options),
	_offset_crossbar(options)
	{}

	template<typename RowFunc, typename ElementFunc, typename Data,
		bool MultiRow = std::is_invocable_v<RowFunc, Data&>>
	Stats run_kernel(RowFunc row_func, ElementFunc elem_func, Data &data) {
		Stats stats;
		if (!populated)
			return stats;

		if constexpr (!MultiRow) {
			for (size_t i = 0; i < _data_crossbar.get_num_rows(); i++) {
				auto real_row = i + _row_offset;

				auto row_input = row_func(data, real_row);
				if (!row_input)
					continue;

				auto [offset_stats, offset_res] = _offset_crossbar.readRow(
						0, i, 1);
				stats += offset_stats;
				auto offset = offset_res[0];
				if (offset.start == std::numeric_limits<int>::max())
					continue;

				auto num_edges = offset.stop - offset.start;
				auto offset_i = offset.start / _data_crossbar.get_num_cols();
				auto offset_j = offset.start % _data_crossbar.get_num_cols();
				auto [read_stats, read_res] = _data_crossbar.readRow(
						offset_i, offset_j, num_edges);
				stats += read_stats;
				_add_dynamic_stats(stats, num_edges);

				for (auto elem : read_res) {
					auto j = elem.dest + _col_offset;
					elem_func(data, j, *row_input);
				}
			}
		} else {
			const auto row_input = row_func(data);
			if (!row_input)
				return stats;

			for (size_t i = 0; i < _data_crossbar.get_num_rows(); i++) {
				auto [offset_stats, offset_res] = _offset_crossbar.readRow(
						0, i, 1);
				stats += offset_stats;
				auto offset = offset_res[0];
				if (offset.start == std::numeric_limits<int>::max())
					continue;

				auto num_edges = offset.stop - offset.start;
				auto offset_i = offset.start / _data_crossbar.get_num_cols();
				auto offset_j = offset.start % _data_crossbar.get_num_cols();
				auto [read_stats, read_res] = _data_crossbar.readRow(
						offset_i, offset_j, num_edges);
				stats += read_stats;
				_add_dynamic_stats(stats, num_edges);

				for (auto elem : read_res) {
					auto j = elem.dest + _col_offset;
					elem_func(data, j, elem.weight);
				}
			}

			for (size_t j = 0; j < _data_crossbar.get_num_cols(); j++)
				elem_func(data, j + _col_offset, *row_input);
		}

		return stats;
	}

	Stats expand_to_crossbar(const SubGraph &sub_graph) {
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
			if (degree > max_rows)
				std::cout << "too large degree: " << degree << std::endl;
			assert(degree <= max_rows);

			auto offset = row * max_rows + column;
			if (offset_array[tuple.i - _row_offset].start == std::numeric_limits<size_t>::max()) {
				if (degree > max_rows - column) {
					stats += _data_crossbar.writeRow(row, 0, column + 1, vals);
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
			vals[column] = Data{static_cast<unsigned short>(tuple.j - _col_offset)
				, tuple.weight};
			column++;
		}
		stats += _data_crossbar.writeRow(row, 0, column, vals);
		stats += _offset_crossbar.writeRow(0, 0, row, offset_array);

		populated = true;

		stats.efficiency += _data_crossbar.space_efficiency([] (Data &val) {
			return val.dest != std::numeric_limits<unsigned short>::max();
		});
		stats.num_efficiencies++;
		return stats;
	}

	Stats clear() {
		populated = false;

		Stats stats;
		stats += _data_crossbar.clear();
		stats += _offset_crossbar.clear();
		return stats;
	}
private:
	void _add_dynamic_stats(Stats &stats, int num) {
		stats.total_periphery_time += num * _options.dynamic_latency;
		stats.total_periphery_energy += num * _options.dynamic_energy;
	}

	CrossbarOptions _options;
	Crossbar<Data> _data_crossbar;
	Crossbar<Offset> _offset_crossbar;
	size_t _row_offset = 0, _col_offset = 0;
	bool populated = false;
};

template <typename Approach, typename Data>
class Experiment {
public:
	template <typename... DataInit>
	Experiment(CrossbarOptions crossbar_options, DataInit... data_init) :
	_global_data(std::forward<DataInit>(data_init)...) {
		const auto num_threads = omp_get_max_threads();
		_local_data.resize(num_threads,
				Data{std::forward<DataInit>(data_init)...});
		_local_stats.resize(num_threads);
		_approaches.resize(num_threads, crossbar_options);
	}

	// Experiments should be unique.
	Experiment(const Experiment &) = delete;
	Experiment(Experiment &&) = default;

	Experiment operator= (const Experiment &) = delete;
	Experiment &operator= (Experiment &&) = default;

	// Runs one full iteration of kernel on multiple crossbars.
	template <typename RowFunc, typename ElementFunc, typename SubgraphFunc>
	void run_kernel(RowFunc row_func, ElementFunc element_func, SubgraphFunc
			subgraph_func) {
		const auto num_threads = omp_get_max_threads();

		_local_stats.clear();
		_local_stats.resize(num_threads);

		std::fill(_local_data.begin(), _local_data.end(),
				_global_data);

		#pragma omp parallel
		{
			const auto t = omp_get_thread_num();
			auto &local_data = _local_data[t];
			auto &approach = _approaches[t];
			auto &stats = _local_stats[t];

			const auto num_subgraphs = _graph->get_num_subgraphs();
			auto subgraphs_per_t = num_subgraphs / num_threads;
			if (t == num_threads - 1)
				subgraphs_per_t += num_subgraphs % num_threads;
			
			for (size_t i = 0; i < subgraphs_per_t; i++) {
				const auto index = i + subgraphs_per_t * t;
				const auto subgraph = _graph->get_subgraph_at(index);

				stats += approach.clear();
				stats += approach.expand_to_crossbar(
						subgraph_func(subgraph, local_data));
				stats += approach.run_kernel(row_func,
						element_func, local_data);
			}
		}
	}

	template <typename F>
	bool aggregate_data(F f) {
		for (auto &stats : _local_stats)
			_global_stats += stats;

		return f(_global_data, _local_data);
	}

	inline void set_graph(std::shared_ptr<Graph> graph) {
		_graph = graph;
	}

	Data &get_data() {
		return _global_data;
	}

	Stats &get_stats() {
		return _global_stats;
	}
private:
	std::shared_ptr<Graph> _graph;
	Data _global_data;
	Stats _global_stats;
	std::vector<Data> _local_data;
	std::vector<Stats> _local_stats;
	std::vector<Approach> _approaches;
};

#endif // EXPERIMENTS_HPP
