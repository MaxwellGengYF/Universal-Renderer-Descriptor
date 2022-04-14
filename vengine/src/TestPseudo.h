#pragma once
#include <Common/Pseudo.h>
// No "_declspec(dllexport)", This class NEVER linked to exe
class DllClassA {
public:
	int v;
	double f = 666;

	DllClassA();
	~DllClassA();
};
// No "_declspec(dllexport)", This class NEVER linked to exe
class DllClass {

public:
	DllClassA a;
	DllClass();
	~DllClass();
};
// No "_declspec(dllexport)", This class NEVER linked to exe
class ITestPseudo : public vstd::IPseudoObject {
protected:
	DllClass value;

public:
	virtual ~ITestPseudo() = default;
	virtual double GetValue() = 0;
};
#ifdef DLL_PROJECT
#define EXPORT _declspec(dllexport)
#else
#define EXPORT _declspec(dllimport)
#endif
EXPORT ITestPseudo* GetPseudo(void* ptr);