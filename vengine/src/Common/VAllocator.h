#pragma once
#include <Common/AllocateType.h>
#include <Common/Memory.h>
#include <Common/StackAllocator.h>

template<VEngine_AllocType tt>
struct VAllocHandle {
	VAllocHandle() {}
	VAllocHandle(VAllocHandle const& v) : VAllocHandle() {}
	VAllocHandle(VAllocHandle&& v) : VAllocHandle() {}
	void* Malloc(size_t sz) const {
		if constexpr (tt == VEngine_AllocType::Default) {
			return vengine_default_malloc(sz);
		} else if constexpr (tt == VEngine_AllocType::VEngine) {
			return vengine_malloc(sz);
		}
	}
	void Free(void* ptr) const {
		if constexpr (tt == VEngine_AllocType::Default) {
			vengine_default_free(ptr);
		} else if constexpr (tt == VEngine_AllocType::VEngine) {
			return vengine_free(ptr);
		}
	}
	void* Realloc(void* ptr, size_t size) const{
		if constexpr (tt == VEngine_AllocType::Default) {
			return vengine_default_realloc(ptr, size);
		} else if constexpr (tt == VEngine_AllocType::VEngine) {
			return vengine_realloc(ptr, size);
		}
	}
};