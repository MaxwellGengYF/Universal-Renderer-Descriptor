#pragma once
#include <ir/api/kernel.h>
namespace toolhub::ir {
class Codegen : public vstd::IOperatorNewBase {
public:
virtual ~Codegen() = default;
virtual vstd::string Gen(Kernel const& kernel) = 0;
};
}// namespace toolhub::ir