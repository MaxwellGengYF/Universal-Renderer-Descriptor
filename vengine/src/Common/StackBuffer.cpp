
#include <Common/StackAllocator.h>
#include <Common/DXMath/DXMath.h>
#include <Common/vstring.h>
#include <Common/Log.h>
namespace valloc {

struct StackData {
	static constexpr size_t VENGINE_STACK_LENGTH = 1024 * 256;
	vbyte* data = nullptr;
	vbyte* offset = nullptr;
	~StackData() {
		if (data) delete data;
	}
	template<size_t align>
	static constexpr size_t CalcAlign(size_t value) {
		return (value + (align - 1)) & ~(align - 1);
	}
	void* GetCurrent() {
		if (!data) {
			data = new vbyte[VENGINE_STACK_LENGTH];
			offset = data;
		}
		return offset;
	}
	template<size_t align>
	void* Alloc(size_t sz) {
		if (!data) {
			data = new vbyte[VENGINE_STACK_LENGTH];
			offset = data;
		}
		offset = reinterpret_cast<vbyte*>(CalcAlign<align>(reinterpret_cast<size_t>(offset)));
		void* ptr = offset;
		offset += sz;
#ifdef DEBUG
		if (reinterpret_cast<size_t>(offset) - reinterpret_cast<size_t>(data) > VENGINE_STACK_LENGTH) {
			VEngine_Log("Stack-Overflow!\n"_sv);
			VENGINE_EXIT;
		}
#endif
		return ptr;
	}
	void ReleaseTo(vbyte* result) {
		offset = reinterpret_cast<vbyte*>(
			std::min(reinterpret_cast<size_t>(offset), reinterpret_cast<size_t>(result)));
	}
};

static thread_local StackData data;
}// namespace valloc

void* StackBuffer::stack_malloc(size_t sz) {
	return valloc::data.Alloc<16>(sz);
}
void StackBuffer::stack_free(void* ptr) {
	valloc::data.ReleaseTo(reinterpret_cast<vbyte*>(ptr));
}
void* StackBuffer::GetCurrentPtr() {
	return valloc::data.GetCurrent();
}

StackBuffer::~StackBuffer() {
	if (ptr)
		valloc::data.ReleaseTo(reinterpret_cast<vbyte*>(ptr));
}

StackBuffer StackBuffer::Allocate(size_t size) {
	return StackBuffer(valloc::data.Alloc<8>(size), size);
}
StackBuffer StackBuffer::Allocate_Align16(size_t size) {
	return StackBuffer(valloc::data.Alloc<16>(size), size);
}
StackBuffer StackBuffer::Allocate_Align32(size_t size) {
	return StackBuffer(valloc::data.Alloc<32>(size), size);
}

StackBuffer::StackBuffer(StackBuffer&& stk) {
	ptr = stk.ptr;
	mLength = stk.mLength;
	stk.ptr = nullptr;
}
