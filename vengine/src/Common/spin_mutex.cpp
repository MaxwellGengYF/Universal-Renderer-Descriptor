
#include <Common/spin_mutex.h>
#include <iostream>
#include <mutex>

#if defined(__x86_64__) || defined(_M_X64)
#include <immintrin.h>
#define VENGINE_INTRIN_PAUSE() _mm_pause()
#elif defined(_M_X64)
#include <windows.h>
#define VENGINE_INTRIN_PAUSE() YieldProcessor()
#elif defined(__aarch64__)
#define VENGINE_INTRIN_PAUSE() asm volatile("isb"_sv)
#else
#define VENGINE_INTRIN_PAUSE() std::this_thread::yield()
#endif
namespace vstd {

void spin_mutex::pause() {
	VENGINE_INTRIN_PAUSE();
}

void spin_mutex::lock() noexcept {
	while (flag.test_and_set(std::memory_order::acquire)) {
		VENGINE_INTRIN_PAUSE();
		while (flag.test(std::memory_order::relaxed)) {
			std::this_thread::yield();
		}
	}
}

bool spin_mutex::try_lock() noexcept {
	return !flag.test_and_set(std::memory_order::acquire);
}

spin_mutex::spin_mutex() {
	flag.clear(std::memory_order::release);
}
bool spin_mutex::isLocked() const noexcept {
	return flag.test(std::memory_order::relaxed);
}
void spin_mutex::unlock() noexcept {
	flag.clear(std::memory_order::release);
}
spin_shared_mutex::spin_shared_mutex() noexcept {}
void spin_shared_mutex::lock() noexcept {
	writeMtx.lock();
}
void spin_shared_mutex::unlock() noexcept {
	writeMtx.unlock();
}
void spin_shared_mutex::lock_shared() noexcept {
	auto readCount = this->readCount.fetch_add(1, std::memory_order_relaxed);
	if(readCount == 0){
		writeMtx.lock();
	}
}
void spin_shared_mutex::unlock_shared() noexcept {
	auto readCount = this->readCount.fetch_sub(1, std::memory_order_relaxed);
	if(readCount == 1){
		writeMtx.unlock();
	}
}
}// namespace vstd
#undef VENGINE_INTRIN_PAUSE