#include "render_pipeline.h"
namespace toolhub::vk {
RenderPipeline::RenderPipeline(Device const* device)
	: pool(device),
	  Resource(device),
	  stateTracker(device),
	  frameRes(device, &pool),
	  callbackThread([this] { CallbackThreadMain(); }) {}
RenderPipeline::~RenderPipeline() {
	{
		std::lock_guard lck(callbackMtx);
		enabled = false;
	}
	callbackCv.notify_all();
	callbackThread.join();
}
void RenderPipeline::CallbackThreadMain() {
	while (true) {
		{
			std::unique_lock lck(callbackMtx);
			if (!enabled) return;
			callbackCv.wait(lck);
		}
		size_t localCount = 0;
		while (auto v = executingList.Pop()) {
			localCount++;
			v->multi_visit(
				[&](size_t v) {
					auto&& frameRes = this->frameRes[v];
					auto&& locker = frameResLock[v];
					frameRes.Wait();
					frameRes.Reset();
					{
						locker.mtx.lock();
						locker.executing = false;
						locker.mtx.unlock();
						locker.cv.notify_all();
					}
				},
				[&](vstd::move_only_func<void()>& func) {
					func();
				});
		}
		{
			std::lock_guard lck(syncMtx);
			callbackPosition += localCount;
		}
		syncCv.notify_all();
	}
}
void RenderPipeline::BeginPrepareFrame() {
	auto curFrame = executeFrameIndex;
	lastAllocFrame = curFrame;
	executeFrameIndex = (++executeFrameIndex) % FRAME_BUFFER;
	auto&& locker = frameResLock[curFrame];
	{
		std::unique_lock lck(locker.mtx);
		while (locker.executing) {
			locker.cv.wait(lck);
		}
	}
}
void RenderPipeline::EndPrepareFrame() {
	auto&& locker = frameResLock[lastAllocFrame];
	auto&& frame = frameRes[lastAllocFrame];
	frame.Execute(lastExecuteFrame);
	lastExecuteFrame = &frame;
	mainThreadPosition++;
	{
		std::lock_guard lck(callbackMtx);
		locker.executing = true;
		executingList.Push(lastAllocFrame);
	}
	callbackCv.notify_one();
}
void RenderPipeline::Complete() {
	std::unique_lock lck(syncMtx);
	while (mainThreadPosition > callbackPosition) {
		syncCv.wait(lck);
	}
}
void RenderPipeline::ForceSyncInRendering() {
	auto&& locker = frameResLock[lastAllocFrame];
	auto&& frame = frameRes[lastAllocFrame];
	frame.Execute(lastExecuteFrame);
	lastExecuteFrame = nullptr;
	Complete();
	frame.Wait();
	frame.Reset();
	frame.GetCmdBuffer();
}
}// namespace toolhub::vk