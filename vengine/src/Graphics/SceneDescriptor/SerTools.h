#pragma once

#include <Common/Common.h>
namespace vstd::serde {
template<typename T>
T GetData(vbyte const*& ptr) {
	T t = *reinterpret_cast<T const*>(ptr);
	ptr += sizeof(T);
	return t;
}
}// namespace vstd::serde