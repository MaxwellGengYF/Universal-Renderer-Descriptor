
#include <Common/Common.h>
#include <Utility/ObjectStackAlloc.h>
struct Test {
	int value;
	Test(int v)
		: value(v) {}
	~Test() {
		std::cout << "Dispose: " << value << '\n';
	}
};
int main() {
	vstd::ObjectStackAlloc alloc(65536);
	auto sp = alloc.AllocateSpan<Test>(12, [](std::span<Test> t) {
		for (auto i : vstd::range(t.size())) {
			new (t.data() + i) Test(i);
		}
	});
	for (auto&& i : sp->span()) {
		std::cout << "Value: " << i.value << '\n';
	}
	return 0;
}