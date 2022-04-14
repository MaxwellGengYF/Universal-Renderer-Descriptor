#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <Database/IJsonObject.h>
// Entry:
// toolhub::net::NetWork* NetWork_GetFactory()

namespace toolhub::net {
class ISocket;
class ISocketAcceptor;
class IFileStream;
class IRpcClient;
class IRpcServer;
using RpcFunc = vstd::funcPtr_t<db::WriteJsonVariant(
	db::IJsonDatabase*,	  //db target
	db::WriteJsonVariant&&//arguments
	)>;
class NetWork {
protected:
	~NetWork() = default;

public:
	virtual ISocketAcceptor* GenServerAcceptor(
		uint16_t port) const = 0;
	virtual ISocket* GenServerTCPSock(
		ISocketAcceptor* acceptor) const = 0;
	virtual ISocket* GenClientTCPSock(
		uint16_t port, char const* address) const = 0;
	virtual IRpcClient* GetClientRpc(vstd::unique_ptr<ISocket>&& socket) const = 0;
	virtual IRpcServer* GetServerRpc(vstd::unique_ptr<ISocket>&& socket) const = 0;
	virtual void RegistFunctor(
		vstd::string_view name,
		RpcFunc func) = 0;
};
#ifdef VENGINE_NETWORK_PROJECT

class NetWorkImpl : public NetWork {
public:
	static NetWorkImpl* current;

	vstd::HashMap<vstd::string_view, RpcFunc> funcMap;

	void* service;
	NetWorkImpl();
	~NetWorkImpl();

	ISocketAcceptor* GenServerAcceptor(
		uint16_t port) const override;
	ISocket* GenServerTCPSock(
		ISocketAcceptor* acceptor) const override;
	ISocket* GenClientTCPSock(
		uint16_t port,
		char const* address) const override;
	IRpcClient* GetClientRpc(vstd::unique_ptr<ISocket>&& socket) const override;
	IRpcServer* GetServerRpc(vstd::unique_ptr<ISocket>&& socket) const override;
	void RegistFunctor(
		vstd::string_view name,
		RpcFunc func) override;
};
#endif
}// namespace toolhub::net