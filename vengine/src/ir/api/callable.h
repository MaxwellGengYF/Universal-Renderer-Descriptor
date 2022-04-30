#pragma once
#include <ir/api/var.h>
#include <ir/api/statement.h>
namespace luisa::ir {
class Callable : public Allocatable{
public:
    vstd::span<Statement const *> statements;
    vstd::span<Var const*> arguments;
};
}// namespace luisa::ir