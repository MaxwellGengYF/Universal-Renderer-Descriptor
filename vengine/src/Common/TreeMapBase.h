#pragma once
#include <Common/Compare.h>
#include <Common/Pool.h>
namespace vstd {
namespace detail {
class VENGINE_DLL_COMMON TreeMapUtility {
public:
	static void fixInsert(void* k, void*& root);
	static void* getNext(void* ptr);
	static void* getLast(void* ptr);
	static void deleteOneNode(void* z, void*& root);
};
}// namespace detail
template<typename K, typename V>
struct TreeElement {
	K first;
	V second;
	template<typename A, typename... B>
	requires(std::is_constructible_v<K, A&&>&& std::is_constructible_v<V, B&&...>) explicit TreeElement(A&& a, B&&... b)
		: first(std::forward<A>(a)),
		  second(std::forward<B>(b)...) {
	}
	TreeElement(TreeElement const&) = default;
	TreeElement(TreeElement&&) = default;
};
template<typename K>
struct TreeElement<K, void> {
	K first;
	template<typename A>
	requires(std::is_constructible_v<K, A&&>) explicit TreeElement(A&& a)
		: first(std::forward<A>(a)) {
	}
	TreeElement(TreeElement const&) = default;
	TreeElement(TreeElement&&) = default;
};
}// namespace vstd