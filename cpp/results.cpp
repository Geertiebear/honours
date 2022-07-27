#include <iostream>
#include <stdio.h>
#include <string>
#include <string.h>

const char *experiments[] = {"wiki-Vote", "soc-Slashdot0902", "amazon0302", "soc-Epinions1"};

struct Result {
	float total_time = 0, total_energy = 0, efficiency = 0,
	      total_periphery_time = 0, total_periphery_energy = 0;
};

static constexpr float READ_TIME = 10e-9;
static constexpr float WRITE_TIME = 100e-9;
static constexpr float ADC_LATENCY = 1e-9;
static constexpr float READ_ENERGY = 40e-15;
static constexpr float WRITE_ENERGY = 20e-12;
static constexpr float ADC_ENERGY = 2e-12;
static constexpr float SA_LATENCY = 1e-9;
static constexpr float SA_ENERGY = 10e-12;

static Result values_from_file(const std::string &filename, const std::string &experiment) {
	std::cout << "doing experiment " << filename << std::endl;
	Result res;
	FILE *fp = fopen(filename.c_str(), "r");

	float static_energy = 0, static_time = 0;
	if (experiment == "graphr") {
		static_energy = 11.8e-12;
		static_time = 0.5e-9;
	}

	float dynamic_energy = 0, dynamic_latency = 0;
	if (experiment == "ours") {
		dynamic_latency = 1.1e-9;
		dynamic_energy = 28.8e-12;
	}

	float energy = 0, latency = 0;

	char line[1024];
	char sense_type[128];
	size_t row, col;
	while (fgets(line, sizeof(line), fp)) {
		char command[128];
		int read;
		sscanf(line, "%s %n", command, &read);
		if (strcmp(command, "init:") == 0) {
			sscanf(line + read, "%ld,%ld,%s\n", &row, &col, sense_type);
			if (strcmp(sense_type, "adc") == 0) {
				energy = ADC_ENERGY;
				latency = ADC_LATENCY;
			} else if (strcmp(sense_type, "sa") == 0) {
				energy = SA_ENERGY;
				latency = SA_LATENCY;
			} else {
				std::cout << "Unkown sense type: " << sense_type << std::endl;
			}
		} else if (strcmp(command, "clear") == 0) {
			res.total_time += row * WRITE_TIME;
			res.total_energy += row * WRITE_ENERGY;
		} else if (strcmp(command, "read") == 0) {
			int adc_acts, row_reads, row_writes, inputs;
			sscanf(line + read, "%d,%d,%d,%d\n", &adc_acts, &row_reads, &row_writes,&inputs);
			res.total_time += (READ_TIME + adc_acts * latency) * inputs;
			res.total_energy += (row_reads * READ_ENERGY + adc_acts * energy) * inputs;
			if (experiment == "graphr") {
				res.total_periphery_energy += static_energy * col;
				res.total_periphery_time += static_time;
			}
		} else if (strcmp(command, "write") == 0) {
			int adc_acts, row_reads, row_writes, inputs;
			sscanf(line + read, "%d,%d,%d,%d\n", &adc_acts, &row_reads, &row_writes,&inputs);
			res.total_time += (WRITE_TIME) * inputs;
			res.total_energy += (row_writes * WRITE_ENERGY) * inputs;
		} else if (strcmp(command, "efficiency") == 0) {
			float efficiency;
			sscanf(line + read, "%f\n", &efficiency);
			res.efficiency = efficiency;
		} else if (strcmp(command, "elements") == 0) {
			int elements;
			sscanf(line + read, "%d\n", &elements);
			res.total_periphery_time += elements * dynamic_latency;
			res.total_periphery_energy += elements * dynamic_energy;
		} else {
			std::cout << "unkown command " << command << std::endl;
			exit(1);
		}
	}

	fclose(fp);
	return res;
}

int main() {
	for (auto experiment : experiments) {
		auto experiment_str = std::string(experiment);

		auto graphr_res = values_from_file(experiment_str + "-graphr.log", "graphr");
		auto our_res = values_from_file(experiment_str + "-ours.log", "ours");
		auto our_offsets_res = values_from_file(experiment_str + "-ours-offsets.log", "offsets");

		auto our_total_energy = our_res.total_energy + our_offsets_res.total_energy
			+ our_res.total_periphery_energy;
		auto our_total_time = our_res.total_time + our_offsets_res.total_time
			+ our_res.total_periphery_time;

		auto graphr_total_energy = graphr_res.total_energy
			+ graphr_res.total_periphery_energy;
		auto graphr_total_time = graphr_res.total_time
			+ graphr_res.total_periphery_time;

		std::cout << "normal graphr time: " << graphr_total_time / our_total_time << std::endl;
		std::cout << "normal graphr energy: " << graphr_total_energy / our_total_energy << std::endl;
		std::cout << "normal graphr efficiency: " << graphr_res.efficiency / our_res.efficiency << std::endl;
	}
	return 0;
}
