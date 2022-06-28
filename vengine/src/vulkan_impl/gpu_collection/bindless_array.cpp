#include <vulkan_impl/gpu_collection/bindless_array.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/runtime/frame_resource.h>
namespace toolhub::vk {
BindlessArray::BindlessArray(
	Device const* device,
	uint arrSize)
	: GPUCollection(device),
	  instanceBuffer(device, arrSize * sizeof(Instance), false, RWState::None),
	  instances(arrSize) {
}
BindlessArray::~BindlessArray() {
	for (auto&& i : instances) {
		if (i.first.buffer != std::numeric_limits<uint16>::max()) {
			device->DeAllocateBindlessIdx(i.first.buffer);
		}
		if (i.first.tex2D != std::numeric_limits<uint16>::max()) {
			device->DeAllocateBindlessIdx(i.first.tex2D);
		}
		if (i.first.tex3D != std::numeric_limits<uint16>::max()) {
			device->DeAllocateBindlessIdx(i.first.tex3D);
		}
	}
}
void BindlessArray::AddRef(GPUCollection const* v) {
	refMap.Emplace(v, 0).Value()++;
}
void BindlessArray::RemoveRef(GPUCollection const* v) {
	auto ite = refMap.Find(v);
	if (!ite) return;
	if (--ite.Value() == 0) {
		refMap.Remove(ite);
	}
}
bool BindlessArray::IsPtrInRes(GPUCollection const* handle) const {
	return refMap.Find(handle);
}
void BindlessArray::Bind(uint index, Buffer const* buffer, uint64 offset) {
	auto&& v = updateList.Emplace(index).Value();
	auto& ref = instances[index].second;
	if (ref.buffer) {
		RemoveRef(ref.buffer);
	}
	AddRef(buffer);
	ref.buffer = buffer;
	v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::BUFFER);
	v.bufferOffset = offset;
}
void BindlessArray::Bind(uint index, Texture const* tex, vbyte offset) {
	auto&& v = updateList.Emplace(index).Value();
	auto& ref = instances[index].second;
	if (tex->Dimension() == VK_IMAGE_TYPE_2D) {
		if (ref.tex2D) {
			RemoveRef(ref.tex2D);
		}
		AddRef(tex);
		ref.tex2D = tex;
		v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::TEX2D);
		v.tex2DOffset = offset;
	} else {
		if (ref.tex3D) {
			RemoveRef(ref.tex3D);
		}
		AddRef(tex);
		ref.tex3D = tex;
		v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::TEX3D);
		v.tex3DOffset = offset;
	}
}
void BindlessArray::UnBindBuffer(uint index) {
	auto&& v = updateList.Emplace(index).Value();
	auto& ref = instances[index].second;
	if (ref.buffer) {
		RemoveRef(ref.buffer);
		ref.buffer = nullptr;
	}
	v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::BUFFER);
}
void BindlessArray::UnBindTex2D(uint index) {
	auto&& v = updateList.Emplace(index).Value();
	auto& ref = instances[index].second;
	if (ref.tex2D) {
		RemoveRef(ref.tex2D);
		ref.tex2D = nullptr;
	}
	v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::TEX2D);
}
void BindlessArray::UnBindTex3D(uint index) {
	auto&& v = updateList.Emplace(index).Value();
	auto& ref = instances[index].second;
	if (ref.tex3D) {
		RemoveRef(ref.tex3D);
		ref.tex3D = nullptr;
	}
	v.flag = (UpdateFlag)((vbyte)v.flag | (vbyte)UpdateFlag::TEX3D);
}
//rendering
void BindlessArray::Preprocess(
	FrameResource* res,
	ResStateTracker& stateTracker,
	CommandBuffer* cb) {
	stateTracker.MarkBufferWrite(
		&instanceBuffer,
		BufferWriteState::Copy);
	auto ite = updateList.begin();
	BufferView upload = res->AllocateUpload(
		updateList.size() * sizeof(Instance));
	auto iterateFunc = [&] -> vstd::optional<VkBufferCopy> {
		if (ite == updateList.end())
			return {};
		vstd::optional<VkBufferCopy> opt;
		opt.New();
		auto index = ite->first;
		auto offset = size_t(index) * sizeof(Instance);
		auto uploadOffset = upload.offset + offset;
		auto& inst = instances[index];
		auto& refOp = ite->second;
		auto& ref = inst.second;
		opt->srcOffset = uploadOffset;
		opt->dstOffset = offset;
		opt->size = sizeof(Instance);
		// update buffer
		if ((static_cast<vbyte>(refOp.flag) & static_cast<vbyte>(UpdateFlag::BUFFER)) != 0) {
			if (ref.buffer) {
				if (inst.first.buffer == std::numeric_limits<uint16>::max()) {
					inst.first.buffer = device->AllocateBindlessIdx();
				}
				device->AddBindlessUpdateCmd(
					inst.first.buffer,
					BufferView(ref.buffer, refOp.bufferOffset));
			} else {
				if (inst.first.buffer != std::numeric_limits<uint16>::max()) {
					device->DeAllocateBindlessIdx(inst.first.buffer);
				}
				inst.first.buffer = std::numeric_limits<uint16>::max();
			}
			inst.second.buffer = ref.buffer;
		}
		// update tex2d
		if ((static_cast<vbyte>(refOp.flag) & static_cast<vbyte>(UpdateFlag::TEX2D)) != 0) {
			if (ref.tex2D) {
				if (inst.first.tex2D == std::numeric_limits<uint16>::max()) {
					inst.first.tex2D = device->AllocateBindlessIdx();
				}
				device->AddBindlessUpdateCmd(
					inst.first.tex2D,
					TexView(ref.tex2D, refOp.tex2DOffset));
			} else {
				if (inst.first.tex2D != std::numeric_limits<uint16>::max()) {
					device->DeAllocateBindlessIdx(inst.first.tex2D);
				}
				inst.first.tex2D = std::numeric_limits<uint16>::max();
			}
			inst.second.tex2D = ref.tex2D;
		}
		// update tex3d
		if ((static_cast<vbyte>(refOp.flag) & static_cast<vbyte>(UpdateFlag::TEX3D)) != 0) {
			if (ref.tex3D) {
				if (!inst.first.tex3D == std::numeric_limits<uint16>::max()) {
					inst.first.tex3D = device->AllocateBindlessIdx();
				}
				device->AddBindlessUpdateCmd(
					inst.first.tex3D,
					TexView(ref.tex3D, refOp.tex3DOffset));
			} else {
				if (inst.first.tex3D != std::numeric_limits<uint16>::max()) {
					device->DeAllocateBindlessIdx(inst.first.tex3D);
				}
				inst.first.tex3D = std::numeric_limits<uint16>::max();
			}
			inst.second.tex3D = ref.tex3D;
		}
		upload.buffer->CopyValueFrom(inst.first, uploadOffset);
		++ite;
		return opt;
	};
	res->AddCopyCmd(
		upload.buffer,
		&instanceBuffer,
		iterateFunc,
		updateList.size());
	updateList.Clear();
	//TODO: write to upload, copy to instance
	//res->AddCopyCmd()
}

}// namespace toolhub::vk