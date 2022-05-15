#ifndef CROSSBAR_H
#define CROSSBAR_H

#include <vector>
#include <string>
#include <fstream>
#include <array>
#include <stddef.h>
#include <stdint.h>

template <typename T, size_t Rows, size_t Cols, size_t InputResolution, size_t ColsPerAdc>
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
	Crossbar(const std::string &log_name)
	: log_file(log_name), crossbar(Cols * Rows)
	{
		log_file << "init: " << Cols << "," << Rows << "\n";
	}

	Crossbar() : crossbar(Cols * Rows) {}

	std::vector<ValueType> readRow(size_t row, size_t offset, size_t num) {
		Stats stats;
		// We need to do DatatypeSize read operations;
		stats.row_reads += 1;
		stats.adc_activations = DatatypeSize * ((offset % ColsPerAdc) + (num * ColsPerAdc) + (num % ColsPerAdc));
		logOperation("read", stats);

		std::vector<ValueType> array(num);
		std::copy(crossbar.begin() + (row * Cols) + offset, crossbar.begin() + (row * Cols) + offset + num, array.begin());
		return array;
	}

	std::vector<ValueType> readWithInput(size_t row, size_t offset, size_t num, int input) {
		Stats stats;
		stats.row_reads += 1;
		stats.adc_activations = DatatypeSize * ((offset % ColsPerAdc) + (num * ColsPerAdc) + (num % ColsPerAdc));
		stats.inputs = DatatypeSize;
		logOperation("read", stats);

		std::vector<ValueType> array(num);
		std::copy(crossbar.begin() + (row * Cols) + offset, crossbar.begin() + (row * Cols) + offset + num, array.begin());
		std::transform(array.begin(), array.end(), array.begin(), [input] (auto a) {
				return a + input;
		});
		return array;
	}

	void writeRow(size_t row, size_t offset, size_t num, std::array<ValueType, Cols> vals) {
		Stats stats;
		assert(row < Rows);
		stats.row_writes += 1;
		std::copy(vals.begin(), vals.end(), crossbar.begin() + row * Cols);
		logOperation("write", stats);
	}

	void clear() {
		log_file << "clear" << std::endl;
		for (auto &a : crossbar)
			a = ValueType{};
	}

	void set_logfile(const std::string &filename) {
		log_file = std::ofstream(filename);
	}

	void init() {
		log_file << "init: " << Cols << "," << Rows << "\n";
	}
private:

	inline void logOperation(const std::string &operationName, const Stats &stats) {
		log_file << operationName << " " << stats.adc_activations << "," << stats.row_reads
			<< "," << stats.row_writes << "," << stats.inputs << "\n";
	}

	std::ofstream log_file;
	std::vector<ValueType> crossbar;
};

#endif
