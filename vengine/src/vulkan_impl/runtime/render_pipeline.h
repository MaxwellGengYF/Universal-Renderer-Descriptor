#pragma once
#include "frame_resource.h"
#include "command_pool.h"
#include "command_buffer.h"
#include "res_state_tracker.h"
namespace toolhub::vk {
class RenderPipeline : public Resource {
	RenderPipeline(RenderPipeline const&) = delete;
	RenderPipeline(RenderPipeline&&) = delete;
	vstd::LockFreeArrayQueue<vstd::variant<
		size_t,
		vstd::move_only_func<void()>>>
		executingList;
	std::mutex callbackMtx;
	std::mutex syncMtx;
	std::condition_variable callbackCv;
	std::condition_variable syncCv;
	uint64 callbackPosition = 0;
	uint64 mainThreadPosition = 0;
	void CallbackThreadMain();

	struct FrameResLck {
		std::mutex mtx;
		std::condition_variable cv;
		bool executing = false;
	};
	static constexpr size_t FRAME_BUFFER = 3;
	ResStateTracker stateTracker;
	CommandPool pool;
	vstd::array<FrameResource, FRAME_BUFFER> frameRes;
	vstd::array<FrameResLck, FRAME_BUFFER> frameResLock;
	//the executing frame
	FrameResource* lastExecuteFrame = nullptr;
	//the preparing frame
	size_t lastAllocFrame = std::numeric_limits<size_t>::max();
	size_t executeFrameIndex = 0;
	bool enabled = true;
	// thread
	std::thread callbackThread;

public:
	FrameResource* PreparingFrame() { return &frameRes[lastAllocFrame]; }
	RenderPipeline(Device const* device);
	~RenderPipeline();
	// get next frame, wait if not finished
	void BeginPrepareFrame();
	void EndPrepareFrame();
	void Complete();
	void ForceSyncInRendering();
};
}// namespace toolhub::vk