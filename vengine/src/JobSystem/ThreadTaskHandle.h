#pragma once
#include <Common/Common.h>
#include <Common/functional.h>
#include <Common/VObject.h>
class ThreadPool;
enum class TaskState : vbyte {
	Waiting = 0,
	Executed = 1,
	Finished = 2,
	Fuck0,
	Fuck1,
	Fuck2,
	Fuck3,
};
struct VENGINE_DLL_COMMON TaskData : public vstd::IOperatorNewBase {
	std::atomic_uint8_t state;
	vstd::function<void()> func;
	//ThreadPool Set
	/* std::mutex mtx;
		std::condition_variable cv;*/
	using Locker = std::pair<std::mutex, std::condition_variable>;
	vstd::optional<Locker> mainThreadLocker;
	vstd::spin_mutex lockerMtx;
	vstd::vector<vstd::ObjectPtr<TaskData>> dependedJobs;
	vstd::vector<vstd::ObjectPtr<TaskData>> dependingJob;
	std::atomic_size_t dependCount = 0;
	TaskData(bool waitable);
	TaskData(vstd::function<void()>&& func, bool waitable);
	~TaskData();
};
class VENGINE_DLL_COMMON ThreadHandleImpl {
private:
	bool isArray;
	union {
		vstd::StackObject<vstd::ObjectPtr<TaskData>> taskFlag;
		vstd::StackObject<vstd::vector<vstd::ObjectPtr<TaskData>>> taskFlags;
	};
	ThreadPool* pool;
	template<typename H>
	void TAddDepend(H&&) const;

protected:
	void mComplete() const;
	ThreadHandleImpl(
		ThreadPool* pool,
		bool waitable);
	ThreadHandleImpl(
		ThreadPool* pool,
		vstd::function<void()>&& func,
		bool waitable);
	ThreadHandleImpl(
		ThreadPool* pool,
		vstd::function<void(size_t)>&& func,
		size_t parallelCount,
		size_t innerLoop,
		bool waitable);
	ThreadHandleImpl(
		ThreadPool* pool,
		vstd::function<void(size_t, size_t)>&& func,
		size_t parallelCount,
		size_t threadCount,
		bool waitable);

public:
	~ThreadHandleImpl();
	ThreadHandleImpl(ThreadHandleImpl const& v);
	void AddDepend(ThreadHandleImpl const& handle) const;
	void AddDepend(vstd::span<ThreadHandleImpl const> handles) const;
	void AddDepend(std::initializer_list<ThreadHandleImpl const> handles) const {
		AddDepend(vstd::span<ThreadHandleImpl const>(handles.begin(), handles.end()));
	}
	ThreadHandleImpl(ThreadHandleImpl&& v);
	template<typename... Args>
	requires(std::is_constructible_v<ThreadHandleImpl, Args&&...>) void operator=(Args&&... args) {
		this->~ThreadHandleImpl();
		new (this) ThreadHandleImpl(std::forward<Args>(args)...);
	}
	bool IsComplete() const;
	void Execute() const;
};

template<bool waitable>
class ThreadTaskHandle : public ThreadHandleImpl {
	friend class ThreadPool;

public:
	template<typename... Args>
	ThreadTaskHandle(Args&&... args)
		: ThreadHandleImpl(std::forward<Args>(args)...) {}

	ThreadTaskHandle(ThreadTaskHandle const&) = default;
	ThreadTaskHandle(ThreadTaskHandle&&) = default;
};
template<>
class ThreadTaskHandle<true> : public ThreadHandleImpl {
	friend class ThreadPool;

public:
	template<typename... Args>
	ThreadTaskHandle(Args&&... args)
		: ThreadHandleImpl(std::forward<Args>(args)...) {}

	ThreadTaskHandle(ThreadTaskHandle const&) = default;
	ThreadTaskHandle(ThreadTaskHandle&&) = default;
	void Complete() const {
		mComplete();
	}
};
