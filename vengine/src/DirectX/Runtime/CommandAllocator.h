#pragma once
#include <Common/functional.h>
#include <DirectX/Runtime/CommandBuffer.h>
#include <Utility/StackAllocator.h>
#include <DirectX/Resource/UploadBuffer.h>
#include <DirectX/Resource/DefaultBuffer.h>
#include <DirectX/Resource/ReadbackBuffer.h>
#include <Common/LockFreeArrayQueue.h>
#include <Common/ObjectPtr.h>
namespace toolhub::directx {
class CommandQueue;
class IPipelineEvent;
class CommandAllocator final : public vstd::IOperatorNewBase {
	friend class CommandQueue;
	friend class CommandBuffer;

private:
	static constexpr size_t TEMP_SIZE = 4ull * 1024ull * 1024ull;
	template<typename Pack>
	class Visitor : public vstd::StackAllocatorVisitor {
	public:
		CommandAllocator* self;
		uint64 Allocate(uint64 size) override;
		void DeAllocate(uint64 handle) override;
	};
	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
	vstd::Pool<CommandBuffer, VEngine_AllocType::VEngine, true> bufferAllocator;
	vstd::vector<CommandBuffer*> bufferPool;
	vstd::vector<ID3D12CommandList*> executeCache;
	vstd::vector<vstd::ObjectPtr<CommandBuffer>> needExecuteBuffers;
	vstd::LockFreeArrayQueue<vstd::function<void()>> executeAfterComplete;
	Visitor<ReadbackBuffer> rbVisitor;
	Visitor<UploadBuffer> ubVisitor;
	vstd::StackAllocator uploadAllocator;
	vstd::StackAllocator readbackAllocator;
	vstd::HashMap<void const*, vstd::unique_ptr<IPipelineEvent>> tempEvent;
	Device* device;
	D3D12_COMMAND_LIST_TYPE type;
	IGpuAllocator* resourceAllocator;
	vstd::unique_ptr<DefaultBuffer> scratchBuffer;
	std::mutex mtx;
	//TODO: allocate commandbuffer
	void CollectBuffer(CommandBuffer* buffer);
	CommandAllocator(Device* device, IGpuAllocator* resourceAllocator, D3D12_COMMAND_LIST_TYPE type);
	void Execute(ID3D12CommandQueue* queue, ID3D12Fence* fence, uint64 fenceIndex);
	void Complete(ID3D12Fence* fence, uint64 fenceIndex);

public:
	~CommandAllocator();
	IPipelineEvent* AddOrGetTempEvent(void const* ptr, vstd::function<IPipelineEvent*()> const& func);
	ID3D12CommandAllocator* Allocator() const { return allocator.Get(); }
	D3D12_COMMAND_LIST_TYPE Type() const { return type; }
	void Reset();
	template<typename Func>
	requires(std::is_constructible_v<vstd::function<void()>, Func&&>) void ExecuteAfterComplete(Func&& func) {
		executeAfterComplete.Push(std::forward<Func>(func));
	}
	DefaultBuffer const* AllocateScratchBuffer(size_t targetSize);
	vstd::ObjectPtr<CommandBuffer> GetBuffer();
	BufferView GetTempReadbackBuffer(uint64 size);
	BufferView GetTempUploadBuffer(uint64 size);
	KILL_COPY_CONSTRUCT(CommandAllocator)
	KILL_MOVE_CONSTRUCT(CommandAllocator)
};
class IPipelineEvent : public vstd::IOperatorNewBase {
public:
	virtual ~IPipelineEvent() = default;
};
}// namespace toolhub::directx