#pragma once

#include <Common/Common.h>
#include <Common/functional.h>
#include <Common/LockFreeArrayQueue.h>
#include <Network/FunctionSerializer.h>
#include <Utility/VGuid.h>
#include <Common/VObject.h>
#include <Network/Future.h>
#include <Database/IJsonObject.h>
namespace toolhub::net {

class ISocket;
class IRpcBase : public vstd::IDisposable {
public:
	virtual db::IJsonDatabase* GetDB() = 0;
	//Error message: empty for no mistake
	virtual vstd::string GetErrorMessage() = 0;
};
class IRpcClient : public IRpcBase {
public:
	virtual Future<db::WriteJsonVariant> CallFunctor(
		vstd::string_view name,
		db::WriteJsonVariant&& arg) = 0;
	virtual void CallFunctors(
		vstd::IEnumerable<std::pair<vstd::string_view, db::WriteJsonVariant>>& strv,
		vstd::vector<Future<db::WriteJsonVariant>>& retValues) = 0;
	virtual bool Block_SendMsg() = 0;
};
class IRpcServer : public IRpcBase {
public:
	virtual bool Block_RecvMsg() = 0;
};
}// namespace toolhub::net

#define VE_NET_REGIST_FUNCTOR(RPC, FUNC) (RPC).RegistFunctor(#FUNC##_sv, FUNC)
