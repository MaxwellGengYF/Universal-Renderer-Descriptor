#pragma once
#include <Common/Common.h>
#include <Common/VObject.h>
namespace toolhub::net {
template<typename T>
class Future {
private:
	struct Data {
		vstd::optional<T> opt;
		std::mutex mtx;
		std::condition_variable cv;
	};
	vstd::ObjectPtr<Data> value;

	void Wait() const {
		Data& data = *value;
		std::unique_lock<std::mutex> lck(data.mtx);
		while (!data.opt) {
			data.cv.wait(lck);
		}
	}

public:
	Future()
		: value(vstd::MakeObjectPtr(new Data())) {
	}
	Future(Future const& v) = default;
	Future(Future&& v) = default;
	template<typename... Args>
	void Set(Args&&... args) const {
		Data& data = *value;
		{
			std::lock_guard lck(data.mtx);
			data.opt.New(std::forward<Args>(args)...);
		}
		data.cv.notify_all();
	}
	bool IsComplete() const {
		Data& data = *value;
		std::lock_guard lck(data.mtx);
		return data.opt;
	}
	T Get() const& {
		Wait();
		return *(value->opt);
	}
	T Get() && {
		Wait();
		return std::move(*(value->opt));
	}
};
}// namespace toolhub::net