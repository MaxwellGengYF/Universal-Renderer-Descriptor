
#include <Graphics/DllManager.h>
namespace toolhub::graphics {
namespace dllmanager {
struct DllManagerInst {
	DllFactoryLoader<db::Database> dbDll;
	db::Database const* db;
	DllManagerInst()
		: dbDll("VEngine_Database.dll", "Database_GetFactory") {
		db = dbDll();
	}
};
static DllManagerInst inst;
}// namespace dllmanager
db::Database const* DllManager::GetDatabase() {
	return dllmanager::inst.db;
}
}// namespace toolhub::graphics