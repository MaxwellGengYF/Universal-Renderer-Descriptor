#pragma once
#include <components/resource.h>
namespace toolhub::vk {
class GPUCollection : public Resource {
public:
	GPUCollection(Device const* d) : Resource(d) {}
    virtual ~GPUCollection() = default;
    enum class Tag : vbyte{
        Buffer,
        Texture,
        Accel,
        Mesh,
        BindlessArray
    };
    virtual Tag GetTag() const = 0;
};
}// namespace toolhub::vk