#include <iostream>
#include <limits>
#include <optional>
#include <functional>
#include <cmath>
#include "experiment.hpp"
#include "util.hpp"
#include "graph.hpp"

namespace {

	constexpr float READ_TIME = 10e-9;
	constexpr float WRITE_TIME = 100e-9;
	constexpr float ADC_LATENCY = 1e-9;
	constexpr float READ_ENERGY = 40e-15;
	constexpr float WRITE_ENERGY = 20e-12;
	constexpr float ADC_ENERGY = 2e-12;
	constexpr float SA_LATENCY = 1e-9;
	constexpr float SA_ENERGY = 10e-15;
	constexpr float STATIC_ENERGY = 11.8e-12;
	constexpr float STATIC_LATENCY = 0.5e-9;
	constexpr float DYNAMIC_LATENCY = 1.1e-9;
	constexpr float DYNAMIC_ENERGY = 28.8e-12;
}

template<typename T>
void add_vectors(std::vector<T> &a, const std::vector<T> &b) {
	assert(a.size() == b.size());
	for (size_t i = 0; i < a.size(); i++)
		a[i] += b[i];
}

void run_sssp(char **argv) {
	if (!argv[1]) {
		std::cout << "Please input a graph!" << std::endl;
		exit(1);
	}

	auto graph = std::make_shared<Graph>(argv[1], 128, 128);
	std::cout << "Read graph of size " << graph->get_dimensions() << std::endl;

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

	auto same_subgraph = [] (const SubGraph &subgraph, Data &data) {
		return subgraph;
	};

	std::vector<short> graphr_result;
	Stats graphr_stats;

	{

		auto elem_func = [] (Data &data, Graphr<false>::Data &elem, size_t j) {
			auto old_d = data.d[j];
			auto int_val = (short)elem.weight;
			if (elem.weight == std::numeric_limits<float>::max())
				int_val = std::numeric_limits<short>::max();
			data.d[j] = std::min(old_d, int_val);
			if (data.d[j] != old_d)
				data.changed_nodes[j] = true;
		};
		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 2;
		options.datatype_size = 16;
		options.input_size = 16;	
		options.read_device = ADC;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = STATIC_ENERGY;
		options.static_latency = STATIC_LATENCY;
		options.dynamic_energy = 0;
		options.dynamic_latency = 0;
		Experiment<Graphr<false>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			data.is_active = false;
			experiment.run_kernel(row_func, elem_func, same_subgraph);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		graphr_result = data.d;
		graphr_stats = experiment.get_stats();
	}

	std::vector<short> sparse_mem_result;
	Stats sparse_mem_stats;

	std::cout << "START OF SPARSEMEM SIMULATION" << std::endl;

	{
		auto elem_func = [] (Data &data, size_t j, short input) {
			auto old_d = data.d[j];
			data.d[j] = std::min(old_d, static_cast<short>(input + 1));
			if (data.d[j] != old_d)
				data.changed_nodes[j] = true;
		};

		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 0.25;
		options.datatype_size = 16;
		options.input_size = 0;
		options.read_device = SA;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = 0;
		options.static_latency = 0;
		options.dynamic_energy = DYNAMIC_ENERGY;
		options.dynamic_latency = DYNAMIC_LATENCY;
		Experiment<SparseMEM<false>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			data.is_active = false;
			experiment.run_kernel(row_func, elem_func, same_subgraph);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		sparse_mem_result = data.d;
		sparse_mem_stats = experiment.get_stats();
	}

	assert(graphr_result.size() == sparse_mem_result.size());
	for (size_t i = 0; i < graphr_result.size(); i++)
		assert(graphr_result[i] == sparse_mem_result[i]);

	std::cout << "Graphr stats: " << std::endl;
	graphr_stats.print();

	std::cout << "SparseMEM stats: " << std::endl;
	sparse_mem_stats.print();
}

void run_bfs(char **argv) {
	if (!argv[1]) {
		std::cout << "Please input a graph!" << std::endl;
		exit(1);
	}

	auto graph = std::make_shared<Graph>(argv[1], 128, 128);
	std::cout << "Read graph of size " << graph->get_dimensions() << std::endl;

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

	auto same_subgraph = [] (const SubGraph &subgraph, Data &data) {
		return subgraph;
	};

	std::vector<short> graphr_result;
	Stats graphr_stats;

	{

		auto elem_func = [] (Data &data, Graphr<false>::Data &elem, size_t j) {
			auto old_d = data.d[j];
			auto int_val = (short)elem.weight;
			if (elem.weight == std::numeric_limits<float>::max())
				int_val = std::numeric_limits<short>::max();
			data.d[j] = std::min(old_d, int_val);
			if (data.d[j] != old_d)
				data.changed_nodes[j] = true;
		};
		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 2;
		options.datatype_size = 1;
		options.input_size = 8;	
		options.read_device = ADC;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = STATIC_ENERGY;
		options.static_latency = STATIC_LATENCY;
		options.dynamic_energy = 0;
		options.dynamic_latency = 0;
		Experiment<Graphr<false>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			data.is_active = false;
			experiment.run_kernel(row_func, elem_func, same_subgraph);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		graphr_result = data.d;
		graphr_stats = experiment.get_stats();
	}

	std::vector<short> sparse_mem_result;
	Stats sparse_mem_stats;

	std::cout << "START OF SPARSEMEM SIMULATION" << std::endl;

	{
		auto elem_func = [] (Data &data, size_t j, short input) {
			auto old_d = data.d[j];
			data.d[j] = std::min(old_d, static_cast<short>(input + 1));
			if (data.d[j] != old_d)
				data.changed_nodes[j] = true;
		};

		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 0.25;
		options.datatype_size = 9;
		options.input_size = 0;
		options.read_device = SA;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = 0;
		options.static_latency = 0;
		options.dynamic_energy = DYNAMIC_ENERGY;
		options.dynamic_latency = DYNAMIC_LATENCY;
		Experiment<SparseMEM <false>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			data.is_active = false;
			experiment.run_kernel(row_func, elem_func, same_subgraph);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		sparse_mem_result = data.d;
		sparse_mem_stats = experiment.get_stats();
	}

	assert(graphr_result.size() == sparse_mem_result.size());
	for (size_t i = 0; i < graphr_result.size(); i++)
		assert(graphr_result[i] == sparse_mem_result[i]);

	std::cout << "Graphr stats: " << std::endl;
	graphr_stats.print();

	std::cout << "SparseMEM stats: " << std::endl;
	sparse_mem_stats.print();
}

void run_pagerank(char **argv) {
	if (!argv[1]) {
		std::cout << "Please input a graph!" << std::endl;
		exit(1);
	}

	const double r = 0.85f;
	const double tol = 1e-9;
	const int max_iterations = 1;

	auto graph = std::make_shared<Graph>(argv[1], 128, 128);
	std::cout << "Read graph of size " << graph->get_dimensions() << std::endl;

	struct Data {
		Data(unsigned int start, size_t graph_dimension,
				size_t crossbar_size)
			:
			iterations(0),
			graph_dimension(graph_dimension),
			teleport_prob(1.0 / static_cast<double>(graph_dimension)),
			score(round_up(graph_dimension, crossbar_size),
					1.0 / static_cast<double>(graph_dimension)),
			new_score(round_up(graph_dimension, crossbar_size))
		{
		}
		int iterations;
		size_t graph_dimension;
		double teleport_prob;
		std::vector<double> score;
		std::vector<double> new_score;
	};


	// Should just return the current score
	auto row_func = [r] (Data &data)
		-> std::optional<double> {
			assert(data.graph_dimension);
			return (1.0f - r) / (double)data.graph_dimension;
	};

	// Should aggregate the new scores, and then swap new and old scores.
	auto aggregate_func = [tol, max_iterations] (Data &data,
			const std::vector<Data> &local_datas) -> bool {
		for (auto &local_data : local_datas)
			add_vectors(data.new_score, local_data.new_score);	

		double error = 0;
		for (size_t i = 0; i < data.score.size(); i++)
			error += std::abs(data.score[i] - data.new_score[i]);
		std::cout << "error: " << error << std::endl;
		bool converged = error < tol;

		data.score = std::move(data.new_score);
		data.new_score.clear();
		data.new_score.resize(data.score.size());

		data.iterations++;
		if (data.iterations >= max_iterations)
			converged = true;

		return !converged;
	};

	auto degree = [r] (const SubGraph &subgraph, Data &data) {
		SubGraph new_subgraph = subgraph;

		std::vector<int> degrees(subgraph.dimensions);

		const auto row_offset = subgraph.row_offset;

		for (auto &t : subgraph.tuples)
			degrees[t.i - row_offset]++;

		
		for (auto &t : new_subgraph.tuples) {
			assert(degrees[t.i - row_offset]);
			t.weight = (r * (float)data.score[t.i] /
					(float)degrees[t.i - row_offset]);
			assert(!std::isnan(t.weight));
			assert(!std::isinf(t.weight));
		}
		return new_subgraph;
	};

	std::vector<double> graphr_result;
	Stats graphr_stats;

	{

		auto elem_func = [] (Data &data, Graphr<true>::Data &elem, size_t j) {
			assert(!std::isinf(data.new_score[j]));
			assert(!std::isinf(elem.weight));
			data.new_score[j] += elem.weight;
		};
		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 4;
		options.datatype_size = 8;
		options.input_size = 8;	
		options.read_device = ADC;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = STATIC_ENERGY;
		options.static_latency = STATIC_LATENCY;
		options.dynamic_energy = 0;
		options.dynamic_latency = 0;
		Experiment<Graphr<true>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			experiment.run_kernel(row_func, elem_func, degree);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		graphr_result = data.score;
		graphr_stats = experiment.get_stats();
	}

	std::vector<double> sparse_mem_result;
	Stats sparse_mem_stats;

	std::cout << "START OF SPARSEMEM SIMULATION" << std::endl;

	{
		auto elem_func = [] (Data &data, size_t j, float input) {
			data.new_score[j] += input;
		};

		CrossbarOptions options;
		options.num_rows = 128;
		options.num_cols = 128;
		options.cols_per_adc = 0.25;
		options.datatype_size = 9;
		options.input_size = 0;
		options.read_device = SA;
		options.read_latency = READ_TIME;
		options.read_energy = READ_ENERGY;
		options.write_latency = WRITE_TIME;
		options.write_energy = WRITE_ENERGY;
		options.adc_latency = ADC_LATENCY;
		options.adc_energy = ADC_ENERGY;
		options.sa_latency = SA_LATENCY;
		options.sa_energy = SA_ENERGY;
		options.static_energy = 0;
		options.static_latency = 0;
		options.dynamic_energy = DYNAMIC_ENERGY;
		options.dynamic_latency = DYNAMIC_LATENCY;
		Experiment<SparseMEM<true>, Data> experiment(options, 5U,
				graph->get_dimensions(), 128LU);
		experiment.set_graph(graph);

		auto &data = experiment.get_data();

		bool is_active = true;
		while (is_active) {
			experiment.run_kernel(row_func, elem_func, degree);
			is_active = experiment.aggregate_data(aggregate_func);
			std::cout << "is_active: " << is_active << std::endl;
		}

		sparse_mem_result = data.score;
		sparse_mem_stats = experiment.get_stats();
	}

	assert(graphr_result.size() == sparse_mem_result.size());
	for (size_t i = 0; i < graphr_result.size(); i++) {
		if (std::abs(graphr_result[i] - sparse_mem_result[i]) >= 0.0000001f)
			std::cout << i << ": " << graphr_result[i] << ", " << sparse_mem_result[i] << std::endl;
		assert(std::abs(graphr_result[i] - sparse_mem_result[i]) < 0.0000001f);
	}

	std::cout << "Graphr stats: " << std::endl;
	graphr_stats.print();

	std::cout << "SparseMEM stats: " << std::endl;
	sparse_mem_stats.print();
}
int main(int argc, char **argv) {
	std::cout << "Running SSSP" << std::endl;
	run_sssp(argv);
	std::cout << "Running BFS" << std::endl;
	run_bfs(argv);
	std::cout << "Running PageRank" << std::endl;
	run_pagerank(argv);
	return 0;
}
