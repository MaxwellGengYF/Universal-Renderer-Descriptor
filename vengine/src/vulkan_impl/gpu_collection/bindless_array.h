#pragma once
#include "buffer.h"
#include "texture.h"
#include <vulkan_impl/types/buffer_view.h>

namespace toolhub::vk {
class GPUCollection;
class FrameResource;
class CommandBuffer;
class ResStateTracker;
class BindlessArray : public GPUCollection {
	struct Instance {
		uint16 tex2D = std::numeric_limits<uint16>::max();
		uint16 buffer = std::numeric_limits<uint16>::max();
		uint16 tex3D = std::numeric_limits<uint16>::max();
		uint16 unused = 0;
	};
	struct Ref {
		Buffer const* buffer = nullptr;
		Texture const* tex2D = nullptr;
		Texture const* tex3D = nullptr;
	};
	enum class UpdateFlag : vbyte {
		None = 0,
		BUFFER = 1,
		TEX2D = 2,
		TEX3D = 4
	};
	struct RefOption {
		uint64 bufferOffset;
		vbyte tex2DOffset;
		vbyte tex3DOffset;
		UpdateFlag flag = UpdateFlag::None;
	};
	Buffer instanceBuffer;
	vstd::HashMap<GPUCollection const*, size_t> refMap;
	vstd::vector<std::pair<Instance, Ref>> instances;
	vstd::HashMap<uint, RefOption> updateList;
	vstd::vector<VkBufferCopy> copyCmds;
	void AddRef(GPUCollection const* v);
	void RemoveRef(GPUCollection const* v);

public:
	//host set
	void Bind(uint index, Buffer const* buffer, uint64 offset);
	void Bind(uint index, Texture const* tex, vbyte mipOffset);
	void UnBindBuffer(uint index);
	void UnBindTex2D(uint index);
	void UnBindTex3D(uint index);
	//rendering
	void Preprocess(
		FrameResource* res,
		ResStateTracker& stateTracker,
		CommandBuffer* cb);
	Buffer const* InstanceBuffer() const { return &instanceBuffer; }

	bool IsPtrInRes(GPUCollection const* handle) const;
	Tag GetTag() const {
		return Tag::BindlessArray;
	}
	BindlessArray(
		Device const* device,
		uint arrSize);
	~BindlessArray();
};
}// namespace toolhub::vk