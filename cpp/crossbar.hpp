#ifndef CROSSBAR_H
#define CROSSBAR_H

#include <vector>
#include <string>
#include <fstream>
#include <array>
#include <stddef.h>
#include <stdint.h>

struct CrossbarOptions {
	size_t rows, cols, input_resolution, cols_per_adc;
	bool adc;
};

template <typename T>
class Crossbar {
	struct Stats {
		uint64_t adc_activations = 0;
		uint64_t row_reads = 0;
		uint64_t row_writes = 0;
		uint64_t inputs = 0;
	};

	static constexpr unsigned int DatatypeSize = sizeof(T);
	using ValueType = T;
public:
	Crossbar(const std::string &log_name, const CrossbarOptions &opts)
	: log_file(new std::ofstream(log_name)), crossbar(opts.rows * opts.cols),
	opts(opts)
	{
		*log_file << "init: " << opts.cols << "," << opts.rows << "\n";
	}

	Crossbar() {}

	std::vector<ValueType> readRow(size_t row, size_t offset, size_t num) {
		Stats stats;
		// We need to do DatatypeSize read operations;
		stats.row_reads += 1;
		stats.adc_activations = DatatypeSize * ((offset % opts.cols_per_adc) + (num * opts.cols_per_adc) + (num % opts.cols_per_adc));
		logOperation("read", stats);

		std::vector<ValueType> array(num);
		std::copy(crossbar.begin() + (row * opts.cols) + offset, crossbar.begin() + (row * opts.cols) + offset + num, array.begin());
		return array;
	}

	std::vector<ValueType> readWithInput(size_t row, size_t offset, size_t num, int input) {
		Stats stats;
		stats.row_reads += 1;
		stats.adc_activations = DatatypeSize * ((offset % opts.cols_per_adc) + (num * opts.cols_per_adc) + (num % opts.cols_per_adc));
		stats.inputs = DatatypeSize;
		logOperation("read", stats);

		std::vector<ValueType> array(num);
		std::copy(crossbar.begin() + (row * opts.cols) + offset, crossbar.begin() + (row * opts.cols) + offset + num, array.begin());
		std::transform(array.begin(), array.end(), array.begin(), [input] (auto a) {
				return a + input;
		});
		return array;
	}

	void writeRow(size_t row, size_t offset, size_t num, const std::vector<ValueType> &vals) {
		Stats stats;
		assert(row < opts.rows);
		assert(vals.size() == opts.cols);
		stats.row_writes += 1;
		std::copy(vals.begin(), vals.end(), crossbar.begin() + row * opts.cols);
		logOperation("write", stats);
	}

	void clear() {
		*log_file << "clear" << std::endl;
		for (auto &a : crossbar)
			a = ValueType{};
	}

	void set_logfile(std::ofstream *stream) {
		log_file = stream;
	}

	void set_options(const CrossbarOptions &opts) {
		this->opts = opts;
		crossbar.resize(opts.rows * opts.cols);
	}

	void init() {
		*log_file << "init: " << opts.cols << "," << opts.rows << "\n";
	}
private:

	inline void logOperation(const std::string &operationName, const Stats &stats) {
		*log_file << operationName << " " << stats.adc_activations << "," << stats.row_reads
			<< "," << stats.row_writes << "," << stats.inputs << "\n";
	}

	std::ofstream *log_file;
	std::vector<ValueType> crossbar;
	CrossbarOptions opts;
};

#endif
