#pragma once
#include <Common/Common.h>
namespace vstd {
template<typename T>
class ObjectTrackFlag;
template<typename T>
class ObjectTracker;
class VENGINE_DLL_COMMON ObjectTrackFlag_Impl {
	template<typename T>
	friend class ObjectTrackFlag;
	template<typename T>
	friend class ObjectTracker;
	friend class vstd::Pool<ObjectTrackFlag_Impl, VEngine_AllocType::VEngine, true>;

private:
	size_t ptr;
	std::atomic_uint64_t refCount;
	static ObjectTrackFlag_Impl* Allocate();
	static void DeAllocate(ObjectTrackFlag_Impl* ptr);

public:
	ObjectTrackFlag_Impl();
	~ObjectTrackFlag_Impl() = default;
};

template<typename T>
class ObjectTrackFlag {
	template<typename T>
	friend class ObjectTracker;

private:
	ObjectTrackFlag_Impl* impl;

protected:
	ObjectTrackFlag(ObjectTrackFlag_Impl* impl) : impl(impl) {
		++impl->refCount;
	}

public:
	T* Get() const {
		return impl ? reinterpret_cast<T*>(impl->ptr) : nullptr;
	}
	ObjectTrackFlag() : impl(nullptr) {}
	ObjectTrackFlag(ObjectTrackFlag const& v) {
		impl = v.impl;
		if (impl)
			++impl->refCount;
	}
	ObjectTrackFlag(ObjectTrackFlag&& v) {
		impl = v.impl;
		v.impl = nullptr;
	}
	ObjectTrackFlag& operator=(ObjectTrackFlag const& v) {
		this->~ObjectTrackFlag();
		new (this) ObjectTrackFlag(v);
		return *this;
	}
	ObjectTrackFlag& operator=(ObjectTrackFlag&& v) {
		this->~ObjectTrackFlag();
		new (this) ObjectTrackFlag(std::move(v));
		return *this;
	}
	~ObjectTrackFlag() {
		if (!impl) return;
		if (--impl->refCount == 0) {
			ObjectTrackFlag_Impl::DeAllocate(impl);
		}
	}
};

template<typename T>
class ObjectTracker {
private:
	ObjectTrackFlag_Impl* impl = nullptr;

protected:
	~ObjectTracker() {
		if (!impl) return;
		if (--impl->refCount == 0) {
			ObjectTrackFlag_Impl::DeAllocate(impl);
		} else {
			impl->ptr = 0;
		}
	}
	ObjectTracker() {}

public:
	ObjectTrackFlag<T> GetTrackFlag() {
		if (!impl) {
			impl = ObjectTrackFlag_Impl::Allocate();
			impl->ptr = reinterpret_cast<size_t>(static_cast<T*>(this));
		}
		return ObjectTrackFlag<T>(impl);
	}
	ObjectTrackFlag<T const> GetTrackFlag() const {
		if (!impl) {
			impl = ObjectTrackFlag_Impl::Allocate();
			impl->ptr = reinterpret_cast<size_t>(static_cast<T const*>(this));
		}
		return ObjectTrackFlag<T const>(impl);
	}
	KILL_COPY_CONSTRUCT(ObjectTracker)
	KILL_MOVE_CONSTRUCT(ObjectTracker)
};
}// namespace vstd