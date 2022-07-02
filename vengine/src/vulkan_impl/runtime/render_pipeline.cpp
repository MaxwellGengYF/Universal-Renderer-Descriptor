#include "render_pipeline.h"
#include "event.h"
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
				},
				[&](std::pair<Event*, size_t>& v) {
					v.first->EndOfFrame(v.second);
				});
		}
		{
			std::lock_guard lck(syncMtx);
			callbackPosition += localCount;
		}
		syncCv.notify_all();
	}
}
FrameResource* RenderPipeline::BeginPrepareFrame() {
	++executeFrameIndex;
	executeFrameIndex %= FRAME_BUFFER;
	auto&& locker = frameResLock[executeFrameIndex];
	{
		std::unique_lock lck(locker.mtx);
		while (locker.executing) {
			locker.cv.wait(lck);
		}
	}
	return PreparingFrame();
}
void RenderPipeline::EndPrepareFrame() {
	auto&& locker = frameResLock[executeFrameIndex];
	auto&& frame = frameRes[executeFrameIndex];
	frame.Execute(lastExecuteFrame);
	lastExecuteFrame = &frame;
	locker.executing = true;
	AddTask(executeFrameIndex);
}
void RenderPipeline::Complete() {
	std::unique_lock lck(syncMtx);
	while (mainThreadPosition > callbackPosition) {
		syncCv.wait(lck);
	}
}
template<typename... Args>
void RenderPipeline::AddTask(Args&&... args) {
	mainThreadPosition++;
	executingList.Push(std::forward<Args>(args)...);
	callbackMtx.lock();
	callbackMtx.unlock();
	callbackCv.notify_one();
}
void RenderPipeline::ForceSyncInRendering() {
	auto&& locker = frameResLock[executeFrameIndex];
	auto&& frame = frameRes[executeFrameIndex];
	frame.Execute(lastExecuteFrame);
	lastExecuteFrame = nullptr;
	Complete();
	frame.Wait();
	frame.Reset();
	frame.GetCmdBuffer();
}
void RenderPipeline::AddEvtSync(Event* evt) {
	auto&& frame = frameRes[executeFrameIndex];
	//TODO: gpu fence
	auto count = ++evt->signalFrame;
	AddTask(evt, count);
}
void RenderPipeline::AddCallback(vstd::move_only_func<void()>&& func) {
	AddTask(std::move(func));
}
}// namespace toolhub::vk