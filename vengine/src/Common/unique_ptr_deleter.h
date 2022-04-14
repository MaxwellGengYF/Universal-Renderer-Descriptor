#pragma once
#include <Common/Memory.h>
namespace vstd {
struct unique_ptr_deleter {
	template<typename T>
	static void Delete(T* ptr) noexcept {
		if constexpr (std::is_base_of_v<IDisposable, T>) {
			ptr->Dispose();
		} else if constexpr (std::is_base_of_v<ISelfPtr, T>) {
			auto selfPtr = ptr->SelfPtr();
			ptr->~T();
			vengine_free(selfPtr);
		} else {
			ptr->~T();
			vengine_free(ptr);
		}
	}
	template<typename T>
	void operator()(T* ptr) const noexcept {
		Delete(ptr);
	}
};
}// namespace vstd