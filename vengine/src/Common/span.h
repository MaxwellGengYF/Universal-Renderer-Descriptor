#pragma once
#include <Common/MetaLib.h>
namespace vstd {
static constexpr size_t dynamic_extent = size_t(-1);

namespace Internal {
// HasSizeAndData
//
// custom type trait to determine if std::data(Container) and std::size(Container) are well-formed.
//
template<typename, typename = void>
struct HasSizeAndData : std::false_type {};

template<typename T>
struct HasSizeAndData<T, std::void_t<decltype(std::size(std::declval<T>())), decltype(std::data(std::declval<T>()))>> : std::true_type {};

// SubspanExtent
//
// Integral constant that calculates the resulting extent of a templated subspan operation.
//
//   If Count is not dynamic_extent then SubspanExtent::value is Count,
//   otherwise, if Extent is not dynamic_extent, SubspanExtent::value is (Extent - Offset),
//   otherwise, SubspanExtent::value is dynamic_extent.
//
template<size_t Extent, size_t Offset, size_t Count>
struct SubspanExtent : std::integral_constant<size_t, (Count != dynamic_extent ? Count : (Extent != dynamic_extent ? (Extent - Offset) : dynamic_extent))> {};
}// namespace Internal

template<typename T, size_t Extent = vstd::dynamic_extent>
class span {
public:
	typedef T element_type;
	typedef std::remove_cv_t<T> value_type;
	typedef size_t index_type;
	typedef ptrdiff_t difference_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;

	static constexpr size_t extent = Extent;

	// constructors / destructor
	constexpr span() noexcept;
	constexpr span(const span& other) noexcept = default;
	constexpr span(pointer ptr, index_type count);
	constexpr span(pointer pBegin, pointer pEnd);
	~span() noexcept = default;

	// copy-assignment operator
	constexpr span& operator=(const span& other) noexcept = default;

	// conversion constructors for c-std::array and std::array
	template<size_t N, typename = std::enable_if_t<(Extent == vstd::dynamic_extent || N == Extent)>>
	constexpr span(element_type (&arr)[N]) noexcept;

	template<size_t N, typename = std::enable_if_t<(Extent == vstd::dynamic_extent || N == Extent)>>
	constexpr span(std::array<value_type, N>& arr) noexcept;

	template<size_t N, typename = std::enable_if_t<(Extent == vstd::dynamic_extent || N == Extent)>>
	constexpr span(const std::array<value_type, N>& arr) noexcept;

	// SfinaeForGenericContainers
	//
	template<typename Container>
	using SfinaeForGenericContainers =
		std::enable_if_t<!std::is_same_v<Container, span> && !std::is_array_v<Container> && Internal::HasSizeAndData<Container>::value && std::is_convertible_v<std::remove_pointer_t<decltype(std::data(std::declval<Container&>()))> (*)[], element_type (*)[]>>;

	// generic container conversion constructors
	template<typename Container, typename = SfinaeForGenericContainers<Container>>
	constexpr span(Container& cont);

	template<typename Container, typename = SfinaeForGenericContainers<const Container>>
	constexpr span(const Container& cont);

	template<typename U, size_t N, typename = std::enable_if_t<(Extent == vstd::dynamic_extent || N == Extent) && (std::is_convertible_v<U (*)[], element_type (*)[]>)>>
	constexpr span(const span<U, N>& s) noexcept;

	// subviews
	template<size_t Count>
	constexpr span<element_type, Count> first() const;
	constexpr span<element_type, dynamic_extent> first(size_t Count) const;

	template<size_t Count>
	constexpr span<element_type, Count> last() const;
	constexpr span<element_type, dynamic_extent> last(size_t Count) const;

	template<size_t Offset, size_t Count = dynamic_extent>
	constexpr span<element_type, Internal::SubspanExtent<Extent, Offset, Count>::value> subspan() const;
	constexpr span<element_type, dynamic_extent> subspan(size_t Offset, size_t Count = dynamic_extent) const;

	// observers
	constexpr pointer data() const noexcept;
	constexpr index_type size() const noexcept;
	constexpr index_type size_bytes() const noexcept;
	constexpr bool empty() const noexcept;

	// subscript operators, element access
	constexpr reference front() const;
	constexpr reference back() const;
	constexpr reference operator[](index_type idx) const;
	constexpr reference operator()(index_type idx) const;

	// iterator support
	constexpr iterator begin() const noexcept;
	constexpr iterator end() const noexcept;
	constexpr const_iterator cbegin() const noexcept;
	constexpr const_iterator cend() const noexcept;
	constexpr reverse_iterator rbegin() const noexcept;
	constexpr reverse_iterator rend() const noexcept;
	constexpr const_reverse_iterator crbegin() const noexcept;
	constexpr const_reverse_iterator crend() const noexcept;

private:
	pointer mpData = nullptr;
	index_type mnSize = 0;

private:
	constexpr bool bounds_check(size_t) const;// utility used in asserts
};

///////////////////////////////////////////////////////////////////////////
// template deduction guides
///////////////////////////////////////////////////////////////////////////
#ifdef __cpp_deduction_guides
template<class T, size_t N>
span(T (&)[N]) -> span<T, N>;
template<class T, size_t N>
span(std::array<T, N>&) -> span<T, N>;
template<class T, size_t N>
span(const std::array<T, N>&) -> span<const T, N>;
template<class Container>
span(Container&) -> span<typename Container::value_type>;
template<class Container>
span(const Container&) -> span<const typename Container::value_type>;
#endif

///////////////////////////////////////////////////////////////////////////
// comparison operators
///////////////////////////////////////////////////////////////////////////

template<class T, size_t X, class U, size_t Y>
constexpr bool operator==(span<T, X> l, span<U, Y> r) {
	return (l.size() == r.size()) && std::equal(l.begin(), l.end(), r.begin());
}

template<class T, size_t X, class U, size_t Y>
constexpr bool operator<(span<T, X> l, span<U, Y> r) {
	return std::lexicographical_compare(l.begin(), l.end(), r.begin(), r.end());
}

template<class T, size_t X, class U, size_t Y>
constexpr bool operator!=(span<T, X> l, span<U, Y> r) { return !(l == r); }

template<class T, size_t X, class U, size_t Y>
constexpr bool operator<=(span<T, X> l, span<U, Y> r) { return !(r < l); }

template<class T, size_t X, class U, size_t Y>
constexpr bool operator>(span<T, X> l, span<U, Y> r) { return r < l; }

template<class T, size_t X, class U, size_t Y>
constexpr bool operator>=(span<T, X> l, span<U, Y> r) { return !(l < r); }

///////////////////////////////////////////////////////////////////////////
// ctor implementations
///////////////////////////////////////////////////////////////////////////

template<typename T, size_t Extent>
constexpr span<T, Extent>::span() noexcept {
	static_assert(Extent == dynamic_extent || Extent == 0, "impossible to default construct a span with a fixed Extent different than 0");
}

template<typename T, size_t Extent>
constexpr span<T, Extent>::span(pointer ptr, index_type size)
	: mpData(ptr), mnSize(size) {
	EASTL_ASSERT_MSG(Extent == dynamic_extent || Extent == mnSize, "impossible to create a span with a fixed Extent different than the size of the supplied buffer");
}

template<typename T, size_t Extent>
constexpr span<T, Extent>::span(pointer pBegin, pointer pEnd)
	: mpData(pBegin), mnSize(static_cast<index_type>(pEnd - pBegin)) {
	EASTL_ASSERT_MSG(Extent == dynamic_extent || Extent == mnSize, "impossible to create a span with a fixed Extent different than the size of the supplied buffer");
}

template<typename T, size_t Extent>
template<size_t N, typename>
constexpr span<T, Extent>::span(element_type (&arr)[N]) noexcept
	: span(arr, static_cast<index_type>(N)) {
}

template<typename T, size_t Extent>
template<size_t N, typename>
constexpr span<T, Extent>::span(std::array<value_type, N>& arr) noexcept
	: span(arr.data(), arr.size()) {
}

template<typename T, size_t Extent>
template<size_t N, typename>
constexpr span<T, Extent>::span(const std::array<value_type, N>& arr) noexcept
	: span(arr.data(), arr.size()) {
}

template<typename T, size_t Extent>
template<typename Container, typename>
constexpr span<T, Extent>::span(Container& cont)
	: span(static_cast<pointer>(std::data(cont)), static_cast<index_type>(std::size(cont))) {
}

template<typename T, size_t Extent>
template<typename Container, typename>
constexpr span<T, Extent>::span(const Container& cont)
	: span(static_cast<pointer>(std::data(cont)), static_cast<index_type>(std::size(cont))) {
}

template<typename T, size_t Extent>
template<typename U, size_t N, typename>
constexpr span<T, Extent>::span(const span<U, N>& s) noexcept
	: span(s.data(), s.size()) {
}

///////////////////////////////////////////////////////////////////////////
// member function implementations
///////////////////////////////////////////////////////////////////////////

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::pointer span<T, Extent>::data() const noexcept {
	return mpData;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::index_type span<T, Extent>::size() const noexcept {
	return mnSize;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::index_type span<T, Extent>::size_bytes() const noexcept {
	return size() * sizeof(element_type);
}

template<typename T, size_t Extent>
constexpr bool span<T, Extent>::empty() const noexcept {
	return size() == 0;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reference span<T, Extent>::front() const {
	EASTL_ASSERT_MSG(!empty(), "undefined behavior accessing an empty span");

	return mpData[0];
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reference span<T, Extent>::back() const {
	EASTL_ASSERT_MSG(!empty(), "undefined behavior accessing an empty span");

	return mpData[mnSize - 1];
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reference span<T, Extent>::operator[](index_type idx) const {
	EASTL_ASSERT_MSG(!empty(), "undefined behavior accessing an empty span");
	EASTL_ASSERT_MSG(bounds_check(idx), "undefined behavior accessing out of bounds");

	return mpData[idx];
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reference span<T, Extent>::operator()(index_type idx) const {
	EASTL_ASSERT_MSG(!empty(), "undefined behavior accessing an empty span");
	EASTL_ASSERT_MSG(bounds_check(idx), "undefined behavior accessing out of bounds");

	return mpData[idx];
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::iterator span<T, Extent>::begin() const noexcept {
	return mpData;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::iterator span<T, Extent>::end() const noexcept {
	return mpData + mnSize;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::const_iterator span<T, Extent>::cbegin() const noexcept {
	return mpData;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::const_iterator span<T, Extent>::cend() const noexcept {
	return mpData + mnSize;
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reverse_iterator span<T, Extent>::rbegin() const noexcept {
	return reverse_iterator(mpData + mnSize);
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::reverse_iterator span<T, Extent>::rend() const noexcept {
	return reverse_iterator(mpData);
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::const_reverse_iterator span<T, Extent>::crbegin() const noexcept {
	return const_reverse_iterator(mpData + mnSize);
}

template<typename T, size_t Extent>
constexpr typename span<T, Extent>::const_reverse_iterator span<T, Extent>::crend() const noexcept {
	return const_reverse_iterator(mpData);
}

template<typename T, size_t Extent>
template<size_t Count>
constexpr span<typename span<T, Extent>::element_type, Count> span<T, Extent>::first() const {
	EASTL_ASSERT_MSG(Count <= size(), "undefined behavior accessing out of bounds");
	return {data(), static_cast<index_type>(Count)};
}

template<typename T, size_t Extent>
constexpr span<typename span<T, Extent>::element_type, dynamic_extent>
span<T, Extent>::first(size_t sz) const {
	EASTL_ASSERT_MSG(sz <= size(), "undefined behavior accessing out of bounds");
	return {data(), static_cast<index_type>(sz)};
}

template<typename T, size_t Extent>
template<size_t Count>
constexpr span<typename span<T, Extent>::element_type, Count> span<T, Extent>::last() const {
	EASTL_ASSERT_MSG(Count <= size(), "undefined behavior accessing out of bounds");
	return {data() + size() - Count, static_cast<index_type>(Count)};
}

template<typename T, size_t Extent>
constexpr span<typename span<T, Extent>::element_type, dynamic_extent>
span<T, Extent>::last(size_t sz) const {
	EASTL_ASSERT_MSG(sz <= size(), "undefined behavior accessing out of bounds");
	return {data() + size() - sz, static_cast<index_type>(sz)};
}

template<typename T, size_t Extent>
template<size_t Offset, size_t Count>
constexpr span<typename span<T, Extent>::element_type, Internal::SubspanExtent<Extent, Offset, Count>::value>
span<T, Extent>::subspan() const {
	EASTL_ASSERT_MSG(Offset <= size(), "undefined behaviour accessing out of bounds");
	EASTL_ASSERT_MSG(Count == dynamic_extent || Count <= (size() - Offset), "undefined behaviour exceeding size of span");

	return {data() + Offset, size_t(Count == dynamic_extent ? size() - Offset : Count)};
}

template<typename T, size_t Extent>
constexpr span<typename span<T, Extent>::element_type, dynamic_extent>
span<T, Extent>::subspan(size_t offset, size_t count) const {
	EASTL_ASSERT_MSG(offset <= size(), "undefined behaviour accessing out of bounds");
	EASTL_ASSERT_MSG(count == dynamic_extent || count <= (size() - offset), "undefined behaviour exceeding size of span");

	return {data() + offset, size_t(count == dynamic_extent ? size() - offset : count)};
}

template<typename T, size_t Extent>
constexpr bool span<T, Extent>::bounds_check(size_t offset) const {
	return offset < size();
}
}