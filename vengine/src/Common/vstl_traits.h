#pragma once
#include <type_traits>
#include <stdint.h>
namespace vstd {

#define EA_CONSTEXPR_OR_CONST constexpr

template<typename T, bool small_>
struct ct_imp2 { typedef const T& param_type; };

template<typename T>
struct ct_imp2<T, true> { typedef const T param_type; };

template<typename T, bool isp, bool b1>
struct ct_imp { typedef const T& param_type; };

template<typename T, bool isp>
struct ct_imp<T, isp, true> { typedef typename ct_imp2<T, sizeof(T) <= sizeof(void*)>::param_type param_type; };

template<typename T, bool b1>
struct ct_imp<T, true, b1> { typedef T const param_type; };
template<typename T>
struct call_traits {
public:
	typedef T value_type;
	typedef T& reference;
	typedef const T& const_reference;
	typedef typename ct_imp<T, std::is_pointer<T>::value, std::is_arithmetic<T>::value>::param_type param_type;
};

template<typename T>
struct call_traits<T&> {
	typedef T& value_type;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T& param_type;
};

template<typename T, size_t N>
struct call_traits<T[N]> {
private:
	typedef T array_type[N];

public:
	typedef const T* value_type;
	typedef array_type& reference;
	typedef const array_type& const_reference;
	typedef const T* const param_type;
};

template<typename T, size_t N>
struct call_traits<const T[N]> {
private:
	typedef const T array_type[N];

public:
	typedef const T* value_type;
	typedef array_type& reference;
	typedef const array_type& const_reference;
	typedef const T* const param_type;
};

template<typename T1, typename T2>
class compressed_pair;

template<typename T1, typename T2, bool isSame, bool firstEmpty, bool secondEmpty>
struct compressed_pair_switch;

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, false, false, false> { static const int value = 0; };

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, false, true, false> { static const int value = 1; };

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, false, false, true> { static const int value = 2; };

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, false, true, true> { static const int value = 3; };

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, true, true, true> { static const int value = 4; };

template<typename T1, typename T2>
struct compressed_pair_switch<T1, T2, true, false, false> { static const int value = 5; };

template<typename T1, typename T2, int version>
class compressed_pair_imp;

template<typename T>
inline void cp_swap(T& t1, T& t2) {
	T tTemp = t1;
	t1 = t2;
	t2 = tTemp;
}

// Derive from neither
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 0> {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type y)
		: mFirst(x), mSecond(y) {}

	compressed_pair_imp(first_param_type x)
		: mFirst(x) {}

	compressed_pair_imp(second_param_type y)
		: mSecond(y) {}

	first_reference first() { return mFirst; }
	first_const_reference first() const { return mFirst; }

	second_reference second() { return mSecond; }
	second_const_reference second() const { return mSecond; }

	void swap(compressed_pair<T1, T2>& y) {
		cp_swap(mFirst, y.first());
		cp_swap(mSecond, y.second());
	}

private:
	first_type mFirst;
	second_type mSecond;
};

// Derive from T1
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 1> : private T1 {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type y)
		: first_type(x), mSecond(y) {}

	compressed_pair_imp(first_param_type x)
		: first_type(x) {}

	compressed_pair_imp(second_param_type y)
		: mSecond(y) {}

	first_reference first() { return *this; }
	first_const_reference first() const { return *this; }

	second_reference second() { return mSecond; }
	second_const_reference second() const { return mSecond; }

	void swap(compressed_pair<T1, T2>& y) {
		// No need to swap empty base class
		cp_swap(mSecond, y.second());
	}

private:
	second_type mSecond;
};

// Derive from T2
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 2> : private T2 {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type y)
		: second_type(y), mFirst(x) {}

	compressed_pair_imp(first_param_type x)
		: mFirst(x) {}

	compressed_pair_imp(second_param_type y)
		: second_type(y) {}

	first_reference first() { return mFirst; }
	first_const_reference first() const { return mFirst; }

	second_reference second() { return *this; }
	second_const_reference second() const { return *this; }

	void swap(compressed_pair<T1, T2>& y) {
		// No need to swap empty base class
		cp_swap(mFirst, y.first());
	}

private:
	first_type mFirst;
};

// Derive from T1 and T2
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 3> : private T1, private T2 {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type y)
		: first_type(x), second_type(y) {}

	compressed_pair_imp(first_param_type x)
		: first_type(x) {}

	compressed_pair_imp(second_param_type y)
		: second_type(y) {}

	first_reference first() { return *this; }
	first_const_reference first() const { return *this; }

	second_reference second() { return *this; }
	second_const_reference second() const { return *this; }

	// No need to swap empty bases
	void swap(compressed_pair<T1, T2>&) {}
};

// T1 == T2, T1 and T2 are both empty
// Note does not actually store an instance of T2 at all;
// but reuses T1 base class for both first() and second().
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 4> : private T1 {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type)
		: first_type(x) {}

	compressed_pair_imp(first_param_type x)
		: first_type(x) {}

	first_reference first() { return *this; }
	first_const_reference first() const { return *this; }

	second_reference second() { return *this; }
	second_const_reference second() const { return *this; }

	void swap(compressed_pair<T1, T2>&) {}
};

// T1 == T2 and are not empty
template<typename T1, typename T2>
class compressed_pair_imp<T1, T2, 5> {
public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair_imp() {}

	compressed_pair_imp(first_param_type x, second_param_type y)
		: mFirst(x), mSecond(y) {}

	compressed_pair_imp(first_param_type x)
		: mFirst(x), mSecond(x) {}

	first_reference first() { return mFirst; }
	first_const_reference first() const { return mFirst; }

	second_reference second() { return mSecond; }
	second_const_reference second() const { return mSecond; }

	void swap(compressed_pair<T1, T2>& y) {
		cp_swap(mFirst, y.first());
		cp_swap(mSecond, y.second());
	}

private:
	first_type mFirst;
	second_type mSecond;
};

template<typename T1, typename T2>
class compressed_pair
	: private compressed_pair_imp<T1, T2,
								  compressed_pair_switch<
									  T1,
									  T2,
									  std::is_same<typename std::remove_cv<T1>::type, typename std::remove_cv<T2>::type>::value,
									  std::is_empty<T1>::value,
									  std::is_empty<T2>::value>::value> {
private:
	typedef compressed_pair_imp<T1, T2,
								compressed_pair_switch<
									T1,
									T2,
									std::is_same<typename std::remove_cv<T1>::type, typename std::remove_cv<T2>::type>::value,
									std::is_empty<T1>::value,
									std::is_empty<T2>::value>::value>
		base;

public:
	typedef T1 first_type;
	typedef T2 second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair() : base() {}
	compressed_pair(first_param_type x, second_param_type y) : base(x, y) {}
	explicit compressed_pair(first_param_type x) : base(x) {}
	explicit compressed_pair(second_param_type y) : base(y) {}

	first_reference first() { return base::first(); }
	first_const_reference first() const { return base::first(); }

	second_reference second() { return base::second(); }
	second_const_reference second() const { return base::second(); }

	void swap(compressed_pair& y) { base::swap(y); }
};

// Partial specialisation for case where T1 == T2:
template<typename T>
class compressed_pair<T, T>
	: private compressed_pair_imp<T, T,
								  compressed_pair_switch<
									  T,
									  T,
									  std::is_same<typename std::remove_cv<T>::type, typename std::remove_cv<T>::type>::value,
									  std::is_empty<T>::value,
									  std::is_empty<T>::value>::value> {
private:
	typedef compressed_pair_imp<T, T,
								compressed_pair_switch<
									T,
									T,
									std::is_same<typename std::remove_cv<T>::type, typename std::remove_cv<T>::type>::value,
									std::is_empty<T>::value,
									std::is_empty<T>::value>::value>
		base;

public:
	typedef T first_type;
	typedef T second_type;
	typedef typename call_traits<first_type>::param_type first_param_type;
	typedef typename call_traits<second_type>::param_type second_param_type;
	typedef typename call_traits<first_type>::reference first_reference;
	typedef typename call_traits<second_type>::reference second_reference;
	typedef typename call_traits<first_type>::const_reference first_const_reference;
	typedef typename call_traits<second_type>::const_reference second_const_reference;

	compressed_pair() : base() {}
	compressed_pair(first_param_type x, second_param_type y) : base(x, y) {}
	explicit compressed_pair(first_param_type x) : base(x) {}

	first_reference first() { return base::first(); }
	first_const_reference first() const { return base::first(); }

	second_reference second() { return base::second(); }
	second_const_reference second() const { return base::second(); }

	void swap(compressed_pair<T, T>& y) { base::swap(y); }
};

template<typename T1, typename T2>
inline void swap(compressed_pair<T1, T2>& x, compressed_pair<T1, T2>& y) {
	x.swap(y);
}
}// namespace vstd