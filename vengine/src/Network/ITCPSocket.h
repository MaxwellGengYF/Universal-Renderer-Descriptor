#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <system_error>
#include <Common/DynamicDLL.h>
namespace toolhub::net {
using Error = vstd::variant<
	std::error_code,
	vstd::string>;
class ITCPSocket : public vstd::IDisposable {
public:
	virtual void StartRead(
		vstd::function<bool(vstd::vector<vbyte>&&)>&& callBack,
		vstd::function<void(Error const&)>&& errorCallBack) = 0;
	virtual void WriteOne(
		vstd::vector<vbyte>&& data,
		vstd::function<void(Error const&)>&& errorCallBack) = 0;
	virtual void WriteMany(
		vstd::IEnumerable<vstd::vector<vbyte>&&>* ite,
		vstd::function<void(Error const&)>&& errorCallBack) = 0;
	virtual vstd::optional<std::error_code> Connect(char const* address, uint port) = 0;
	virtual void Accept(
		uint port,
		vstd::function<void(Error const&)>&& errorCallBack,
		size_t targetConsoleCount = std::numeric_limits<size_t>::max()) = 0;
};
class ITCPContext : public vstd::IDisposable {
public:
	virtual ITCPSocket* CreateSocket() = 0;
};
inline vstd::funcPtr_t<ITCPContext*()> GetTCPContext(DynamicDLL* dll) {
	return dll->GetDLLFunc<ITCPContext*()>("Network_GetTCPContext");
}
}// namespace toolhub::net