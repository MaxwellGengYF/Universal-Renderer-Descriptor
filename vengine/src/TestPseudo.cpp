
#define DLL_PROJECT
#include <TestPseudo.h>
DllClassA::DllClassA() {}
DllClassA ::~DllClassA() {}
DllClass::DllClass() {}
DllClass ::~DllClass() {}
class TestPseudoImpl : public ITestPseudo {
	double GetValue() override {
		return value.a.f;
	}

	PSEUDO_FUNC_IMPL(ITestPseudo)
};
ITestPseudo* GetPseudo(void* ptr) {
	return new (ptr) TestPseudoImpl();
}