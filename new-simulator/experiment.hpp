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

class Graphr {
public:
	struct Data {
		Data()
		: weight(std::numeric_limits<short>::max())
		{}

		Data(short weight)
		: weight(weight)
		{}

		Data operator+(short other_weight) {
			return Data{weight + static_cast<short>(other_weight)};
		}

		short weight;
	};

	Graphr(CrossbarOptions crossbar_options)
	: _crossbar(crossbar_options)
	{}

	template<typename RowFunc, typename ElementFunc, typename Data>
	Stats run_kernel(RowFunc row_func, ElementFunc element_func, Data &data) {
		Stats stats;
		if (!populated)
			return stats;

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
					elem.weight = std::numeric_limits<short>::max();

				element_func(data, elem, j);
				j++;
			}
		}

		return stats;
	}

	Stats expand_to_crossbar(const SubGraph &sub_graph);
	Stats clear();
private:
	Crossbar<Data> _crossbar;
	size_t _row_offset = 0, _col_offset = 0;
	bool populated = false;
};

class SparseMEM {
public:
	struct Data {
		Data()
		: dest(std::numeric_limits<unsigned short>::max())
		{}

		Data(unsigned short dest)
		: dest(dest)
		{}

		unsigned short dest;
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

	template<typename RowFunc, typename ElementFunc, typename Data>
	Stats run_kernel(RowFunc row_func, ElementFunc elem_func, Data &data) {
		Stats stats;
		if (!populated)
			return stats;

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

		return stats;
	}

	Stats expand_to_crossbar(const SubGraph &sub_graph);
	Stats clear();
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
	template <typename RowFunc, typename ElementFunc>
	void run_kernel(RowFunc row_func, ElementFunc element_func) {
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
				stats += approach.expand_to_crossbar(subgraph);
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
