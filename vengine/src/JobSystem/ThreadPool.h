#pragma once
#include <Common/Common.h>
#include <Common/VObject.h>
#include <Common/LockFreeArrayQueue.h>
#include <JobSystem/ThreadTaskHandle.h>
class VENGINE_DLL_COMMON ThreadPool final {
	using TTH = ThreadHandleImpl;
	friend class ThreadHandleImpl;
	vstd::vector<std::thread> threads;
	vstd::vector<std::thread> backupThreads;
	vstd::LockFreeArrayQueue<vstd::ObjectPtr<TaskData>> taskList;
	void ThreadExecute(TaskData*);
	std::atomic_flag enabled;
	std::mutex threadLock;
	std::mutex backupThreadLock;
	vstd::spin_mutex threadVectorLock;
	std::condition_variable cv;
	std::condition_variable backupCV;
	void ExecuteTask(vstd::ObjectPtr<TaskData> const& task, int64& accumulateCount);
	void EnableThread(int64 enableCount);
	size_t workerThreadCount;
	void ActiveOneBackupThread();
	vstd::function<void(size_t)> runBackupThread;
	std::atomic_int64_t waitingBackupThread = 0;
	std::atomic_int64_t pausedWorkingThread = 0;
	struct Counter {
		std::atomic_size_t counter;
		std::atomic_size_t finishedCount;
		Counter(
			size_t f)
			: counter(0), finishedCount(f) {}
	};
	vstd::Pool<Counter> counters;
	vstd::spin_mutex counterMtx;

public:
	//Thread Execute
	ThreadPool(size_t targetThreadCount);
	~ThreadPool();
	static bool IsWorkerThread();
	size_t WorkerThreadCount() const { return workerThreadCount; }
	template<bool waitable>
	ThreadTaskHandle<waitable> GetTask(vstd::function<void()> func) {
		return {this, std::move(func), true};
	}
	template<bool waitable>
	ThreadTaskHandle<waitable> GetFence() {
		return {this, true};
	}
	template<bool waitable>
	ThreadTaskHandle<waitable> GetParallelTask(
		vstd::function<void(size_t)> func,
		size_t parallelCount,
		size_t innerLoopCount = 1) {
		if (parallelCount) {
			return {this, std::move(func), parallelCount, innerLoopCount, waitable};
		}
		return GetFence<waitable>();
	}
	template<bool waitable>
	ThreadTaskHandle<waitable> GetBeginEndTask(vstd::function<void(size_t, size_t)> func, size_t parallelCount, size_t threadCount) {
		return {this, std::move(func), parallelCount, threadCount, waitable};

	}
	template<bool waitable>
	ThreadTaskHandle<waitable> GetBeginEndTask(vstd::function<void(size_t, size_t)> func, size_t parallelCount) {
		return {this, std::move(func), parallelCount, parallelCount, waitable};

	}
};