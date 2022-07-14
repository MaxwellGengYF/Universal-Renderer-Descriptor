#pragma once
#include <VEngineConfig.h>
#include <atomic>
namespace vstd {

class VENGINE_DLL_COMMON spin_mutex final {
private:
	std::atomic_flag flag;

public:
	bool try_lock() noexcept;
	void lock() noexcept;
	spin_mutex() noexcept;
	bool isLocked() const noexcept;
	void unlock() noexcept;
	static void pause();
};
class VENGINE_DLL_COMMON spin_shared_mutex final {
	spin_mutex writeMtx;
	std::atomic_size_t readCount = 0;

public:
	spin_shared_mutex() noexcept;
	void lock() noexcept;
	void unlock() noexcept;
	void lock_shared() noexcept;
	void unlock_shared() noexcept;
};
}// namespace vstd