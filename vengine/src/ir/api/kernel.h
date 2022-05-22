#pragma once
#include <ir/api/callable.h>
#include <Utility/ObjectStackAlloc.h>
namespace toolhub::ir {
class Kernel : public vstd::IOperatorNewBase {
public:
	vstd::ObjectStackAlloc allocator;
	static Type const* GetBuiltinType(vstd::HashCache<vstd::string_view> const& description);
	vstd::vector<Callable const*> callables;
	vstd::span<Type const*> types;
	vstd::vector<ConstantVar> constants;
	Callable const* kernel;
	Kernel();
	~Kernel();
};
}// namespace toolhub::ir