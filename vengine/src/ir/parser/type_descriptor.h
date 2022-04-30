#pragma once
#include <Common/Common.h>
namespace luisa::ir {
struct TypeDescriptor {
	vstd::string_view typeName;
	size_t typeArrSize;
	TypeDescriptor(
		vstd::string_view typeName,
		size_t typeArrSize)
		: typeName(typeName),
		  typeArrSize(typeArrSize) {}
};
struct VarDescriptor {
	vstd::string_view varName;
	TypeDescriptor typeDesc;
	VarDescriptor(
		vstd::string_view varName,
		TypeDescriptor const& typeDesc)
		: varName(varName), typeDesc(typeDesc) {}
};
}// namespace luisa::ir