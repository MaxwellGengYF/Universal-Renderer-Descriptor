#pragma once
template <typename T>
class LinearAllocator {
public:
	struct AllocatedChunk {
		friend class LinearAllocator<T>;
	private:
		T* ptr;
		uint64 offset;
		uint64 byteSize;
	public:
		T* GetPtr() const {
			return ptr;
		}
		uint64 GetOffset() const {
			return offset;
		}
		uint64 GetByteSize() const {
			return byteSize;
		}
	};
private:
	struct Buffer {
		std::unique_ptr<T> bufferPtr;
		uint64 offset = 0;
		Buffer(T* ptr)
			: bufferPtr(ptr) {}
		Buffer(Buffer&& o)
			: bufferPtr(std::move(o.bufferPtr)),
			offset(o.offset)
		{
		}
	};
	vstd::function<T* (size_t)> constructFunc;
	vstd::vector<AllocatedChunk> allocatedChunks;
	vstd::vector<Buffer> allocatedBuffer;
	uint64 maxAllocateSize;
public:
	LinearAllocator(
		vstd::function<T* (size_t)>&& constructFunc,
		uint64 maxAllocateSize)
		: maxAllocateSize(maxAllocateSize),
		constructFunc(std::move(constructFunc)) {
	}
	AllocatedChunk Allocate(size_t size) {
		if (size > maxAllocateSize) {
			AllocatedChunk nullChunk;
			nullChunk.ptr = nullptr;
			nullChunk.offset = 0;
			nullChunk.byteSize = 0;
			return nullChunk;
		}
		//Return existed
		if (!allocatedChunks.empty()) {
			for (auto ite = allocatedChunks.end() - 1;; --ite) {
				const AllocatedChunk i = *ite;
				if (i.byteSize >= size) {
					auto lastIte = allocatedChunks.end() - 1;
					if (ite != lastIte)
						*ite = *lastIte;
					allocatedChunks.erase(lastIte);
					return i;
				}
				if (ite == allocatedChunks.begin()) break;
			}
		}
		if (allocatedBuffer.empty()) {
			allocatedBuffer.emplace_back(constructFunc(size));
		}
		uint64 leftedValue = maxAllocateSize - (allocatedBuffer.end() - 1)->offset;
		if (leftedValue < size) {
			if (leftedValue > 0) {
				auto lastIte = allocatedBuffer.end() - 1;
				auto&& i = *lastIte;
				AllocatedChunk chunk;
				chunk.ptr = i.bufferPtr.get();
				chunk.offset = i.offset;
				chunk.byteSize = size;
				allocatedChunks.push_back(chunk);
				i.offset = maxAllocateSize;
			}
			allocatedBuffer.emplace_back(constructFunc(size));
		}
		auto lastIte = allocatedBuffer.end() - 1;
		auto&& i = *lastIte;
		AllocatedChunk chunk;
		chunk.ptr = i.bufferPtr.get();
		chunk.offset = i.offset;
		chunk.byteSize = size;
		i.offset += size;
		return chunk;
	}
	void Dispose(AllocatedChunk const& chunk) {
		if (chunk.byteSize == 0) return;
		allocatedChunks.push_back(chunk);
	}
};

template <typename T>
using LinearAllocatorChunk = typename LinearAllocator<T>::AllocatedChunk;