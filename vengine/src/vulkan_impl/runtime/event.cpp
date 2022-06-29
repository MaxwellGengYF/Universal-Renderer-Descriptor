#include "event.h"
namespace toolhub::vk {
Event::Event(Device const* device)
	: Resource(device) {
	VkSemaphoreCreateInfo semaphoreInfo{VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
	vkCreateSemaphore(
		device->device,
		&semaphoreInfo,
		Device::Allocator(),
		&semaphore);
}
void Event::EndOfFrame(uint64 targetFrame) {
	{
		std::lock_guard lck(mtx);
        finishedFrame = targetFrame;
	}
	cv.notify_all();
}
Event::~Event() {
	vkDestroySemaphore(
		device->device,
		semaphore,
		Device::Allocator());
}
void Event::Wait(){
    std::unique_lock lck(mtx);
    while(signalFrame > finishedFrame){
        cv.wait(lck);
    }
}
}// namespace toolhub::vk