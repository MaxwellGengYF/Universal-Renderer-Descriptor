#pragma once
#include <Common/vstring.h>
class CodeCompiler;
class IPreProcessor
{
	friend class CodeCompiler;
protected:
	CodeCompiler* compiler;
public:
	virtual bool PreProcess(vstd::string& source, vstd::string& dest) = 0;
};