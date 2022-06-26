#pragma once
#include <vulkan_impl/components/resource.h>
namespace toolhub::vk {
class FrameResource;
class Event : public Resource {
	friend class FrameResource;
	// manage queue 2 queue sync
	VkSemaphore semaphore;
	std::mutex mtx;
	std::condition_variable cv;
	uint64 signalFrame = 0;
	uint64 finishedFrame = 0;
	void EndOfFrame(uint64 targetFrame);

public:
	void Wait();
	VkSemaphore Semaphore() const { return semaphore; }
	Event(Device const* device);
	~Event();
};
}// namespace toolhub::vk