#pragma once
#include <Common/Common.h>
#include <Common/unique_ptr.h>
namespace toolhub::net {

class ISocket : public vstd::IDisposable {
protected:
	ISocket() = default;
	virtual ~ISocket() = default;

public:
	virtual bool Read(vstd::vector<vbyte>& data, size_t maxSize) = 0;
	virtual bool Write(vstd::span<vbyte const> data) = 0;
	virtual bool Connect() = 0;
	virtual vstd::string const& ErrorMessage() = 0;
};
class ISocketAcceptor : public vstd::IDisposable {
protected:
	ISocketAcceptor() = default;
	virtual ~ISocketAcceptor() = default;

public:
	virtual bool Accept(vstd::string& errorMsg, ISocket* socket) = 0;
};
}// namespace toolhub::net
