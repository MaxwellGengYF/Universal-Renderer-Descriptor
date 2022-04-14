#pragma once

#include <Common/Common.h>
#include <Network/INetworkService.h>
#include <Network/ISocket.h>
#include <Network/IRegistObject.h>
#include <Utility/VGuid.h>
namespace toolhub::net {
class ObjectRegister : public vstd::IOperatorNewBase {
private:
	struct RegistObj {
		IRegistObject* ptr;
		RegistObj(IRegistObject* ptr)
			: ptr(ptr) {
		}
		~RegistObj() {
			if (ptr)
				ptr->Dispose();
		}
		IRegistObject* get() const {
			return ptr;
		}
	};
	vstd::HashMap<vstd::Guid, RegistObj> allObjects;
	std::mutex mtx;

	void DisposeObj(
		vstd::Guid const& id);
	IRegistObject* CreateObj(
		vstd::function<IRegistObject*()> const& creater,
		vstd::Guid const& newGuid);

public:
	KILL_COPY_CONSTRUCT(ObjectRegister)
	KILL_MOVE_CONSTRUCT(ObjectRegister)
	static ObjectRegister* GetSingleton();
	static void DisposeSingleton();
	ObjectRegister();
	~ObjectRegister();
	IRegistObject* CreateObjLocally(
		vstd::function<IRegistObject*()> const& creater);
	IRegistObject* CreateObjByRemote(
		vstd::function<IRegistObject*()> const& creater,
		vstd::Guid const& remoteID);
	IRegistObject* GetObject(vstd::Guid const& id);
};
}// namespace toolhub::net