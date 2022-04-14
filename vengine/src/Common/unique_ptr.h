#pragma once
#include <Common/MetaLib.h>
#include <Common/Memory.h>
#include <Common/vstl_traits.h>
#include <Common/unique_ptr_deleter.h>
namespace eastl {
namespace Internal {
// Tells if the Deleter type has a typedef for pointer to T. If so then return it,
// else return T*. The large majority of the time the pointer type will be T*.
// The C++11 Standard requires that scoped_ptr let the deleter define the pointer type.
//
// Example usage:
//     typedef typename unique_pointer_type<int, SomeDeleter>::type pointer
//
template<typename T, typename Deleter>
class unique_pointer_type {
	template<typename U>
	static typename U::pointer test(typename U::pointer*);

	template<typename U>
	static T* test(...);

public:
	typedef decltype(test<typename std::remove_reference<Deleter>::type>(0)) type;
};

///////////////////////////////////////////////////////////////////////
// is_array_cv_convertible
//
// Tells if the array pointer P1 is cv-convertible to array pointer P2.
// The two types have two be equivalent pointer types and be convertible
// when you consider const/volatile properties of them.
//
// Example usage:
//     is_array_cv_convertible<int, Base*>::value             => false
//     is_array_cv_convertible<Base, Base*>::value            => false
//     is_array_cv_convertible<double*, bool*>::value         => false
//     is_array_cv_convertible<Subclass*, Base*>::value       => false
//     is_array_cv_convertible<const Base*, Base*>::value     => false
//     is_array_cv_convertible<Base*, Base*>::value           => true
//     is_array_cv_convertible<Base*, const Base*>::value     => true
//     is_array_cv_convertible<Base*, volatile Base*>::value  => true
///////////////////////////////////////////////////////////////////////

#define EASTL_TYPE_TRAIT_is_array_cv_convertible_CONFORMANCE 1

template<typename P1, typename P2, bool = std::is_same_v<std::remove_cv_t<typename std::pointer_traits<P1>::element_type>, std::remove_cv_t<typename std::pointer_traits<P2>::element_type>>>
struct is_array_cv_convertible_impl
	: public std::is_convertible<P1, P2> {};// Return true if P1 is convertible to P2.

template<typename P1, typename P2>
struct is_array_cv_convertible_impl<P1, P2, false>
	: public std::false_type {};// P1's underlying type is not the same as P2's, so it can't be converted, even if P2 refers to a subclass of P1. Parent == Child, but Parent[] != Child[]

template<typename P1, typename P2, bool = std::is_scalar_v<P1> && !std::is_pointer_v<P1>>
struct is_array_cv_convertible
	: public is_array_cv_convertible_impl<P1, P2> {};

template<typename P1, typename P2>
struct is_array_cv_convertible<P1, P2, true>
	: public std::false_type {};// P1 is scalar not a pointer, so it can't be converted to a pointer.

///////////////////////////////////////////////////////////////////////
// is_derived
//
// Given two (possibly identical) types Base and Derived, is_base_of<Base, Derived>::value == true
// if and only if Base is a direct or indirect base class of Derived. This is like is_base_of<Base, Derived>
// but returns false if Derived is the same as Base. So is_derived is true only if Derived is actually a subclass
// of Base and not Base itself.
//
// is_derived may only be applied to complete types.
//
// Example usage:
//     is_derived<int, int>::value             => false
//     is_derived<int, bool>::value            => false
//     is_derived<Parent, Child>::value        => true
//     is_derived<Child, Parent>::value        => false
///////////////////////////////////////////////////////////////////////

#if EASTL_TYPE_TRAIT_is_base_of_CONFORMANCE
#define EASTL_TYPE_TRAIT_is_derived_CONFORMANCE 1

template<typename Base, typename Derived>
struct is_derived : public std::integral_constant<bool, std::is_base_of<Base, Derived>::value && !std::is_same<typename std::remove_cv<Base>::type, typename std::remove_cv<Derived>::type>::value> {};
#else
#define EASTL_TYPE_TRAIT_is_derived_CONFORMANCE 0

template<typename Base, typename Derived>// This returns true if Derived is unrelated to Base. That's a wrong answer, but is better for us than returning false for compilers that don't support is_base_of.
struct is_derived : public std::integral_constant<bool, !std::is_same<typename std::remove_cv<Base>::type, typename std::remove_cv<Derived>::type>::value> {};
#endif

///////////////////////////////////////////////////////////////////////
// is_safe_array_conversion
//
// Say you have two array types: T* t and U* u. You want to assign the u to t but only if
// that's a safe thing to do. As shown in the logic below, the array conversion
// is safe if U* and T* are convertible, if U is an array, and if either U or T is not
// a pointer or U is not derived from T.
//
// Note: Usage of this class could be replaced with is_array_cv_convertible usage.
// To do: Do this replacement and test it.
//
///////////////////////////////////////////////////////////////////////

template<typename T, typename T_pointer, typename U, typename U_pointer>
struct is_safe_array_conversion : public std::integral_constant<bool, std::is_convertible<U_pointer, T_pointer>::value && std::is_array<U>::value && (!std::is_pointer<U_pointer>::value || !std::is_pointer<T_pointer>::value || !Internal::is_derived<T, typename std::remove_extent<U>::type>::value)> {};

}// namespace Internal

/// default_delete
///
/// C++11 smart pointer default delete function class.
///
/// Provides a default way to delete an object. This default is simply to call delete on the
/// object pointer. You can provide an alternative to this class or you can override this on
/// a class-by-class basis like the following:
///     template <>
///     struct smart_ptr_deleter<MyClass>
///     {
///         void operator()(MyClass* p) const
///            { SomeCustomFunction(p); }
///     };
///
template<typename T>
struct default_delete {
#if defined(EA_COMPILER_GNUC) && (EA_COMPILER_VERSION <= 4006)// GCC prior to 4.7 has a bug with noexcept here.
	constexpr default_delete() = default;
#else
	constexpr default_delete() noexcept = default;
#endif

	template<typename U>// Enable if T* can be constructed with U* (i.e. U* is convertible to T*).
	default_delete(const default_delete<U>&, typename std::enable_if<std::is_convertible<U*, T*>::value>::type* = 0) noexcept {}

	void operator()(T* p) const noexcept {
		delete p;
	}
};

template<typename T>
struct default_delete<T[]>// Specialization for arrays.
{
#if defined(EA_COMPILER_GNUC) && (EA_COMPILER_VERSION <= 4006)// GCC prior to 4.7 has a bug with noexcept here.
	constexpr default_delete() = default;
#else
	constexpr default_delete() noexcept = default;
#endif

	template<typename U>// This ctor is enabled if T is equal to or a base of U, and if U is less or equal const/volatile-qualified than T.
	default_delete(const default_delete<U[]>&, typename std::enable_if<Internal::is_array_cv_convertible<U*, T*>::value>::type* = 0) noexcept {}

	void operator()(T* p) const noexcept { delete[] p; }
};

/// smart_ptr_deleter
///
/// Deprecated in favor of the C++11 name: default_delete
///
template<typename T>
struct smart_ptr_deleter {
	typedef T value_type;

	void operator()(const value_type* p) const// We use a const argument type in order to be most flexible with what types we accept.
	{ delete const_cast<value_type*>(p); }
};

template<>
struct smart_ptr_deleter<void> {
	typedef void value_type;

	void operator()(const void* p) const { delete[](char*) p; }// We don't seem to have much choice but to cast to a scalar type.
};

template<>
struct smart_ptr_deleter<const void> {
	typedef void value_type;

	void operator()(const void* p) const { delete[](char*) p; }// We don't seem to have much choice but to cast to a scalar type.
};

/// smart_array_deleter
///
/// Deprecated in favor of the C++11 name: default_delete
///
template<typename T>
struct smart_array_deleter {
	typedef T value_type;

	void operator()(const value_type* p) const// We use a const argument type in order to be most flexible with what types we accept.
	{ delete[] const_cast<value_type*>(p); }
};

template<>
struct smart_array_deleter<void> {
	typedef void value_type;

	void operator()(const void* p) const { delete[](char*) p; }// We don't seem to have much choice but to cast to a scalar type.
};


template<typename T, typename Deleter = std::default_delete<T>>
class unique_ptr {
	static_assert(!std::is_rvalue_reference<Deleter>::value, "The supplied Deleter cannot be a r-value reference.");

public:
	typedef Deleter deleter_type;
	typedef T element_type;
	typedef unique_ptr<element_type, deleter_type> this_type;
	typedef typename Internal::unique_pointer_type<element_type, deleter_type>::type pointer;

public:
	/// unique_ptr
	/// Construct a unique_ptr from a pointer allocated via new.
	/// Example usage:
	///    unique_ptr<int> ptr;
	constexpr unique_ptr() noexcept
		: mPair(pointer()) {
		static_assert(!std::is_pointer<deleter_type>::value, "unique_ptr deleter default-constructed with null pointer. Use a different constructor or change your deleter to a class.");
	}

	/// unique_ptr
	/// Construct a unique_ptr from a null pointer.
	/// Example usage:
	///    unique_ptr<int> ptr(nullptr);
	constexpr unique_ptr(std::nullptr_t) noexcept
		: mPair(pointer()) {
		static_assert(!std::is_pointer<deleter_type>::value, "unique_ptr deleter default-constructed with null pointer. Use a different constructor or change your deleter to a class.");
	}

	/// unique_ptr
	/// Construct a unique_ptr from a pointer allocated via new.
	/// Example usage:
	///    unique_ptr<int> ptr(new int(3));
	explicit unique_ptr(pointer pValue) noexcept
		: mPair(pValue) {
		static_assert(!std::is_pointer<deleter_type>::value, "unique_ptr deleter default-constructed with null pointer. Use a different constructor or change your deleter to a class.");
	}

	/// unique_ptr
	/// Constructs a unique_ptr with the owner pointer and deleter specified
	/// Example usage:
	///     std::smart_ptr_deleter<int> del;
	///     unique_ptr<int> ptr(new int(3), del);
	unique_ptr(pointer pValue, typename std::conditional<std::is_lvalue_reference<deleter_type>::value, deleter_type, typename std::add_lvalue_reference<const deleter_type>::type>::type deleter) noexcept
		: mPair(pValue, deleter) {}

	/// unique_ptr
	/// Constructs a unique_ptr with the owned pointer and deleter specified (rvalue)
	/// Example usage:
	///     unique_ptr<int> ptr(new int(3), std::smart_ptr_deleter<int>());
	unique_ptr(pointer pValue, typename std::remove_reference<deleter_type>::type&& deleter) noexcept
		: mPair(pValue, std::move(deleter)) {
		static_assert(!std::is_lvalue_reference<deleter_type>::value, "deleter_type reference refers to an rvalue deleter. The reference will probably become invalid before used. Change the deleter_type to not be a reference or construct with permanent deleter.");
	}

	/// unique_ptr
	/// Move constructor
	/// Example usage:
	///     unique_ptr<int> ptr(new int(3));
	///     unique_ptr<int> newPtr = std::move(ptr);
	unique_ptr(this_type&& x) noexcept
		: mPair(x.release(), std::forward<deleter_type>(x.get_deleter())) {}

	/// unique_ptr
	/// Move constructor
	/// Example usage:
	///     unique_ptr<int> ptr(new int(3));
	///     unique_ptr<int> newPtr = std::move(ptr);
	template<typename U, typename E>
	unique_ptr(unique_ptr<U, E>&& u, typename std::enable_if<!std::is_array<U>::value && std::is_convertible<typename unique_ptr<U, E>::pointer, pointer>::value && std::is_convertible<E, deleter_type>::value && (std::is_same<deleter_type, E>::value || !std::is_lvalue_reference<deleter_type>::value)>::type* = 0) noexcept
		: mPair(u.release(), std::forward<E>(u.get_deleter())) {}

	/// unique_ptr
	/// Move assignment
	/// Example usage:
	///     unique_ptr<int> ptr(new int(3));
	///     unique_ptr<int> newPtr(new int(4));
	///     ptr = std::move(newPtr);  // Deletes int(3) and assigns mpValue to int(4)
	this_type& operator=(this_type&& x) noexcept {
		reset(x.release());
		mPair.second() = std::move(std::forward<deleter_type>(x.get_deleter()));
		return *this;
	}

	/// unique_ptr
	/// Move assignment
	template<typename U, typename E>
	typename std::enable_if<!std::is_array<U>::value && std::is_convertible<typename unique_ptr<U, E>::pointer, pointer>::value && std::is_assignable<deleter_type&, E&&>::value, this_type&>::type
	operator=(unique_ptr<U, E>&& u) noexcept {
		reset(u.release());
		mPair.second() = std::move(std::forward<E>(u.get_deleter()));
		return *this;
	}

	/// operator=(nullptr_t)
	this_type& operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	/// ~unique_ptr
	/// Destroys the owned pointer. The destructor for the object
	/// referred to by the owned pointer will be called.
	~unique_ptr() noexcept {
		reset();
	}

	/// reset
	/// Deletes the owned pointer and takes ownership of the
	/// passed in pointer. If the passed in pointer is the same
	/// as the owned pointer, nothing is done.
	/// Example usage:
	///    unique_ptr<int> ptr(new int(3));
	///    ptr.reset(new int(4));  // deletes int(3)
	///    ptr.reset(NULL);        // deletes int(4)
	void reset(pointer pValue = pointer()) noexcept {
		if (pValue != mPair.first()) {
			if (auto first = std::exchange(mPair.first(), pValue))
				get_deleter()(first);
		}
	}

	/// release
	/// This simply forgets the owned pointer. It doesn't
	/// free it but rather assumes that the user does.
	/// Example usage:
	///    unique_ptr<int> ptr(new int(3));
	///    int* pInt = ptr.release();
	///    delete pInt;
	pointer release() noexcept {
		pointer const pTemp = mPair.first();
		mPair.first() = pointer();
		return pTemp;
	}

	/// detach
	/// For backwards-compatibility with pre-C++11 code.
	pointer detach() noexcept { return release(); }

	/// swap
	/// Exchanges the owned pointer beween two unique_ptr objects.
	void swap(this_type& x) noexcept {
		mPair.swap(x.mPair);
	}

	/// operator*
	/// Returns the owner pointer dereferenced.
	/// Example usage:
	///    unique_ptr<int> ptr(new int(3));
	///    int x = *ptr;
	typename std::add_lvalue_reference<T>::type operator*() const// Not noexcept, because the pointer may be NULL.
	{
		return *mPair.first();
	}

	/// operator->
	/// Allows access to the owned pointer via operator->()
	/// Example usage:
	///    struct X{ void DoSomething(); };
	///    unique_ptr<int> ptr(new X);
	///    ptr->DoSomething();
	pointer operator->() const noexcept {
		return mPair.first();
	}

	/// get
	/// Returns the owned pointer. Note that this class does
	/// not provide an operator T() function. This is because such
	/// a thing (automatic conversion) is deemed unsafe.
	/// Example usage:
	///    struct X{ void DoSomething(); };
	///    unique_ptr<int> ptr(new X);
	///    X* pX = ptr.get();
	///    pX->DoSomething();
	pointer get() const noexcept {
		return mPair.first();
	}

	/// get_deleter
	/// Returns the deleter used to delete the owned pointer
	/// Example usage:
	/// unique_ptr<int> ptr(new int(3));
	/// std::smart_ptr_deleter<int>& del = ptr.get_deleter();
	deleter_type& get_deleter() noexcept {
		return mPair.second();
	}

	/// get_deleter
	/// Const version for getting the deleter
	const deleter_type& get_deleter() const noexcept {
		return mPair.second();
	}

#ifdef EA_COMPILER_NO_EXPLICIT_CONVERSION_OPERATORS
	/// Note that below we do not use operator bool(). The reason for this
	/// is that booleans automatically convert up to short, int, float, etc.
	/// The result is that this: if(uniquePtr == 1) would yield true (bad).
	typedef T* (this_type::*bool_)() const;
	operator bool_() const noexcept {
		if (mPair.first())
			return &this_type::get;
		return NULL;
	}

	bool operator!() const noexcept {
		return (mPair.first() == pointer());
	}
#else
	/// operator bool
	/// Allows for using a unique_ptr as a boolean.
	/// Example usage:
	///    unique_ptr<int> ptr(new int(3));
	///    if(ptr)
	///        ++*ptr;
	///
	explicit operator bool() const noexcept {
		return (mPair.first() != pointer());
	}
#endif

	/// These functions are deleted in order to prevent copying, for safety.
	unique_ptr(const this_type&) = delete;
	unique_ptr& operator=(const this_type&) = delete;
	unique_ptr& operator=(pointer pValue) = delete;

protected:
	vstd::compressed_pair<pointer, deleter_type> mPair;
};// class unique_ptr

/// unique_ptr specialization for unbounded arrays.
///
/// Differences from unique_ptr<T>:
///     - Conversions between different types of unique_ptr<T[], D> or to or
///       from the non-array forms of unique_ptr produce an ill-formed program.
///     - Pointers to types derived from T are rejected by the constructors, and by reset.
///     - The observers operator* and operator-> are not provided.
///     - The indexing observer operator[] is provided.
///     - The default deleter will call delete[].
///
/// It's not possible to create a unique_ptr for arrays of a known bound (e.g. int[4] as opposed to int[]).
///
/// Example usage:
///     unique_ptr<int[]> ptr(new int[10]);
///     ptr[4] = 4;
///
template<typename T, typename Deleter>
class unique_ptr<T[], Deleter> {
public:
	typedef Deleter deleter_type;
	typedef T element_type;
	typedef unique_ptr<element_type[], deleter_type> this_type;
	typedef typename Internal::unique_pointer_type<element_type, deleter_type>::type pointer;

public:
	constexpr unique_ptr() noexcept
		: mPair(pointer()) {
		static_assert(!std::is_pointer<deleter_type>::value, "unique_ptr deleter default-constructed with null pointer. Use a different constructor or change your deleter to a class.");
	}

	constexpr unique_ptr(std::nullptr_t) noexcept
		: mPair(pointer()) {
		static_assert(!std::is_pointer<deleter_type>::value, "unique_ptr deleter default-constructed with null pointer. Use a different constructor or change your deleter to a class.");
	}

	template<typename P,
			 typename = std::enable_if_t<Internal::is_array_cv_convertible<P, pointer>::value>>// Pointers to types derived from T are rejected by the constructors, and by reset.
	explicit unique_ptr(P pArray) noexcept
		: mPair(pArray) {
		static_assert(!std::is_pointer<deleter_type>::value,
					  "unique_ptr deleter default-constructed with null pointer. Use a different constructor or "
					  "change your deleter to a class.");
	}

	template<typename P>
	unique_ptr(P pArray, typename std::conditional<std::is_lvalue_reference<deleter_type>::value, deleter_type, typename std::add_lvalue_reference<const deleter_type>::type>::type deleter,
			   typename std::enable_if<Internal::is_array_cv_convertible<P, pointer>::value>::type* = 0) noexcept
		: mPair(pArray, deleter) {}

	template<typename P>
	unique_ptr(P pArray, typename std::remove_reference<deleter_type>::type&& deleter, std::enable_if_t<Internal::is_array_cv_convertible<P, pointer>::value>* = 0) noexcept
		: mPair(pArray, std::move(deleter)) {
		static_assert(!std::is_lvalue_reference<deleter_type>::value, "deleter_type reference refers to an rvalue deleter. The reference will probably become invalid before used. Change the deleter_type to not be a reference or construct with permanent deleter.");
	}

	unique_ptr(this_type&& x) noexcept
		: mPair(x.release(), std::forward<deleter_type>(x.get_deleter())) {}

	template<typename U, typename E>
	unique_ptr(unique_ptr<U, E>&& u, typename std::enable_if<Internal::is_safe_array_conversion<T, pointer, U, typename unique_ptr<U, E>::pointer>::value && std::is_convertible<E, deleter_type>::value && (!std::is_lvalue_reference<deleter_type>::value || std::is_same<E, deleter_type>::value)>::type* = 0) noexcept
		: mPair(u.release(), std::forward<E>(u.get_deleter())) {}

	this_type& operator=(this_type&& x) noexcept {
		reset(x.release());
		mPair.second() = std::move(std::forward<deleter_type>(x.get_deleter()));
		return *this;
	}

	template<typename U, typename E>
	typename std::enable_if<Internal::is_safe_array_conversion<T, pointer, U, typename unique_ptr<U, E>::pointer>::value && std::is_assignable<deleter_type&, E&&>::value, this_type&>::type
	operator=(unique_ptr<U, E>&& u) noexcept {
		reset(u.release());
		mPair.second() = std::move(std::forward<E>(u.get_deleter()));
		return *this;
	}

	this_type& operator=(std::nullptr_t) noexcept {
		reset();
		return *this;
	}

	~unique_ptr() noexcept {
		reset();
	}

	void reset(pointer pArray = pointer()) noexcept {
		if (pArray != mPair.first()) {
			if (auto first = std::exchange(mPair.first(), pArray))
				get_deleter()(first);
		}
	}

	pointer release() noexcept {
		pointer const pTemp = mPair.first();
		mPair.first() = pointer();
		return pTemp;
	}

	/// detach
	/// For backwards-compatibility with pre-C++11 code.
	pointer detach() noexcept { return release(); }

	void swap(this_type& x) noexcept {
		mPair.swap(x.mPair);
	}

	/// operator[]
	/// Returns a reference to the specified item in the owned pointer
	/// array.
	/// Example usage:
	///    unique_ptr<int> ptr(new int[6]);
	///    int x = ptr[2];
	typename std::add_lvalue_reference<T>::type operator[](ptrdiff_t i) const {
		// assert(mpArray && (i >= 0));
		return mPair.first()[i];
	}

	pointer get() const noexcept {
		return mPair.first();
	}

	deleter_type& get_deleter() noexcept {
		return mPair.second();
	}

	const deleter_type& get_deleter() const noexcept {
		return mPair.second();
	}

#ifdef EA_COMPILER_NO_EXPLICIT_CONVERSION_OPERATORS
	typedef T* (this_type::*bool_)() const;
	operator bool_() const noexcept {
		if (mPair.first())
			return &this_type::get;
		return NULL;
	}

	bool operator!() const noexcept {
		return (mPair.first() == pointer());
	}
#else
	explicit operator bool() const noexcept {
		return (mPair.first() != pointer());
	}
#endif

	/// These functions are deleted in order to prevent copying, for safety.
	unique_ptr(const this_type&) = delete;
	unique_ptr& operator=(const this_type&) = delete;
	unique_ptr& operator=(pointer pArray) = delete;

protected:
	vstd::compressed_pair<pointer, deleter_type> mPair;
};

/// make_unique
///
/// The C++11 Standard doesn't have make_unique, but there's no agreed reason as to why.
/// http://stackoverflow.com/questions/12580432/why-does-c11-have-make-shared-but-not-make-unique
/// http://herbsutter.com/2013/05/29/gotw-89-solution-smart-pointers/
/// Herb's solution is OK but doesn't support unique_ptr<[]> (array version). We do the same
/// thing libc++ does and make a specialization of make_unique for arrays.
///
/// make_unique has two cases where you can't use it and need to directly use unique_ptr:
///     - You need to construct the unique_ptr with a raw pointer.
///     - You need to specify a custom deleter.
///
/// Note: This function uses global new T by default to create the ptr instance, as per
/// the C++11 Standard make_shared_ptr.
///
/// Example usage:
///     struct Test{ Test(int, int){} };
///     auto p = make_unique<Test>(1, 2);
///
///     auto pArray = make_unique<Test[]>(4);
///
namespace Internal {
template<typename T>
struct unique_type { typedef unique_ptr<T> unique_type_single; };

template<typename T>
struct unique_type<T[]> { typedef unique_ptr<T[]> unique_type_unbounded_array; };

template<typename T, size_t N>
struct unique_type<T[N]> { typedef void unique_type_bounded_array; };
}// namespace Internal

template<typename T, typename... Args>
inline typename Internal::unique_type<T>::unique_type_single make_unique(Args&&... args) { return unique_ptr<T>(new T(std::forward<Args>(args)...)); }

template<typename T>
inline typename Internal::unique_type<T>::unique_type_unbounded_array make_unique(size_t n) {
	typedef typename std::remove_extent<T>::type TBase;
	return unique_ptr<T>(new TBase[n]);
}

// It's not possible to create a unique_ptr for arrays of a known bound (e.g. int[4] as opposed to int[]).
template<typename T, typename... Args>
typename Internal::unique_type<T>::unique_type_bounded_array
make_unique(Args&&...) = delete;

/// swap
/// Exchanges the owned pointer beween two unique_ptr objects.
/// This non-member version is useful for compatibility of unique_ptr
/// objects with the C++ Standard Library and other libraries.
template<typename T, typename D>
inline void swap(unique_ptr<T, D>& a, unique_ptr<T, D>& b) noexcept {
	a.swap(b);
}

template<typename T1, typename D1, typename T2, typename D2>
inline bool operator==(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	return (a.get() == b.get());
}

template<typename T1, typename D1, typename T2, typename D2>
inline bool operator!=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	return !(a.get() == b.get());
}

/// Returns which unique_ptr is 'less' than the other. Useful when storing
/// sorted containers of unique_ptr objects.
template<typename T1, typename D1, typename T2, typename D2>
inline bool operator<(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	//typedef typename std::unique_ptr<T1, D1>::pointer P1;       // We currently need to make these temporary variables, as otherwise clang complains about CPointer being int*&&&.
	//typedef typename std::unique_ptr<T2, D2>::pointer P2;       // I think there's something wrong with our common_type type trait implementation.
	//typedef typename std::common_type<P1, P2>::type   PCommon;  // "in instantiation of function template specialization 'std::operator<<int, int>, no known conversion from 'element_type *' (aka 'int *') to 'int *&&&' for 1st argument"
	//return less<PCommon>()(a.get(), b.get());                     // It looks like common_type is making CPointer be (e.g.) int*&& instead of int*, though the problem may be in how less<> deals with that.

	typedef typename std::unique_ptr<T1, D1>::pointer P1;
	typedef typename std::unique_ptr<T2, D2>::pointer P2;
	typedef typename std::common_type<P1, P2>::type PCommon;
	PCommon pT1 = a.get();
	PCommon pT2 = b.get();
	return less<PCommon>()(pT1, pT2);
}

template<typename T1, typename D1, typename T2, typename D2>
inline bool operator>(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	return (b < a);
}

template<typename T1, typename D1, typename T2, typename D2>
inline bool operator<=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	return !(b < a);
}

template<typename T1, typename D1, typename T2, typename D2>
inline bool operator>=(const unique_ptr<T1, D1>& a, const unique_ptr<T2, D2>& b) {
	return !(a < b);
}

template<typename T, typename D>
inline bool operator==(const unique_ptr<T, D>& a, std::nullptr_t) noexcept {
	return !a;
}

template<typename T, typename D>
inline bool operator==(std::nullptr_t, const unique_ptr<T, D>& a) noexcept {
	return !a;
}

template<typename T, typename D>
inline bool operator!=(const unique_ptr<T, D>& a, std::nullptr_t) noexcept {
	return static_cast<bool>(a);
}

template<typename T, typename D>
inline bool operator!=(std::nullptr_t, const unique_ptr<T, D>& a) noexcept {
	return static_cast<bool>(a);
}

template<typename T, typename D>
inline bool operator<(const unique_ptr<T, D>& a, std::nullptr_t) {
	typedef typename unique_ptr<T, D>::pointer pointer;
	return less<pointer>()(a.get(), nullptr);
}

template<typename T, typename D>
inline bool operator<(std::nullptr_t, const unique_ptr<T, D>& b) {
	typedef typename unique_ptr<T, D>::pointer pointer;
	pointer pT = b.get();
	return less<pointer>()(nullptr, pT);
}

template<typename T, typename D>
inline bool operator>(const unique_ptr<T, D>& a, std::nullptr_t) {
	return (nullptr < a);
}

template<typename T, typename D>
inline bool operator>(std::nullptr_t, const unique_ptr<T, D>& b) {
	return (b < nullptr);
}

template<typename T, typename D>
inline bool operator<=(const unique_ptr<T, D>& a, std::nullptr_t) {
	return !(nullptr < a);
}

template<typename T, typename D>
inline bool operator<=(std::nullptr_t, const unique_ptr<T, D>& b) {
	return !(b < nullptr);
}

template<typename T, typename D>
inline bool operator>=(const unique_ptr<T, D>& a, std::nullptr_t) {
	return !(a < nullptr);
}

template<typename T, typename D>
inline bool operator>=(std::nullptr_t, const unique_ptr<T, D>& b) {
	return !(nullptr < b);
}

}// namespace eastl

namespace vstd {
template<typename T>
using unique_ptr = eastl::unique_ptr<T, unique_ptr_deleter>;
template<typename T>
unique_ptr<T> create_unique(T* ptr) {
	return unique_ptr<T>(ptr);
}
}// namespace vstd