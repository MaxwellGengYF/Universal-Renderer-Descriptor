#pragma once
#include <tuple>
namespace vstd {
template<class... Fs>
class CompositedFunctor {
private:
	const std::tuple<Fs...> fs_;
	const static std::size_t _size_ = sizeof...(Fs);
	template<class Arg, size_t i>
	decltype(auto) call_impl(Arg&& arg) {
		if constexpr (i == 0) {
			return std::get<0>(fs_)(std::forward<Arg>(arg));
		} else {
			return std::get<i>(fs_)(call_impl<Arg, i - 1>(std::forward<Arg>(arg)));
		}
	}

public:
	CompositedFunctor() {}
	CompositedFunctor(std::tuple<Fs...>&& fs) : fs_(std::forward<decltype(fs)>(fs)) {}

	template<class F>
	inline auto composite(F&& f) const {
		return CompositedFunctor<Fs..., F>(std::tuple_cat(fs_, std::make_tuple(std::forward<F>(f))));
	}

	template<class Arg>
	inline auto operator()(Arg&& arg) {
		return call_impl<Arg, _size_ - 1>(std::forward<Arg>(arg));
	}
};
template<class... Fs, class F>
decltype(auto) operator|(CompositedFunctor<Fs...>&& composited, F&& f) {
	return composited.composite(std::forward<F>(f));
}
inline decltype(auto) curry() {
	return CompositedFunctor<>();
}
}// namespace vstd