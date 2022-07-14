#pragma once
#include "MetaLib.h"
namespace vstd {
template<
	typename Value,
	typename GetNextValueFunc,
	typename FilterFunc>
requires(
	std::is_invocable_r_v<vstd::optional<Value>, GetNextValueFunc>&&
		std::is_invocable_r_v<bool, FilterFunc, Value const&>) class FilterRange {
	GetNextValueFunc getNextFunc;
	FilterFunc filterFunc;
	vstd::optional<Value> next;

	void GetNext() {
		while (auto v = getNextFunc()) {
			if (filterFunc(*v)) {
				next = *v;
				return;
			}
		}
		next.Delete();
	}

public:
	template<typename GetNextValueFuncArg, typename FilterFuncArg>
	requires(std::is_constructible_v<GetNextValueFunc, GetNextValueFuncArg&&>&& std::is_constructible_v<FilterFunc, FilterFuncArg&&>)
		FilterRange(
			GetNextValueFuncArg&& getNextFunc,
			FilterFuncArg&& filterFunc)
		: getNextFunc(std::forward<GetNextValueFuncArg>(getNextFunc)),
		  filterFunc(std::forward<FilterFuncArg>(filterFunc)) {
		GetNext();
	}
	bool operator==(vstd::IteEndTag) {
		return !next;
	}
	void operator++() {
		GetNext();
	}
	Value& operator*() {
		return *next;
	}
};
}// namespace vstd