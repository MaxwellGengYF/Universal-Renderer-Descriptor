#pragma once
#include <VEngineConfig.h>
#include <mutex>
#include <condition_variable>
#include <thread>
class VENGINE_DLL_COMMON ThreadResetEvent {
private:
	static std::atomic<uint64_t> uID;
	uint64_t currentID;
	std::condition_variable var;
	std::mutex mtx;
	bool isLocked;

public:
	bool IsLocked() const { return isLocked; }
	ThreadResetEvent(bool isLockedInit) : isLocked(isLockedInit) {
		if (isLockedInit) {
			currentID = uID++;
		}
	}
	uint64_t GetCurrentID() const {
		return currentID;
	}

	void WaitCheckID(uint64_t id);
	void Wait();
	void WaitAutoLock();
	uint64_t Lock();
	void Unlock();
};