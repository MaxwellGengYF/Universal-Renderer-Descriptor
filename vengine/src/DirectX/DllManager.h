#pragma once
#include <Common/Common.h>
#include <Common/DynamicDLL.h>
#include <Database/DatabaseInclude.h>
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::directx {
class DllManager {
	DllFactoryLoader<db::Database> dbDll;

public:
	static db::Database const* GetDatabase();
	static graphics::IGraphicsFactory const* GetGraphics();
	static DynamicDLL const* GetGraphicsDll();
};
}// namespace toolhub::directx