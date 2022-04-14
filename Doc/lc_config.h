#pragma once
#ifndef LC_INCLUDE_CONFIG_
#define LC_INCLUDE_CONFIG_
#define EA_HAVE_CPP11_CONTAINERS 1
#define EA_HAVE_CPP11_ATOMIC 1
#define EA_HAVE_CPP11_CONDITION_VARIABLE 1
#define EA_HAVE_CPP11_MUTEX 1
#define EA_HAVE_CPP11_THREAD 1
#define EA_HAVE_CPP11_FUTURE 1
#define EA_HAVE_CPP11_TYPE_TRAITS 1
#define EA_HAVE_CPP11_TUPLES 1
#define EA_HAVE_CPP11_REGEX 1
#define EA_HAVE_CPP11_RANDOM 1
#define EA_HAVE_CPP11_CHRONO 1
#define EA_HAVE_CPP11_SCOPED_ALLOCATOR 1
#define EA_HAVE_CPP11_INITIALIZER_LIST 1
#define EA_HAVE_CPP11_SYSTEM_ERROR 1
#define EA_HAVE_CPP11_TYPEINDEX 1
#define EASTL_STD_ITERATOR_CATEGORY_ENABLED 1
#define EASTL_STD_TYPE_TRAITS_AVAILABLE 1
#define EASTL_MOVE_SEMANTICS_ENABLED 1
#define EASTL_VARIADIC_TEMPLATES_ENABLED 1
#define EASTL_VARIABLE_TEMPLATES_ENABLED 1
#define EASTL_INLINE_VARIABLE_ENABLED 1
#define EASTL_HAVE_CPP11_TYPE_TRAITS 1
#define EASTL_INLINE_NAMESPACES_ENABLED 1
#define EASTL_ALLOCATOR_EXPLICIT_ENABLED 1
#define EA_DLL 1
#define EASTL_USER_DEFINED_ALLOCATOR 1
#define _SILENCE_ALL_CXX17_DEPRECATION_WARNINGS 1
#define FMT_CONSTEVAL constexpr
#define FMT_EXCEPTION  0
#define FMT_HEADER_ONLY 1
#define FMT_USE_NOEXCEPT 1
#define XXH_INLINE_ALL 1
#endif
/*
required include path:
./
ext/xxhash
ext/EASTL/include
ext/EASTL/packages/EABase/include/Common/
ext/fmt/include
ext/spdlog/include
*/