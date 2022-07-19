#pragma once
#include "Memory.h"
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
class BuilderFlag : public IOperatorNewBase {};
class RangeFlag : public IOperatorNewBase {};
template<typename T>
concept EndlessIterable = requires(T&& t, IteEndTag endTag) {
	std::is_base_of_v<RangeFlag, std::remove_cvref_t<T>>;
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
template<typename T>
concept BuilderType = requires(T t) {
	std::is_base_of_v<BuilderFlag, std::remove_cvref_t<T>>;
};
template<typename T>
concept BuilderOrRangeType = requires(T t) {
	std::is_base_of_v<BuilderFlag, std::remove_cvref_t<T>> ||
		std::is_base_of_v<RangeFlag, std::remove_cvref_t<T>>;
};
template<typename Begin, typename End, typename Map>
concept BeginEndFunc = requires(Begin&& beginFunc, End&& endFunc, Map map) {
	RegularIterable<decltype(beginFunc(map)), decltype(endFunc(map))>;
};

template<EndlessIterable Ite, BuilderType Builder>
class Combiner : public RangeFlag {
	Ite ite;
	Builder builder;

public:
	Combiner(Ite&& ite, Builder&& builder)
		: ite(std::forward<Ite>(ite)), builder(std::forward<Builder>(builder)) {}
	IteRef<Combiner> begin() {
		builder.begin(ite);
		return {this};
	}
	IteEndTag end() const { return {}; }
	void operator++() {
		builder.next(ite);
	}
	decltype(auto) operator*() {
		return builder.value(ite);
	}
	bool operator==(IteEndTag) const {
		return builder.is_end(ite);
	}
	bool operator!=(IteEndTag tag) const { return !operator==(tag); }
};
template<typename T, typename Ite>
struct BuilderHolder : RangeFlag {
	T& t;
	Ite& ite;
	BuilderHolder(T& t, Ite& ite)
		: t(t), ite(ite) {}
	void begin() {
		t.begin(ite);
	}
	void operator++() {
		t.next(ite);
	}
	decltype(auto) operator*() {
		return t.value(ite);
	}
	bool operator==(IteEndTag) const {
		return t.is_end(ite);
	}
};

template<BuilderType LeftBD, BuilderType RightBD>
class BuilderCombiner : public BuilderFlag {
	LeftBD left;
	RightBD right;

public:
	BuilderCombiner(LeftBD&& left, RightBD&& right)
		: left(std::forward<LeftBD>(left)),
		  right(std::forward<RightBD>(right)) {}

	template<typename Ite>
	void begin(Ite&& ite) {
		right.begin(BuilderHolder<LeftBD, Ite&>(left, ite));
	}
	template<typename Ite>
	bool is_end(Ite&& ite) const {
		return right.is_end(BuilderHolder<LeftBD const, Ite&>(left, ite));
	}
	template<typename Ite>
	void next(Ite&& ite) {
		right.next(BuilderHolder<LeftBD, Ite&>(left, ite));
	}
	template<typename Ite>
	auto value(Ite&& ite) {
		return right.value(BuilderHolder<LeftBD, Ite&>(left, ite));
	}
};
template<BuilderOrRangeType Ite, BuilderOrRangeType Dst>
inline decltype(auto) operator|(Ite&& ite, Dst&& dst) {
	if constexpr (std::is_base_of_v<RangeFlag, std::remove_cvref_t<Ite>>) {
		return Combiner<Ite, Dst>(std::forward<Ite>(ite), std::forward<Dst>(dst));
	} else {
		return BuilderCombiner<Ite, Dst>(std::forward<Ite>(ite), std::forward<Dst>(dst));
	}
}
}// namespace detail

template<typename T>
class IRange : public detail::RangeFlag {
public:
	virtual ~IRange() = default;
	virtual IteRef<IRange> begin() = 0;
	virtual bool operator==(IteEndTag) const = 0;
	virtual void operator++() = 0;
	virtual T operator*() = 0;
	IteEndTag end() const { return {}; }
};
template<detail::EndlessIterable Ite>
class v_IRangeImpl : public IRange<decltype(*std::declval<Ite>())> {
	using Value = decltype(*std::declval<Ite>());
	Ite ptr;

public:
	v_IRangeImpl(Ite&& ptr) : ptr(std::forward<Ite>(ptr)) {}
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
	v_IRangeImpl(v_IRangeImpl const&) = delete;
	v_IRangeImpl(v_IRangeImpl&&) = default;
};
template<typename T>
class IRangePipeline : public detail::BuilderFlag {
public:
	virtual ~IRangePipeline() = default;
	virtual void begin(IRange<T>& range) = 0;
	virtual bool is_end(IRange<T>& range) const = 0;
	virtual void next(IRange<T>& range) = 0;
	virtual T value(IRange<T>& range) = 0;
};
template<typename T, typename Ite>
class v_IRangePipelineImpl : public IRangePipeline<T> {
	Ite ite;

public:
	v_IRangePipelineImpl(Ite&& ite) : ite(std::forward<Ite>(ite)) {}
	void begin(IRange<T>& range) override { ite.begin(range); }
	bool is_end(IRange<T>& range) const override { return ite.is_end(range); }
	void next(IRange<T>& range) override { ite.next(range); }
	T value(IRange<T>& range) override { return ite.value(range); }
};
class v_ValueRange : public detail::BuilderFlag {
public:
	template<typename Ite>
	void begin(Ite&& ite) { ite.begin(); }
	template<typename Ite>
	bool is_end(Ite&& ite) const { return ite == IteEndTag{}; }
	template<typename Ite>
	void next(Ite&& ite) { ++ite; }
	template<typename Ite>
	auto value(Ite&& ite) { return *ite; }
};
template<typename FilterFunc>
class v_FilterRange : public detail::BuilderFlag {
private:
	FilterFunc func;
	template<typename Ite>
	void GetNext(Ite&& ite) {
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
		return ite == IteEndTag{};
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
	v_FilterRange(FilterFunc&& func)
		: func(std::forward<FilterFunc>(func)) {
	}
};
template<typename GetValue>
class v_TransformRange : public detail::BuilderFlag {
	GetValue getValue;

public:
	template<typename Ite>
	void begin(Ite&& ite) {
		ite.begin();
	}
	v_TransformRange(GetValue&& getValueFunc)
		: getValue(std::forward<GetValue>(getValueFunc)) {}
	template<typename Ite>
	bool is_end(Ite&& ite) const {
		return ite == IteEndTag{};
	}
	template<typename Ite>
	void next(Ite&& ite) {
		++ite;
	}
	template<typename Ite>
	decltype(auto) value(Ite&& ite) {
		return getValue(*ite);
	}
};
template<typename Dst>
class v_StaticCastRange : public detail::BuilderFlag {
public:
	template<typename Ite>
	void begin(Ite&& ite) { ite.begin(); }
	template<typename Ite>
	bool is_end(Ite&& ite) const { return ite == IteEndTag{}; }
	template<typename Ite>
	void next(Ite&& ite) { ++ite; }
	template<typename Ite>
	Dst value(Ite&& ite) { return static_cast<Dst>(*ite); }
	v_StaticCastRange() {}
};
template<typename Dst>
class v_ReinterpretCastRange : public detail::BuilderFlag {
public:
	template<typename Ite>
	void begin(Ite&& ite) { ite.begin(); }
	template<typename Ite>
	bool is_end(Ite&& ite) const { return ite == IteEndTag{}; }
	template<typename Ite>
	void next(Ite&& ite) { ++ite; }
	template<typename Ite>
	Dst value(Ite&& ite) { return reinterpret_cast<Dst>(*ite); }
	v_ReinterpretCastRange() {}
};
template<typename Dst>
class v_ConstCastRange : public detail::BuilderFlag {
public:
	template<typename Ite>
	void begin(Ite&& ite) { ite.begin(); }
	template<typename Ite>
	bool is_end(Ite&& ite) const { return ite == IteEndTag{}; }
	template<typename Ite>
	void next(Ite&& ite) { ++ite; }
	template<typename Ite>
	Dst value(Ite&& ite) { return const_cast<Dst>(*ite); }
	v_ConstCastRange() {}
};

template<detail::Iterable Map>
class v_CacheEndRange : public detail::RangeFlag {
public:
	using IteBegin = decltype(std::declval<Map>().begin());
	using IteEnd = decltype(std::declval<Map>().begin());

private:
	Map map;
	optional<IteBegin> ite;

public:
	v_CacheEndRange(Map&& map)
		: map(std::forward<Map>(map)) {
	}
	IteRef<v_CacheEndRange> begin() {
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
	IteEndTag end() const { return {}; }
};
template<typename BeginFunc, typename EndFunc, typename Map>
class v_CustomRange : public detail::RangeFlag {
public:
	using IteBegin = decltype(std::declval<BeginFunc>()(std::declval<Map>()));
	using IteEnd = decltype(std::declval<EndFunc>()(std::declval<Map>()));

private:
	Map map;
	BeginFunc beginFunc;
	EndFunc endFunc;
	optional<IteBegin> ite;

public:
	v_CustomRange(
		Map&& map,
		BeginFunc&& beginFunc,
		EndFunc&& endFunc)
		: map(std::forward<Map>(map)),
		  beginFunc(std::forward<BeginFunc>(beginFunc)),
		  endFunc(std::forward<EndFunc>(endFunc)) {
	}
	IteRef<v_CustomRange> begin() {
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
	IteEndTag end() const { return {}; }
};
class range : detail::RangeFlag {
	int64 num;
	int64 b;
	int64 e;
	int64 inc;

public:
	IteRef<range> begin() {
		num = b;
		return {this};
	}
	IteEndTag end() const { return {}; }
	bool operator==(IteEndTag) const {
		return num == e;
	}
	void operator++() { num += inc; }
	int64 operator*() {
		return num;
	}

	range(int64 b, int64 e, int64 inc = 1) : b(b), e(e), inc(inc) {}
	range(int64 e) : b(0), e(e), inc(1) {}
};
template<typename T>
class ptr_range : detail::RangeFlag {
	T* ptr;
	T* b;
	T* e;
	int64_t inc;

public:
	ptr_range(T* b, T* e, int64_t inc = 1) : b(b), e(e), inc(inc) {}
	ptr_range(T* b, size_t e, int64_t inc = 1) : b(b), e(b + e), inc(inc) {}
	IteRef<ptr_range> begin() {
		ptr = b;
		return {this};
	}
	IteEndTag end() const { return {}; }
	bool operator==(IteEndTag) const {
		return ptr == e;
	}
	void operator++() {
		ptr += inc;
	}
	T& operator*() {
		return *ptr;
	}
};
template<typename Func>
decltype(auto) FilterRange(Func&& func) {
	return v_FilterRange<Func>(std::forward<Func>(func));
}
template<typename GetValue>
decltype(auto) TransformRange(GetValue&& func) {
	return v_TransformRange<GetValue>(
		std::forward<GetValue>(func));
}
template<detail::Iterable Map>
decltype(auto) CacheEndRange(Map&& map) {
	return v_CacheEndRange<Map>(std::forward<Map>(map));
}
template<typename BeginFunc, typename EndFunc, typename Map>
	requires(detail::BeginEndFunc<BeginFunc, EndFunc, Map>)
decltype(auto) CustomRange(
	Map&& map,
	BeginFunc&& beginFunc,
	EndFunc&& endFunc) {
	return v_CustomRange<BeginFunc, EndFunc, Map>(std::forward<Map>(map), std::forward<BeginFunc>(beginFunc), std::forward<EndFunc>(endFunc));
}
template<detail::EndlessIterable Map>
decltype(auto) IRangeImpl(Map&& map) {
	return v_IRangeImpl<Map>(std::forward<Map>(map));
}
template<typename Dst>
v_StaticCastRange<Dst> StaticCastRange() { return {}; };
template<typename Dst>
v_ReinterpretCastRange<Dst> ReinterpretCastRange() { return {}; };
template<typename Dst>
v_ConstCastRange<Dst> ConstCastRange() { return {}; }
template <typename Dst>
decltype(auto) IRangePipelineImpl(auto&& func){
	using T = decltype(func);
	return v_IRangePipelineImpl<Dst, T>(std::forward<T>(func));

}
}// namespace vstd