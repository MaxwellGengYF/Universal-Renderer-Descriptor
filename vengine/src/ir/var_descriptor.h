#pragma once
#include <Common/Common.h>
namespace luisa::ir {
struct VarDescriptor {
	vstd::string_view typeName;
	vstd::string_view varName;
	size_t typeArrSize;
	VarDescriptor(
		vstd::string_view typeName,
		vstd::string_view varName,
		size_t typeArrSize)
		: typeName(typeName),
		  varName(varName),
		  typeArrSize(typeArrSize) {}
};
}// namespace luisa::ir