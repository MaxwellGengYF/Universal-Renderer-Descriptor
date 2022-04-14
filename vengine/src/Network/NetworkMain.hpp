#pragma once

#include <Network/NetworkInclude.h>
#include <boost/asio.hpp>
namespace toolhub::net {
NetWorkImpl* NetWorkImpl::current = nullptr;
static NetWorkImpl network;
}

toolhub::net::NetWorkImpl::NetWorkImpl() {
	service = new asio::io_service(std::thread::hardware_concurrency());
}

toolhub::net::NetWorkImpl::~NetWorkImpl() {
	delete reinterpret_cast<asio::io_service*>(service);
}

VENGINE_UNITY_EXTERN toolhub::net::NetWork* NetWork_GetFactory() {
	using namespace toolhub::net;
	NetWorkImpl::current = &network;
	return &network;
}
