
#include <JobSystem/ThreadPool.h>
static thread_local bool vengineTpool_isWorkerThread = false;
bool ThreadPool::IsWorkerThread() {
	return vengineTpool_isWorkerThread;
}
ThreadPool::ThreadPool(size_t workerThreadCount)
	: counters(32) {
	workerThreadCount = std::max<size_t>(workerThreadCount, 1);
	this->workerThreadCount = workerThreadCount;
	enabled.test_and_set(std::memory_order_release);
	threads.reserve(workerThreadCount);
	auto LockThread = [this](std::mutex& mtx, std::condition_variable& cv, auto&& beforeLock, auto&& afterLock) {
		beforeLock();
		std::unique_lock lck(mtx);
		while (
			enabled.test(std::memory_order_acquire)
			&& taskList.Length() == 0) {
			cv.wait(lck);
		}
		afterLock();
	};
	vstd::function<void()> runThread = [this, LockThread]() {
		vengineTpool_isWorkerThread = true;
		while (enabled.test(std::memory_order_acquire)) {
			while (auto func = taskList.Pop()) {
				TaskData* ptr = *func;
				ThreadExecute(ptr);
			}
			LockThread(
				threadLock, cv, []() {}, []() {});
		}
	};
	runBackupThread = [this, LockThread](size_t threadIndex) {
		vengineTpool_isWorkerThread = true;
		while (enabled.test(std::memory_order_acquire)) {
			while (auto func = taskList.Pop()) {
				ThreadExecute(*func);
				if (threadIndex >= pausedWorkingThread) break;
			}
			LockThread(
				backupThreadLock, backupCV, [&]() { waitingBackupThread++; },
				[&]() { waitingBackupThread--; });
		}
	};
	for (auto i : vstd::range(workerThreadCount - 1)) {
		threads.emplace_back(runThread);
	}
	threads.emplace_back(std::move(runThread));
}

void ThreadPool::ActiveOneBackupThread() {
	if (!vengineTpool_isWorkerThread) return;
	if (waitingBackupThread > 0) {
		backupCV.notify_one();
		return;
	}
	std::lock_guard ll(threadVectorLock);
	auto sz = backupThreads.size();
	std::thread t(runBackupThread, sz);
	backupThreads.emplace_back(std::move(t));
}

void ThreadPool::ThreadExecute(TaskData* ptr) {
	ptr->func();
	ptr->state.store(static_cast<vbyte>(TaskState::Finished), std::memory_order_release);
	if (ptr->mainThreadLocker) {
		ptr->mainThreadLocker->second.notify_all();
	}
	int64 needNotifyThread = 0;
	for (auto& i : ptr->dependedJobs) {
		TaskData* d = i;
		if (d->dependCount.fetch_sub(1, std::memory_order_acq_rel) == 1) {
			if (d->state.load(std::memory_order_acquire) != static_cast<vbyte>(TaskState::Executed)) continue;
			taskList.Push(std::move(i));
			needNotifyThread++;
		}
	}
	ptr->dependedJobs.clear();
	if (needNotifyThread > 0) {
		if (needNotifyThread < workerThreadCount) {
			for (auto i : vstd::range(needNotifyThread))
				cv.notify_one();
		} else {
			cv.notify_all();
		}
	}
}

ThreadPool::~ThreadPool() {
	enabled.clear(std::memory_order_release);
	cv.notify_all();
	{
		std::lock_guard lck(threadLock);
		backupCV.notify_all();
	}
	for (auto& i : backupThreads) {
		i.join();
	}
	for (auto& i : threads) {
		i.join();
	}
	threads.clear();
	backupThreads.clear();
}
void ThreadPool::ExecuteTask(vstd::ObjectPtr<TaskData> const& task, int64& accumulateCount) {
	TaskData* ptr = task;
	task->state.store(static_cast<vbyte>(TaskState::Executed), std::memory_order_release);
	if (ptr->dependCount.load(std::memory_order_acquire) > 0) {
		for (auto& i : ptr->dependingJob) {
			ExecuteTask(i, accumulateCount);
		}
		ptr->dependingJob.clear();
	} else {
		taskList.Push(task);
		accumulateCount++;
	}
}
void ThreadPool::EnableThread(int64 enableCount) {
	enableCount -= vengineTpool_isWorkerThread ? 1 : 0;

	if (enableCount <= 0)
		return;
	std::lock_guard lck(threadLock);
	if (enableCount < workerThreadCount) {
		for (auto i : vstd::range(enableCount))
			cv.notify_one();
	} else {
		cv.notify_all();
	}
}