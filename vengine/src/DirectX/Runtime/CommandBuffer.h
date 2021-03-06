#pragma once
#include <DirectX/Resource/BindProperty.h>
#include <DirectX/Resource/TextureBase.h>
namespace toolhub::directx {
class CommandAllocator;
class Resource;
class ComputeShader;
class DescriptorHeap;
class Shader;
class RTShader;
class CommandBuffer;
class CommandBufferBuilder {
	friend class CommandBuffer;

private:
	CommandBuffer const* cb;
	CommandBufferBuilder(CommandBuffer const* cb);
	void SetResources(
		Shader* s,
		vstd::span<const BindProperty> resources);
	DescriptorHeap const* currentDesc = nullptr;

public:
	CommandBuffer const* GetCB() const { return cb; }
	ID3D12GraphicsCommandList4* CmdList() const;
	void SetDescHeap(DescriptorHeap const* heap);

	void DispatchCompute(
		ComputeShader* cs,
		uint3 dispatchId,
		vstd::span<const BindProperty> resources);
	void DispatchCompute(
		ComputeShader* cs,
		uint3 dispatchId,
		std::initializer_list<BindProperty> resources) {
		DispatchCompute(
			cs,
			dispatchId,
			vstd::span<const BindProperty>{resources.begin(), resources.size()});
	}
	void DispatchRT(
		RTShader* rt,
		uint3 dispatchId,
		vstd::span<const BindProperty> resources);
	void DispatchRT(
		RTShader* rt,
		uint3 dispatchId,
		std::initializer_list<BindProperty> resources) {
		DispatchRT(
			rt,
			dispatchId,
			vstd::span<const BindProperty>{resources.begin(), resources.size()});
	}
	void CopyBuffer(
		Buffer const* src,
		Buffer const* dst,
		uint64 srcOffset,
		uint64 dstOffset,
		uint64 byteSize);
	void Upload(BufferView const& buffer, void const* src);
	void Readback(BufferView const& buffer, void* dst);
	void CopyBufferToTexture(
		BufferView const& sourceBuffer,
		TextureBase* texture,
		uint targetMip,
		uint width,
		uint height,
		uint depth);
	~CommandBufferBuilder();
};
class CommandBuffer : public vstd::IDisposable {
	friend class CommandBufferBuilder;
	friend class CommandAllocator;
	void Reset() const;
	void Close() const;
	Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList4> cmdList;
	CommandAllocator* alloc;

public:
	ID3D12GraphicsCommandList4* CmdList() const { return cmdList.Get(); }
	CommandBuffer(
		Device* device,
		CommandAllocator* alloc);
	CommandAllocator* GetAlloc() const { return alloc; }
	void Dispose() override;
	~CommandBuffer();
	CommandBuffer(CommandBuffer&& v) = default;
	CommandBufferBuilder Build() const { return CommandBufferBuilder(this); }
	KILL_COPY_CONSTRUCT(CommandBuffer)
};

}// namespace toolhub::directx