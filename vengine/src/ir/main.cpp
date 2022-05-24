#include <ir/parser/parser.h>
#include <ir/codegen/ir_codegen.h>
int main() {
	vstd::string data(R"(
struct 16 T0 {
	float
	float2
	float4
	float[5]
}
struct 16 T1{
	T0
}
const uint[5] fuck {1}
def callable_fuck(): int[5]{

}

def callable_0 (tt: T0&) {
	v2 = get_member(tt, 0, 2, 3): int
	v3 = get_member(tt, 0, 3): int
	v4 = add(v2, v2): int
	sb = callable_fuck(): int[5]
	if(true)

	endif()
}
)"_sv);
	using namespace toolhub::ir;
	Parser parser;
	auto errorMsg = parser.Parse(data);
	vstd::unique_ptr<Kernel> kk;
	errorMsg.multi_visit(
		[&](vstd::unique_ptr<Kernel>& kernel) {
			std::cout << "Callable count: " << kernel->callables.size() << '\n'
					  << "Custom struct size: " << kernel->types.size() << '\n';
			kk = std::move(kernel);
		},
		[&](vstd::string const& str) {
			std::cout << "Error: " << str << '\n';
		});
	if (!kk) return 1;
	IRCodegen codegen;
	std::cout << codegen.Gen(*kk) << '\n';
	return 0;
}
