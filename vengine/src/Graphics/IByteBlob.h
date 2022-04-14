#pragma once
#include <Common/Common.h>
namespace toolhub::graphics {
class IByteBlob : public vstd::ISelfPtr {
public:
	virtual vbyte* GetBufferPtr() const = 0;
	virtual size_t GetBufferSize() const = 0;
	virtual ~IByteBlob() = default;
};
}// namespace toolhub::graphics