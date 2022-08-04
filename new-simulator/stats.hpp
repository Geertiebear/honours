#ifndef STATS_HPP
#define STATS_HPP

#include <stdint.h>
#include <stdio.h>

struct Stats {
	float total_crossbar_time = 0, total_crossbar_energy = 0, efficiency = 0,
		total_periphery_time = 0, total_periphery_energy = 0;
	unsigned int num_efficiencies = 0;

	void operator+= (const Stats &other) {
		total_crossbar_time += other.total_crossbar_time;
		total_crossbar_energy += other.total_crossbar_energy;
		efficiency += other.efficiency;
		num_efficiencies += other.num_efficiencies;
		total_periphery_time += other.total_periphery_time;
		total_periphery_energy += other.total_periphery_energy;
	}

	float get_average_efficiency() const {
		return efficiency / num_efficiencies;
	}

	void print() const {
		printf("\ttotal_crossbar_time: %f\n", total_crossbar_time);
		printf("\ttotal_crossbar_energy: %f\n", total_crossbar_energy);
		printf("\tefficiency: %f\n", get_average_efficiency());
		printf("\ttotal_periphery_time: %f\n", total_periphery_time);
		printf("\ttotal_periphery_energy: %f\n", total_periphery_energy);
	}

	Stats() = default;
};

#endif // STATS_HPP
