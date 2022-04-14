#pragma once
#include <VEngineConfig.h>
#include <stdint.h>
#include <xhash>
#include <iostream>
#include <ctype.h> // toupper, etc.
#include <string.h>// memset, etc.
#include <assert.h>
#include <Common/Hash.h>
#include <Common/Memory.h>
#include <Common/Compare.h>
#include <Common/vector.h>
#include <Common/vstl_traits.h>
///////////// vstd::string
VENGINE_DLL_COMMON void VEngine_Log(char const* chunk);

namespace vstd {
///////////////////////////////////////////////////////////////////////////////
/// DecodePart
///
/// These implement UTF8/UCS2/UCS4 encoding/decoding.
///
VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char*& pDest, char* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char*& pSrc, const char* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char*& pDest, char* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char*& pDest, char* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char*& pDest, char* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
VENGINE_DLL_COMMON bool DecodePart(const int*& pSrc, const int* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

#if EA_CHAR8_UNIQUE
bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd);

bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char*& pDest, char* pDestEnd);
bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

bool DecodePart(const char*& pSrc, const char* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd);
bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd);
bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd);
#endif

#if EA_WCHAR_UNIQUE
bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd);

bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char*& pDest, char* pDestEnd);
bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd);
bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd);

bool DecodePart(const char*& pSrc, const char* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd);
bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd);
bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd);
#endif

#if EA_CHAR8_UNIQUE && EA_WCHAR_UNIQUE
bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd);
bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd);
#endif

#if EA_WCHAR_UNIQUE
inline bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd) {
	return DecodePart(reinterpret_cast<const char*&>(pSrc), reinterpret_cast<const char*>(pSrcEnd), reinterpret_cast<char*&>(pDest), reinterpret_cast<char*&>(pDestEnd));
}

inline bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char*& pDest, char* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(reinterpret_cast<const char16_t*&>(pSrc), reinterpret_cast<const char16_t*>(pSrcEnd), pDest, pDestEnd);
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(reinterpret_cast<const char32_t*&>(pSrc), reinterpret_cast<const char32_t*>(pSrcEnd), pDest, pDestEnd);
#endif
}

inline bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(reinterpret_cast<const char16_t*&>(pSrc), reinterpret_cast<const char16_t*>(pSrcEnd), pDest, pDestEnd);
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(reinterpret_cast<const char32_t*&>(pSrc), reinterpret_cast<const char32_t*>(pSrcEnd), pDest, pDestEnd);
#endif
}

inline bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(reinterpret_cast<const char16_t*&>(pSrc), reinterpret_cast<const char16_t*>(pSrcEnd), pDest, pDestEnd);
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(reinterpret_cast<const char32_t*&>(pSrc), reinterpret_cast<const char32_t*>(pSrcEnd), pDest, pDestEnd);
#endif
}

inline bool DecodePart(const char*& pSrc, const char* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char16_t*&>(pDest), reinterpret_cast<char16_t*>(pDestEnd));
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char32_t*&>(pDest), reinterpret_cast<char32_t*>(pDestEnd));
#endif
}

inline bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char16_t*&>(pDest), reinterpret_cast<char16_t*>(pDestEnd));
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char32_t*&>(pDest), reinterpret_cast<char32_t*>(pDestEnd));
#endif
}

inline bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char16_t*&>(pDest), reinterpret_cast<char16_t*>(pDestEnd));
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char32_t*&>(pDest), reinterpret_cast<char32_t*>(pDestEnd));
#endif
}
#endif

#if EA_CHAR8_UNIQUE
inline bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd) {
	return DecodePart(reinterpret_cast<const char*&>(pSrc), reinterpret_cast<const char*>(pSrcEnd), reinterpret_cast<char*&>(pDest), reinterpret_cast<char*&>(pDestEnd));
}

inline bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char*& pDest, char* pDestEnd) {
	return DecodePart(reinterpret_cast<const char*&>(pSrc), reinterpret_cast<const char*>(pSrcEnd), pDest, pDestEnd);
}

inline bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char16_t*& pDest, char16_t* pDestEnd) {
	return DecodePart(reinterpret_cast<const char*&>(pSrc), reinterpret_cast<const char*>(pSrcEnd), pDest, pDestEnd);
}

inline bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, char32_t*& pDest, char32_t* pDestEnd) {
	return DecodePart(reinterpret_cast<const char*&>(pSrc), reinterpret_cast<const char*>(pSrcEnd), pDest, pDestEnd);
}

inline bool DecodePart(const char*& pSrc, const char* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd) {
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char*&>(pDest), reinterpret_cast<char*&>(pDestEnd));
}

inline bool DecodePart(const char16_t*& pSrc, const char16_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd) {
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char*&>(pDest), reinterpret_cast<char*&>(pDestEnd));
}

inline bool DecodePart(const char32_t*& pSrc, const char32_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd) {
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char*&>(pDest), reinterpret_cast<char*&>(pDestEnd));
}
#endif

#if EA_CHAR8_UNIQUE && EA_WCHAR_UNIQUE
inline bool DecodePart(const char8_t*& pSrc, const char8_t* pSrcEnd, wchar_t*& pDest, wchar_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char16_t*&>(pDest), reinterpret_cast<char16_t*>(pDestEnd));
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(pSrc, pSrcEnd, reinterpret_cast<char32_t*&>(pDest), reinterpret_cast<char32_t*>(pDestEnd));
#endif
}

inline bool DecodePart(const wchar_t*& pSrc, const wchar_t* pSrcEnd, char8_t*& pDest, char8_t* pDestEnd) {
#if (EA_WCHAR_SIZE == 2)
	return DecodePart(reinterpret_cast<const char16_t*&>(pSrc), reinterpret_cast<const char16_t*>(pSrcEnd), reinterpret_cast<char*&>(pDest), reinterpret_cast<char*>(pDestEnd));
#elif (EA_WCHAR_SIZE == 4)
	return DecodePart(reinterpret_cast<const char32_t*&>(pSrc), reinterpret_cast<const char32_t*>(pSrcEnd), reinterpret_cast<char*&>(pDest), reinterpret_cast<char*>(pDestEnd));
#endif
}
#endif

///////////////////////////////////////////////////////////////////////////////
// 'char traits' functionality
//
inline char CharToLower(char c) { return (char)tolower((uint8_t)c); }

template<typename T>
inline T CharToLower(T c) {
	if ((unsigned)c <= 0xff) return (T)tolower((uint8_t)c);
	return c;
}

inline char CharToUpper(char c) { return (char)toupper((uint8_t)c); }

template<typename T>
inline T CharToUpper(T c) {
	if ((unsigned)c <= 0xff) return (T)toupper((uint8_t)c);
	return c;
}

template<typename T>
int Compare(const T* p1, const T* p2, size_t n) {
	for (; n > 0; ++p1, ++p2, --n) {
		if (*p1 != *p2)
			return (static_cast<typename std::make_unsigned<T>::type>(*p1) < static_cast<typename std::make_unsigned<T>::type>(*p2)) ? -1 : 1;
	}
	return 0;
}

inline int Compare(const char* p1, const char* p2, size_t n) {
	return memcmp(p1, p2, n);
}

template<typename T>
inline int CompareI(const T* p1, const T* p2, size_t n) {
	for (; n > 0; ++p1, ++p2, --n) {
		const T c1 = CharToLower(*p1);
		const T c2 = CharToLower(*p2);

		if (c1 != c2)
			return (static_cast<typename std::make_unsigned<T>::type>(c1) < static_cast<typename std::make_unsigned<T>::type>(c2)) ? -1 : 1;
	}
	return 0;
}

template<typename T>
inline const T* Find(const T* p, T c, size_t n) {
	for (; n > 0; --n, ++p) {
		if (*p == c)
			return p;
	}

	return NULL;
}

inline const char* Find(const char* p, char c, size_t n) {
	return (const char*)memchr(p, c, n);
}

template<typename T>
inline constexpr size_t CharStrlen(const T* p) {
	const auto* pCurrent = p;
	while (*pCurrent)
		++pCurrent;
	return (size_t)(pCurrent - p);
}

template<typename T>
inline T* CharStringUninitializedCopy(const T* pSource, const T* pSourceEnd, T* pDestination) {
	memmove(pDestination, pSource, (size_t)(pSourceEnd - pSource) * sizeof(T));
	return pDestination + (pSourceEnd - pSource);
}

template<typename T>
const T* CharTypeStringFindEnd(const T* pBegin, const T* pEnd, T c) {
	const T* pTemp = pEnd;
	while (--pTemp >= pBegin) {
		if (*pTemp == c)
			return pTemp;
	}

	return pEnd;
}

template<typename T>
const T* CharTypeStringRSearch(const T* p1Begin, const T* p1End,
							   const T* p2Begin, const T* p2End) {
	// Test for zero length strings, in which case we have a match or a failure,
	// but the return value is the same either way.
	if ((p1Begin == p1End) || (p2Begin == p2End))
		return p1Begin;

	// Test for a pattern of length 1.
	if ((p2Begin + 1) == p2End)
		return CharTypeStringFindEnd(p1Begin, p1End, *p2Begin);

	// Test for search string length being longer than string length.
	if ((p2End - p2Begin) > (p1End - p1Begin))
		return p1End;

	// General case.
	const T* pSearchEnd = (p1End - (p2End - p2Begin) + 1);
	const T* pCurrent1;
	const T* pCurrent2;

	while (pSearchEnd != p1Begin) {
		// Search for the last occurrence of *p2Begin.
		pCurrent1 = CharTypeStringFindEnd(p1Begin, pSearchEnd, *p2Begin);
		if (pCurrent1 == pSearchEnd)// If the first char of p2 wasn't found,
			return p1End;			// then we immediately have failure.

		// In this case, *pTemp == *p2Begin. So compare the rest.
		pCurrent2 = p2Begin;
		while (*pCurrent1++ == *pCurrent2++) {
			if (pCurrent2 == p2End)
				return (pCurrent1 - (p2End - p2Begin));
		}

		// A smarter algorithm might know to subtract more than just one,
		// but in most cases it won't make much difference anyway.
		--pSearchEnd;
	}

	return p1End;
}

template<typename T>
inline const T* CharTypeStringFindFirstOf(const T* p1Begin, const T* p1End, const T* p2Begin, const T* p2End) {
	for (; p1Begin != p1End; ++p1Begin) {
		for (const T* pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*p1Begin == *pTemp)
				return p1Begin;
		}
	}
	return p1End;
}

template<typename T>
inline const T* CharTypeStringRFindFirstNotOf(const T* p1RBegin, const T* p1REnd, const T* p2Begin, const T* p2End) {
	for (; p1RBegin != p1REnd; --p1RBegin) {
		const T* pTemp;
		for (pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*(p1RBegin - 1) == *pTemp)
				break;
		}
		if (pTemp == p2End)
			return p1RBegin;
	}
	return p1REnd;
}

template<typename T>
inline const T* CharTypeStringFindFirstNotOf(const T* p1Begin, const T* p1End, const T* p2Begin, const T* p2End) {
	for (; p1Begin != p1End; ++p1Begin) {
		const T* pTemp;
		for (pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*p1Begin == *pTemp)
				break;
		}
		if (pTemp == p2End)
			return p1Begin;
	}
	return p1End;
}

template<typename T>
inline const T* CharTypeStringRFindFirstOf(const T* p1RBegin, const T* p1REnd, const T* p2Begin, const T* p2End) {
	for (; p1RBegin != p1REnd; --p1RBegin) {
		for (const T* pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*(p1RBegin - 1) == *pTemp)
				return p1RBegin;
		}
	}
	return p1REnd;
}

template<typename T>
inline const T* CharTypeStringRFind(const T* pRBegin, const T* pREnd, const T c) {
	while (pRBegin > pREnd) {
		if (*(pRBegin - 1) == c)
			return pRBegin;
		--pRBegin;
	}
	return pREnd;
}

inline char* CharStringUninitializedFillN(char* pDestination, size_t n, const char c) {
	if (n)// Some compilers (e.g. GCC 4.3+) generate a warning (which can't be disabled) if you call memset with a size of 0.
		memset(pDestination, (uint8_t)c, (size_t)n);
	return pDestination + n;
}

template<typename T>
inline T* CharStringUninitializedFillN(T* pDestination, size_t n, const T c) {
	T* pDest = pDestination;
	const T* const pEnd = pDestination + n;
	while (pDest < pEnd)
		*pDest++ = c;
	return pDestination + n;
}

inline char* CharTypeAssignN(char* pDestination, size_t n, char c) {
	if (n)// Some compilers (e.g. GCC 4.3+) generate a warning (which can't be disabled) if you call memset with a size of 0.
		return (char*)memset(pDestination, c, (size_t)n);
	return pDestination;
}

template<typename T>
inline T* CharTypeAssignN(T* pDestination, size_t n, T c) {
	T* pDest = pDestination;
	const T* const pEnd = pDestination + n;
	while (pDest < pEnd)
		*pDest++ = c;
	return pDestination;
}
///////////////////////////////////////////////////////////////////////////////
// EASTL_STRING_OPT_XXXX
//
// Enables some options / optimizations options that cause the string class
// to behave slightly different from the C++ standard basic_string. These are
// options whereby you can improve performance by avoiding operations that
// in practice may never occur for you.
//
///////////////////////////////////////////////////////////////////////////////

#ifndef EASTL_STRING_OPT_EXPLICIT_CTORS
// Defined as 0 or 1. Default is 0.
// Defines if we should implement explicity in constructors where the C++
// standard string does not. The advantage of enabling explicit constructors
// is that you can do this: string s = "hello"; in addition to string s("hello");
// The disadvantage of enabling explicity constructors is that there can be
// silent conversions done which impede performance if the user isn't paying
// attention.
// C++ standard string ctors are not explicit.
#define EASTL_STRING_OPT_EXPLICIT_CTORS 0
#endif

#ifndef EASTL_STRING_OPT_LENGTH_ERRORS
// Defined as 0 or 1. Default is equal to EASTL_EXCEPTIONS_ENABLED.
// Defines if we check for string values going beyond kMaxSize
// (a very large value) and throw exections if so.
// C++ standard strings are expected to do such checks.
#define EASTL_STRING_OPT_LENGTH_ERRORS EASTL_EXCEPTIONS_ENABLED
#endif

#ifndef EASTL_STRING_OPT_RANGE_ERRORS
// Defined as 0 or 1. Default is equal to EASTL_EXCEPTIONS_ENABLED.
// Defines if we check for out-of-bounds references to string
// positions and throw exceptions if so. Well-behaved code shouldn't
// refence out-of-bounds positions and so shouldn't need these checks.
// C++ standard strings are expected to do such range checks.
#define EASTL_STRING_OPT_RANGE_ERRORS EASTL_EXCEPTIONS_ENABLED
#endif

#ifndef EASTL_STRING_OPT_ARGUMENT_ERRORS
// Defined as 0 or 1. Default is 0.
// Defines if we check for NULL ptr arguments passed to string
// functions by the user and throw exceptions if so. Well-behaved code
// shouldn't pass bad arguments and so shouldn't need these checks.
// Also, some users believe that strings should check for NULL pointers
// in all their arguments and do no-ops if so. This is very debatable.
// C++ standard strings are not required to check for such argument errors.
#define EASTL_STRING_OPT_ARGUMENT_ERRORS 0
#endif

#ifndef EASTL_BASIC_STRING_DEFAULT_ALLOCATOR
#define EASTL_BASIC_STRING_DEFAULT_ALLOCATOR allocator_type()
#endif
template<typename T>
class basic_string_view;
template<typename T, typename Allocator = allocator<T>>
class basic_string {
public:
	typedef basic_string<T, Allocator> this_type;
	typedef basic_string_view<T> view_type;
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;// Maintainer note: We want to leave iterator defined as T* -- at least in release builds -- as this gives some algorithms an advantage that optimizers cannot get around.
	typedef const T* const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef size_t size_type;// See config.h for the definition of eastl_size_t, which defaults to size_t.
	typedef ptrdiff_t difference_type;
	typedef Allocator allocator_type;
	static const size_type npos = (size_type)-1;/// 'npos' means non-valid position or simply non-position.
												// CtorDoNotInitialize exists so that we can create a constructor that allocates but doesn't
	// initialize and also doesn't collide with any other constructor declaration.
	struct CtorDoNotInitialize {
	};

// CtorSprintf exists so that we can create a constructor that accepts printf-style
// arguments but also doesn't collide with any other constructor declaration.
#ifdef EA_PLATFORM_MINGW
	// Workaround for MinGW compiler bug: variadic arguments are corrupted if empty object is passed before it
	struct CtorSprintf {
		int dummy;
	};
#else
	struct CtorSprintf {
	};
#endif

	// CtorConvert exists so that we can have a constructor that implements string encoding
	// conversion, such as between UCS2 char16_t and UTF8 char8_t.
	struct CtorConvert {
	};

protected:
// Masks used to determine if we are in SSO or Heap
#ifdef EA_SYSTEM_BIG_ENDIAN
	// Big Endian use LSB, unless we want to reorder struct layouts on endianness, Bit is set when we are in Heap
	static EA_CONSTEXPR_OR_CONST size_type kHeapMask = 0x1;
	static EA_CONSTEXPR_OR_CONST size_type kSSOMask = 0x1;
#else
	// Little Endian use MSB
	static EA_CONSTEXPR_OR_CONST size_type kHeapMask = ~(size_type(~size_type(0)) >> 1);
	static EA_CONSTEXPR_OR_CONST size_type kSSOMask = 0x80;
#endif

public:
#ifdef EA_SYSTEM_BIG_ENDIAN
	static EA_CONSTEXPR_OR_CONST size_type kMaxSize = (~kHeapMask) >> 1;
#else
	static EA_CONSTEXPR_OR_CONST size_type kMaxSize = ~kHeapMask;
#endif

protected:
	// The view of memory when the string data is obtained from the allocator.
	struct HeapLayout {
		value_type* mpBegin; // Begin of string.
		size_type mnSize;	 // Size of the string. Number of characters currently in the string, not including the
							 // trailing '0'
		size_type mnCapacity;// Capacity of the string. Number of characters string can hold, not including the
							 // trailing '0'
	};

	template<typename CharT, size_t = sizeof(CharT)>
	struct SSOPadding {
		char padding[sizeof(CharT) - sizeof(char)];
	};

	template<typename CharT>
	struct SSOPadding<CharT, 1> {
		// template specialization to remove the padding structure to avoid warnings on zero length arrays
		// also, this allows us to take advantage of the empty-base-class optimization.
	};

	// The view of memory when the string data is able to store the string data locally (without a heap allocation).
	struct SSOLayout {
		static EA_CONSTEXPR_OR_CONST size_type SSO_CAPACITY =
			(sizeof(HeapLayout) - sizeof(char)) / sizeof(value_type);

		// mnSize must correspond to the last byte of HeapLayout.mnCapacity, so we don't want the compiler to insert
		// padding after mnSize if sizeof(value_type) != 1; Also ensures both layouts are the same size.
		struct SSOSize : SSOPadding<value_type> {
			char mnRemainingSize;
		};

		value_type mData[SSO_CAPACITY];// Local buffer for string data.
		SSOSize mRemainingSizeField;
	};

	// This view of memory is a utility structure for easy copying of the string data.
	struct RawLayout {
		char mBuffer[sizeof(HeapLayout)];
	};

	static_assert(sizeof(SSOLayout) == sizeof(HeapLayout), "heap and sso layout structures must be the same size");
	static_assert(sizeof(HeapLayout) == sizeof(RawLayout), "heap and raw layout structures must be the same size");

	// This implements the 'short string optimization' or SSO. SSO reuses the existing storage of string class to
	// hold string data short enough to fit therefore avoiding a heap allocation. The number of characters stored in
	// the string SSO buffer is variable and depends on the string character width. This implementation favors a
	// consistent string size than increasing the size of the string local data to accommodate a consistent number
	// of characters despite character width.
	struct Layout {
		union {
			HeapLayout heap;
			SSOLayout sso;
			RawLayout raw;
		};

		Layout() { ResetToSSO(); }// start as SSO by default
		Layout(const Layout& other) { Copy(*this, other); }
		Layout(Layout&& other) { Move(*this, other); }
		Layout& operator=(const Layout& other) {
			Copy(*this, other);
			return *this;
		}
		Layout& operator=(Layout&& other) {
			Move(*this, other);
			return *this;
		}

		// We are using Heap when the bit is set, easier to conceptualize checking IsHeap instead of IsSSO
		inline bool IsHeap() const noexcept { return !!(sso.mRemainingSizeField.mnRemainingSize & kSSOMask); }
		inline bool IsSSO() const noexcept { return !IsHeap(); }
		inline value_type* SSOBufferPtr() noexcept { return sso.mData; }
		inline const value_type* SSOBufferPtr() const noexcept { return sso.mData; }

		// Largest value for SSO.mnSize == 23, which has two LSB bits set, but on big-endian (BE)
		// use least significant bit (LSB) to denote heap so shift.
		inline size_type GetSSOSize() const noexcept {
#ifdef EA_SYSTEM_BIG_ENDIAN
			return SSOLayout::SSO_CAPACITY - (sso.mRemainingSizeField.mnRemainingSize >> 2);
#else
			return (SSOLayout::SSO_CAPACITY - sso.mRemainingSizeField.mnRemainingSize);
#endif
		}
		inline size_type GetHeapSize() const noexcept { return heap.mnSize; }
		inline size_type GetSize() const noexcept { return IsHeap() ? GetHeapSize() : GetSSOSize(); }

		inline void SetSSOSize(size_type size) noexcept {
#ifdef EA_SYSTEM_BIG_ENDIAN
			sso.mRemainingSizeField.mnRemainingSize = (char)((SSOLayout::SSO_CAPACITY - size) << 2);
#else
			sso.mRemainingSizeField.mnRemainingSize = (char)(SSOLayout::SSO_CAPACITY - size);
#endif
		}

		inline void SetHeapSize(size_type size) noexcept { heap.mnSize = size; }
		inline void SetSize(size_type size) noexcept { IsHeap() ? SetHeapSize(size) : SetSSOSize(size); }

		inline size_type GetRemainingCapacity() const noexcept { return size_type(CapacityPtr() - EndPtr()); }

		inline value_type* HeapBeginPtr() noexcept { return heap.mpBegin; };
		inline const value_type* HeapBeginPtr() const noexcept { return heap.mpBegin; };

		inline value_type* SSOBeginPtr() noexcept { return sso.mData; }
		inline const value_type* SSOBeginPtr() const noexcept { return sso.mData; }

		inline value_type* BeginPtr() noexcept { return IsHeap() ? HeapBeginPtr() : SSOBeginPtr(); }
		inline const value_type* BeginPtr() const noexcept { return IsHeap() ? HeapBeginPtr() : SSOBeginPtr(); }

		inline value_type* HeapEndPtr() noexcept { return heap.mpBegin + heap.mnSize; }
		inline const value_type* HeapEndPtr() const noexcept { return heap.mpBegin + heap.mnSize; }

		inline value_type* SSOEndPtr() noexcept { return sso.mData + GetSSOSize(); }
		inline const value_type* SSOEndPtr() const noexcept { return sso.mData + GetSSOSize(); }

		// Points to end of character stream, *ptr == '0'
		inline value_type* EndPtr() noexcept { return IsHeap() ? HeapEndPtr() : SSOEndPtr(); }
		inline const value_type* EndPtr() const noexcept { return IsHeap() ? HeapEndPtr() : SSOEndPtr(); }

		inline value_type* HeapCapacityPtr() noexcept { return heap.mpBegin + GetHeapCapacity(); }
		inline const value_type* HeapCapacityPtr() const noexcept { return heap.mpBegin + GetHeapCapacity(); }

		inline value_type* SSOCapacityPtr() noexcept { return sso.mData + SSOLayout::SSO_CAPACITY; }
		inline const value_type* SSOCapacityPtr() const noexcept { return sso.mData + SSOLayout::SSO_CAPACITY; }

		// Points to end of the buffer at the terminating '0', *ptr == '0' <- only true when size() == capacity()
		inline value_type* CapacityPtr() noexcept { return IsHeap() ? HeapCapacityPtr() : SSOCapacityPtr(); }
		inline const value_type* CapacityPtr() const noexcept {
			return IsHeap() ? HeapCapacityPtr() : SSOCapacityPtr();
		}

		inline void SetHeapBeginPtr(value_type* pBegin) noexcept { heap.mpBegin = pBegin; }

		inline void SetHeapCapacity(size_type cap) noexcept {
#ifdef EA_SYSTEM_BIG_ENDIAN
			heap.mnCapacity = (cap << 1) | kHeapMask;
#else
			heap.mnCapacity = (cap | kHeapMask);
#endif
		}

		inline size_type GetHeapCapacity() const noexcept {
#ifdef EA_SYSTEM_BIG_ENDIAN
			return (heap.mnCapacity >> 1);
#else
			return (heap.mnCapacity & ~kHeapMask);
#endif
		}

		inline void Copy(Layout& dst, const Layout& src) noexcept { dst.raw = src.raw; }
		inline void Move(Layout& dst, Layout& src) noexcept { std::swap(dst.raw, src.raw); }
		inline void Swap(Layout& a, Layout& b) noexcept { std::swap(a.raw, b.raw); }

		inline void ResetToSSO() noexcept {
			*SSOBeginPtr() = 0;
			SetSSOSize(0);
		}
	};
	vstd::compressed_pair<Layout, allocator_type> mPair;
	inline Layout& internalLayout() noexcept { return mPair.first(); }
	inline const Layout& internalLayout() const noexcept { return mPair.first(); }
	inline allocator_type& internalAllocator() noexcept { return mPair.second(); }
	inline const allocator_type& internalAllocator() const noexcept { return mPair.second(); }

public:
	basic_string() noexcept;
	explicit basic_string(const allocator_type& allocator) noexcept;
	basic_string(const this_type& x, size_type position, size_type n = npos);
	basic_string(const value_type* p,
				 size_type n,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(const value_type* p,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(size_type n, value_type c, const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(const this_type& x);
	basic_string(const this_type& x, const allocator_type& allocator);
	basic_string(const value_type* pBegin,
				 const value_type* pEnd,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(CtorDoNotInitialize,
				 size_type n,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(CtorSprintf, const value_type* pFormat, ...);
	basic_string(std::initializer_list<value_type> init,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);

	basic_string(this_type&& x) noexcept;
	basic_string(this_type&& x, const allocator_type& allocator);

	explicit basic_string(const view_type& sv,
						  const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);
	basic_string(const view_type& sv,
				 size_type position,
				 size_type n,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);

	template<typename OtherCharType>
	basic_string(CtorConvert,
				 const OtherCharType* p,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);

	template<typename OtherCharType>
	basic_string(CtorConvert,
				 const OtherCharType* p,
				 size_type n,
				 const allocator_type& allocator = EASTL_BASIC_STRING_DEFAULT_ALLOCATOR);

	template<typename OtherStringType>// Unfortunately we need the CtorConvert here because otherwise this
	// function would collide with the value_type* constructor.
	basic_string(CtorConvert, const OtherStringType& x);

	~basic_string();

	// Allocator
	const allocator_type& get_allocator() const noexcept;
	allocator_type& get_allocator() noexcept;
	void set_allocator(const allocator_type& allocator);

	// Implicit conversion operator
	operator view_type() const noexcept {
		return {data(), size()};
	}

	// Operator=
	this_type& operator=(const this_type& x);
	this_type& operator=(const value_type* p);
	this_type& operator=(value_type c);
	this_type& operator=(std::initializer_list<value_type> ilist);
	this_type& operator=(view_type v);
	this_type& operator=(this_type&& x);// TODO(c++17):
										// noexcept(allocator_traits<Allocator>::propagate_on_container_move_assignment::value
										// || allocator_traits<Allocator>::is_always_equal::value);

#if EASTL_OPERATOR_EQUALS_OTHER_ENABLED
	this_type& operator=(value_type* p) {
		return operator=((const value_type*)p);
	}// We need this because otherwise the const value_type* version can collide with the const OtherStringType&
	 // version below.

	template<typename OtherCharType>
	this_type& operator=(const OtherCharType* p);

	template<typename OtherStringType>
	this_type& operator=(const OtherStringType& x);
#endif

	void swap(
		this_type& x);// TODO(c++17): noexcept(allocator_traits<Allocator>::propagate_on_container_swap::value ||
					  // allocator_traits<Allocator>::is_always_equal::value);

	// Assignment operations
	this_type& assign(const this_type& x);
	this_type& assign(const this_type& x, size_type position, size_type n = npos);
	this_type& assign(const value_type* p, size_type n);
	this_type& assign(const value_type* p);
	this_type& assign(size_type n, value_type c);
	this_type& assign(const value_type* pBegin, const value_type* pEnd);
	this_type& assign(this_type&& x);// TODO(c++17):
									 // noexcept(allocator_traits<Allocator>::propagate_on_container_move_assignment::value
									 // || allocator_traits<Allocator>::is_always_equal::value);
	this_type& assign(std::initializer_list<value_type>);

	template<typename OtherCharType>
	this_type& assign_convert(const OtherCharType* p);

	template<typename OtherCharType>
	this_type& assign_convert(const OtherCharType* p, size_type n);

	template<typename OtherStringType>
	this_type& assign_convert(const OtherStringType& x);

	// Iterators.
	iterator begin() noexcept;			  // Expanded in source code as: mpBegin
	const_iterator begin() const noexcept;// Expanded in source code as: mpBegin
	const_iterator cbegin() const noexcept;

	iterator end() noexcept;			// Expanded in source code as: mpEnd
	const_iterator end() const noexcept;// Expanded in source code as: mpEnd
	const_iterator cend() const noexcept;

	reverse_iterator rbegin() noexcept;
	const_reverse_iterator rbegin() const noexcept;
	const_reverse_iterator crbegin() const noexcept;

	reverse_iterator rend() noexcept;
	const_reverse_iterator rend() const noexcept;
	const_reverse_iterator crend() const noexcept;

	// Size-related functionality
	bool empty() const noexcept;
	size_type size() const noexcept;
	size_type length() const noexcept;
	size_type max_size() const noexcept;
	size_type capacity() const noexcept;
	void resize(size_type n, value_type c);
	void resize(size_type n);
	void reserve(size_type = 0);
	void set_capacity(
		size_type n = npos);// Revises the capacity to the user-specified value. Resizes the container to match the
							// capacity if the requested capacity n is less than the current size. If n == npos
							// then the capacity is reallocated (if necessary) such that capacity == size.
	void force_size(
		size_type n);// Unilaterally moves the string end position (mpEnd) to the given location. Useful for when
					 // the user writes into the string via some extenal means such as C strcpy or sprintf. This
					 // allows for more efficient use than using resize to achieve this.
	void shrink_to_fit();

	// Raw access
	const value_type* data() const noexcept;
	value_type* data() noexcept;
	const value_type* c_str() const noexcept;

	// Element access
	reference operator[](size_type n);
	const_reference operator[](size_type n) const;
	reference at(size_type n);
	const_reference at(size_type n) const;
	reference front();
	const_reference front() const;
	reference back();
	const_reference back() const;

	// Append operations
	this_type& operator+=(const this_type& x);
	this_type& operator+=(const value_type* p);
	this_type& operator+=(value_type c);

	this_type& append(const this_type& x);
	this_type& append(const this_type& x, size_type position, size_type n = npos);
	this_type& append(const value_type* p, size_type n);
	this_type& append(const value_type* p);
	this_type& append(value_type p);
	this_type& append(view_type const& v) {
		return append(v.data(), v.size());
	}
	template<typename A>
	this_type& operator+=(A&& a) {
		return append(std::forward<A>(a));
	}
	this_type& append(size_type n, value_type c);
	this_type& append(const value_type* pBegin, const value_type* pEnd);

	template<typename OtherCharType>
	this_type& append_convert(const OtherCharType* p);

	template<typename OtherCharType>
	this_type& append_convert(const OtherCharType* p, size_type n);

	template<typename OtherStringType>
	this_type& append_convert(const OtherStringType& x);

	void push_back(value_type c);
	void pop_back();

	// Insertion operations
	this_type& insert(size_type position, const this_type& x);
	this_type& insert(size_type position, const this_type& x, size_type beg, size_type n);
	this_type& insert(size_type position, const value_type* p, size_type n);
	this_type& insert(size_type position, const value_type* p);
	this_type& insert(size_type position, size_type n, value_type c);
	iterator insert(const_iterator p, value_type c);
	iterator insert(const_iterator p, size_type n, value_type c);
	iterator insert(const_iterator p, const value_type* pBegin, const value_type* pEnd);
	iterator insert(const_iterator p, std::initializer_list<value_type>);

	// Erase operations
	this_type& erase(size_type position = 0, size_type n = npos);
	iterator erase(const_iterator p);
	iterator erase(const_iterator pBegin, const_iterator pEnd);
	reverse_iterator erase(reverse_iterator position);
	reverse_iterator erase(reverse_iterator first, reverse_iterator last);
	void clear() noexcept;

	// Detach memory
	pointer detach() noexcept;

	// Replacement operations
	this_type& replace(size_type position, size_type n, const this_type& x);
	this_type& replace(size_type pos1, size_type n1, const this_type& x, size_type pos2, size_type n2 = npos);
	this_type& replace(size_type position, size_type n1, const value_type* p, size_type n2);
	this_type& replace(size_type position, size_type n1, const value_type* p);
	this_type& replace(size_type position, size_type n1, size_type n2, value_type c);
	this_type& replace(const_iterator first, const_iterator last, const this_type& x);
	this_type& replace(const_iterator first, const_iterator last, const value_type* p, size_type n);
	this_type& replace(const_iterator first, const_iterator last, const value_type* p);
	this_type& replace(const_iterator first, const_iterator last, size_type n, value_type c);
	this_type& replace(const_iterator first, const_iterator last, const value_type* pBegin, const value_type* pEnd);
	size_type copy(value_type* p, size_type n, size_type position = 0) const;

	// Find operations
	size_type find(const this_type& x, size_type position = 0) const noexcept;
	size_type find(const value_type* p, size_type position = 0) const;
	size_type find(const value_type* p, size_type position, size_type n) const;
	size_type find(value_type c, size_type position = 0) const noexcept;

	// Reverse find operations
	size_type rfind(const this_type& x, size_type position = npos) const noexcept;
	size_type rfind(const value_type* p, size_type position = npos) const;
	size_type rfind(const value_type* p, size_type position, size_type n) const;
	size_type rfind(value_type c, size_type position = npos) const noexcept;

	// Find first-of operations
	size_type find_first_of(const this_type& x, size_type position = 0) const noexcept;
	size_type find_first_of(const value_type* p, size_type position = 0) const;
	size_type find_first_of(const value_type* p, size_type position, size_type n) const;
	size_type find_first_of(value_type c, size_type position = 0) const noexcept;

	// Find last-of operations
	size_type find_last_of(const this_type& x, size_type position = npos) const noexcept;
	size_type find_last_of(const value_type* p, size_type position = npos) const;
	size_type find_last_of(const value_type* p, size_type position, size_type n) const;
	size_type find_last_of(value_type c, size_type position = npos) const noexcept;

	// Find first not-of operations
	size_type find_first_not_of(const this_type& x, size_type position = 0) const noexcept;
	size_type find_first_not_of(const value_type* p, size_type position = 0) const;
	size_type find_first_not_of(const value_type* p, size_type position, size_type n) const;
	size_type find_first_not_of(value_type c, size_type position = 0) const noexcept;

	// Find last not-of operations
	size_type find_last_not_of(const this_type& x, size_type position = npos) const noexcept;
	size_type find_last_not_of(const value_type* p, size_type position = npos) const;
	size_type find_last_not_of(const value_type* p, size_type position, size_type n) const;
	size_type find_last_not_of(value_type c, size_type position = npos) const noexcept;

	// Substring functionality
	this_type substr(size_type position = 0, size_type n = npos) const;

	// Comparison operations
	int compare(const this_type& x) const noexcept;
	int compare(size_type pos1, size_type n1, const this_type& x) const;
	int compare(size_type pos1, size_type n1, const this_type& x, size_type pos2, size_type n2) const;
	int compare(const value_type* p) const;
	int compare(size_type pos1, size_type n1, const value_type* p) const;
	int compare(size_type pos1, size_type n1, const value_type* p, size_type n2) const;
	static int compare(const value_type* pBegin1,
					   const value_type* pEnd1,
					   const value_type* pBegin2,
					   const value_type* pEnd2);

	// Case-insensitive comparison functions. Not part of C++ this_type. Only ASCII-level locale functionality is
	// supported. Thus this is not suitable for localization purposes.
	int comparei(const this_type& x) const noexcept;
	int comparei(const value_type* p) const;
	static int comparei(const value_type* pBegin1,
						const value_type* pEnd1,
						const value_type* pBegin2,
						const value_type* pEnd2);

	// Misc functionality, not part of C++ this_type.
	void make_lower();
	void make_upper();
	void ltrim();
	void rtrim();
	void trim();
	void ltrim(const value_type* p);
	void rtrim(const value_type* p);
	void trim(const value_type* p);
	this_type left(size_type n) const;
	this_type right(size_type n) const;
	bool validate() const noexcept;
	int validate_iterator(const_iterator i) const noexcept;

protected:
	// Helper functions for initialization/insertion operations.
	value_type* DoAllocate(size_type n);
	void DoFree(value_type* p, size_type n);
	size_type GetNewCapacity(size_type currentCapacity);
	size_type GetNewCapacity(size_type currentCapacity, size_type minimumGrowSize);
	void AllocateSelf();
	void AllocateSelf(size_type n);
	void DeallocateSelf();
	iterator InsertInternal(const_iterator p, value_type c);
	void RangeInitialize(const value_type* pBegin, const value_type* pEnd);
	void RangeInitialize(const value_type* pBegin);
	void SizeInitialize(size_type n, value_type c);

	bool IsSSO() const noexcept;

	void ThrowLengthException() const;
	void ThrowRangeException() const;
	void ThrowInvalidArgumentException() const;

#if EASTL_OPERATOR_EQUALS_OTHER_ENABLED
	template<typename CharType>
	void DoAssignConvert(CharType c, true_type);

	template<typename StringType>
	void DoAssignConvert(const StringType& x, false_type);
#endif

	// Replacements for STL template functions.
	static const value_type* CharTypeStringFindEnd(const value_type* pBegin, const value_type* pEnd, value_type c);
	static const value_type* CharTypeStringRFind(const value_type* pRBegin,
												 const value_type* pREnd,
												 const value_type c);
	static const value_type* CharTypeStringSearch(const value_type* p1Begin,
												  const value_type* p1End,
												  const value_type* p2Begin,
												  const value_type* p2End);
	static const value_type* CharTypeStringRSearch(const value_type* p1Begin,
												   const value_type* p1End,
												   const value_type* p2Begin,
												   const value_type* p2End);
	static const value_type* CharTypeStringFindFirstOf(const value_type* p1Begin,
													   const value_type* p1End,
													   const value_type* p2Begin,
													   const value_type* p2End);
	static const value_type* CharTypeStringRFindFirstOf(const value_type* p1RBegin,
														const value_type* p1REnd,
														const value_type* p2Begin,
														const value_type* p2End);
	static const value_type* CharTypeStringFindFirstNotOf(const value_type* p1Begin,
														  const value_type* p1End,
														  const value_type* p2Begin,
														  const value_type* p2End);
	static const value_type* CharTypeStringRFindFirstNotOf(const value_type* p1RBegin,
														   const value_type* p1REnd,
														   const value_type* p2Begin,
														   const value_type* p2End);
};
template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string() noexcept
	: mPair(allocator_type()) {
	AllocateSelf();
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const allocator_type& allocator) noexcept : mPair(allocator) {
	AllocateSelf();
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const this_type& x) : mPair(x.get_allocator()) {
	RangeInitialize(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(const this_type& x, const allocator_type& allocator) : mPair(allocator) {
	RangeInitialize(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
template<typename OtherStringType>
inline basic_string<T, Allocator>::basic_string(CtorConvert, const OtherStringType& x) : mPair(x.get_allocator()) {
	AllocateSelf();
	append_convert(x.c_str(), x.length());
}

template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(const this_type& x, size_type position, size_type n)
	: mPair(x.get_allocator()) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > x.internalLayout().GetSize()))// 21.4.2 p4
	{
		ThrowRangeException();
		AllocateSelf();
	} else
		RangeInitialize(
			x.internalLayout().BeginPtr() + position,
			x.internalLayout().BeginPtr() + position + std::min_alt(n, x.internalLayout().GetSize() - position));
#else
	RangeInitialize(
		x.internalLayout().BeginPtr() + position,
		x.internalLayout().BeginPtr() + position + std::min_alt(n, x.internalLayout().GetSize() - position));
#endif
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const value_type* p, size_type n, const allocator_type& allocator)
	: mPair(allocator) {
	RangeInitialize(p, p + n);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const view_type& sv, const allocator_type& allocator)
	: basic_string(sv.data(), sv.size(), allocator) {
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const view_type& sv,
												size_type position,
												size_type n,
												const allocator_type& allocator)
	: basic_string(sv.substr(position, n), allocator) {
}

template<typename T, typename Allocator>
template<typename OtherCharType>
inline basic_string<T, Allocator>::basic_string(CtorConvert,
												const OtherCharType* p,
												const allocator_type& allocator)
	: mPair(allocator) {
	AllocateSelf();	  // In this case we are converting from one string encoding to another, and we
	append_convert(p);// implement this in the simplest way, by simply default-constructing and calling assign.
}

template<typename T, typename Allocator>
template<typename OtherCharType>
inline basic_string<T, Allocator>::basic_string(CtorConvert,
												const OtherCharType* p,
												size_type n,
												const allocator_type& allocator)
	: mPair(allocator) {
	AllocateSelf();		 // In this case we are converting from one string encoding to another, and we
	append_convert(p, n);// implement this in the simplest way, by simply default-constructing and calling assign.
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const value_type* p, const allocator_type& allocator)
	: mPair(allocator) {
	RangeInitialize(p);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(size_type n, value_type c, const allocator_type& allocator)
	: mPair(allocator) {
	SizeInitialize(n, c);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::basic_string(const value_type* pBegin,
												const value_type* pEnd,
												const allocator_type& allocator)
	: mPair(allocator) {
	RangeInitialize(pBegin, pEnd);
}

// CtorDoNotInitialize exists so that we can create a version that allocates but doesn't
// initialize but also doesn't collide with any other constructor declaration.
template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(CtorDoNotInitialize /*unused*/,
										 size_type n,
										 const allocator_type& allocator)
	: mPair(allocator) {
	// Note that we do not call SizeInitialize here.
	AllocateSelf(n);
	internalLayout().SetSize(0);
	*internalLayout().EndPtr() = 0;
}

// CtorSprintf exists so that we can create a version that does a variable argument
// sprintf but also doesn't collide with any other constructor declaration.
template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(CtorSprintf /*unused*/, const value_type* pFormat, ...) : mPair() {
	const size_type n = (size_type)CharStrlen(pFormat);
	AllocateSelf(n);
	internalLayout().SetSize(0);

	va_list arguments;
	va_start(arguments, pFormat);
	append_sprintf_va_list(pFormat, arguments);
	va_end(arguments);
}

template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(std::initializer_list<value_type> init, const allocator_type& allocator)
	: mPair(allocator) {
	RangeInitialize(init.begin(), init.end());
}

template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(this_type&& x) noexcept : mPair(x.get_allocator()) {
	internalLayout() = std::move(x.internalLayout());
	x.AllocateSelf();
}

template<typename T, typename Allocator>
basic_string<T, Allocator>::basic_string(this_type&& x, const allocator_type& allocator) : mPair(allocator) {
	if (get_allocator() == x.get_allocator())// If we can borrow from x...
	{
		internalLayout() = std::move(x.internalLayout());
		x.AllocateSelf();
	} else if (x.internalLayout().BeginPtr()) {
		RangeInitialize(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
		// Let x destruct its own items.
	}
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>::~basic_string() {
	DeallocateSelf();
}

template<typename T, typename Allocator>
inline const typename basic_string<T, Allocator>::allocator_type& basic_string<T, Allocator>::get_allocator() const
	noexcept {
	return internalAllocator();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::allocator_type& basic_string<T, Allocator>::get_allocator() noexcept {
	return internalAllocator();
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::set_allocator(const allocator_type& allocator) {
	get_allocator() = allocator;
}

template<typename T, typename Allocator>
inline const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::data() const noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::c_str() const noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::data() noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::begin() noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::end() noexcept {
	return internalLayout().EndPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_iterator basic_string<T, Allocator>::begin() const noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_iterator basic_string<T, Allocator>::cbegin() const noexcept {
	return internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_iterator basic_string<T, Allocator>::end() const noexcept {
	return internalLayout().EndPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_iterator basic_string<T, Allocator>::cend() const noexcept {
	return internalLayout().EndPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reverse_iterator basic_string<T, Allocator>::rbegin() noexcept {
	return reverse_iterator(internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reverse_iterator basic_string<T, Allocator>::rend() noexcept {
	return reverse_iterator(internalLayout().BeginPtr());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reverse_iterator basic_string<T, Allocator>::rbegin() const
	noexcept {
	return const_reverse_iterator(internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reverse_iterator basic_string<T, Allocator>::crbegin() const
	noexcept {
	return const_reverse_iterator(internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reverse_iterator basic_string<T, Allocator>::rend() const
	noexcept {
	return const_reverse_iterator(internalLayout().BeginPtr());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reverse_iterator basic_string<T, Allocator>::crend() const
	noexcept {
	return const_reverse_iterator(internalLayout().BeginPtr());
}

template<typename T, typename Allocator>
inline bool basic_string<T, Allocator>::empty() const noexcept {
	return (internalLayout().GetSize() == 0);
}

template<typename T, typename Allocator>
inline bool basic_string<T, Allocator>::IsSSO() const noexcept {
	return internalLayout().IsSSO();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::size() const noexcept {
	return internalLayout().GetSize();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::length() const noexcept {
	return internalLayout().GetSize();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::max_size() const noexcept {
	return kMaxSize;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::capacity() const noexcept {
	if (internalLayout().IsHeap()) {
		return internalLayout().GetHeapCapacity();
	}
	return SSOLayout::SSO_CAPACITY;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reference basic_string<T, Allocator>::operator[](
	size_type n) const {
#if assert_ENABLED// We allow the user to reference the trailing 0 char without asserting. Perhaps we shouldn't.
	if (EASTL_UNLIKELY(n > internalLayout().GetSize())) {
		EASTL_FAIL_MSG("basic_string::operator[] -- out of range");
	}
#endif

	return internalLayout().BeginPtr()[n];// Sometimes done as *(mpBegin + n)
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reference basic_string<T, Allocator>::operator[](size_type n) {
#if assert_ENABLED// We allow the user to reference the trailing 0 char without asserting. Perhaps we shouldn't.
	if (EASTL_UNLIKELY(n > internalLayout().GetSize())) {
		EASTL_FAIL_MSG("basic_string::operator[] -- out of range");
	}
#endif

	return internalLayout().BeginPtr()[n];// Sometimes done as *(mpBegin + n)
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(const this_type& x) {
	if (&x != this) {
#if EASTL_ALLOCATOR_COPY_ENABLED
		bool bSlowerPathwayRequired = (get_allocator() != x.get_allocator());
#else
		bool bSlowerPathwayRequired = false;
#endif

		if (bSlowerPathwayRequired) {
			set_capacity(
				0);// Must use set_capacity instead of clear because set_capacity frees our memory, unlike clear.

#if EASTL_ALLOCATOR_COPY_ENABLED
			get_allocator() = x.get_allocator();
#endif
		}

		assign(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
	}
	return *this;
}

#if EASTL_OPERATOR_EQUALS_OTHER_ENABLED
template<typename T, typename Allocator>
template<typename CharType>
inline void basic_string<T, Allocator>::DoAssignConvert(CharType c, true_type) {
	assign_convert(
		&c, 1);// Call this version of append because it will result in the encoding-converting append being used.
}

template<typename T, typename Allocator>
template<typename StringType>
inline void basic_string<T, Allocator>::DoAssignConvert(const StringType& x, false_type) {
	// if(&x != this) // Unnecessary because &x cannot possibly equal this.
	{
#if EASTL_ALLOCATOR_COPY_ENABLED
		get_allocator() = x.get_allocator();
#endif

		assign_convert(x.c_str(), x.length());
	}
}

template<typename T, typename Allocator>
template<typename OtherStringType>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(
	const OtherStringType& x) {
	clear();
	DoAssignConvert(x, is_integral<OtherStringType>());
	return *this;
}

template<typename T, typename Allocator>
template<typename OtherCharType>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(const OtherCharType* p) {
	return assign_convert(p);
}
#endif

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(const value_type* p) {
	return assign(p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(value_type c) {
	return assign((size_type)1, c);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(this_type&& x) {
	return assign(std::move(x));
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(
	std::initializer_list<value_type> ilist) {
	return assign(ilist.begin(), ilist.end());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::this_type& basic_string<T, Allocator>::operator=(view_type v) {
	return assign(v.data(), static_cast<this_type::size_type>(v.size()));
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::resize(size_type n, value_type c) {
	const size_type s = internalLayout().GetSize();

	if (n < s)
		erase(internalLayout().BeginPtr() + n, internalLayout().EndPtr());
	else if (n > s)
		append(n - s, c);
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::resize(size_type n) {
	// C++ basic_string specifies that resize(n) is equivalent to resize(n, value_type()).
	// For built-in types, value_type() is the same as zero (value_type(0)).
	// We can improve the efficiency (especially for long strings) of this
	// string class by resizing without assigning to anything.

	const size_type s = internalLayout().GetSize();

	if (n < s)
		erase(internalLayout().BeginPtr() + n, internalLayout().EndPtr());
	else if (n > s) {
		append(n - s, value_type());
	}
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::reserve(size_type n) {
#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(n > max_size())) {
		ThrowLengthException();
	}
#endif

	// C++20 says if the passed in capacity is less than the current capacity we do not shrink
	// If new_cap is less than or equal to the current capacity(), there is no effect.
	// http://en.cppreference.com/w/cpp/string/basic_string/reserve

	n = std::max(
		n, internalLayout().GetSize());// Calculate the new capacity, which needs to be >= container size.

	if (n > capacity())
		set_capacity(n);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::shrink_to_fit() {
	set_capacity(internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::set_capacity(size_type n) {
	if (n == npos)
		// If the user wants to set the capacity to equal the current size...
		// '-1' because we pretend that we didn't allocate memory for the terminating 0.
		n = internalLayout().GetSize();
	else if (n < internalLayout().GetSize()) {
		internalLayout().SetSize(n);
		*internalLayout().EndPtr() = 0;
	}

	if ((n < capacity() && internalLayout().IsHeap()) || (n > capacity())) {
		// In here the string is transition from heap->heap, heap->sso or sso->heap

		if (EASTL_LIKELY(n)) {

			if (n <= SSOLayout::SSO_CAPACITY) {
				// heap->sso
				// A heap based layout wants to reduce its size to within sso capacity
				// An sso layout wanting to reduce its capacity will not get in here
				pointer pOldBegin = internalLayout().BeginPtr();
				const size_type nOldCap = internalLayout().GetHeapCapacity();

				CharStringUninitializedCopy(pOldBegin, pOldBegin + n, internalLayout().SSOBeginPtr());
				internalLayout().SetSSOSize(n);
				*internalLayout().SSOEndPtr() = 0;

				DoFree(pOldBegin, nOldCap + 1);

				return;
			}

			pointer pNewBegin = DoAllocate(n + 1);			  // We need the + 1 to accomodate the trailing 0.
			size_type nSavedSize = internalLayout().GetSize();// save the size in case we transition from sso->heap

			pointer pNewEnd =
				CharStringUninitializedCopy(internalLayout().BeginPtr(), internalLayout().EndPtr(), pNewBegin);
			*pNewEnd = 0;

			DeallocateSelf();

			internalLayout().SetHeapBeginPtr(pNewBegin);
			internalLayout().SetHeapCapacity(n);
			internalLayout().SetHeapSize(nSavedSize);
		} else {
			DeallocateSelf();
			AllocateSelf();
		}
	}
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::force_size(size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(n > capacity())) {
		ThrowRangeException();
	}
#elif assert_ENABLED
	if (EASTL_UNLIKELY(n > capacity())) {
		EASTL_FAIL_MSG("basic_string::force_size -- out of range");
	}
#endif

	internalLayout().SetSize(n);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::clear() noexcept {
	internalLayout().SetSize(0);
	*internalLayout().BeginPtr() = value_type(0);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::pointer basic_string<T, Allocator>::detach() noexcept {
	// The detach function is an extension function which simply forgets the
	// owned pointer. It doesn't free it but rather assumes that the user
	// does. If the string is utilizing the short-string-optimization when a
	// detach is requested, a copy of the string into a seperate memory
	// allocation occurs and the owning pointer is given to the user who is
	// responsible for freeing the memory.

	pointer pDetached = nullptr;

	if (internalLayout().IsSSO()) {
		const size_type n = internalLayout().GetSize() + 1;// +1' so that we have room for the terminating 0.
		pDetached = DoAllocate(n);
		pointer pNewEnd =
			CharStringUninitializedCopy(internalLayout().BeginPtr(), internalLayout().EndPtr(), pDetached);
		*pNewEnd = 0;
	} else {
		pDetached = internalLayout().BeginPtr();
	}

	AllocateSelf();// reset to string to empty
	return pDetached;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reference basic_string<T, Allocator>::at(size_type n) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(n >= internalLayout().GetSize()))
		ThrowRangeException();
#elif assert_ENABLED// We assert if the user references the trailing 0 char.
	if (EASTL_UNLIKELY(n >= internalLayout().GetSize())) {
		EASTL_FAIL_MSG("basic_string::at -- out of range");
	}
#endif

	return internalLayout().BeginPtr()[n];
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reference basic_string<T, Allocator>::at(size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(n >= internalLayout().GetSize()))
		ThrowRangeException();
#elif assert_ENABLED// We assert if the user references the trailing 0 char.
	if (EASTL_UNLIKELY(n >= internalLayout().GetSize())) {
		EASTL_FAIL_MSG("basic_string::at -- out of range");
	}
#endif

	return internalLayout().BeginPtr()[n];
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reference basic_string<T, Allocator>::front() {
#if assert_ENABLED && EASTL_EMPTY_REFERENCE_ASSERT_ENABLED
	if (EASTL_UNLIKELY(internalLayout().GetSize() <= 0))// We assert if the user references the trailing 0 char.
		EASTL_FAIL_MSG("basic_string::front -- empty string");
#else
	// We allow the user to reference the trailing 0 char without asserting.
#endif

	return *internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reference basic_string<T, Allocator>::front() const {
#if assert_ENABLED && EASTL_EMPTY_REFERENCE_ASSERT_ENABLED
	if (EASTL_UNLIKELY(internalLayout().GetSize() <= 0))// We assert if the user references the trailing 0 char.
		EASTL_FAIL_MSG("basic_string::front -- empty string");
#else
	// We allow the user to reference the trailing 0 char without asserting.
#endif

	return *internalLayout().BeginPtr();
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reference basic_string<T, Allocator>::back() {
#if assert_ENABLED && EASTL_EMPTY_REFERENCE_ASSERT_ENABLED
	if (EASTL_UNLIKELY(internalLayout().GetSize() <= 0))// We assert if the user references the trailing 0 char.
		EASTL_FAIL_MSG("basic_string::back -- empty string");
#else
	// We allow the user to reference the trailing 0 char without asserting.
#endif

	return *(internalLayout().EndPtr() - 1);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::const_reference basic_string<T, Allocator>::back() const {
#if assert_ENABLED && EASTL_EMPTY_REFERENCE_ASSERT_ENABLED
	if (EASTL_UNLIKELY(internalLayout().GetSize() <= 0))// We assert if the user references the trailing 0 char.
		EASTL_FAIL_MSG("basic_string::back -- empty string");
#else
	// We allow the user to reference the trailing 0 char without asserting.
#endif

	return *(internalLayout().EndPtr() - 1);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::operator+=(const this_type& x) {
	return append(x);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::operator+=(const value_type* p) {
	return append(p);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::operator+=(value_type c) {
	push_back(c);
	return *this;
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::append(const this_type& x) {
	return append(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::append(const this_type& x,
																	  size_type position,
																	  size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(
			position >= x.internalLayout().GetSize()))// position must be < x.mpEnd, but position + n may be > mpEnd.
		ThrowRangeException();
#endif

	return append(
		x.internalLayout().BeginPtr() + position,
		x.internalLayout().BeginPtr() + position + std::min_alt(n, x.internalLayout().GetSize() - position));
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::append(const value_type* p, size_type n) {
	return append(p, p + n);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::append(const value_type* p) {
	return append(p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
template<typename OtherCharType>
basic_string<T, Allocator>& basic_string<T, Allocator>::append_convert(const OtherCharType* pOther) {
	return append_convert(pOther, (size_type)CharStrlen(pOther));
}

template<typename T, typename Allocator>
template<typename OtherStringType>
basic_string<T, Allocator>& basic_string<T, Allocator>::append_convert(const OtherStringType& x) {
	return append_convert(x.c_str(), x.length());
}

template<typename T, typename Allocator>
template<typename OtherCharType>
basic_string<T, Allocator>& basic_string<T, Allocator>::append_convert(const OtherCharType* pOther, size_type n) {
	// Question: What do we do in the case that we have an illegally encoded source string?
	// This can happen with UTF8 strings. Do we throw an exception or do we ignore the input?
	// One argument is that it's not a string class' job to handle the security aspects of a
	// program and the higher level application code should be verifying UTF8 string validity,
	// and thus we should do the friendly thing and ignore the invalid characters as opposed
	// to making the user of this function handle exceptions that are easily forgotten.

	const size_t kBufferSize = 512;
	value_type
		selfBuffer[kBufferSize];// This assumes that value_type is one of char8_t, char16_t, char32_t, or wchar_t.
								// Or more importantly, a type with a trivial constructor and destructor.
	value_type* const selfBufferEnd = selfBuffer + kBufferSize;
	const OtherCharType* pOtherEnd = pOther + n;

	while (pOther != pOtherEnd) {
		value_type* pSelfBufferCurrent = selfBuffer;
		DecodePart(pOther, pOtherEnd, pSelfBufferCurrent,
				   selfBufferEnd);// Write pOther to pSelfBuffer, converting encoding as we go. We currently ignore
								  // the return value, as we don't yet have a plan for handling encoding errors.
		append(selfBuffer, pSelfBufferCurrent);
	}

	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::append(size_type n, value_type c) {
	if (n > 0) {
		const size_type nSize = internalLayout().GetSize();
		const size_type nCapacity = capacity();

		if ((nSize + n) > nCapacity)
			reserve(GetNewCapacity(nCapacity, (nSize + n) - nCapacity));

		pointer pNewEnd = CharStringUninitializedFillN(internalLayout().EndPtr(), n, c);
		*pNewEnd = 0;
		internalLayout().SetSize(nSize + n);
	}

	return *this;
}
template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::append(value_type c) {
	const size_type nSize = internalLayout().GetSize() + 1;
	const size_type nCapacity = capacity();

	if (nSize > nCapacity)
		reserve(GetNewCapacity(nCapacity, nSize - nCapacity));

	pointer pNewEnd = CharStringUninitializedFillN(internalLayout().EndPtr(), 1, c);
	*pNewEnd = 0;
	internalLayout().SetSize(nSize);
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::append(const value_type* pBegin, const value_type* pEnd) {
	if (pBegin != pEnd) {
		const size_type nOldSize = internalLayout().GetSize();
		const size_type n = (size_type)(pEnd - pBegin);
		const size_type nCapacity = capacity();
		const size_type nNewSize = nOldSize + n;

		if (nNewSize > nCapacity) {
			const size_type nLength = GetNewCapacity(nCapacity, nNewSize - nCapacity);

			pointer pNewBegin = DoAllocate(nLength + 1);

			pointer pNewEnd =
				CharStringUninitializedCopy(internalLayout().BeginPtr(), internalLayout().EndPtr(), pNewBegin);
			pNewEnd = CharStringUninitializedCopy(pBegin, pEnd, pNewEnd);
			*pNewEnd = 0;

			DeallocateSelf();
			internalLayout().SetHeapBeginPtr(pNewBegin);
			internalLayout().SetHeapCapacity(nLength);
			internalLayout().SetHeapSize(nNewSize);
		} else {
			pointer pNewEnd = CharStringUninitializedCopy(pBegin, pEnd, internalLayout().EndPtr());
			*pNewEnd = 0;
			internalLayout().SetSize(nNewSize);
		}
	}

	return *this;
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::push_back(value_type c) {
	append((size_type)1, c);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::pop_back() {
#if assert_ENABLED
	if (EASTL_UNLIKELY(internalLayout().GetSize() <= 0))
		EASTL_FAIL_MSG("basic_string::pop_back -- empty string");
#endif

	internalLayout().EndPtr()[-1] = value_type(0);
	internalLayout().SetSize(internalLayout().GetSize() - 1);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(const this_type& x) {
	// The C++11 Standard 21.4.6.3 p6 specifies that assign from this_type assigns contents only and not the
	// allocator.
	return assign(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(const this_type& x,
																	  size_type position,
																	  size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > x.internalLayout().GetSize()))
		ThrowRangeException();
#endif

	// The C++11 Standard 21.4.6.3 p6 specifies that assign from this_type assigns contents only and not the
	// allocator.
	return assign(
		x.internalLayout().BeginPtr() + position,
		x.internalLayout().BeginPtr() + position + std::min_alt(n, x.internalLayout().GetSize() - position));
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(const value_type* p, size_type n) {
	return assign(p, p + n);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(const value_type* p) {
	return assign(p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::assign(size_type n, value_type c) {
	if (n <= internalLayout().GetSize()) {
		CharTypeAssignN(internalLayout().BeginPtr(), n, c);
		erase(internalLayout().BeginPtr() + n, internalLayout().EndPtr());
	} else {
		CharTypeAssignN(internalLayout().BeginPtr(), internalLayout().GetSize(), c);
		append(n - internalLayout().GetSize(), c);
	}
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::assign(const value_type* pBegin, const value_type* pEnd) {
	const size_type n = (size_type)(pEnd - pBegin);
	if (n <= internalLayout().GetSize()) {
		memmove(internalLayout().BeginPtr(), pBegin, (size_t)n * sizeof(value_type));
		erase(internalLayout().BeginPtr() + n, internalLayout().EndPtr());
	} else {
		memmove(internalLayout().BeginPtr(), pBegin, (size_t)(internalLayout().GetSize()) * sizeof(value_type));
		append(pBegin + internalLayout().GetSize(), pEnd);
	}
	return *this;
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(std::initializer_list<value_type> ilist) {
	return assign(ilist.begin(), ilist.end());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::assign(this_type&& x) {
	if (get_allocator() == x.get_allocator()) {
		std::swap(internalLayout(), x.internalLayout());
	} else
		assign(x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());

	return *this;
}

template<typename T, typename Allocator>
template<typename OtherCharType>
basic_string<T, Allocator>& basic_string<T, Allocator>::assign_convert(const OtherCharType* p) {
	clear();
	append_convert(p);
	return *this;
}

template<typename T, typename Allocator>
template<typename OtherCharType>
basic_string<T, Allocator>& basic_string<T, Allocator>::assign_convert(const OtherCharType* p, size_type n) {
	clear();
	append_convert(p, n);
	return *this;
}

template<typename T, typename Allocator>
template<typename OtherStringType>
basic_string<T, Allocator>& basic_string<T, Allocator>::assign_convert(const OtherStringType& x) {
	clear();
	append_convert(x.data(), x.length());
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::insert(size_type position, const this_type& x) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(internalLayout().GetSize() > (max_size() - x.internalLayout().GetSize())))
		ThrowLengthException();
#endif

	insert(internalLayout().BeginPtr() + position, x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::insert(size_type position,
															   const this_type& x,
															   size_type beg,
															   size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY((position > internalLayout().GetSize()) || (beg > x.internalLayout().GetSize())))
		ThrowRangeException();
#endif

	size_type nLength = std::min_alt(n, x.internalLayout().GetSize() - beg);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(internalLayout().GetSize() > (max_size() - nLength)))
		ThrowLengthException();
#endif

	insert(internalLayout().BeginPtr() + position, x.internalLayout().BeginPtr() + beg,
		   x.internalLayout().BeginPtr() + beg + nLength);
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::insert(size_type position, const value_type* p, size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(internalLayout().GetSize() > (max_size() - n)))
		ThrowLengthException();
#endif

	insert(internalLayout().BeginPtr() + position, p, p + n);
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::insert(size_type position, const value_type* p) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	size_type nLength = (size_type)CharStrlen(p);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(internalLayout().GetSize() > (max_size() - nLength)))
		ThrowLengthException();
#endif

	insert(internalLayout().BeginPtr() + position, p, p + nLength);
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::insert(size_type position, size_type n, value_type c) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(internalLayout().GetSize() > (max_size() - n)))
		ThrowLengthException();
#endif

	insert(internalLayout().BeginPtr() + position, n, c);
	return *this;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::insert(const_iterator p,
																						value_type c) {
	if (p == internalLayout().EndPtr()) {
		push_back(c);
		return internalLayout().EndPtr() - 1;
	}
	return InsertInternal(p, c);
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::insert(const_iterator p,
																				 size_type n,
																				 value_type c) {
	const difference_type nPosition = (p - internalLayout().BeginPtr());// Save this because we might reallocate.

#if assert_ENABLED
	if (EASTL_UNLIKELY((p < internalLayout().BeginPtr()) || (p > internalLayout().EndPtr())))
		EASTL_FAIL_MSG("basic_string::insert -- invalid position");
#endif

	if (n)// If there is anything to insert...
	{
		if (internalLayout().GetRemainingCapacity() >= n)// If we have enough capacity...
		{
			const size_type nElementsAfter = (size_type)(internalLayout().EndPtr() - p);

			if (nElementsAfter >= n)// If there's enough space for the new chars between the insert position and the end...
			{
				// Ensure we save the size before we do the copy, as we might overwrite the size field with the NULL
				// terminator in the edge case where we are inserting enough characters to equal our capacity
				const size_type nSavedSize = internalLayout().GetSize();
				CharStringUninitializedCopy((internalLayout().EndPtr() - n) + 1, internalLayout().EndPtr() + 1,
											internalLayout().EndPtr() + 1);
				internalLayout().SetSize(nSavedSize + n);
				memmove(const_cast<value_type*>(p) + n, p, (size_t)((nElementsAfter - n) + 1) * sizeof(value_type));
				CharTypeAssignN(const_cast<value_type*>(p), n, c);
			} else {
				pointer pOldEnd = internalLayout().EndPtr();
#if EASTL_EXCEPTIONS_ENABLED
				const size_type nOldSize = internalLayout().GetSize();
#endif
				CharStringUninitializedFillN(internalLayout().EndPtr() + 1, n - nElementsAfter - 1, c);
				internalLayout().SetSize(internalLayout().GetSize() + (n - nElementsAfter));

#if EASTL_EXCEPTIONS_ENABLED
				try {
#endif
					// See comment in if block above
					const size_type nSavedSize = internalLayout().GetSize();
					CharStringUninitializedCopy(p, pOldEnd + 1, internalLayout().EndPtr());
					internalLayout().SetSize(nSavedSize + nElementsAfter);
#if EASTL_EXCEPTIONS_ENABLED
				} catch (...) {
					internalLayout().SetSize(nOldSize);
					throw;
				}
#endif

				CharTypeAssignN(const_cast<value_type*>(p), nElementsAfter + 1, c);
			}
		} else {
			const size_type nOldSize = internalLayout().GetSize();
			const size_type nOldCap = capacity();
			const size_type nLength = GetNewCapacity(nOldCap, (nOldSize + n) - nOldCap);

			iterator pNewBegin = DoAllocate(nLength + 1);

			iterator pNewEnd = CharStringUninitializedCopy(internalLayout().BeginPtr(), p, pNewBegin);
			pNewEnd = CharStringUninitializedFillN(pNewEnd, n, c);
			pNewEnd = CharStringUninitializedCopy(p, internalLayout().EndPtr(), pNewEnd);
			*pNewEnd = 0;

			DeallocateSelf();
			internalLayout().SetHeapBeginPtr(pNewBegin);
			internalLayout().SetHeapCapacity(nLength);
			internalLayout().SetHeapSize(nOldSize + n);
		}
	}

	return internalLayout().BeginPtr() + nPosition;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::insert(const_iterator p,
																				 const value_type* pBegin,
																				 const value_type* pEnd) {
	const difference_type nPosition = (p - internalLayout().BeginPtr());// Save this because we might reallocate.

#if assert_ENABLED
	if (EASTL_UNLIKELY((p < internalLayout().BeginPtr()) || (p > internalLayout().EndPtr())))
		EASTL_FAIL_MSG("basic_string::insert -- invalid position");
#endif

	const size_type n = (size_type)(pEnd - pBegin);

	if (n) {
		const bool bCapacityIsSufficient = (internalLayout().GetRemainingCapacity() >= n);
		const bool bSourceIsFromSelf =
			((pEnd >= internalLayout().BeginPtr()) && (pBegin <= internalLayout().EndPtr()));

		if (bSourceIsFromSelf && internalLayout().IsSSO()) {
			// pBegin to pEnd will be <= this->GetSize(), so stackTemp will guaranteed be an SSO String
			// If we are inserting ourself into ourself and we are SSO, then on the recursive call we can
			// guarantee 0 or 1 allocation depending if we need to realloc
			// We don't do this for Heap strings as then this path may do 1 or 2 allocations instead of
			// only 1 allocation when we fall through to the last else case below
			const this_type stackTemp(pBegin, pEnd, get_allocator());
			return insert(p, stackTemp.data(), stackTemp.data() + stackTemp.size());
		}

		// If bSourceIsFromSelf is true, then we reallocate. This is because we are
		// inserting ourself into ourself and thus both the source and destination
		// be modified, making it rather tricky to attempt to do in place. The simplest
		// resolution is to reallocate. To consider: there may be a way to implement this
		// whereby we don't need to reallocate or can often avoid reallocating.
		if (bCapacityIsSufficient && !bSourceIsFromSelf) {
			const size_type nElementsAfter = (size_type)(internalLayout().EndPtr() - p);

			if (nElementsAfter >= n)// If there are enough characters between insert pos and end
			{
				// Ensure we save the size before we do the copy, as we might overwrite the size field with the NULL
				// terminator in the edge case where we are inserting enough characters to equal our capacity
				const size_type nSavedSize = internalLayout().GetSize();
				CharStringUninitializedCopy((internalLayout().EndPtr() - n) + 1, internalLayout().EndPtr() + 1,
											internalLayout().EndPtr() + 1);
				internalLayout().SetSize(nSavedSize + n);
				memmove(const_cast<value_type*>(p) + n, p, (size_t)((nElementsAfter - n) + 1) * sizeof(value_type));
				memmove(const_cast<value_type*>(p), pBegin, (size_t)(n) * sizeof(value_type));
			} else {
				pointer pOldEnd = internalLayout().EndPtr();
#if EASTL_EXCEPTIONS_ENABLED
				const size_type nOldSize = internalLayout().GetSize();
#endif
				const value_type* const pMid = pBegin + (nElementsAfter + 1);

				CharStringUninitializedCopy(pMid, pEnd, internalLayout().EndPtr() + 1);
				internalLayout().SetSize(internalLayout().GetSize() + (n - nElementsAfter));

#if EASTL_EXCEPTIONS_ENABLED
				try {
#endif
					// See comment in if block above
					const size_type nSavedSize = internalLayout().GetSize();
					CharStringUninitializedCopy(p, pOldEnd + 1, internalLayout().EndPtr());
					internalLayout().SetSize(nSavedSize + nElementsAfter);
#if EASTL_EXCEPTIONS_ENABLED
				} catch (...) {
					internalLayout().SetSize(nOldSize);
					throw;
				}
#endif

				CharStringUninitializedCopy(pBegin, pMid, const_cast<value_type*>(p));
			}
		} else// Else we need to reallocate to implement this.
		{
			const size_type nOldSize = internalLayout().GetSize();
			const size_type nOldCap = capacity();
			size_type nLength;

			if (bCapacityIsSufficient)// If bCapacityIsSufficient is true, then bSourceIsFromSelf must be true.
				nLength = nOldSize + n;
			else
				nLength = GetNewCapacity(nOldCap, (nOldSize + n) - nOldCap);

			pointer pNewBegin = DoAllocate(nLength + 1);

			pointer pNewEnd = CharStringUninitializedCopy(internalLayout().BeginPtr(), p, pNewBegin);
			pNewEnd = CharStringUninitializedCopy(pBegin, pEnd, pNewEnd);
			pNewEnd = CharStringUninitializedCopy(p, internalLayout().EndPtr(), pNewEnd);
			*pNewEnd = 0;

			DeallocateSelf();
			internalLayout().SetHeapBeginPtr(pNewBegin);
			internalLayout().SetHeapCapacity(nLength);
			internalLayout().SetHeapSize(nOldSize + n);
		}
	}

	return internalLayout().BeginPtr() + nPosition;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::insert(
	const_iterator p,
	std::initializer_list<value_type> ilist) {
	return insert(p, ilist.begin(), ilist.end());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::erase(size_type position, size_type n) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

#if assert_ENABLED
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		EASTL_FAIL_MSG("basic_string::erase -- invalid position");
#endif

	erase(internalLayout().BeginPtr() + position,
		  internalLayout().BeginPtr() + position + std::min_alt(n, internalLayout().GetSize() - position));

	return *this;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::erase(const_iterator p) {
#if assert_ENABLED
	if (EASTL_UNLIKELY((p < internalLayout().BeginPtr()) || (p >= internalLayout().EndPtr())))
		EASTL_FAIL_MSG("basic_string::erase -- invalid position");
#endif

	memmove(const_cast<value_type*>(p), p + 1, (size_t)(internalLayout().EndPtr() - p) * sizeof(value_type));
	internalLayout().SetSize(internalLayout().GetSize() - 1);
	return const_cast<value_type*>(p);
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::erase(const_iterator pBegin,
																				const_iterator pEnd) {
#if assert_ENABLED
	if (EASTL_UNLIKELY((pBegin < internalLayout().BeginPtr()) || (pBegin > internalLayout().EndPtr()) || (pEnd < internalLayout().BeginPtr()) || (pEnd > internalLayout().EndPtr()) || (pEnd < pBegin))) {
		EASTL_FAIL_MSG("basic_string::erase -- invalid position");
	}
#endif

	if (pBegin != pEnd) {
		memmove(const_cast<value_type*>(pBegin), pEnd,
				(size_t)((internalLayout().EndPtr() - pEnd) + 1) * sizeof(value_type));
		const size_type n = (size_type)(pEnd - pBegin);
		internalLayout().SetSize(internalLayout().GetSize() - n);
	}
	return const_cast<value_type*>(pBegin);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::reverse_iterator basic_string<T, Allocator>::erase(
	reverse_iterator position) {
	return reverse_iterator(erase((++position).base()));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::reverse_iterator basic_string<T, Allocator>::erase(reverse_iterator first,
																						reverse_iterator last) {
	return reverse_iterator(erase((++last).base(), (++first).base()));
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(size_type position, size_type n, const this_type& x) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	const size_type nLength = std::min_alt(n, internalLayout().GetSize() - position);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY((internalLayout().GetSize() - nLength) >= (max_size() - x.internalLayout().GetSize())))
		ThrowLengthException();
#endif

	return replace(internalLayout().BeginPtr() + position, internalLayout().BeginPtr() + position + nLength,
				   x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(size_type pos1,
																size_type n1,
																const this_type& x,
																size_type pos2,
																size_type n2) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY((pos1 > internalLayout().GetSize()) || (pos2 > x.internalLayout().GetSize())))
		ThrowRangeException();
#endif

	const size_type nLength1 = std::min_alt(n1, internalLayout().GetSize() - pos1);
	const size_type nLength2 = std::min_alt(n2, x.internalLayout().GetSize() - pos2);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY((internalLayout().GetSize() - nLength1) >= (max_size() - nLength2)))
		ThrowLengthException();
#endif

	return replace(internalLayout().BeginPtr() + pos1, internalLayout().BeginPtr() + pos1 + nLength1,
				   x.internalLayout().BeginPtr() + pos2, x.internalLayout().BeginPtr() + pos2 + nLength2);
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(size_type position,
																size_type n1,
																const value_type* p,
																size_type n2) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	const size_type nLength = std::min_alt(n1, internalLayout().GetSize() - position);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY((n2 > max_size()) || ((internalLayout().GetSize() - nLength) >= (max_size() - n2))))
		ThrowLengthException();
#endif

	return replace(internalLayout().BeginPtr() + position, internalLayout().BeginPtr() + position + nLength, p,
				   p + n2);
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(size_type position,
																size_type n1,
																const value_type* p) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	const size_type nLength = std::min_alt(n1, internalLayout().GetSize() - position);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	const size_type n2 = (size_type)CharStrlen(p);
	if (EASTL_UNLIKELY((n2 > max_size()) || ((internalLayout().GetSize() - nLength) >= (max_size() - n2))))
		ThrowLengthException();
#endif

	return replace(internalLayout().BeginPtr() + position, internalLayout().BeginPtr() + position + nLength, p,
				   p + CharStrlen(p));
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(size_type position,
																size_type n1,
																size_type n2,
																value_type c) {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	const size_type nLength = std::min_alt(n1, internalLayout().GetSize() - position);

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY((n2 > max_size()) || (internalLayout().GetSize() - nLength) >= (max_size() - n2)))
		ThrowLengthException();
#endif

	return replace(internalLayout().BeginPtr() + position, internalLayout().BeginPtr() + position + nLength, n2, c);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::replace(const_iterator pBegin,
																	   const_iterator pEnd,
																	   const this_type& x) {
	return replace(pBegin, pEnd, x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::replace(const_iterator pBegin,
																	   const_iterator pEnd,
																	   const value_type* p,
																	   size_type n) {
	return replace(pBegin, pEnd, p, p + n);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator>& basic_string<T, Allocator>::replace(const_iterator pBegin,
																	   const_iterator pEnd,
																	   const value_type* p) {
	return replace(pBegin, pEnd, p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(const_iterator pBegin,
																const_iterator pEnd,
																size_type n,
																value_type c) {
#if assert_ENABLED
	if (EASTL_UNLIKELY((pBegin < internalLayout().BeginPtr()) || (pBegin > internalLayout().EndPtr()) || (pEnd < internalLayout().BeginPtr()) || (pEnd > internalLayout().EndPtr()) || (pEnd < pBegin)))
		EASTL_FAIL_MSG("basic_string::replace -- invalid position");
#endif

	const size_type nLength = static_cast<size_type>(pEnd - pBegin);

	if (nLength >= n) {
		CharTypeAssignN(const_cast<value_type*>(pBegin), n, c);
		erase(pBegin + n, pEnd);
	} else {
		CharTypeAssignN(const_cast<value_type*>(pBegin), nLength, c);
		insert(pEnd, n - nLength, c);
	}
	return *this;
}

template<typename T, typename Allocator>
basic_string<T, Allocator>& basic_string<T, Allocator>::replace(const_iterator pBegin1,
																const_iterator pEnd1,
																const value_type* pBegin2,
																const value_type* pEnd2) {
#if assert_ENABLED
	if (EASTL_UNLIKELY((pBegin1 < internalLayout().BeginPtr()) || (pBegin1 > internalLayout().EndPtr()) || (pEnd1 < internalLayout().BeginPtr()) || (pEnd1 > internalLayout().EndPtr()) || (pEnd1 < pBegin1)))
		EASTL_FAIL_MSG("basic_string::replace -- invalid position");
#endif

	const size_type nLength1 = (size_type)(pEnd1 - pBegin1);
	const size_type nLength2 = (size_type)(pEnd2 - pBegin2);

	if (nLength1 >= nLength2)// If we have a non-expanding operation...
	{
		if ((pBegin2 > pEnd1) || (pEnd2 <= pBegin1))// If we have a non-overlapping operation...
			memcpy(const_cast<value_type*>(pBegin1), pBegin2, (size_t)(pEnd2 - pBegin2) * sizeof(value_type));
		else
			memmove(const_cast<value_type*>(pBegin1), pBegin2, (size_t)(pEnd2 - pBegin2) * sizeof(value_type));
		erase(pBegin1 + nLength2, pEnd1);
	} else// Else we are expanding.
	{
		if ((pBegin2 > pEnd1) || (pEnd2 <= pBegin1))// If we have a non-overlapping operation...
		{
			const value_type* const pMid2 = pBegin2 + nLength1;

			if ((pEnd2 <= pBegin1) || (pBegin2 > pEnd1))
				memcpy(const_cast<value_type*>(pBegin1), pBegin2, (size_t)(pMid2 - pBegin2) * sizeof(value_type));
			else
				memmove(const_cast<value_type*>(pBegin1), pBegin2, (size_t)(pMid2 - pBegin2) * sizeof(value_type));
			insert(pEnd1, pMid2, pEnd2);
		} else// else we have an overlapping operation.
		{
			// I can't think of any easy way of doing this without allocating temporary memory.
			const size_type nOldSize = internalLayout().GetSize();
			const size_type nOldCap = capacity();
			const size_type nNewCapacity = GetNewCapacity(nOldCap, (nOldSize + (nLength2 - nLength1)) - nOldCap);

			pointer pNewBegin = DoAllocate(nNewCapacity + 1);

			pointer pNewEnd = CharStringUninitializedCopy(internalLayout().BeginPtr(), pBegin1, pNewBegin);
			pNewEnd = CharStringUninitializedCopy(pBegin2, pEnd2, pNewEnd);
			pNewEnd = CharStringUninitializedCopy(pEnd1, internalLayout().EndPtr(), pNewEnd);
			*pNewEnd = 0;

			DeallocateSelf();
			internalLayout().SetHeapBeginPtr(pNewBegin);
			internalLayout().SetHeapCapacity(nNewCapacity);
			internalLayout().SetHeapSize(nOldSize + (nLength2 - nLength1));
		}
	}
	return *this;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::copy(value_type* p,
																				size_type n,
																				size_type position) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	// C++ std says the effects of this function are as if calling char_traits::copy()
	// thus the 'p' must not overlap *this string, so we can use memcpy
	const size_type nLength = std::min_alt(n, internalLayout().GetSize() - position);
	CharStringUninitializedCopy(internalLayout().BeginPtr() + position,
								internalLayout().BeginPtr() + position + nLength, p);
	return nLength;
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::swap(this_type& x) {
	if (get_allocator() == x.get_allocator() || (internalLayout().IsSSO() && x.internalLayout().IsSSO()))// If allocators are equivalent...
	{
		// We leave mAllocator as-is.
		std::swap(internalLayout(), x.internalLayout());
	} else// else swap the contents.
	{
		const this_type temp(*this);// Can't call std::swap because that would
		*this = x;					// itself call this member swap function.
		x = temp;
	}
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find(const this_type& x,
																					   size_type position) const
	noexcept {
	return find(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find(const value_type* p,
																					   size_type position) const {
	return find(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find(const value_type* p,
																				size_type position,
																				size_type n) const {
	// It is not clear what the requirements are for position, but since the C++ standard
	// appears to be silent it is assumed for now that position can be any value.
	//#if assert_ENABLED
	//    if(EASTL_UNLIKELY(position > (size_type)(mpEnd - mpBegin)))
	//        EASTL_FAIL_MSG("basic_string::find -- invalid position");
	//#endif

	if (EASTL_LIKELY(((npos - n) >= position) && (position + n) <= internalLayout().GetSize()))// If the range is valid...
	{
		const value_type* const pTemp =
			std::search(internalLayout().BeginPtr() + position, internalLayout().EndPtr(), p, p + n);

		if ((pTemp != internalLayout().EndPtr()) || (n == 0))
			return (size_type)(pTemp - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find(value_type c, size_type position)
	const noexcept {
	// It is not clear what the requirements are for position, but since the C++ standard
	// appears to be silent it is assumed for now that position can be any value.
	//#if assert_ENABLED
	//    if(EASTL_UNLIKELY(position > (size_type)(mpEnd - mpBegin)))
	//        EASTL_FAIL_MSG("basic_string::find -- invalid position");
	//#endif

	if (EASTL_LIKELY(position < internalLayout().GetSize()))// If the position is valid...
	{
		const const_iterator pResult =
			std::find(internalLayout().BeginPtr() + position, internalLayout().EndPtr(), c);

		if (pResult != internalLayout().EndPtr())
			return (size_type)(pResult - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::rfind(const this_type& x,
																						size_type position) const
	noexcept {
	return rfind(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::rfind(const value_type* p,
																						size_type position) const {
	return rfind(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::rfind(const value_type* p,
																				 size_type position,
																				 size_type n) const {
	// Disabled because it's not clear what values are valid for position.
	// It is documented that npos is a valid value, though. We return npos and
	// don't crash if postion is any invalid value.
	//#if assert_ENABLED
	//    if(EASTL_UNLIKELY((position != npos) && (position > (size_type)(mpEnd - mpBegin))))
	//        EASTL_FAIL_MSG("basic_string::rfind -- invalid position");
	//#endif

	// Note that a search for a zero length string starting at position = end() returns end() and not npos.
	// Note by Paul Pedriana: I am not sure how this should behave in the case of n == 0 and position > size.
	// The standard seems to suggest that rfind doesn't act exactly the same as find in that input position
	// can be > size and the return value can still be other than npos. Thus, if n == 0 then you can
	// never return npos, unlike the case with find.
	const size_type nLength = internalLayout().GetSize();

	if (EASTL_LIKELY(n <= nLength)) {
		if (EASTL_LIKELY(n)) {
			const const_iterator pEnd = internalLayout().BeginPtr() + std::min_alt(nLength - n, position) + n;
			const const_iterator pResult = CharTypeStringRSearch(internalLayout().BeginPtr(), pEnd, p, p + n);

			if (pResult != pEnd)
				return (size_type)(pResult - internalLayout().BeginPtr());
		} else
			return std::min_alt(nLength, position);
	}
	return npos;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::rfind(value_type c, size_type position)
	const noexcept {
	// If n is zero or position is >= size, we return npos.
	const size_type nLength = internalLayout().GetSize();

	if (EASTL_LIKELY(nLength)) {
		const value_type* const pEnd = internalLayout().BeginPtr() + std::min_alt(nLength - 1, position) + 1;
		const value_type* const pResult = CharTypeStringRFind(pEnd, internalLayout().BeginPtr(), c);

		if (pResult != internalLayout().BeginPtr())
			return (size_type)((pResult - 1) - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_of(
	const this_type& x,
	size_type position) const noexcept {
	return find_first_of(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_of(
	const value_type* p,
	size_type position) const {
	return find_first_of(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_of(const value_type* p,
																						 size_type position,
																						 size_type n) const {
	// If position is >= size, we return npos.
	if (EASTL_LIKELY((position < internalLayout().GetSize()))) {
		const value_type* const pBegin = internalLayout().BeginPtr() + position;
		const const_iterator pResult = CharTypeStringFindFirstOf(pBegin, internalLayout().EndPtr(), p, p + n);

		if (pResult != internalLayout().EndPtr())
			return (size_type)(pResult - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_of(
	value_type c,
	size_type position) const noexcept {
	return find(c, position);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_of(
	const this_type& x,
	size_type position) const noexcept {
	return find_last_of(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_of(
	const value_type* p,
	size_type position) const {
	return find_last_of(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_of(const value_type* p,
																						size_type position,
																						size_type n) const {
	// If n is zero or position is >= size, we return npos.
	const size_type nLength = internalLayout().GetSize();

	if (EASTL_LIKELY(nLength)) {
		const value_type* const pEnd = internalLayout().BeginPtr() + std::min_alt(nLength - 1, position) + 1;
		const value_type* const pResult = CharTypeStringRFindFirstOf(pEnd, internalLayout().BeginPtr(), p, p + n);

		if (pResult != internalLayout().BeginPtr())
			return (size_type)((pResult - 1) - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_of(
	value_type c,
	size_type position) const noexcept {
	return rfind(c, position);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_not_of(
	const this_type& x,
	size_type position) const noexcept {
	return find_first_not_of(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_not_of(
	const value_type* p,
	size_type position) const {
	return find_first_not_of(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_not_of(const value_type* p,
																							 size_type position,
																							 size_type n) const {
	if (EASTL_LIKELY(position <= internalLayout().GetSize())) {
		const const_iterator pResult = CharTypeStringFindFirstNotOf(internalLayout().BeginPtr() + position,
																	internalLayout().EndPtr(), p, p + n);

		if (pResult != internalLayout().EndPtr())
			return (size_type)(pResult - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_first_not_of(
	value_type c,
	size_type position) const noexcept {
	if (EASTL_LIKELY(position <= internalLayout().GetSize())) {
		// Todo: Possibly make a specialized version of CharTypeStringFindFirstNotOf(pBegin, pEnd, c).
		const const_iterator pResult = CharTypeStringFindFirstNotOf(internalLayout().BeginPtr() + position,
																	internalLayout().EndPtr(), &c, &c + 1);

		if (pResult != internalLayout().EndPtr())
			return (size_type)(pResult - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_not_of(
	const this_type& x,
	size_type position) const noexcept {
	return find_last_not_of(x.internalLayout().BeginPtr(), position, x.internalLayout().GetSize());
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_not_of(
	const value_type* p,
	size_type position) const {
	return find_last_not_of(p, position, (size_type)CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_not_of(const value_type* p,
																							size_type position,
																							size_type n) const {
	const size_type nLength = internalLayout().GetSize();

	if (EASTL_LIKELY(nLength)) {
		const value_type* const pEnd = internalLayout().BeginPtr() + std::min_alt(nLength - 1, position) + 1;
		const value_type* const pResult =
			CharTypeStringRFindFirstNotOf(pEnd, internalLayout().BeginPtr(), p, p + n);

		if (pResult != internalLayout().BeginPtr())
			return (size_type)((pResult - 1) - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::find_last_not_of(
	value_type c,
	size_type position) const noexcept {
	const size_type nLength = internalLayout().GetSize();

	if (EASTL_LIKELY(nLength)) {
		// Todo: Possibly make a specialized version of CharTypeStringRFindFirstNotOf(pBegin, pEnd, c).
		const value_type* const pEnd = internalLayout().BeginPtr() + std::min_alt(nLength - 1, position) + 1;
		const value_type* const pResult =
			CharTypeStringRFindFirstNotOf(pEnd, internalLayout().BeginPtr(), &c, &c + 1);

		if (pResult != internalLayout().BeginPtr())
			return (size_type)((pResult - 1) - internalLayout().BeginPtr());
	}
	return npos;
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator> basic_string<T, Allocator>::substr(size_type position, size_type n) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		ThrowRangeException();
#elif assert_ENABLED
	if (EASTL_UNLIKELY(position > internalLayout().GetSize()))
		EASTL_FAIL_MSG("basic_string::substr -- invalid position");
#endif

	// C++ std says the return string allocator must be default constructed, not a copy of this->get_allocator()
	return basic_string(
		internalLayout().BeginPtr() + position,
		internalLayout().BeginPtr() + position + std::min_alt(n, internalLayout().GetSize() - position),
		get_allocator());
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(const this_type& x) const noexcept {
	return compare(internalLayout().BeginPtr(), internalLayout().EndPtr(), x.internalLayout().BeginPtr(),
				   x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(size_type pos1, size_type n1, const this_type& x) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(pos1 > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	return compare(internalLayout().BeginPtr() + pos1,
				   internalLayout().BeginPtr() + pos1 + std::min_alt(n1, internalLayout().GetSize() - pos1),
				   x.internalLayout().BeginPtr(), x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(size_type pos1,
											   size_type n1,
											   const this_type& x,
											   size_type pos2,
											   size_type n2) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY((pos1 > (size_type)(internalLayout().EndPtr() - internalLayout().BeginPtr())) || (pos2 > (size_type)(x.internalLayout().EndPtr() - x.internalLayout().BeginPtr()))))
		ThrowRangeException();
#endif

	return compare(internalLayout().BeginPtr() + pos1,
				   internalLayout().BeginPtr() + pos1 + std::min_alt(n1, internalLayout().GetSize() - pos1),
				   x.internalLayout().BeginPtr() + pos2,
				   x.internalLayout().BeginPtr() + pos2 + std::min_alt(n2, x.internalLayout().GetSize() - pos2));
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(const value_type* p) const {
	return compare(internalLayout().BeginPtr(), internalLayout().EndPtr(), p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(size_type pos1, size_type n1, const value_type* p) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(pos1 > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	return compare(internalLayout().BeginPtr() + pos1,
				   internalLayout().BeginPtr() + pos1 + std::min_alt(n1, internalLayout().GetSize() - pos1), p,
				   p + CharStrlen(p));
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::compare(size_type pos1,
											   size_type n1,
											   const value_type* p,
											   size_type n2) const {
#if EASTL_STRING_OPT_RANGE_ERRORS
	if (EASTL_UNLIKELY(pos1 > internalLayout().GetSize()))
		ThrowRangeException();
#endif

	return compare(internalLayout().BeginPtr() + pos1,
				   internalLayout().BeginPtr() + pos1 + std::min_alt(n1, internalLayout().GetSize() - pos1), p,
				   p + n2);
}

// make_lower
// This is a very simple ASCII-only case conversion function
// Anything more complicated should use a more powerful separate library.
template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::make_lower() {
	for (pointer p = internalLayout().BeginPtr(); p < internalLayout().EndPtr(); ++p)
		*p = (value_type)CharToLower(*p);
}

// make_upper
// This is a very simple ASCII-only case conversion function
// Anything more complicated should use a more powerful separate library.
template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::make_upper() {
	for (pointer p = internalLayout().BeginPtr(); p < internalLayout().EndPtr(); ++p)
		*p = (value_type)CharToUpper(*p);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::ltrim() {
	const value_type array[] = {' ', '\t', 0};// This is a pretty simplistic view of whitespace.
	erase(0, find_first_not_of(array));
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::rtrim() {
	const value_type array[] = {' ', '\t', 0};// This is a pretty simplistic view of whitespace.
	erase(find_last_not_of(array) + 1);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::trim() {
	ltrim();
	rtrim();
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::ltrim(const value_type* p) {
	erase(0, find_first_not_of(p));
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::rtrim(const value_type* p) {
	erase(find_last_not_of(p) + 1);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::trim(const value_type* p) {
	ltrim(p);
	rtrim(p);
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator> basic_string<T, Allocator>::left(size_type n) const {
	const size_type nLength = length();
	if (n < nLength)
		return substr(0, n);
	// C++ std says that substr must return default constructed allocated, but we do not.
	// Instead it is much more practical to provide the copy of the current allocator
	return basic_string(*this, get_allocator());
}

template<typename T, typename Allocator>
inline basic_string<T, Allocator> basic_string<T, Allocator>::right(size_type n) const {
	const size_type nLength = length();
	if (n < nLength)
		return substr(nLength - n, n);
	// C++ std says that substr must return default constructed allocated, but we do not.
	// Instead it is much more practical to provide the copy of the current allocator
	return basic_string(*this, get_allocator());
}

template<typename T, typename Allocator>
int basic_string<T, Allocator>::compare(const value_type* pBegin1,
										const value_type* pEnd1,
										const value_type* pBegin2,
										const value_type* pEnd2) {
	const difference_type n1 = pEnd1 - pBegin1;
	const difference_type n2 = pEnd2 - pBegin2;
	const difference_type nMin = std::min_alt(n1, n2);
	const int cmp = Compare(pBegin1, pBegin2, (size_t)nMin);

	return (cmp != 0 ? cmp : (n1 < n2 ? -1 : (n1 > n2 ? 1 : 0)));
}

template<typename T, typename Allocator>
int basic_string<T, Allocator>::comparei(const value_type* pBegin1,
										 const value_type* pEnd1,
										 const value_type* pBegin2,
										 const value_type* pEnd2) {
	const difference_type n1 = pEnd1 - pBegin1;
	const difference_type n2 = pEnd2 - pBegin2;
	const difference_type nMin = std::min_alt(n1, n2);
	const int cmp = CompareI(pBegin1, pBegin2, (size_t)nMin);

	return (cmp != 0 ? cmp : (n1 < n2 ? -1 : (n1 > n2 ? 1 : 0)));
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::comparei(const this_type& x) const noexcept {
	return comparei(internalLayout().BeginPtr(), internalLayout().EndPtr(), x.internalLayout().BeginPtr(),
					x.internalLayout().EndPtr());
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::comparei(const value_type* p) const {
	return comparei(internalLayout().BeginPtr(), internalLayout().EndPtr(), p, p + CharStrlen(p));
}

template<typename T, typename Allocator>
typename basic_string<T, Allocator>::iterator basic_string<T, Allocator>::InsertInternal(const_iterator p,
																						 value_type c) {
	iterator pNewPosition = const_cast<value_type*>(p);

	if ((internalLayout().EndPtr() + 1) <= internalLayout().CapacityPtr()) {
		const size_type nSavedSize = internalLayout().GetSize();
		memmove(const_cast<value_type*>(p) + 1, p, (size_t)(internalLayout().EndPtr() - p) * sizeof(value_type));
		*(internalLayout().EndPtr() + 1) = 0;
		*pNewPosition = c;
		internalLayout().SetSize(nSavedSize + 1);
	} else {
		const size_type nOldSize = internalLayout().GetSize();
		const size_type nOldCap = capacity();
		const size_type nLength = GetNewCapacity(nOldCap, 1);

		iterator pNewBegin = DoAllocate(nLength + 1);

		pNewPosition = CharStringUninitializedCopy(internalLayout().BeginPtr(), p, pNewBegin);
		*pNewPosition = c;

		iterator pNewEnd = pNewPosition + 1;
		pNewEnd = CharStringUninitializedCopy(p, internalLayout().EndPtr(), pNewEnd);
		*pNewEnd = 0;

		DeallocateSelf();
		internalLayout().SetHeapBeginPtr(pNewBegin);
		internalLayout().SetHeapCapacity(nLength);
		internalLayout().SetHeapSize(nOldSize + 1);
	}
	return pNewPosition;
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::SizeInitialize(size_type n, value_type c) {
	AllocateSelf(n);

	CharStringUninitializedFillN(internalLayout().BeginPtr(), n, c);
	*internalLayout().EndPtr() = 0;
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::RangeInitialize(const value_type* pBegin, const value_type* pEnd) {
#if EASTL_STRING_OPT_ARGUMENT_ERRORS
	if (EASTL_UNLIKELY(!pBegin && (pEnd < pBegin)))// 21.4.2 p7
		ThrowInvalidArgumentException();
#endif

	const size_type n = (size_type)(pEnd - pBegin);

	AllocateSelf(n);

	CharStringUninitializedCopy(pBegin, pEnd, internalLayout().BeginPtr());
	*internalLayout().EndPtr() = 0;
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::RangeInitialize(const value_type* pBegin) {
#if EASTL_STRING_OPT_ARGUMENT_ERRORS
	if (EASTL_UNLIKELY(!pBegin))
		ThrowInvalidArgumentException();
#endif

	RangeInitialize(pBegin, pBegin + CharStrlen(pBegin));
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::DoAllocate(size_type n) {
	return (value_type*)EASTLAlloc(get_allocator(), n * sizeof(value_type));
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::DoFree(value_type* p, size_type n) {
	if (p)
		EASTLFree(get_allocator(), p, n * sizeof(value_type));
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::GetNewCapacity(
	size_type currentCapacity) {
	return GetNewCapacity(currentCapacity, 1);
}

template<typename T, typename Allocator>
inline typename basic_string<T, Allocator>::size_type basic_string<T, Allocator>::GetNewCapacity(
	size_type currentCapacity,
	size_type minimumGrowSize) {
#if EASTL_STRING_OPT_LENGTH_ERRORS
	const size_type nRemainingSize = max_size() - currentCapacity;
	if (EASTL_UNLIKELY((minimumGrowSize > nRemainingSize))) {
		ThrowLengthException();
	}
#endif

	const size_type nNewCapacity = std::max(currentCapacity + minimumGrowSize, currentCapacity * 2);

	return nNewCapacity;
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::AllocateSelf() {
	internalLayout().ResetToSSO();
}

template<typename T, typename Allocator>
void basic_string<T, Allocator>::AllocateSelf(size_type n) {
#if assert_ENABLED
	if (EASTL_UNLIKELY(n >= 0x40000000)) {
		EASTL_FAIL_MSG("basic_string::AllocateSelf -- improbably large request.");
	}
#endif

#if EASTL_STRING_OPT_LENGTH_ERRORS
	if (EASTL_UNLIKELY(n > max_size()))
		ThrowLengthException();
#endif

	if (n > SSOLayout::SSO_CAPACITY) {
		pointer pBegin = DoAllocate(n + 1);
		internalLayout().SetHeapBeginPtr(pBegin);
		internalLayout().SetHeapCapacity(n);
		internalLayout().SetHeapSize(n);
	} else
		internalLayout().SetSSOSize(n);
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::DeallocateSelf() {
	if (internalLayout().IsHeap()) {
		DoFree(internalLayout().BeginPtr(), internalLayout().GetHeapCapacity() + 1);
	}
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::ThrowLengthException() const {
#if EASTL_EXCEPTIONS_ENABLED
	throw std::length_error("basic_string -- length_error");
#elif assert_ENABLED
	EASTL_FAIL_MSG("basic_string -- length_error");
#endif
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::ThrowRangeException() const {
#if EASTL_EXCEPTIONS_ENABLED
	throw std::out_of_range("basic_string -- out of range");
#elif assert_ENABLED
	EASTL_FAIL_MSG("basic_string -- out of range");
#endif
}

template<typename T, typename Allocator>
inline void basic_string<T, Allocator>::ThrowInvalidArgumentException() const {
#if EASTL_EXCEPTIONS_ENABLED
	throw std::invalid_argument("basic_string -- invalid argument");
#elif assert_ENABLED
	EASTL_FAIL_MSG("basic_string -- invalid argument");
#endif
}

// CharTypeStringFindEnd
// Specialized char version of STL find() from back function.
// Not the same as RFind because search range is specified as forward iterators.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type*
basic_string<T, Allocator>::CharTypeStringFindEnd(const value_type* pBegin, const value_type* pEnd, value_type c) {
	const value_type* pTemp = pEnd;
	while (--pTemp >= pBegin) {
		if (*pTemp == c)
			return pTemp;
	}

	return pEnd;
}

// CharTypeStringRFind
// Specialized value_type version of STL find() function in reverse.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringRFind(
	const value_type* pRBegin,
	const value_type* pREnd,
	const value_type c) {
	while (pRBegin > pREnd) {
		if (*(pRBegin - 1) == c)
			return pRBegin;
		--pRBegin;
	}
	return pREnd;
}

// CharTypeStringSearch
// Specialized value_type version of STL search() function.
// Purpose: find p2 within p1. Return p1End if not found or if either string is zero length.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringSearch(
	const value_type* p1Begin,
	const value_type* p1End,
	const value_type* p2Begin,
	const value_type* p2End) {
	// Test for zero length strings, in which case we have a match or a failure,
	// but the return value is the same either way.
	if ((p1Begin == p1End) || (p2Begin == p2End))
		return p1Begin;

	// Test for a pattern of length 1.
	if ((p2Begin + 1) == p2End)
		return std::find(p1Begin, p1End, *p2Begin);

	// General case.
	const value_type* pTemp;
	const value_type* pTemp1 = (p2Begin + 1);
	const value_type* pCurrent = p1Begin;

	while (p1Begin != p1End) {
		p1Begin = std::find(p1Begin, p1End, *p2Begin);
		if (p1Begin == p1End)
			return p1End;

		pTemp = pTemp1;
		pCurrent = p1Begin;
		if (++pCurrent == p1End)
			return p1End;

		while (*pCurrent == *pTemp) {
			if (++pTemp == p2End)
				return p1Begin;
			if (++pCurrent == p1End)
				return p1End;
		}

		++p1Begin;
	}

	return p1Begin;
}

// CharTypeStringRSearch
// Specialized value_type version of STL find_end() function (which really is a reverse search function).
// Purpose: find last instance of p2 within p1. Return p1End if not found or if either string is zero length.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringRSearch(
	const value_type* p1Begin,
	const value_type* p1End,
	const value_type* p2Begin,
	const value_type* p2End) {
	// Test for zero length strings, in which case we have a match or a failure,
	// but the return value is the same either way.
	if ((p1Begin == p1End) || (p2Begin == p2End))
		return p1Begin;

	// Test for a pattern of length 1.
	if ((p2Begin + 1) == p2End)
		return CharTypeStringFindEnd(p1Begin, p1End, *p2Begin);

	// Test for search string length being longer than string length.
	if ((p2End - p2Begin) > (p1End - p1Begin))
		return p1End;

	// General case.
	const value_type* pSearchEnd = (p1End - (p2End - p2Begin) + 1);
	const value_type* pCurrent1;
	const value_type* pCurrent2;

	while (pSearchEnd != p1Begin) {
		// Search for the last occurrence of *p2Begin.
		pCurrent1 = CharTypeStringFindEnd(p1Begin, pSearchEnd, *p2Begin);
		if (pCurrent1 == pSearchEnd)// If the first char of p2 wasn't found,
			return p1End;			// then we immediately have failure.

		// In this case, *pTemp == *p2Begin. So compare the rest.
		pCurrent2 = p2Begin;
		while (*pCurrent1++ == *pCurrent2++) {
			if (pCurrent2 == p2End)
				return (pCurrent1 - (p2End - p2Begin));
		}

		// A smarter algorithm might know to subtract more than just one,
		// but in most cases it won't make much difference anyway.
		--pSearchEnd;
	}

	return p1End;
}

// CharTypeStringFindFirstOf
// Specialized value_type version of STL find_first_of() function.
// This function is much like the C runtime strtok function, except the strings aren't null-terminated.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringFindFirstOf(
	const value_type* p1Begin,
	const value_type* p1End,
	const value_type* p2Begin,
	const value_type* p2End) {
	for (; p1Begin != p1End; ++p1Begin) {
		for (const value_type* pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*p1Begin == *pTemp)
				return p1Begin;
		}
	}
	return p1End;
}

// CharTypeStringRFindFirstOf
// Specialized value_type version of STL find_first_of() function in reverse.
// This function is much like the C runtime strtok function, except the strings aren't null-terminated.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringRFindFirstOf(
	const value_type* p1RBegin,
	const value_type* p1REnd,
	const value_type* p2Begin,
	const value_type* p2End) {
	for (; p1RBegin != p1REnd; --p1RBegin) {
		for (const value_type* pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*(p1RBegin - 1) == *pTemp)
				return p1RBegin;
		}
	}
	return p1REnd;
}

// CharTypeStringFindFirstNotOf
// Specialized value_type version of STL find_first_not_of() function.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringFindFirstNotOf(
	const value_type* p1Begin,
	const value_type* p1End,
	const value_type* p2Begin,
	const value_type* p2End) {
	for (; p1Begin != p1End; ++p1Begin) {
		const value_type* pTemp;
		for (pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*p1Begin == *pTemp)
				break;
		}
		if (pTemp == p2End)
			return p1Begin;
	}
	return p1End;
}

// CharTypeStringRFindFirstNotOf
// Specialized value_type version of STL find_first_not_of() function in reverse.
template<typename T, typename Allocator>
const typename basic_string<T, Allocator>::value_type* basic_string<T, Allocator>::CharTypeStringRFindFirstNotOf(
	const value_type* p1RBegin,
	const value_type* p1REnd,
	const value_type* p2Begin,
	const value_type* p2End) {
	for (; p1RBegin != p1REnd; --p1RBegin) {
		const value_type* pTemp;
		for (pTemp = p2Begin; pTemp != p2End; ++pTemp) {
			if (*(p1RBegin - 1) == *pTemp)
				break;
		}
		if (pTemp == p2End)
			return p1RBegin;
	}
	return p1REnd;
}

// iterator operators
template<typename T, typename Allocator>
inline bool operator==(const typename basic_string<T, Allocator>::reverse_iterator& r1,
					   const typename basic_string<T, Allocator>::reverse_iterator& r2) {
	return r1.mpCurrent == r2.mpCurrent;
}

template<typename T, typename Allocator>
inline bool operator!=(const typename basic_string<T, Allocator>::reverse_iterator& r1,
					   const typename basic_string<T, Allocator>::reverse_iterator& r2) {
	return r1.mpCurrent != r2.mpCurrent;
}

// Operator +
template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	typedef typename basic_string<T, Allocator>::CtorDoNotInitialize CtorDoNotInitialize;
	CtorDoNotInitialize cDNI;// GCC 2.x forces us to declare a named temporary like this.
	basic_string<T, Allocator> result(
		cDNI, a.size() + b.size(),
		const_cast<basic_string<T, Allocator>&>(a).get_allocator());// Note that we choose to assign a's allocator.
	result.append(a);
	result.append(b);
	return result;
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(const typename basic_string<T, Allocator>::value_type* p,
									 const basic_string<T, Allocator>& b) {
	typedef typename basic_string<T, Allocator>::CtorDoNotInitialize CtorDoNotInitialize;
	CtorDoNotInitialize cDNI;// GCC 2.x forces us to declare a named temporary like this.
	const typename basic_string<T, Allocator>::size_type n =
		(typename basic_string<T, Allocator>::size_type)CharStrlen(p);
	basic_string<T, Allocator> result(cDNI, n + b.size(),
									  const_cast<basic_string<T, Allocator>&>(b).get_allocator());
	result.append(p, p + n);
	result.append(b);
	return result;
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(typename basic_string<T, Allocator>::value_type c,
									 const basic_string<T, Allocator>& b) {
	typedef typename basic_string<T, Allocator>::CtorDoNotInitialize CtorDoNotInitialize;
	CtorDoNotInitialize cDNI;// GCC 2.x forces us to declare a named temporary like this.
	basic_string<T, Allocator> result(cDNI, 1 + b.size(),
									  const_cast<basic_string<T, Allocator>&>(b).get_allocator());
	result.push_back(c);
	result.append(b);
	return result;
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(const basic_string<T, Allocator>& a,
									 const typename basic_string<T, Allocator>::value_type* p) {
	typedef typename basic_string<T, Allocator>::CtorDoNotInitialize CtorDoNotInitialize;
	CtorDoNotInitialize cDNI;// GCC 2.x forces us to declare a named temporary like this.
	const typename basic_string<T, Allocator>::size_type n =
		(typename basic_string<T, Allocator>::size_type)CharStrlen(p);
	basic_string<T, Allocator> result(cDNI, a.size() + n,
									  const_cast<basic_string<T, Allocator>&>(a).get_allocator());
	result.append(a);
	result.append(p, p + n);
	return result;
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(const basic_string<T, Allocator>& a,
									 typename basic_string<T, Allocator>::value_type c) {
	typedef typename basic_string<T, Allocator>::CtorDoNotInitialize CtorDoNotInitialize;
	CtorDoNotInitialize cDNI;// GCC 2.x forces us to declare a named temporary like this.
	basic_string<T, Allocator> result(cDNI, a.size() + 1,
									  const_cast<basic_string<T, Allocator>&>(a).get_allocator());
	result.append(a);
	result.push_back(c);
	return result;
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(basic_string<T, Allocator>&& a, basic_string<T, Allocator>&& b) {
	a.append(b);// Using an rvalue by name results in it becoming an lvalue.
	return std::move(a);
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(basic_string<T, Allocator>&& a, const basic_string<T, Allocator>& b) {
	a.append(b);
	return std::move(a);
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(const typename basic_string<T, Allocator>::value_type* p,
									 basic_string<T, Allocator>&& b) {
	b.insert(0, p);
	return std::move(b);
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(basic_string<T, Allocator>&& a,
									 const typename basic_string<T, Allocator>::value_type* p) {
	a.append(p);
	return std::move(a);
}

template<typename T, typename Allocator>
basic_string<T, Allocator> operator+(basic_string<T, Allocator>&& a,
									 typename basic_string<T, Allocator>::value_type c) {
	a.push_back(c);
	return std::move(a);
}

template<typename T, typename Allocator>
inline bool basic_string<T, Allocator>::validate() const noexcept {
	if ((internalLayout().BeginPtr() == nullptr) || (internalLayout().EndPtr() == nullptr))
		return false;
	if (internalLayout().EndPtr() < internalLayout().BeginPtr())
		return false;
	if (internalLayout().CapacityPtr() < internalLayout().EndPtr())
		return false;
	if (*internalLayout().EndPtr() != 0)
		return false;
	return true;
}

template<typename T, typename Allocator>
inline int basic_string<T, Allocator>::validate_iterator(const_iterator i) const noexcept {
	if (i >= internalLayout().BeginPtr()) {
		if (i < internalLayout().EndPtr())
			return (isf_valid | isf_current | isf_can_dereference);

		if (i <= internalLayout().EndPtr())
			return (isf_valid | isf_current);
	}

	return isf_none;
}

///////////////////////////////////////////////////////////////////////
// global operators
///////////////////////////////////////////////////////////////////////

// Operator== and operator!=
template<typename T, typename Allocator>
inline bool operator==(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return ((a.size() == b.size()) && (memcmp(a.data(), b.data(), (size_t)a.size() * sizeof(typename basic_string<T, Allocator>::value_type)) == 0));
}

template<typename T, typename Allocator>
inline bool operator==(const typename basic_string<T, Allocator>::value_type* p,
					   const basic_string<T, Allocator>& b) {
	typedef typename basic_string<T, Allocator>::size_type size_type;
	const size_type n = (size_type)CharStrlen(p);
	return ((n == b.size()) && (memcmp(p, b.data(), (size_t)n * sizeof(*p)) == 0));
}

template<typename T, typename Allocator>
inline bool operator==(const basic_string<T, Allocator>& a,
					   const typename basic_string<T, Allocator>::value_type* p) {
	typedef typename basic_string<T, Allocator>::size_type size_type;
	const size_type n = (size_type)CharStrlen(p);
	return ((a.size() == n) && (memcmp(a.data(), p, (size_t)n * sizeof(*p)) == 0));
}

template<typename T, typename Allocator>
inline bool operator!=(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return !(a == b);
}

template<typename T, typename Allocator>
inline bool operator!=(const typename basic_string<T, Allocator>::value_type* p,
					   const basic_string<T, Allocator>& b) {
	return !(p == b);
}

template<typename T, typename Allocator>
inline bool operator!=(const basic_string<T, Allocator>& a,
					   const typename basic_string<T, Allocator>::value_type* p) {
	return !(a == p);
}

// Operator< (and also >, <=, and >=).
template<typename T, typename Allocator>
inline bool operator<(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return basic_string<T, Allocator>::compare(a.begin(), a.end(), b.begin(), b.end()) < 0;
}

template<typename T, typename Allocator>
inline bool operator<(const typename basic_string<T, Allocator>::value_type* p, const basic_string<T, Allocator>& b) {
	typedef typename basic_string<T, Allocator>::size_type size_type;
	const size_type n = (size_type)CharStrlen(p);
	return basic_string<T, Allocator>::compare(p, p + n, b.begin(), b.end()) < 0;
}

template<typename T, typename Allocator>
inline bool operator<(const basic_string<T, Allocator>& a, const typename basic_string<T, Allocator>::value_type* p) {
	typedef typename basic_string<T, Allocator>::size_type size_type;
	const size_type n = (size_type)CharStrlen(p);
	return basic_string<T, Allocator>::compare(a.begin(), a.end(), p, p + n) < 0;
}

template<typename T, typename Allocator>
inline bool operator>(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return b < a;
}

template<typename T, typename Allocator>
inline bool operator>(const typename basic_string<T, Allocator>::value_type* p, const basic_string<T, Allocator>& b) {
	return b < p;
}

template<typename T, typename Allocator>
inline bool operator>(const basic_string<T, Allocator>& a, const typename basic_string<T, Allocator>::value_type* p) {
	return p < a;
}

template<typename T, typename Allocator>
inline bool operator<=(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return !(b < a);
}

template<typename T, typename Allocator>
inline bool operator<=(const typename basic_string<T, Allocator>::value_type* p,
					   const basic_string<T, Allocator>& b) {
	return !(b < p);
}

template<typename T, typename Allocator>
inline bool operator<=(const basic_string<T, Allocator>& a,
					   const typename basic_string<T, Allocator>::value_type* p) {
	return !(p < a);
}

template<typename T, typename Allocator>
inline bool operator>=(const basic_string<T, Allocator>& a, const basic_string<T, Allocator>& b) {
	return !(a < b);
}

template<typename T, typename Allocator>
inline bool operator>=(const typename basic_string<T, Allocator>::value_type* p,
					   const basic_string<T, Allocator>& b) {
	return !(p < b);
}

template<typename T, typename Allocator>
inline bool operator>=(const basic_string<T, Allocator>& a,
					   const typename basic_string<T, Allocator>::value_type* p) {
	return !(a < p);
}

using string = basic_string<char>;
using wstring = basic_string<wchar_t>;
template<typename T>
class basic_string_view {
public:
	typedef basic_string_view<T> this_type;
	typedef T value_type;
	typedef T* pointer;
	typedef const T* const_pointer;
	typedef T& reference;
	typedef const T& const_reference;
	typedef T* iterator;
	typedef const T* const_iterator;
	typedef std::reverse_iterator<iterator> reverse_iterator;
	typedef std::reverse_iterator<const_iterator> const_reverse_iterator;
	typedef size_t size_type;
	typedef ptrdiff_t difference_type;

	static const constexpr size_type npos = size_type(-1);

protected:
	const_pointer mpBegin = nullptr;
	size_type mnCount = 0;

public:
	// 21.4.2.1, construction and assignment
	constexpr basic_string_view() noexcept : mpBegin(nullptr), mnCount(0) {}
	constexpr basic_string_view(const basic_string_view& other) noexcept = default;

	constexpr basic_string_view(const T* s, size_type count) : mpBegin(s), mnCount(count) {}
	constexpr basic_string_view(const T* s, const T* e) : mpBegin(s), mnCount((size_t)(e - s)) {}
	constexpr basic_string_view(const T* s) : mpBegin(s), mnCount(s != nullptr ? CharStrlen(s) : 0) {}
	basic_string_view& operator=(const basic_string_view& view) = default;

	// 21.4.2.2, iterator support
	constexpr const_iterator begin() const noexcept { return mpBegin; }
	constexpr const_iterator cbegin() const noexcept { return mpBegin; }
	constexpr const_iterator end() const noexcept { return mpBegin + mnCount; }
	constexpr const_iterator cend() const noexcept { return mpBegin + mnCount; }
	constexpr const_reverse_iterator rbegin() const noexcept { return const_reverse_iterator(mpBegin + mnCount); }
	constexpr const_reverse_iterator crbegin() const noexcept { return const_reverse_iterator(mpBegin + mnCount); }
	constexpr const_reverse_iterator rend() const noexcept { return const_reverse_iterator(mpBegin); }
	constexpr const_reverse_iterator crend() const noexcept { return const_reverse_iterator(mpBegin); }

	// 21.4.2.4, element access
	constexpr const_pointer data() const { return mpBegin; }
	constexpr const_reference front() const {
		return [&] { EASTL_ASSERT_MSG(!empty(), "behavior is undefined if string_view is empty"); }(), mpBegin[0];
	}

	constexpr const_reference back() const {
		return [&] { EASTL_ASSERT_MSG(!empty(), "behavior is undefined if string_view is empty"); }(), mpBegin[mnCount - 1];
	}
	constexpr const_reference operator[](size_type pos) const {
		// As per the standard spec: No bounds checking is performed: the behavior is undefined if pos >= size().
		return mpBegin[pos];
	}

	constexpr const_reference at(size_type pos) const {
#if EASTL_EXCEPTIONS_ENABLED
		if (EASTL_UNLIKELY(pos >= mnCount))
			throw std::out_of_range("string_view::at -- out of range");
#elif EASTL_ASSERT_ENABLED
		if (EASTL_UNLIKELY(pos >= mnCount))
			EASTL_FAIL_MSG("string_view::at -- out of range");
#endif

		return mpBegin[pos];
	}

	// 21.4.2.3, capacity
	constexpr size_type size() const noexcept { return mnCount; }
	constexpr size_type length() const noexcept { return mnCount; }

	// avoid macro expansion of max(...) from windows headers (potentially included before this file)
	// by wrapping function name in brackets
	constexpr size_type max_size() const noexcept { return (std::numeric_limits<size_type>::max)(); }
	constexpr bool empty() const noexcept { return mnCount == 0; }

	// 21.4.2.5, modifiers
	constexpr void swap(basic_string_view& v) {
		std::swap(mpBegin, v.mpBegin);
		std::swap(mnCount, v.mnCount);
	}

	constexpr void remove_prefix(size_type n) {
		EASTL_ASSERT_MSG(n <= mnCount, "behavior is undefined if moving past the end of the string");
		mpBegin += n;
		mnCount -= n;
	}

	constexpr void remove_suffix(size_type n) {
		EASTL_ASSERT_MSG(n <= mnCount, "behavior is undefined if moving past the beginning of the string");
		mnCount -= n;
	}

	// 21.4.2.6, string operations
	size_type copy(T* pDestination, size_type count, size_type pos = 0) const {
#if EASTL_EXCEPTIONS_ENABLED
		if (EASTL_UNLIKELY(pos > mnCount))
			throw std::out_of_range("string_view::copy -- out of range");
#elif EASTL_ASSERT_ENABLED
		if (EASTL_UNLIKELY(pos > mnCount))
			EASTL_FAIL_MSG("string_view::copy -- out of range");
#endif

		count = std::min<size_type>(count, mnCount - pos);
		auto* pResult = CharStringUninitializedCopy(mpBegin + pos, mpBegin + pos + count, pDestination);
		// *pResult = 0; // don't write the null-terminator
		return pResult - pDestination;
	}

	constexpr basic_string_view substr(size_type pos = 0, size_type count = npos) const {
#if EASTL_EXCEPTIONS_ENABLED
		if (EASTL_UNLIKELY(pos > mnCount))
			throw std::out_of_range("string_view::substr -- out of range");
#elif EASTL_ASSERT_ENABLED
		if (EASTL_UNLIKELY(pos > mnCount))
			EASTL_FAIL_MSG("string_view::substr -- out of range");
#endif

		count = std::min<size_type>(count, mnCount - pos);
		return this_type(mpBegin + pos, count);
	}

	static constexpr int compare(const T* pBegin1, const T* pEnd1, const T* pBegin2, const T* pEnd2) {
		const ptrdiff_t n1 = pEnd1 - pBegin1;
		const ptrdiff_t n2 = pEnd2 - pBegin2;
		const ptrdiff_t nMin = std::min(n1, n2);
		const int cmp = Compare(pBegin1, pBegin2, (size_type)nMin);

		return (cmp != 0 ? cmp : (n1 < n2 ? -1 : (n1 > n2 ? 1 : 0)));
	}

	constexpr int compare(basic_string_view sw) const noexcept {
		return compare(mpBegin, mpBegin + mnCount, sw.mpBegin, sw.mpBegin + sw.mnCount);
	}

	constexpr int compare(size_type pos1, size_type count1, basic_string_view sw) const {
		return substr(pos1, count1).compare(sw);
	}

	constexpr int compare(size_type pos1,
						  size_type count1,
						  basic_string_view sw,
						  size_type pos2,
						  size_type count2) const {
		return substr(pos1, count1).compare(sw.substr(pos2, count2));
	}

	constexpr int compare(const T* s) const { return compare(basic_string_view(s)); }

	constexpr int compare(size_type pos1, size_type count1, const T* s) const {
		return substr(pos1, count1).compare(basic_string_view(s));
	}

	constexpr int compare(size_type pos1, size_type count1, const T* s, size_type count2) const {
		return substr(pos1, count1).compare(basic_string_view(s, count2));
	}

	constexpr size_type find(basic_string_view sw, size_type pos = 0) const noexcept {
		auto* pEnd = mpBegin + mnCount;
		if (EASTL_LIKELY(((npos - sw.size()) >= pos) && (pos + sw.size()) <= mnCount)) {
			const value_type* const pTemp = std::search(mpBegin + pos, pEnd, sw.data(), sw.data() + sw.size());

			if ((pTemp != pEnd) || (sw.size() == 0))
				return (size_type)(pTemp - mpBegin);
		}
		return npos;
	}

	constexpr size_type find(T c, size_type pos = 0) const noexcept {
		return find(basic_string_view(&c, 1), pos);
	}

	constexpr size_type find(const T* s, size_type pos, size_type count) const {
		return find(basic_string_view(s, count), pos);
	}

	constexpr size_type find(const T* s, size_type pos = 0) const { return find(basic_string_view(s), pos); }

	constexpr size_type rfind(basic_string_view sw, size_type pos = npos) const noexcept {
		return rfind(sw.mpBegin, pos, sw.mnCount);
	}

	constexpr size_type rfind(T c, size_type pos = npos) const noexcept {
		if (EASTL_LIKELY(mnCount)) {
			const value_type* const pEnd = mpBegin + std::min(mnCount - 1, pos) + 1;
			const value_type* const pResult = CharTypeStringRFind(pEnd, mpBegin, c);

			if (pResult != mpBegin)
				return (size_type)((pResult - 1) - mpBegin);
		}
		return npos;
	}

	constexpr size_type rfind(const T* s, size_type pos, size_type n) const {
		// Disabled because it's not clear what values are valid for position.
		// It is documented that npos is a valid value, though. We return npos and
		// don't crash if postion is any invalid value.
		//#if EASTL_ASSERT_ENABLED
		//    if(EASTL_UNLIKELY((position != npos) && (position > (size_type)(mpEnd - mpBegin))))
		//        EASTL_FAIL_MSG("basic_string::rfind -- invalid position");
		//#endif

		// Note that a search for a zero length string starting at position = end() returns end() and not npos.
		// Note by Paul Pedriana: I am not sure how this should behave in the case of n == 0 and position > size.
		// The standard seems to suggest that rfind doesn't act exactly the same as find in that input position
		// can be > size and the return value can still be other than npos. Thus, if n == 0 then you can
		// never return npos, unlike the case with find.
		if (EASTL_LIKELY(n <= mnCount)) {
			if (EASTL_LIKELY(n)) {
				const const_iterator pEnd = mpBegin + std::min(mnCount - n, pos) + n;
				const const_iterator pResult = CharTypeStringRSearch(mpBegin, pEnd, s, s + n);

				if (pResult != pEnd) {
					return (size_type)(pResult - mpBegin);
				}
			} else
				return std::min(mnCount, pos);
		}
		return npos;
	}

	constexpr size_type rfind(const T* s, size_type pos = npos) const {
		return rfind(s, pos, (size_type)CharStrlen(s));
	}

	constexpr size_type find_first_of(basic_string_view sw, size_type pos = 0) const noexcept {
		return find_first_of(sw.mpBegin, pos, sw.mnCount);
	}

	constexpr size_type find_first_of(T c, size_type pos = 0) const noexcept { return find(c, pos); }

	constexpr size_type find_first_of(const T* s, size_type pos, size_type n) const {
		// If position is >= size, we return npos.
		if (EASTL_LIKELY((pos < mnCount))) {
			const value_type* const pBegin = mpBegin + pos;
			const value_type* const pEnd = mpBegin + mnCount;
			const const_iterator pResult = CharTypeStringFindFirstOf(pBegin, pEnd, s, s + n);

			if (pResult != pEnd)
				return (size_type)(pResult - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_first_of(const T* s, size_type pos = 0) const {
		return find_first_of(s, pos, (size_type)CharStrlen(s));
	}

	constexpr size_type find_last_of(basic_string_view sw, size_type pos = npos) const noexcept {
		return find_last_of(sw.mpBegin, pos, sw.mnCount);
	}

	constexpr size_type find_last_of(T c, size_type pos = npos) const noexcept { return rfind(c, pos); }

	constexpr size_type find_last_of(const T* s, size_type pos, size_type n) const {
		// If n is zero or position is >= size, we return npos.
		if (EASTL_LIKELY(mnCount)) {
			const value_type* const pEnd = mpBegin + std::min(mnCount - 1, pos) + 1;
			const value_type* const pResult = CharTypeStringRFindFirstOf(pEnd, mpBegin, s, s + n);

			if (pResult != mpBegin)
				return (size_type)((pResult - 1) - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_last_of(const T* s, size_type pos = npos) const {
		return find_last_of(s, pos, (size_type)CharStrlen(s));
	}

	constexpr size_type find_first_not_of(basic_string_view sw, size_type pos = 0) const noexcept {
		return find_first_not_of(sw.mpBegin, pos, sw.mnCount);
	}

	constexpr size_type find_first_not_of(T c, size_type pos = 0) const noexcept {
		if (EASTL_LIKELY(pos <= mnCount)) {
			const auto pEnd = mpBegin + mnCount;
			// Todo: Possibly make a specialized version of CharTypeStringFindFirstNotOf(pBegin, pEnd, c).
			const const_iterator pResult = CharTypeStringFindFirstNotOf(mpBegin + pos, pEnd, &c, &c + 1);

			if (pResult != pEnd)
				return (size_type)(pResult - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_first_not_of(const T* s, size_type pos, size_type n) const {
		if (EASTL_LIKELY(pos <= mnCount)) {
			const auto pEnd = mpBegin + mnCount;
			const const_iterator pResult = CharTypeStringFindFirstNotOf(mpBegin + pos, pEnd, s, s + n);

			if (pResult != pEnd)
				return (size_type)(pResult - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_first_not_of(const T* s, size_type pos = 0) const {
		return find_first_not_of(s, pos, (size_type)CharStrlen(s));
	}

	constexpr size_type find_last_not_of(basic_string_view sw, size_type pos = npos) const noexcept {
		return find_last_not_of(sw.mpBegin, pos, sw.mnCount);
	}

	constexpr size_type find_last_not_of(T c, size_type pos = npos) const noexcept {
		if (EASTL_LIKELY(mnCount)) {
			// Todo: Possibly make a specialized version of CharTypeStringRFindFirstNotOf(pBegin, pEnd, c).
			const value_type* const pEnd = mpBegin + std::min(mnCount - 1, pos) + 1;
			const value_type* const pResult = CharTypeStringRFindFirstNotOf(pEnd, mpBegin, &c, &c + 1);

			if (pResult != mpBegin)
				return (size_type)((pResult - 1) - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_last_not_of(const T* s, size_type pos, size_type n) const {
		if (EASTL_LIKELY(mnCount)) {
			const value_type* const pEnd = mpBegin + std::min(mnCount - 1, pos) + 1;
			const value_type* const pResult = CharTypeStringRFindFirstNotOf(pEnd, mpBegin, s, s + n);

			if (pResult != mpBegin)
				return (size_type)((pResult - 1) - mpBegin);
		}
		return npos;
	}

	constexpr size_type find_last_not_of(const T* s, size_type pos = npos) const {
		return find_last_not_of(s, pos, (size_type)CharStrlen(s));
	}

	// starts_with
	constexpr bool starts_with(basic_string_view x) const noexcept {
		return (size() >= x.size()) && (compare(0, x.size(), x) == 0);
	}

	constexpr bool starts_with(T x) const noexcept {
		return starts_with(basic_string_view(&x, 1));
	}

	constexpr bool starts_with(const T* s) const {
		return starts_with(basic_string_view(s));
	}

	// ends_with
	constexpr bool ends_with(basic_string_view x) const noexcept {
		return (size() >= x.size()) && (compare(size() - x.size(), npos, x) == 0);
	}

	constexpr bool ends_with(T x) const noexcept {
		return ends_with(basic_string_view(&x, 1));
	}

	constexpr bool ends_with(const T* s) const {
		return ends_with(basic_string_view(s));
	}
};
template<typename T, class Traits, typename Allocator>
inline std::basic_ostream<T, Traits>& operator<<(std::basic_ostream<T, Traits>& out, const basic_string<T, Allocator>& obj) noexcept {
	for (auto&& i : obj) {
		out << i;
	}
	return out;
}
template<typename T, class Traits>
inline std::basic_ostream<T, Traits>& operator<<(std::basic_ostream<T, Traits>& out, const basic_string_view<T>& obj) noexcept {
	for (auto&& i : obj) {
		out << i;
	}
	return out;
}
template<typename T, class Traits, typename Allocator>
inline std::basic_istream<T, Traits>& operator>>(std::basic_istream<T, Traits>& in, basic_string<T, Allocator>& obj) noexcept {
	char cArr[1025];
	in.getline(cArr, 1024);
	obj = cArr;
	return in;
}
// global operators

template<class CharT>
inline constexpr bool operator==(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return (lhs.size() == rhs.size()) && (lhs.compare(rhs) == 0);
}
template<class CharT>
inline constexpr bool operator==(std::decay_t<basic_string_view<CharT>> lhs, basic_string_view<CharT> rhs) noexcept {
	return (lhs.size() == rhs.size()) && (lhs.compare(rhs) == 0);
}
template<class CharT>
inline constexpr bool operator==(basic_string_view<CharT> lhs, std::decay_t<basic_string_view<CharT>> rhs) noexcept {
	return (lhs.size() == rhs.size()) && (lhs.compare(rhs) == 0);
}
template<class CharT>
inline constexpr bool operator==(basic_string_view<CharT> lhs, CharT const* v) noexcept {
	return operator==(lhs, basic_string_view<CharT>{v, CharStrlen(v)});
}
template<class CharT>
inline constexpr bool operator!=(basic_string_view<CharT> lhs, CharT const* v) noexcept {
	return !operator==(lhs, v);
}
template<class CharT>
inline constexpr bool operator==(CharT const* v, basic_string_view<CharT> lhs) noexcept {
	return operator==(lhs, basic_string_view<CharT>{v, CharStrlen(v)});
}
template<class CharT>
inline constexpr bool operator!=(CharT const* v, basic_string_view<CharT> lhs) noexcept {
	return !operator==(v, lhs);
}

template<class CharT>
inline constexpr bool operator==(std::decay_t<basic_string_view<CharT>> lhs, std::decay_t<basic_string_view<CharT>> rhs) noexcept {
	return (lhs.size() == rhs.size()) && (lhs.compare(rhs) == 0);
}
template<class CharT>
inline constexpr bool operator!=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return !(lhs == rhs);
}
template<class CharT>
inline constexpr bool operator<(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return lhs.compare(rhs) < 0;
}

template<class CharT>
inline constexpr bool operator<=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return !(rhs < lhs);
}

template<class CharT>
inline constexpr bool operator>(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return rhs < lhs;
}

template<class CharT>
inline constexpr bool operator>=(basic_string_view<CharT> lhs, basic_string_view<CharT> rhs) noexcept {
	return !(lhs < rhs);
}

// string_view / wstring_view
typedef basic_string_view<char> string_view;
typedef basic_string_view<wchar_t> wstring_view;

// C++17 string types
typedef basic_string_view<char8_t> u8string_view;// C++20 feature, but always present for consistency.
typedef basic_string_view<char16_t> u16string_view;
typedef basic_string_view<char32_t> u32string_view;

template<class Elem, class UTy>
Elem* UIntegral_to_buff(Elem* RNext, UTy UVal) noexcept {// format UVal into buffer *ending at* RNext
	static_assert(std::is_unsigned_v<UTy>, "UTy must be unsigned");
	auto UVal_trunc = UVal;
	do {
		*--RNext = static_cast<Elem>('0' + UVal_trunc % 10);
		UVal_trunc /= 10;
	} while (UVal_trunc != 0);
	return RNext;
}

template<class Ty, class CharT, class Str>
inline void IntegerToString(const Ty Val, Str& str, bool negative = false) noexcept {// convert Val to string
	static_assert(std::is_integral_v<Ty>, "_Ty must be integral");
	using UTy = std::make_unsigned_t<Ty>;
	CharT Buff[21];// can hold -2^63 and 2^64 - 1, plus NUL
	CharT* const Buff_end = std::end(Buff);
	CharT* RNext = Buff_end;
	const auto UVal = static_cast<UTy>(Val);
	if (Val < 0 || negative) {
		RNext = UIntegral_to_buff(RNext, static_cast<UTy>(0 - UVal));
		*--RNext = '-';
	} else {
		RNext = UIntegral_to_buff(RNext, UVal);
	}
	str.append(RNext, Buff_end - RNext);
}
template<class Ty, class CharT>
inline string IntegerToString(const Ty Val) noexcept {// convert Val to string
	string s;
	IntegerToString<Ty, CharT, string>(Val, s);
	return s;
}
template<class Ty, class CharT>
inline wstring IntegerToWString(const Ty Val) noexcept {// convert Val to string
	wstring s;
	IntegerToString<Ty, CharT, wstring>(Val, s);
	return s;
}

inline void to_string(double Val, string& str) noexcept {
	int64 v = (int64)Val;
	IntegerToString<int64, char, string>(v, str, Val < 0);
	Val -= v;
	Val = abs(Val);
	char tempArr[12];
	str.push_back('.');
	for (auto i : range(12)) {
		Val *= 10;
		char x = (char)Val;
		Val -= x;
		tempArr[i] = (char)(x + 48);
	}
	size_t cullSize = 12;
	for (auto&& i : ptr_range(tempArr + 11, tempArr - 1, -1)) {
		if (i != '0') break;
		cullSize--;
	}
	str.append(tempArr, cullSize);
}

inline void to_wstring(double Val, wstring& str) noexcept {
	int64 v = (int64)Val;
	IntegerToString<int64, wchar_t, wstring>(v, str, Val < 0);
	Val -= v;
	Val = abs(Val);
	wchar_t tempArr[12];
	str.push_back('.');
	for (auto i : range(12)) {
		Val *= 10;
		wchar_t x = (wchar_t)Val;
		Val -= x;
		tempArr[i] = (wchar_t)(x + 48);
	}
	size_t cullSize = 12;
	for (auto&& i : ptr_range(tempArr + 11, tempArr - 1, -1)) {
		if (i != '0') break;
		cullSize--;
	}
	str.append(tempArr, cullSize);
}
inline string to_string(double Val) noexcept {
	string str;
	to_string(Val, str);
	return str;
}
inline wstring to_wstring(double Val) noexcept {
	wstring str;
	to_wstring(Val, str);
	return str;
}

namespace detail {

template<size_t size, bool is_signed>
struct make_integer {};

template<bool is_signed>
struct make_integer<1u, is_signed> {
	using type = std::conditional_t<is_signed, int8_t, vbyte>;
};

template<bool is_signed>
struct make_integer<2u, is_signed> {
	using type = std::conditional_t<is_signed, int16_t, uint16_t>;
};

template<bool is_signed>
struct make_integer<4u, is_signed> {
	using type = std::conditional_t<is_signed, int32_t, uint32_t>;
};

template<bool is_signed>
struct make_integer<8u, is_signed> {
	using type = std::conditional_t<is_signed, int64_t, uint64_t>;
};

}// namespace detail

template<typename T>
using canonical_integer_t = typename detail::make_integer<sizeof(T), std::is_signed_v<T>>::type;

template<typename T>
requires(std::is_integral_v<T>)
	[[nodiscard]] inline auto to_string(T val) noexcept {
	return IntegerToString<canonical_integer_t<T>, char>(static_cast<canonical_integer_t<T>>(val));
}
template<typename T>
requires(std::is_integral_v<T>) inline void to_string(T Val, string& str) noexcept {
	IntegerToString<canonical_integer_t<T>, char>(static_cast<canonical_integer_t<T>>(Val), str);
}
inline void to_string(float Val, string& str) noexcept {
	to_string((double)Val, str);
	str += 'f';
}
inline string to_string(float Val) noexcept {
	string str;
	to_string(Val, str);
	return str;
}
template<typename T>
requires(std::is_integral_v<T>)
	[[nodiscard]] inline auto to_wstring(T val) noexcept {
	return IntegerToWString<canonical_integer_t<T>, wchar_t>(static_cast<canonical_integer_t<T>>(val));
}
template<typename T>
requires(std::is_integral_v<T>) inline void to_wstring(T Val, wstring& str) noexcept {
	IntegerToWString<canonical_integer_t<T>, wchar_t>(static_cast<canonical_integer_t<T>>(Val), str);
}
inline void to_wstring(float Val, wstring& str) noexcept {
	to_wstring((double)Val, str);
	str += 'f';
}
inline wstring to_wstring(float Val) noexcept {
	wstring str;
	to_wstring(Val, str);
	return str;
}

template<typename T>
struct hash<basic_string_view<T>> {
	inline size_t operator()(const basic_string_view<T>& str) const noexcept {
		return Hash::CharArrayHash(reinterpret_cast<const char*>(str.data()), str.size() * sizeof(T));
	}
};
template<typename T, typename Allocator>
struct hash<basic_string<T, Allocator>> {
	inline size_t operator()(const basic_string<T, Allocator>& str) const noexcept {
		return hash<basic_string_view<T>>()(str);
	}
};
template<typename T, typename Allocator>
struct compare<basic_string<T, Allocator>> {
	using Type = basic_string<T, Allocator>;
	using ViewType = basic_string_view<T>;
	int32_t compareFunc(Type const& a, T const* ptr, size_t size) const noexcept {
		if (a.size() == size)
			return memcmp(a.data(), ptr, size * sizeof(T));
		else
			return (a.size() > size) ? 1 : -1;
	}
	int32 operator()(Type const& a, T const* ptr, size_t size) const noexcept {
		return compareFunc(a, ptr, size);
	}
	int32 operator()(Type const& a, Type const& b) const noexcept {
		return compareFunc(a, b.data(), b.size());
	}
	int32 operator()(Type const& a, basic_string_view<T> const& b) const noexcept {
		return compareFunc(a, b.data(), b.size());
	}
	int32 operator()(Type const& a, T const* ptr) const noexcept {
		return compareFunc(a, ptr, CharStrlen(ptr));
	}
};
template<typename T>
struct compare<basic_string_view<T>> {
	using Type = basic_string_view<T>;
	using ViewType = basic_string_view<T>;
	int32 compareFunc(Type const& a, T const* ptr, size_t size) const noexcept {
		if (a.size() == size)
			return memcmp(a.data(), ptr, size * sizeof(T));
		else
			return (a.size() > size) ? 1 : -1;
	}
	int32 operator()(Type const& a, T const* ptr, size_t size) const noexcept {
		return compareFunc(a, ptr, size);
	}
	int32 operator()(Type const& a, Type const& b) const noexcept {
		return compareFunc(a, b.data(), b.size());
	}
	template<typename Allocator>
	int32 operator()(Type const& a, basic_string<T, Allocator> const& b) const noexcept {
		return compareFunc(a, b.data(), b.size());
	}
	int32 operator()(Type const& a, T const* ptr) const noexcept {
		return compareFunc(a, ptr, CharStrlen(ptr));
	}
};

template<>
struct hash<char const*> {
	inline size_t operator()(char const* ptr) const noexcept {
		auto sz = strlen(ptr);
		return Hash::CharArrayHash(ptr, sz);
	}
};
template<>
struct hash<char*> {
	inline size_t operator()(char* ptr) const noexcept {
		auto sz = strlen(ptr);
		return Hash::CharArrayHash(ptr, sz);
	}
};
template<size_t i>
struct hash<char[i]> {
	inline size_t operator()(char const* ptr) const noexcept {
		return Hash::CharArrayHash(ptr, i - 1);
	}
};
template<>
struct hash<wchar_t const*> {
	inline size_t operator()(wchar_t const* ptr) const noexcept {
		auto sz = CharStrlen(ptr);
		return Hash::CharArrayHash(reinterpret_cast<char const*>(ptr), sz);
	}
};
template<>
struct hash<wchar_t*> {
	inline size_t operator()(wchar_t* ptr) const noexcept {
		hash<wchar_t const*> hs;
		return hs(ptr);
	}
};
template<size_t i>
struct hash<wchar_t[i]> {
	inline size_t operator()(wchar_t const* ptr) const noexcept {
		return Hash::CharArrayHash(reinterpret_cast<char const*>(ptr), (i - 1) * 2);
	}
};
}// namespace vstd
template<typename T>
auto& operator<<(vstd::string& s, T&& v) noexcept {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, char>) {
		s += v;
		return s;
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, wchar_t>) {
		s += static_cast<char>(v);
		return s;
	} else {
		return s.append(std::forward<T>(v));
	}
}

template<typename T>
auto& operator<<(vstd::wstring& s, T&& v) noexcept {
	if constexpr (std::is_same_v<std::remove_cvref_t<T>, wchar_t>) {
		s += v;
		return s;
	} else if constexpr (std::is_same_v<std::remove_cvref_t<T>, char>) {
		s += static_cast<wchar_t>(v);
		return s;
	} else {
		return s.append(std::forward<T>(v));
	}
}
namespace vstd {

inline std::ostream& operator<<(std::ostream& out, const string_view& obj) noexcept {
	if (!obj.data()) return out;
	auto end = obj.data() + obj.size();
	for (auto i = obj.data(); i < end; ++i) {
		out << *i;
	}
	return out;
}

}// namespace vstd
inline bool operator==(vstd::string_view const& strv, char c) {
	return strv.size() == 1 && strv[0] == c;
}
inline bool operator==(char c, vstd::string_view const& strv) {
	return operator==(strv, c);
}
inline bool operator!=(vstd::string_view const& strv, char c) {
	return !operator==(strv, c);
}
inline bool operator!=(char c, vstd::string_view const& strv) {
	return !operator==(strv, c);
}

inline constexpr vstd::string_view operator"" _sv(const char* _Str, size_t _Len) noexcept {
	return vstd::string_view(_Str, _Len);
}

inline constexpr vstd::wstring_view operator"" _sv(const wchar_t* _Str, size_t _Len) noexcept {
	return vstd::wstring_view(_Str, _Len);
}

#ifdef __cpp_char8_t
inline constexpr vstd::basic_string_view<char8_t> operator"" _sv(const char8_t* _Str, size_t _Len) noexcept {
	return vstd::basic_string_view<char8_t>(_Str, _Len);
}
#endif// __cpp_char8_t

inline constexpr vstd::u16string_view operator"" _sv(const char16_t* _Str, size_t _Len) noexcept {
	return vstd::u16string_view(_Str, _Len);
}

inline constexpr vstd::u32string_view operator"" _sv(const char32_t* _Str, size_t _Len) noexcept {
	return vstd::u32string_view(_Str, _Len);
}