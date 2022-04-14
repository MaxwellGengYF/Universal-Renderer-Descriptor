#pragma once
///////////////////////// Switchers

//#define VENGINE_REL_WITH_DEBUG_INFO
#define _ENABLE_EXTENDED_ALIGNED_STORAGE
#define VENGINE_DISABLE_M128
#define NOMINMAX
#undef max
#undef min
///////////////////////// Clang
#ifdef VENGINE_DISABLE_M128
#define _XM_NO_INTRINSICS_
#define m128_f32 vector4_f32
#define m128_u32 vector4_u32
#endif//VENGINE_DISABLE_M128
#include <csignal>
#define VENGINE_EXIT std::abort()
#ifndef UNICODE
#define UNICODE//Disable this in non-unicode system
#endif

#ifdef VENGINE_REL_WITH_DEBUG_INFO
#define DEBUG
#endif
#if defined(_DEBUG)
#define DEBUG
#endif
#define _ITERATOR_DEBUG_LEVEL 0
#define _SCL_SECURE_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define _CONSOLE

//////////////////////// Renderer Switcher
#define VENGINE_PLATFORM_DIRECTX_12 1
#pragma endregion

//////////////////////// Main Engine Switcher
#define VENGINE_USE_GPU_DRIVEN 1
#define VENGINE_LOAD_SCENE 1
#define VENGINE_USE_RAYTRACING 1
#define VENGINE_PYTHON_SUPPORT
#define VENGINE_CSHARP_SUPPORT
#define VENGINE_DB_EXPORT_C

//////////////////////// DLL
#ifdef DLL_DEBUG
#define VENGINE_DLL_COMMON
#define VENGINE_DLL_RENDERER
#define VENGINE_DLL_FUNC
#define VENGINE_DLL_COMPUTE
#define VENGINE_DLL_NETWORK
#else

#ifdef COMMON_DLL_PROJECT
#define VENGINE_DLL_COMMON _declspec(dllexport)
#define VENGINE_C_FUNC_COMMON extern "C" _declspec(dllexport)
#else
#define VENGINE_DLL_COMMON _declspec(dllimport)
#define VENGINE_C_FUNC_COMMON extern "C" _declspec(dllimport)
#endif

#ifdef VENGINE_COMPUTE_PROJECT
#define VENGINE_DLL_COMPUTE _declspec(dllexport)
#else
#define VENGINE_DLL_COMPUTE _declspec(dllimport)
#endif

#endif//DLL_DEBUG

#define VENGINE_CDECL _cdecl
#define VENGINE_STD_CALL _stdcall
#define VENGINE_VECTOR_CALL _vectorcall
#define VENGINE_FAST_CALL _fastcall
#define EXPORT_UNITY_FUNCTION
#ifdef EXPORT_UNITY_FUNCTION
#define VENGINE_UNITY_EXTERN extern "C" _declspec(dllexport)
#else
#define VENGINE_UNITY_EXTERN
#endif

/////////////////////// THREAD PAUSE
#include <stdint.h>
#include <new>
VENGINE_C_FUNC_COMMON void* vengine_malloc(size_t size);
VENGINE_C_FUNC_COMMON void vengine_free(void* ptr);
inline void* operator new(size_t n) {
	return vengine_malloc(n);
}
inline void* operator new[](size_t n) {
	return vengine_malloc(n);
}

inline void operator delete(void* p) {
	vengine_free(p);
}
inline void operator delete[](void* p) {
	vengine_free(p);
}
inline void operator delete(void* p, size_t) {
	vengine_free(p);
}
inline void operator delete[](void* p, size_t) {
	vengine_free(p);
}

inline void* operator new(size_t n, ::std::nothrow_t const&) {
	return vengine_malloc(n);
}
inline void* operator new[](size_t n, ::std::nothrow_t const&) {
	return vengine_malloc(n);
}

inline void operator delete(void* p, ::std::nothrow_t const&) {
	vengine_free(p);
}
inline void operator delete[](void* p, ::std::nothrow_t const&) {
	vengine_free(p);
}
inline void operator delete(void* p, size_t, ::std::nothrow_t const&) {
	vengine_free(p);
}
inline void operator delete[](void* p, size_t, ::std::nothrow_t const&) {
	vengine_free(p);
}

//EASTL define
#define EA_CHAR8_UNIQUE 1
#define EA_WCHAR_UNIQUE 1
#ifndef EASTLAlloc// To consider: Instead of calling through pAllocator, just go directly to operator new, since that's what allocator does.
#define EASTLAlloc(allocator, n) (allocator).allocate(n);
#endif
#ifndef EASTLFree
#define EASTLFree(allocator, p, size) (allocator).deallocate((p), (size))// Important to cast to void* as p may be non-const.
#endif
#ifndef EASTL_LIKELY
#if defined(__GNUC__) && (__GNUC__ >= 3)
#define EASTL_LIKELY(x) __builtin_expect(!!(x), true)
#define EASTL_UNLIKELY(x) __builtin_expect(!!(x), false)
#else
#define EASTL_LIKELY(x) x
#define EASTL_UNLIKELY(x) x
#endif
#endif
#ifdef DEBUG
#define assert_ENABLED 1
#define EASTL_ASSERT_ENABLED 1
#endif
#ifndef EASTL_FAIL_MSG
#define EASTL_FAIL_MSG(msg) VEngine_Log(msg); VENGINE_EXIT;
#endif
#ifndef EASTL_ASSERT_MSG
#if EASTL_ASSERT_ENABLED
#define EASTL_ASSERT_MSG(expression, message)                          \
	if (!(expression)) {VEngine_Log(message); VENGINE_EXIT;}
#else
#define EASTL_ASSERT_MSG(expression, message)
#endif
#endif

#include <stdint.h>
using uint = uint32_t;
using uint16 = uint16_t;
using int16 = int16_t;
using uint64 = uint64_t;
using int64 = int64_t;
using int32 = int32_t;
using vbyte = uint8_t;