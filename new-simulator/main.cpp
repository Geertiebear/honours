#include <iostream>
#include <optional>
#include <functional>
#include "experiment.hpp"
#include "util.hpp"
#include "graph.hpp"

int main(int argc, char **argv) {

	if (!argv[1]) {
		std::cout << "Please input a graph!" << std::endl;
		return 1;
	}

	auto graph = std::make_shared<Graph>(argv[1], 128, 128);
	std::cout << "Read graph of size " << graph->get_dimensions() << std::endl;

	{
		struct Data {
			Data(unsigned int start, size_t graph_dimension,
					size_t crossbar_size)
			: is_active(true),
			active_nodes(round_up(graph_dimension, crossbar_size), false),
			changed_nodes(round_up(graph_dimension, crossbar_size), false),
			d(round_up(graph_dimension, crossbar_size),
					std::numeric_limits<short>::max())
			{
				d[start] = 0;
				active_nodes[start] = true;
			}
			bool is_active;
			std::vector<bool> active_nodes;
			std::vector<bool> changed_nodes;
			std::vector<short> d;
		};

		auto min = [] (auto a, auto b) {
			return std::min(a, b);
		};

		auto row_func = [] (Data &data, size_t real_row)
		-> std::optional<short> {
			if (!data.active_nodes[real_row])
				return std::nullopt;
			data.is_active = true;
			return data.d[real_row];
		};

		auto elem_func = [] (Data &data, Graphr::Data &elem, size_t j) {
			auto old_d = data.d[j];
			data.d[j] = std::min(old_d, elem.weight);
			if (data.d[j] != old_d)
				data.changed_nodes[j] = true;
		};

		auto aggregate_func = [min] (Data &data,
				const std::vector<Data> &local_datas) -> bool {
			for (auto &local_data : local_datas) {
				data.is_active |= local_data.is_active;
				vector_binop(data.changed_nodes,
						local_data.changed_nodes,
						std::bit_or<void>());
				vector_binop(data.d, local_data.d, min);
			}

			data.active_nodes = data.changed_nodes;
			std::fill(data.changed_nodes.begin(),
					data.changed_nodes.end(), false);

			return data.is_active;
		};

		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 32;
		options.read_device = ADC;
		Experiment<Graphr, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			data.is_active = false;
			experiment.run_kernel(row_func, elem_func);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		for (auto elem : data.d)
			std::cout << "d: " << elem << std::endl;
	}
}
