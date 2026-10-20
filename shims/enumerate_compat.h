#ifndef ENUMERATE_COMPAT_H
#define ENUMERATE_COMPAT_H
#include <ranges>
#include <version>
#include <utility>
#include <cstddef>
#if __cplusplus >= 202002L && !defined(__cpp_lib_ranges_enumerate)
namespace std::ranges {
template <class R> struct enumerate_view {
	R & r;
	struct iter {
		std::size_t i;
		std::ranges::iterator_t<R> it;
		bool operator!=(const iter & o) const { return it != o.it; }
		iter & operator++() { ++i; ++it; return *this; }
		auto operator*() const { return std::pair<std::size_t, std::ranges::range_reference_t<R>>(i, *it); }
	};
	iter begin() { return {0, std::ranges::begin(r)}; }
	iter end() { return {0, std::ranges::end(r)}; }
};
template <class R> enumerate_view(R &) -> enumerate_view<R>;
}
#endif
#endif
