#pragma once
#include <ir/kernel.h>
#include <bitset>
#include <Utility/StringUtility.h>
namespace luisa::ir {

template<typename T>
struct StateRecorder {
	T states[std::numeric_limits<char>::max() + 1];
	template<typename Func>
	void Init(
		Func&& initFunc) {
		if constexpr (std::is_trivially_constructible_v<T>)
			memset(states, 0, sizeof(states));
		initFunc(states);
	}
	T const& Get(char p) const {
		return states[p];
	}
	T const& operator[](char p) const {
		return states[p];
	}
	vstd::string_view GetNext(char const*& iterator, T const& value) const {
		char const* cur = iterator;
		for (; *iterator; ++iterator) {
			if (Get(*iterator) != value) return {cur, (size_t)(iterator - cur)};
		}
		return {};
	}
	template<typename Func>
	requires(std::is_invocable_r_v<bool, Func, T const&>)
		vstd::string_view GetNext(char const*& iterator, Func&& func)
	const {
		char const* cur = iterator;
		for (; *iterator; ++iterator) {
			if (!func(Get(*iterator))) return {cur, (size_t)(iterator - cur)};
		}
		return {};
	}
};

template<>
struct StateRecorder<bool> {
	using ArrayType = std::bitset<std::numeric_limits<char>::max() + 1>;
	ArrayType states;
	template<typename Func>
	requires(std::is_invocable_v<Func, ArrayType&>) void Init(Func&& initFunc) {
		initFunc(states);
	}

	bool Get(char p) const {
		return states[p];
	}
	bool operator[](char p) const {
		return states[p];
	}
	void GetNext(char const*& iterator) const {
		for (; *iterator; ++iterator) {
			if (!Get(*iterator)) return;
		}
		return;
	}
};

}// namespace luisa::ir