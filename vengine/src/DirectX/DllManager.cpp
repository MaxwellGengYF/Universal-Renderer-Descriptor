
#include <DirectX/DllManager.h>
#include <Graphics/IGraphicsFactory.h>
namespace toolhub::directx{
namespace dllmanager {
struct DllManagerInst {
	DllFactoryLoader<db::Database> dbDll;
	DllFactoryLoader<graphics::IGraphicsFactory> graDll;
	db::Database const* db;
	graphics::IGraphicsFactory const* gra;
	DllManagerInst()
		: dbDll("VEngine_Database.dll", "Database_GetFactory"),
		  graDll("VEngine_Graphics.dll", "Graphics_GetFactory") {
		db = dbDll();
		gra = graDll();
	}
};
static DllManagerInst inst;
}// namespace dllmanager
db::Database const* DllManager::GetDatabase() {
	return dllmanager::inst.db;
}
graphics::IGraphicsFactory const* DllManager::GetGraphics() {
	return dllmanager::inst.gra;
}
DynamicDLL const* DllManager::GetGraphicsDll() {
	return dllmanager::inst.graDll.GetDynamicDLL();
}

}// namespace toolhub
