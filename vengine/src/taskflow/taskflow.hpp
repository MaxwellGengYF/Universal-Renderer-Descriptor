#pragma once
#include "config.h"
#include "core/executor.hpp"
#include "algorithm/critical.hpp"
#include "algorithm/for_each.hpp"
namespace tf {
template<typename C>
requires(std::is_invocable_v<C, size_t>)
	Task FlowBuilder::emplace_all(C&& callable, size_t task_count, size_t thread_count) {
	if (thread_count > 0)
		return emplace([c = std::forward<C>(callable), task_count, thread_count](tf::Subflow& f) mutable {
			vstd::ParallelTask task(std::move(c), task_count);
			if (thread_count > 1) {
				size_t last = thread_count - 1;
				for (size_t i = 0; i < last; ++i) {
					f.emplace([task] { task(); });
				}
			}
			f.emplace([t = std::move(task)] { t(); });
			f.join();
		});
	return {};
}
template<
	typename B,
	typename C,
	typename D>
requires(std::is_invocable_v<C, size_t>&& std::is_invocable_v<B>&& std::is_invocable_v<D>)
	Task FlowBuilder::emplace_all(B&& beforeTask, C&& callable, D&& afterTask, size_t task_count, size_t thread_count) {
	if (thread_count > 0)
		return emplace([b = std::forward<B>(beforeTask), c = std::forward<C>(callable), d = std::forward<D>(afterTask), task_count, thread_count](tf::Subflow& f) mutable {
			b();
			vstd::ParallelTask task(std::move(c), task_count);
			if (thread_count > 1) {
				size_t last = thread_count - 1;
				for (size_t i = 0; i < last; ++i) {
					f.emplace([task] { task(); });
				}
			}
			f.emplace([t = std::move(task)] { t(); });
			f.join();
			d();
		});
	return {};
}
}// namespace tf

/**
@dir taskflow
@brief root taskflow include dir
*/

/**
@dir taskflow/core
@brief taskflow core include dir
*/

/**
@dir taskflow/algorithm
@brief taskflow algorithms include dir
*/

/**
@dir taskflow/cuda
@brief taskflow CUDA include dir
*/

/**
@file taskflow/taskflow.hpp
@brief main taskflow include file
*/

// TF_VERSION % 100 is the patch level
// TF_VERSION / 100 % 1000 is the minor version
// TF_VERSION / 100000 is the major version

// current version: 3.4.0
#define TF_VERSION 300400

#define TF_MAJOR_VERSION TF_VERSION / 100000
#define TF_MINOR_VERSION TF_VERSION / 100 % 1000
#define TF_PATCH_VERSION TF_VERSION % 100

/**
@brief taskflow namespace
*/
namespace tf {

/**
@private
*/
namespace detail {
}

/**
@brief queries the version information in a string format @c major.minor.patch
*/
constexpr const char* version() {
	return "3.4.0";
}

}// namespace tf
