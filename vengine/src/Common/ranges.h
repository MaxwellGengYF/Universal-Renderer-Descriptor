#pragma once
#include "MetaLib.h"
namespace vstd {

namespace detail {
template<typename T>
concept EndlessIterable = requires(T&& t) {
	t.begin();
	std::is_same_v<decltype(t.end()), IteEndTag>;
};
template<typename T>
concept Iterable = requires(T&& t) {
	t.begin();
	t.end();
};
template<typename T>
using IteValueType = decltype(*std::declval<T>());
}// namespace detail

template<typename T>
class IRange {
public:
	virtual ~IRange() = default;
	virtual bool operator==(IteEndTag) const = 0;
	virtual bool operator!=(IteEndTag) const = 0;
	virtual void operator++() = 0;
	virtual T operator*() = 0;
};
template<typename Ite>
class IRangeImpl : public IRange<decltype(*std::declval<Ite>())> {
public:
	using Value = decltype(*std::declval<Ite>());
	Ite ptr;
	IRangeImpl(Ite&& ptr) : ptr(ptr) {}
	bool operator==(IteEndTag t) const override { return ptr == t; }
	bool operator!=(IteEndTag t) const override { return !(ptr == t); }
	void operator++() override {
		++ptr;
	}
	Value operator*() override {
		return *ptr;
	}
};
template<detail::EndlessIterable Map>
class ValueRange {
public:
	using IteBegin = decltype(std::declval<Map>().begin());

protected:
	IteBegin ite;
	using Value = std::remove_reference_t<decltype(*ite)>;

public:
	ValueRange(Map&& map)
		: ite(map.begin()) {
	}
	bool operator==(IteEndTag t) const {
		return ite == t;
	}
	void operator++() {
		++ite;
	}
	Value operator*() {
		return *ite;
	}
};
template<detail::EndlessIterable Map, typename FilterFunc>
class FilterRange {
public:
	using IteBegin = decltype(std::declval<Map>().begin());

private:
	IteBegin ite;
	using Value = std::remove_reference_t<decltype(*ite)>;
	optional<Value> next;
	FilterFunc func;
	void GetNext() {
		while (ite != IteEndTag{}) {
			auto disp = create_disposer([&] { ++ite; });
			if (func(*ite)) {
				next = *ite;
				return;
			}
		}
		next.Delete();
	}

public:
	FilterRange(Map&& map, FilterFunc&& func)
		: ite(map.begin()), func(std::forward<FilterFunc>(func)) {
		GetNext();
	}
	bool operator==(IteEndTag) const {
		return !next;
	}
	void operator++() {
		GetNext();
	}
	Value& operator*() {
		return *next;
	}
};
template<detail::EndlessIterable Map, typename GetValue>
class GetValueRange {
public:
	using IteBegin = decltype(std::declval<Map>().begin());

private:
	IteBegin ite;
	GetValue getValue;

public:
	GetValueRange(Map&& map, GetValue&& getValueFunc)
		: ite(map.begin()), getValue(std::forward<GetValue>(getValueFunc)) {}
	bool operator==(IteEndTag v) const {
		return ite == v;
	}
	void operator++() { ++ite; }
	decltype(auto) operator*() {
		return getValue(*ite);
	}
};
template<detail::Iterable Map>
class CacheEndRange {
public:
	using IteBegin = decltype(std::declval<Map>().begin());
	using IteEnd = decltype(std::declval<Map>().begin());

private:
	IteBegin ite;
	IteEnd end;

public:
	CacheEndRange(Map&& map)
		: ite(map.begin()), end(map.end()) {
	}
	bool operator==(IteEndTag) const {
		return ite == end;
	}
	void operator++() { ++ite; }
	decltype(auto) operator*() {
		return *ite;
	}
};
template<detail::EndlessIterable Map, typename Func>
requires(std::is_invocable_r_v<bool, Func, decltype(*std::declval<Map>().begin())>)

	decltype(auto) MakeFilterRange(Map&& map, Func&& func) {
	using BeginType = decltype(map.begin());
	return IteWrapper<FilterRange<Map, Func>>(
		std::forward<Map>(map),
		std::forward<Func>(func));
}
template<detail::EndlessIterable Map, typename GetValue>
requires(std::is_invocable_v<GetValue, decltype(*std::declval<Map>().begin())>)

	decltype(auto) MakeGetValueRange(Map&& map, GetValue&& func) {
	return IteWrapper<GetValueRange<Map, GetValue>>(
		std::forward<Map>(map),
		std::forward<GetValue>(func));
}
template<detail::Iterable Map>
decltype(auto) MakeCacheEndRange(Map&& map) {
	return IteWrapper<CacheEndRange<Map>>(std::forward<Map>(map));
}
template<detail::EndlessIterable Map>
decltype(auto) MakeValueRange(Map&& map){
	return IteWrapper<ValueRange<Map>>(std::forward<Map>(map));
}
}// namespace vstd