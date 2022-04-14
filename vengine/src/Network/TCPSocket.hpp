#pragma once

#include <Network/ISocket.h>
#include <Network/NetworkInclude.h>
#include <boost/asio.hpp>
namespace toolhub::net {

class TCPIOBase {
public:
	asio::io_service* service;
	asio::ip::tcp::socket socket;
	TCPIOBase(
		asio::io_service* service)
		: service(service),
		  socket(*service) {
	}
	bool Read(
		vstd::string& errorMsg,
		vstd::vector<vbyte>& data, size_t maxSize) {
		data.clear();
		data.reserve(maxSize);
		auto size = socket.try_read_some(errorMsg, asio::buffer(data.data(), maxSize));
		if (!size) return false;
		data.resize(*size);
		return true;
	}
	bool Write(
		vstd::string& errorMsg,
		vstd::span<vbyte const> data) {
		return socket.try_write_some(errorMsg, asio::buffer(data.data(), data.size()));
	}
};
class TCPSocketAcceptor : public ISocketAcceptor, public vstd::IOperatorNewBase {
public:
	vstd::StackObject<asio::ip::tcp::endpoint> ep;
	vstd::StackObject<asio::ip::tcp::acceptor> acceptor;
	TCPSocketAcceptor(
		asio::io_service* service,
		uint16_t port) {
		ep.New(asio::ip::tcp::v4(), port);
		acceptor.New(*service, *ep);
	}
	~TCPSocketAcceptor() {
		ep.Delete();
		acceptor.Delete();
	}
	bool Accept(vstd::string& errorMsg, ISocket* socket) override;
	void Dispose() override {
		delete this;
	}
};

class TCPServer_Impl final : public ISocket, public vstd::IOperatorNewBase {
public:
	vstd::string errorMsg;
	//vstd::optional<> acc;
	vstd::optional<TCPIOBase> io;
	bool successAccept = false;
	ISocketAcceptor* acc;
	//Server
	TCPServer_Impl(
		asio::io_service* service,
		ISocketAcceptor* acceptor) {
		io.New(service);
		acc = acceptor;
		//acc.New(*io->service, io->ep);
	}

	bool Connect() override {
		if (successAccept) return true;
		successAccept = acc->Accept(errorMsg, this);
		return successAccept;
	}
	vstd::string const& ErrorMessage() override {
		return errorMsg;
	}
	~TCPServer_Impl() {
	}
	bool Read(vstd::vector<vbyte>& data, size_t maxSize) override {
		if (!successAccept) return false;
		return io->Read(errorMsg, data, maxSize);
	}
	bool Write(vstd::span<vbyte const> data) override {
		if (!successAccept) return false;
		return io->Write(errorMsg, data);
	}
	void Dispose() override {
		delete this;
	}
};

bool TCPSocketAcceptor::Accept(vstd::string& errorMsg, ISocket* socket) {
	return acceptor->try_accept(errorMsg, static_cast<TCPServer_Impl*>(socket)->io->socket);
}

class TCPClient_Impl final : public ISocket, public vstd::IOperatorNewBase {
public:
	TCPIOBase io;
	asio::ip::tcp::endpoint ep;
	bool successAccept;
	//Server
	vstd::string errorMsg;
	vstd::string const& ErrorMessage() override {
		return errorMsg;
	}

	TCPClient_Impl(asio::io_service* service, uint16_t port, asio::ip::address&& add)
		: io(service),
		  ep(std::move(add), port) {
	}
	bool Connect() override {
		if (successAccept) return true;
		successAccept = io.socket.try_connect(errorMsg, ep);
		return successAccept;
	}

	~TCPClient_Impl() {
	}
	bool Read(vstd::vector<vbyte>& data, size_t maxSize) override {
		if (!successAccept) return false;
		return io.Read(errorMsg, data, maxSize);
	}
	bool Write(vstd::span<vbyte const> data) override {
		if (!successAccept) return false;
		return io.Write(errorMsg, data);
	}
	void Dispose() override {
		delete this;
	}
};
ISocketAcceptor* NetWorkImpl::GenServerAcceptor(
	uint16_t port) const {
	return new TCPSocketAcceptor(
		reinterpret_cast<asio::io_service*>(service),
		port);
}
ISocket* NetWorkImpl::GenServerTCPSock(
	ISocketAcceptor* acceptor) const {
	return new TCPServer_Impl(
		reinterpret_cast<asio::io_service*>(service),
		acceptor);
}
ISocket* NetWorkImpl::GenClientTCPSock(
	uint16_t port,
	char const* address) const {
	return new TCPClient_Impl(
		reinterpret_cast<asio::io_service*>(service),
		port,
		asio::ip::address::from_string(address));
}
}// namespace toolhub::net