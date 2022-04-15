#pragma once
#ifdef COMMON_DLL_PROJECT
#define TF_API _declspec(dllexport)
#else
#define TF_API _declspec(dllimport)
#endif
#include <Common/Common.h>
#include <string_view>
#include <ostream>
namespace tf {
using string = std::basic_string<char, std::char_traits<char>, vstd::allocator<char>>;
using std::string_view;
using stringstream = std::basic_stringstream<char, std::char_traits<char>, vstd::allocator<char>>;
using stringbuf = std::basic_stringbuf<char, std::char_traits<char>, vstd::allocator<char>>;
using ostringstream = std::basic_ostringstream<char, std::char_traits<char>, vstd::allocator<char>>;
}// namespace tf