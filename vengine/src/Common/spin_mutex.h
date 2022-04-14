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
}// namespace vstd