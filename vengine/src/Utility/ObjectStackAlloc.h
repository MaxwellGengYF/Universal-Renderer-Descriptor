#pragma once
#include <Common/Common.h>
#include <Utility/StackAllocator.h>
#include <span>
namespace vstd {
class VENGINE_DLL_COMMON ObjectStackAlloc {
	struct AllocChunk {
		void* ptr;
		vstd::funcPtr_t<void(void*)> disposer;
	};
	template<typename T>
	struct Span {
		size_t size;
		T* ptr() const {
			size_t ptr = reinterpret_cast<size_t>(this) + sizeof(size_t);
			ptr = (ptr + (alignof(T) - 1)) & ~(alignof(T) - 1);
			return reinterpret_cast<T*>(ptr);
		}
		vstd::span<T> span() const {
			return {ptr(), size};
		}
		Span(Span const&) = delete;
		Span(Span&&) = delete;
		Span() = delete;
		~Span() = delete;
	};
	vstd::vector<AllocChunk> ptr;
	struct Visitor : public StackAllocatorVisitor {
		uint64 Allocate(uint64 size) override {
			return reinterpret_cast<uint64>(vengine_malloc(size));
		}
		void DeAllocate(uint64 handle) override {
			vengine_free(reinterpret_cast<void*>(handle));
		}
	};
	Visitor visitor;
	StackAllocator alloc;

public:
	ObjectStackAlloc(
		uint64 initByteSize);
	~ObjectStackAlloc();
	void* AllocateMemory(size_t size, size_t align, vstd::funcPtr_t<void(void*)> disposer);
	void Clear();
	template<typename T, typename ... Args>
	requires(std::is_constructible_v<T, Args&&...>)
	T* Allocate(Args&&... args) {
		auto type = reinterpret_cast<T*>(
			AllocateMemory(sizeof(T), alignof(T), [](void* ptr) {
				reinterpret_cast<T*>(ptr)->~T();
			}));
		new (type) T(std::forward<Args>(args)...);
		return type;
	}
	template<typename T>
	Span<T> const* AllocateSpan(size_t count, vstd::funcPtr_t<void(vstd::span<T>)> constructor) {
		size_t allocateSize = sizeof(T) * count + sizeof(size_t);
		allocateSize = (allocateSize + (alignof(T) - 1)) & ~(alignof(T) - 1);
		auto result = reinterpret_cast<Span<T>*>(
			AllocateMemory(allocateSize, alignof(T), [](void* ptr) {
				auto span = reinterpret_cast<Span<T>*>(ptr);
				auto valuePtr = span->ptr();
				for (auto&& i : ptr_range(valuePtr, valuePtr + span->size)) {
					i.~T();
				}
			}));
		result->size = count;
		constructor({result->ptr(), count});
		return result;
	}
};
}// namespace vstd