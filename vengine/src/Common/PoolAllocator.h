#pragma once
#include <Common/MetaLib.h>
#include <Common/Memory.h>
#include <Common/Log.h>
#include <Common/Pool.h>

template<typename T>
class PoolAllocator {
private:
	vstd::StackObject<vstd::Pool<vstd::Storage<T, 1>, VEngine_AllocType::Default, true>> globalTransformPool;
	std::mutex transPoolLock;
	vstd::spin_mutex transAllocLock;
	std::atomic_bool poolInited = false;

public:
	PoolAllocator() {}
	PoolAllocator(PoolAllocator&& o) {
		poolInited = o.poolInited;
		o.poolInited = false;
		globalTransformPool.New(std::move(*o.globalTransformPool));
	}
	void* Allocate(size_t checkSize) {
#ifdef DEBUG
		if (checkSize != sizeof(T)) {
			VEngine_Log(
				{"Failed Allocate vstd::Type ",
				 typeid(T).name(),
				 "\n"});
			VENGINE_EXIT;
		}
#endif
		if (!poolInited) {
			std::lock_guard lck(transPoolLock);
			globalTransformPool.New(512);
			poolInited = true;
		}
		return globalTransformPool->New_Lock(transAllocLock);
	}
	void Free(void* ptr, size_t checkSize) {
#ifdef DEBUG
		if (checkSize != sizeof(T)) {
			VEngine_Log(
				{"Failed Deallocate vstd::Type ",
				 typeid(T).name(),
				 "\n"});
			VENGINE_EXIT;
		}
#endif
		globalTransformPool->Delete_Lock(transAllocLock, ptr);
	}
	~PoolAllocator() {
		if (poolInited) {
			globalTransformPool.Delete();
		}
	}
};

#define DECLARE_POOL_OPERATOR_NEW                                      \
	static void* operator new(size_t size) noexcept;                   \
	static void* operator new(size_t, void* place) noexcept {          \
		return place;                                                  \
	}                                                                  \
	static void operator delete(void* pdead, size_t size) noexcept;    \
	static void* operator new[](size_t size) noexcept {                \
		return vengine_malloc(size);                                   \
	}                                                                  \
	static void operator delete[](void* pdead, size_t size) noexcept { \
		vengine_free(pdead);                                           \
	}

#define IMPLEMENT_POOL_OPERATOR_NEW(T)                           \
	static PoolAllocator<T> T##_AllocatePool;                    \
	void* T::operator new(size_t size) noexcept {                \
		return T##_AllocatePool.Allocate(size);                  \
	}                                                            \
	void T::operator delete(void* pdead, size_t size) noexcept { \
		T##_AllocatePool.Free(pdead, size);                      \
	}
