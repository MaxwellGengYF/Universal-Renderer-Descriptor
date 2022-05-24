#pragma once
#include <ir/api/var.h>
#include <ir/api/statement.h>
namespace toolhub::ir {
class Callable : public Allocatable{
public:
    vstd::vector<Statement const *> statements;
    vstd::span<Var const*> arguments;
    Type const* retType;
    size_t index;
};
}// namespace toolhub::ir