#pragma once
#include "MetaLib.h"
namespace vstd {
template<typename T>
class IteRef {
	T* ptr;

public:
	IteRef(T* ptr) : ptr(ptr) {}
	decltype(auto) operator*() const {
		return ptr->operator*();
	}
	void operator++() {
		ptr->operator++();
	}
	void operator++(int32) {
		ptr->operator++();
	}
	bool operator==(IteEndTag tag) const {
		return ptr->operator==(tag);
	}
	bool operator!=(IteEndTag tag) const { return !operator==(tag); }
};
namespace detail {
template<typename T>
concept EndlessIterable = requires(T&& t, vstd::IteEndTag endTag) {
	std::is_same_v<IteRef<T>, decltype(t.begin())>;
	t.operator*();
	t.operator++();
	std::is_same_v<bool, decltype(t.operator==(endTag))>;
	std::is_same_v<decltype(t.end()), IteEndTag>;
};
template<typename T, typename End>
concept RegularIterable = requires(T&& t, End endTag) {
	t.operator*();
	t.operator++();
	std::is_same_v<bool, decltype(t.operator==(endTag))>;
};
template<typename T>
concept Iterable = requires(T&& t) {
	RegularIterable<decltype(t.begin()), decltype(t.end())>;
};
template<typename T, typename Ite>
concept BuilderType = requires(T t, Ite ite) {
	t.begin(ite);
	t.next(ite);
	std::is_same_v<bool, decltype(t.is_end(ite))>;
	t.value(ite);
};
template<typename Begin, typename End, typename Map>
concept BeginEndFunc = requires(Begin&& beginFunc, End&& endFunc, Map map) {
	RegularIterable<decltype(beginFunc(map)), decltype(endFunc(map))>;
};
template<EndlessIterable Ite, BuilderType<Ite&&> Builder>
class Combiner {
	Ite ite;
	Builder builder;

public:
	Combiner(Ite&& ite, Builder&& builder)
		: ite(std::forward<Ite>(ite)), builder(std::forward<Builder>(builder)) {}
	IteRef<Combiner> begin() {
		builder.begin(ite);
		return {this};
	}
	vstd::IteEndTag end() const { return {}; }
	void operator++() {
		builder.next(ite);
	}
	decltype(auto) operator*() {
		return builder.value(ite);
	}
	bool operator==(vstd::IteEndTag) const {
		return builder.is_end(ite);
	}
	bool operator!=(IteEndTag tag) const { return !operator==(tag); }
};
template<typename Dst>
class BuilderBase {
public:
	template<EndlessIterable Ite>
	friend decltype(auto) operator>>(Ite&& ite, Dst&& dst) {
		return Combiner<Ite, Dst>(std::forward<Ite>(ite), std::forward<Dst>(dst));
	}
};
}// namespace detail

template<typename T>
class IRange {
public:
	virtual ~IRange() = default;
	virtual IteRef<IRange> begin() = 0;
	virtual bool operator==(IteEndTag) const = 0;
	virtual void operator++() = 0;
	virtual T operator*() = 0;
	vstd::IteEndTag end() const { return {}; }
};
template<detail::EndlessIterable Ite>
class IRangeImpl : public IRange<decltype(*std::declval<Ite>())> {
	using Value = decltype(*std::declval<Ite>());
	Ite ptr;

public:
	IRangeImpl(Ite&& ptr) : ptr(std::forward<Ite>(ptr)) {}
	IteRef<IRange<Value>> begin() override {
		ptr.begin();
		return {this};
	}
	bool operator==(IteEndTag t) const override { return ptr == t; }
	void operator++() override {
		++ptr;
	}
	Value operator*() override {
		return *ptr;
	}
	IRangeImpl(IRangeImpl const&) = delete;
	IRangeImpl(IRangeImpl&&) = default;
};
class ValueRange : public detail::BuilderBase<ValueRange> {
public:
	template<typename Ite>
	void begin(Ite&& ite) { ite.begin(); }
	template<typename Ite>
	bool is_end(Ite&& ite) const { return ite == vstd::IteEndTag{}; }
	template<typename Ite>
	void next(Ite&& ite) { ++ite; }
	template<typename Ite>
	auto value(Ite&& ite) { return *ite; }
	ValueRange(ValueRange const&) = delete;
	ValueRange(ValueRange&&) = default;
};
template<typename FilterFunc>
class FilterRange : public detail::BuilderBase<FilterRange<FilterFunc>> {
private:
	FilterFunc func;
	template<typename Ite>
	void GetNext(Ite& ite) {
		while (ite != IteEndTag{}) {
			if (func(*ite)) {
				return;
			}
			++ite;
		}
	}

public:
	template<typename Ite>
	void begin(Ite&& ite) {
		ite.begin();
		GetNext(ite);
	}
	template<typename Ite>
	bool is_end(Ite&& ite) const {
		return ite == vstd::IteEndTag{};
	}
	template<typename Ite>
	void next(Ite&& ite) {
		++ite;
		GetNext(ite);
	}
	template<typename Ite>
	decltype(auto) value(Ite&& ite) {
		return *ite;
	}
	FilterRange(FilterFunc&& func)
		: func(std::forward<FilterFunc>(func)) {
	}
	FilterRange(FilterRange const&) = delete;
	FilterRange(FilterRange&&) = default;
};
template<typename GetValue>
class TransformRange : public detail::BuilderBase<TransformRange<GetValue>> {
	GetValue getValue;

public:
	template<typename Ite>
	void begin(Ite&& ite) {
		ite.begin();
	}
	TransformRange(GetValue&& getValueFunc)
		: getValue(std::forward<GetValue>(getValueFunc)) {}
	template<typename Ite>
	bool is_end(Ite&& ite) const {
		return ite == vstd::IteEndTag{};
	}
	template<typename Ite>
	void next(Ite&& ite) {
		++ite;
	}
	template<typename Ite>
	decltype(auto) value(Ite&& ite) {
		return getValue(*ite);
	}
	TransformRange(TransformRange const&) = delete;
	TransformRange(TransformRange&&) = default;
};
template<detail::Iterable Map>
class CacheEndRange {
public:
	using IteBegin = decltype(std::declval<Map>().begin());
	using IteEnd = decltype(std::declval<Map>().begin());

private:
	Map map;
	vstd::optional<IteBegin> ite;

public:
	CacheEndRange(Map&& map)
		: map(std::forward<Map>(map)) {
	}
	IteRef<CacheEndRange> begin() {
		ite = map.begin();
		return {this};
	}
	bool operator==(IteEndTag) const {
		return (*ite) == map.end();
	}
	void operator++() { ++(*ite); }
	decltype(auto) operator*() {
		return **ite;
	}
	vstd::IteEndTag end() const { return {}; }
	CacheEndRange(CacheEndRange const&) = delete;
	CacheEndRange(CacheEndRange&&) = default;
};
template<typename BeginFunc, typename EndFunc, typename Map>
class CustomRange {
public:
	using IteBegin = decltype(std::declval<BeginFunc>()(std::declval<Map>()));
	using IteEnd = decltype(std::declval<EndFunc>()(std::declval<Map>()));

private:
	Map map;
	BeginFunc beginFunc;
	EndFunc endFunc;
	vstd::optional<IteBegin> ite;

public:
	CustomRange(
		Map&& map,
		BeginFunc&& beginFunc,
		EndFunc&& endFunc)
		: map(std::forward<Map>(map)),
		  beginFunc(std::forward<BeginFunc>(beginFunc)),
		  endFunc(std::forward<EndFunc>(endFunc)) {
	}
	IteRef<CustomRange> begin() {
		ite = beginFunc(map);
		return {this};
	}
	bool operator==(IteEndTag) const {
		return (*ite) == endFunc(map);
	}
	void operator++() { ++(*ite); }
	decltype(auto) operator*() {
		return **ite;
	}
	vstd::IteEndTag end() const { return {}; }
	CustomRange(CustomRange const&) = delete;
	CustomRange(CustomRange&&) = default;
};
template<typename Func>
decltype(auto) MakeFilterRange(Func&& func) {
	return FilterRange<Func>(std::forward<Func>(func));
}
template<typename GetValue>
decltype(auto) MakeTransformRange(GetValue&& func) {
	return TransformRange<GetValue>(
		std::forward<GetValue>(func));
}
template<detail::Iterable Map>
decltype(auto) MakeCacheEndRange(Map&& map) {
	return CacheEndRange<Map>(std::forward<Map>(map));
}
template<typename BeginFunc, typename EndFunc, typename Map>
requires(detail::BeginEndFunc<BeginFunc, EndFunc, Map>)

	decltype(auto) MakeCustomRange(
		Map&& map,
		BeginFunc&& beginFunc,
		EndFunc&& endFunc) {
	return CustomRange<BeginFunc, EndFunc, Map>(std::forward<Map>(map), std::forward<BeginFunc>(beginFunc), std::forward<EndFunc>(endFunc));
}
template<detail::EndlessIterable Map>
decltype(auto) MakeIRangeImpl(Map&& map) {
	return IRangeImpl<Map>(std::forward<Map>(map));
}
}// namespace vstd