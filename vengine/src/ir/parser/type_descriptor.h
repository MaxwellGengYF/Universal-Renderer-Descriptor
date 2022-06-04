#pragma once
#include <Common/Common.h>
namespace toolhub::ir {
struct TypeDescriptor : public vstd::IOperatorNewBase{
	vstd::string_view typeName;
	size_t typeArrSize;
	vstd::unique_ptr<TypeDescriptor> subType;
	TypeDescriptor() : typeArrSize(0) {}
	TypeDescriptor(
		vstd::string_view typeName,
		size_t typeArrSize)
		: typeName(typeName),
		  typeArrSize(typeArrSize) {}
};
struct VarDescriptor {
	vstd::string_view varName;
	TypeDescriptor typeDesc;
	VarDescriptor() {}
	VarDescriptor(
		vstd::string_view varName,
		TypeDescriptor&& typeDesc)
		: varName(varName), typeDesc(std::move(typeDesc)) {}
};
}// namespace toolhub::ir