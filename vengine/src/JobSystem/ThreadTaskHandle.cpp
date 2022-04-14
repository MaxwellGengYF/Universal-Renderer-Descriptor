
#include <JobSystem/ThreadTaskHandle.h>
#include <JobSystem/ThreadPool.h>
TaskData::TaskData(
	vstd::function<void()>&& func, bool waitable)
	: func(std::move(func)) {
	if (waitable)
		mainThreadLocker.New();
	state.store(static_cast<vbyte>(TaskState::Waiting), std::memory_order_release);
}
TaskData::TaskData(bool waitable) {
	if (waitable)
		mainThreadLocker.New();
	state.store(static_cast<vbyte>(TaskState::Waiting), std::memory_order_release);
}
TaskData::~TaskData() {
}

ThreadHandleImpl::ThreadHandleImpl(
	ThreadPool* pool, bool waitable) : pool(pool) {
	isArray = false;
	auto ptr = vstd::MakeObjectPtr(
		new TaskData(waitable));
	taskFlag.New(std::move(ptr));
}
ThreadHandleImpl::ThreadHandleImpl(
	ThreadPool* pool,
	vstd::function<void()>&& func, bool waitable) : pool(pool) {
	isArray = false;
	auto ptr = vstd::MakeObjectPtr(new TaskData(std::move(func), waitable));
	taskFlag.New(std::move(ptr));
}

ThreadHandleImpl::ThreadHandleImpl(
	ThreadPool* tPool,
	vstd::function<void(size_t)>&& func,
	size_t parallelCount,
	size_t innerLoop,
	bool waitable) : pool(tPool) {
	innerLoop = std::max<size_t>(innerLoop, 1);
	size_t threadCount = (parallelCount + innerLoop - 1) / innerLoop;
	threadCount = std::min<size_t>(threadCount, tPool->workerThreadCount);
	isArray = true;
	taskFlags.New();
	auto&& tasks = *taskFlags;
	tasks.reserve(threadCount + 1);

	auto taskCounter = tPool->counters.New_Lock(tPool->counterMtx, threadCount);
	auto startTask = [&](auto&& executeFunc) {
		for (size_t v = 0; v < threadCount - 1; ++v) {

			tasks.emplace_back(vstd::MakeObjectPtr(
				new TaskData(
					[=]() {
						executeFunc(func);
					},
					waitable)));
		}
		tasks.emplace_back(vstd::MakeObjectPtr(
			new TaskData(
				[=, func = std::move(func)]() {
					executeFunc(func);
				},
				waitable)));
	};
	if (innerLoop <= 1) {
		auto executeFunc = [=](auto& func) {
			auto i = taskCounter->counter.fetch_add(1, std::memory_order_acq_rel);
			while (i < parallelCount) {
				func(i);
				i = taskCounter->counter.fetch_add(1, std::memory_order_acq_rel);
			}
			if (taskCounter->finishedCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
				tPool->counters.Delete_Lock(tPool->counterMtx, taskCounter);
			}
		};
		startTask(std::move(executeFunc));
	} else {
		auto executeFunc = [=](auto&& func) {
			auto i = taskCounter->counter.fetch_add(innerLoop, std::memory_order_acq_rel);
			while (i < parallelCount) {
				for (auto v : vstd::range(i, std::min<size_t>(i + innerLoop, parallelCount))) {
					func(v);
				}
				i = taskCounter->counter.fetch_add(innerLoop, std::memory_order_acq_rel);
			}
			if (taskCounter->finishedCount.fetch_sub(1, std::memory_order_relaxed) == 1) {
				tPool->counters.Delete_Lock(tPool->counterMtx, taskCounter);
			}
		};
		startTask(std::move(executeFunc));
	}
}

ThreadHandleImpl::ThreadHandleImpl(
	ThreadPool* tPool,
	vstd::function<void(size_t, size_t)>&& func,
	size_t parallelCount,
	size_t threadCount,
	bool waitable) : pool(tPool) {
	threadCount = std::min<size_t>(threadCount, tPool->workerThreadCount);
	threadCount = std::min<size_t>(threadCount, parallelCount);
	isArray = true;
	taskFlags.New();
	auto&& tasks = *taskFlags;
	size_t eachJobCount = parallelCount / threadCount;
	tasks.reserve(threadCount + 1);
	auto AddTask = [&](size_t beg, size_t ed) {
		tasks.emplace_back(vstd::MakeObjectPtr(
			new TaskData(
				[=]() {
					func(beg, ed);
				},
				waitable)));
	};
	for (size_t i = 0; i < threadCount; ++i) {
		AddTask(i * eachJobCount, (i + 1) * eachJobCount);
	}
	size_t full = eachJobCount * threadCount;
	size_t lefted = parallelCount - full;
	if (lefted > 0) {
		AddTask(full, parallelCount);
	}
}
void ThreadHandleImpl::mComplete() const {
	struct TPoolCounter {
		ThreadPool* t;
		TPoolCounter(
			ThreadPool* t) : t(t) {
			t->pausedWorkingThread++;
		}
		~TPoolCounter() {
			t->pausedWorkingThread--;
		}
	};
	TPoolCounter tcounter(pool);
	int64 needEnableState = 0;
	auto checkExecuteFunc = [&](vstd::ObjectPtr<TaskData> const& p) {
		pool->ExecuteTask(p, needEnableState);
	};
	auto func = [&](TaskData* p) {
		auto state = static_cast<TaskState>(p->state.load(std::memory_order_acquire));
		if (state == TaskState::Finished) return;
		TaskData::Locker* mtxPtr = p->mainThreadLocker;
		if (mtxPtr) {
			std::unique_lock lck(mtxPtr->first);
			while (p->state.load() != 2) {
				mtxPtr->second.wait(lck);
			}
		}
	};
	if (isArray) {
		for (auto& taskFlag : *taskFlags)
			checkExecuteFunc(taskFlag);
		pool->EnableThread(needEnableState);
		pool->ActiveOneBackupThread();
		for (auto& taskFlag : *taskFlags) {
			func(taskFlag);
		}
	} else {
		checkExecuteFunc(*taskFlag);
		pool->EnableThread(needEnableState);
		pool->ActiveOneBackupThread();
		func(*taskFlag);
	}
}

bool ThreadHandleImpl::IsComplete() const {
	if (!pool) return true;
	auto func = [&](TaskData* p) {
		return static_cast<TaskState>(p->state.load(std::memory_order_relaxed)) == TaskState::Finished;
	};
	if (isArray) {
		for (auto& taskFlag : *taskFlags) {
			if (!func(taskFlag)) return false;
		}
		return true;
	} else {
		return func(*taskFlag);
	}
}
template<typename H>
void ThreadHandleImpl::TAddDepend(H&& handle) const {
	auto func = [&](vstd::ObjectPtr<TaskData> const& selfPtr, auto&& dep, uint64& dependAdd) {
		TaskData* p = dep;
		TaskData* self = selfPtr;

		TaskState state = static_cast<TaskState>(p->state.load(std::memory_order_acquire));
		if ((vbyte)state < (vbyte)TaskState::Executed) {
			p->dependedJobs.push_back(selfPtr);
			if constexpr (std::is_lvalue_reference_v<H>) {
				self->dependingJob.push_back(dep);
			} else {
				self->dependingJob.push_back(std::move(dep));
			}
			dependAdd++;
		} else {
			VEngine_Log("Try depend on executing task!");
			VENGINE_EXIT;
		}
	};
	auto executeSelf = [&](vstd::ObjectPtr<TaskData> const& self, auto&& handle) {
		uint64 v = 0;
		if (handle.isArray) {
			for (auto& i : *handle.taskFlags) {
				func(self, i, v);
			}
		} else {
			func(self, *handle.taskFlag, v);
		}
		self->dependCount.fetch_add(v, std::memory_order_relaxed);
	};
	if (isArray) {
		for (auto& i : *taskFlags) {
			executeSelf(i, handle);
		}
	} else {
		executeSelf(*taskFlag, handle);
	}
}
void ThreadHandleImpl::AddDepend(ThreadHandleImpl const& handle) const {
	TAddDepend<ThreadHandleImpl const&>(handle);
}

void ThreadHandleImpl::AddDepend(vstd::span<ThreadHandleImpl const> handles) const {

	for (auto& handle : handles) {
		AddDepend(handle);
	}
}

void ThreadHandleImpl::Execute() const {
	int64 needEnableState = 0;
	if (isArray) {
		for (auto& i : *taskFlags) {
			pool->ExecuteTask(i, needEnableState);
		}
	} else {
		pool->ExecuteTask(*taskFlag, needEnableState);
	}
	pool->EnableThread(needEnableState);
}
ThreadHandleImpl::ThreadHandleImpl(ThreadHandleImpl const& v)
	: pool(v.pool),
	  isArray(v.isArray) {
	if (isArray) {
		taskFlags.New(*v.taskFlags);
	} else {
		taskFlag.New(*v.taskFlag);
	}
}
ThreadHandleImpl::ThreadHandleImpl(ThreadHandleImpl&& v)
	: pool(v.pool),
	  isArray(v.isArray) {
	if (isArray) {
		taskFlags.New(std::move(*v.taskFlags));
	} else {
		taskFlag.New(std::move(*v.taskFlag));
	}
}
ThreadHandleImpl::~ThreadHandleImpl() {
	if (isArray) {
		taskFlags.Delete();
	} else {
		taskFlag.Delete();
	}
}
