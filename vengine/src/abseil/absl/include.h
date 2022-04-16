#pragma once
#define ABSL_CONSUME_DLL
#include <absl/container/flat_hash_map.h>
#include <absl/container/flat_hash_set.h>
#include <absl/container/internal/raw_hash_set.h>
#include <Common/Hash.h>
#include <Common/Memory.h>
namespace vstd {
template<typename K, typename V, typename Hash = vstd::hash<K>,
		 typename Eq = absl::container_internal::hash_default_eq<K>,
		 typename Allocator = vstd::allocator<std::pair<const K, V>>>
using flat_hash_map = absl::flat_hash_map<K, V, Hash, Eq, Allocator>;

template<typename K, typename Hash = vstd::hash<K>,
		 typename Eq = absl::container_internal::hash_default_eq<K>,
		 typename Allocator = vstd::allocator<const K>>
using flat_hash_set = absl::flat_hash_set<K, Hash, Eq, Allocator>;

};// namespace vstd