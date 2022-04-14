#pragma once

#include <Network/ISocket.h>
#include <Network/NetworkInclude.h>
#include <boost/asio.hpp>
namespace toolhub::net {
//TODO: UDP
/*
class UDPIOBase {
	asio::io_service* service;
	asio::ip::udp::endpoint ep;
	asio::ip::udp::socket socket;
	UDPIOBase(
		asio::io_service* service, uint16_t port, asio::ip::udp&& udp)
		: service(service),
		  ep(std::move(udp), port),
		  socket(*service) {
	}
	UDPIOBase(
		asio::io_service* service, uint16_t port, asio::ip::address&& add)
		: service(service),
		  ep(std::move(add), port),
		  socket(*service) {
		socket.open()
	}
	bool Read(
		vstd::string& errorMsg,
		vstd::vector<vbyte>& data, size_t maxSize) {
		data.clear();
		data.reserve(maxSize);
		auto size = socket.try_receive(errorMsg, asio::buffer(data.data(), maxSize));
		if (!size) return false;
		data.resize(*size);
		return true;
	}
	bool Write(
		vstd::string& errorMsg,
		vstd::span<vbyte const> data) {
		socket.send_to(asio::buffer(data.data(), data.size()), ep);

	}
};
class UDPServer_Impl final : public ISocket {
public:
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW

	vstd::string errorMsg;
	vstd::optional<UDPIOBase> io;
};
*/
}// namespace toolhub::net