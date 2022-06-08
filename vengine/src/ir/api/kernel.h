#pragma once
#include <ir/api/callable.h>
#include <Utility/ObjectStackAlloc.h>
namespace toolhub::ir {
class Kernel : public vstd::IOperatorNewBase {
public:
	vstd::ObjectStackAlloc allocator;
	static Type const* GetBuiltinType(vstd::HashCache<vstd::string_view> const& description);
	vstd::vector<Callable const*> callables;
	Callable const* mainCallable;
	vstd::span<Type const*> types;
	vstd::vector<ConstantVar const*> constants;
	Callable const* kernel;
	bool CheckValid() const;
	Kernel();
	~Kernel();
};
}// namespace toolhub::ir