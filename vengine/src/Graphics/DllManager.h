#pragma once
#include <Common/Common.h>
#include <Common/DynamicDLL.h>
#include <Database/DatabaseInclude.h>
namespace toolhub::graphics {
	class DllManager {
public:
	static db::Database const* GetDatabase();
};
}// namespace toolhub