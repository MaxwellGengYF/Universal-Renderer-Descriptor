#pragma once
#include <Graphics/ShaderCompiler/CodeSample/IPreProcessor.h>
#include <Common/Common.h>
#include <Common/functional.h>
class CommentPreProcessor : public IPreProcessor {
public:
	bool PreProcess(vstd::string& source, vstd::string& dest) override;
	static void Execute(
		vstd::string const& source,
		vstd::function<void(vstd::string_view)> const& func);
};