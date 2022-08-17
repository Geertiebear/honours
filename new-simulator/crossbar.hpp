#ifndef CROSSBAR_HPP
#define CROSSBAR_HPP

#include "stats.hpp"

#include <stddef.h>
#include <assert.h>
#include <vector>
#include <tuple>

enum ReadDevice {
	ADC,
	SA
};

struct CrossbarOptions {
	size_t num_rows, num_cols;
	int datatype_size;
	float cols_per_adc;
	ReadDevice read_device;
	float read_latency;
	float write_latency;
	float adc_latency;
	float read_energy;
	float write_energy;
	float adc_energy;
	float sa_latency;
	float sa_energy;
	float static_energy;
	float static_latency;
	float dynamic_energy;
	float dynamic_latency;
};

template <typename T>
class Crossbar {
public:
	Crossbar(CrossbarOptions options)
	: _options(options), _crossbar(options.num_cols * options.num_rows)
	{}

	std::tuple<Stats, std::vector<T>> readRow(size_t row, size_t offset, size_t num) {
		Stats stats;
		// Pattern: more adcs should decrease latency but increase
		// energy.
		const auto adc_activations = _options.cols_per_adc *
			_options.datatype_size;
		const auto adc_latency = adc_activations * _analogue_latency();
		const auto adc_energy = _options.num_cols / _options.cols_per_adc
			* _options.datatype_size * _analogue_energy();
		const auto static_latency = _options.static_latency;
		const auto static_energy = num * _options.static_energy;

		stats.total_crossbar_time += adc_latency + _options.read_latency;
		stats.total_crossbar_energy += adc_energy +
			num * _options.datatype_size * _options.read_energy;
		stats.total_periphery_time += static_latency;
		stats.total_periphery_energy += static_energy;

		std::vector<T> array(num);
		std::copy(_crossbar.begin() + (row * _options.num_cols) + offset,
				_crossbar.begin() + (row * _options.num_cols) +
				offset + num, array.begin());
		return std::make_tuple(stats, array);
	}

	std::tuple<Stats, std::vector<T>> readWithInput(size_t row, size_t offset, size_t num, int input) {
		Stats stats;

		const auto adc_activations = _options.cols_per_adc *
			_options.datatype_size * _options.datatype_size;
		const auto adc_latency = adc_activations * _analogue_latency();
		const auto adc_energy = num * _options.datatype_size *
			_options.datatype_size * _analogue_energy();
		const auto static_latency = _options.static_latency;
		const auto static_energy = num * _options.static_energy;

		stats.total_crossbar_time += adc_latency + _options.read_latency;
		stats.total_crossbar_energy += adc_energy +
			num * _options.datatype_size * _options.read_energy;
		stats.total_periphery_time += static_latency;
		stats.total_periphery_energy += static_energy;

		std::vector<T> array(num);
		std::copy(_crossbar.begin() + (row * _options.num_cols) + offset,
				_crossbar.begin() + (row * _options.num_cols) +
				offset + num, array.begin());
		std::transform(array.begin(), array.end(), array.begin(), [input] (auto a) {
				return a + input;
		});
		return std::make_tuple(stats, array);
	}

	Stats writeRow(size_t row, size_t offset, size_t num, const std::vector<T> &vals) {
		Stats stats;

		assert(row < _options.num_rows);
		assert(vals.size() == _options.num_cols);

		stats.total_crossbar_time += _options.write_latency;	
		stats.total_crossbar_energy += _options.write_energy * num
			* _options.datatype_size;

		std::copy(vals.begin(), vals.end(), _crossbar.begin() + row * _options.num_cols);
		return stats;
	}

	Stats clear() {
		Stats stats;
		stats.total_crossbar_time += _options.write_latency;

		stats.total_crossbar_energy += _options.write_energy *
			_options.num_rows * _options.num_cols;
		for (auto &a : _crossbar)
			a = T{};
		return stats;
	}

	template <typename F>
	double space_efficiency(F func) {
		size_t num_present = 0;
		for (auto val : _crossbar)
			if (func(val))
				num_present++;

		return static_cast<double>(num_present) /
			static_cast<double>(_crossbar.size());
	}

	inline size_t get_num_rows() const {
		return _options.num_rows;
	}
	inline size_t get_num_cols() const {
		return _options.num_cols;
	}
private:
	inline float _analogue_latency() const {
		switch (_options.read_device) {
			case ADC:
				return _options.adc_latency;
			case SA:
				return _options.sa_latency;
			default:
				assert(!"What");
		}
	}

	inline float _analogue_energy() const {
		switch (_options.read_device) {
			case ADC:
				return _options.adc_energy;
			case SA:
				return _options.sa_energy;
			default:
				assert(!"What");
		}
	}

	CrossbarOptions _options;
	std::vector<T> _crossbar;
};

#endif // CROSSBAR_HPP
