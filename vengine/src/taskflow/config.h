#pragma once
#ifdef TASKFLOW_EXPORT
#define TF_API _declspec(dllexport)
#else
#define TF_API _declspec(dllimport)
#endif
#include <Common/Common.h>
#include <Common/functional.h>
#include <string_view>
#include <ostream>
namespace tf {
using string = std::basic_string<char, std::char_traits<char>, vstd::allocator<char>>;
using std::string_view;
using stringstream = std::basic_stringstream<char, std::char_traits<char>, vstd::allocator<char>>;
using stringbuf = std::basic_stringbuf<char, std::char_traits<char>, vstd::allocator<char>>;
using ostringstream = std::basic_ostringstream<char, std::char_traits<char>, vstd::allocator<char>>;
template<typename T>
using vector = vstd::vector<T>;
template<typename T>
using function = vstd::function<T>;
inline void* tf_malloc(size_t size) { return vengine_malloc(size); }
inline void tf_free(void* ptr) { vengine_free(ptr); }
inline void* tf_realloc(void* ptr, size_t newSize) { return vengine_realloc(ptr, newSize); }
using IOperatorNewBase = vstd::IOperatorNewBase;
}// namespace tf