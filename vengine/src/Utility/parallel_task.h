#pragma once
#include <Common/Common.h>
namespace vstd {
template<typename Func>
requires(std::is_invocable_v<Func, size_t>) class ParallelTask {
	struct Pack {
		std::atomic_size_t num;
		std::atomic_size_t refCount;
		size_t count;
		std::remove_reference_t<Func> func;
	};
	Pack* p;

public:
	ParallelTask(Func&& func, size_t count) {
		p = reinterpret_cast<Pack*>(vengine_malloc(sizeof(Pack)));
		new (p) Pack{
			size_t(0),
			size_t(1),
			count,
			std::forward<Func>(func)};
	}
	ParallelTask(Func const& func, size_t count) {
		p = reinterpret_cast<Pack*>(vengine_malloc(sizeof(Pack)));
		new (p) Pack{
			size_t(0),
			size_t(1),
			count,
			func};
	}
	void operator()() const {
		while (true) {
			size_t i = p->num++;
			if (i < p->count) {
				p->func(i);
			} else {
				break;
			}
		}
	}
	ParallelTask(ParallelTask const& v) {
		p = v.p;
		if (p) p->refCount++;
	}
	ParallelTask(ParallelTask&& v) {
		p = v.p;
		v.p = nullptr;
	}
	~ParallelTask() {
		if (p) {
			auto c = --p->refCount;
			if (c == 0) vengine_delete(p);
		}
	}
};
}// namespace vstd