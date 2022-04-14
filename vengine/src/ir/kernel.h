#pragma once
#include <ir/callable.h>
#include <Utility/ObjectStackAlloc.h>
namespace luisa::ir {
class Kernel : public vstd::IOperatorNewBase {
public:
	vstd::ObjectStackAlloc allocator;
	static Type const* GetBuiltinType(vstd::HashCache<vstd::string_view> const& description);
	vstd::span<Callable const*> callables;
	vstd::span<Var const*> vars;
	vstd::span<Type const*> types;
	Callable const* kernel;
	Kernel();
	~Kernel();
};
}// namespace luisa::ir