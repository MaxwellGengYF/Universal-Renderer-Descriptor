#pragma once
#include <Utility/BuddyAllocator.h>
#include <Common/LinkedList.h>

class VENGINE_DLL_COMMON ElementAllocator : public vstd::IOperatorNewBase{
public:
	struct AllocatedElement {
		BuddyNode* parentNode;
		LinkedNode<AllocatedElement>* brother;
		uint64_t offset;
		uint64_t size;
		bool avaliable;
	};
	struct AllocateHandle {
		friend class ElementAllocator;

	private:
		LinkedNode<AllocatedElement>* node;
		inline AllocateHandle(
			LinkedNode<AllocatedElement>* node) : node(node) {
		}

	public:
		inline AllocateHandle(AllocateHandle const& ele) {
			node = ele.node;
		}
		inline void operator=(AllocateHandle const& ele) {
			node = ele.node;
		}
		inline AllocateHandle() : node(nullptr) {}
		template<typename T>
		T* GetBlockResource() const {
			return node->obj.parentNode->GetBlockResource<T>();
		}
		inline uint64_t GetPosition() const {
			return node->obj.offset + node->obj.parentNode->GetPosition();
		}
		inline operator bool() const {
			return node;
		}
		inline bool operator!() const {
			return !operator bool();
		}
	};

private:
	vstd::StackObject<BuddyAllocator> buddyAlloc;
	vstd::Pool<LinkedNode<AllocatedElement>> bdyNodePool;
	vstd::vector<LinkedList<AllocatedElement>> linkLists;
	size_t maxSize;

public:
	ElementAllocator(
		size_t fullSize,
		vstd::function<void*(uint64_t)>&& blockConstructor,
		vstd::function<void(void*)>&& resourceBlockDestructor);
	size_t MaxSize() const { return maxSize; }
	AllocateHandle Allocate(size_t size);
	void Release(AllocateHandle handle);
	~ElementAllocator();

	KILL_COPY_CONSTRUCT(ElementAllocator)
};
