#ifndef UTIL_HPP
#define UTIL_HPP

#include <vector>
#include <assert.h>

inline constexpr auto round_up(const auto a, const auto b) {
	return ((a + b - 1) / b)*b;
}

template <typename T, typename BinOp>
inline void vector_binop(std::vector<T> &a, const std::vector<T> &b, BinOp op) {
	assert(a.size() == b.size());
	for (size_t i = 0; i < a.size(); i++)
		a[i] = op(a[i], b[i]);
}

#endif // UTIL_HPP
