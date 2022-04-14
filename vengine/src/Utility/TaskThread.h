#pragma once
#include <VEngineConfig.h>
#include <Common/TypeWiper.h>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <Common/Memory.h>
class VENGINE_DLL_COMMON TaskThread : public vstd::IOperatorNewBase {
private:
	std::thread thd;
	std::mutex mtx;
	std::mutex mainThreadMtx;
	std::condition_variable cv;
	std::condition_variable mainThreadCV;
	std::atomic_bool enabled;
	std::atomic_bool runNext;
	std::atomic_bool mainThreadLocked;
	void (*funcData)(void*);
	void* funcBody;
	static void RunThread(TaskThread* ptr);

public:
	TaskThread();
	void ExecuteNext();
	void Complete();
	bool IsCompleted() const { return !mainThreadLocked; }
	~TaskThread();
	template<typename T>
	requires(std::is_constructible_v<T>) void SetFunctor(T& func) {
		using Type = typename std::remove_const_t<T>;
		funcData = [](void* ptr) {
			(*(T*)ptr)();
		};
		funcBody = &(const_cast<Type&>(func));
	}
};
