#pragma once
#include <VEngineConfig.h>
#include <Common/Common.h>
struct ActorPointer {
	void* ptr;
	void (*disposer)(void*);
	void operator=(ActorPointer const& p);
	~ActorPointer();
	ActorPointer(){};
	ActorPointer(
		void* ptr,
		void (*disposer)(void*)) : ptr(ptr), disposer(disposer) {}
};
class VENGINE_DLL_COMMON Actor final : public vstd::IOperatorNewBase
{
private:
	
	vstd::HashMap<vstd::Type, ActorPointer> hash;
	void* GetComponent(vstd::Type t) const;
	void RemoveComponent(vstd::Type t);
	void SetComponent(vstd::Type t, void* ptr, void(*disposer)(void*));
	mutable vstd::spin_mutex mtx;

public:
	Actor();
	Actor(uint32_t initComponentCapacity);
	~Actor();
	template <typename T>
	T* GetComponent() const
	{
		return reinterpret_cast<T*>(GetComponent(typeid(T)));
	}
	template <typename T>
	void RemoveComponent()
	{
		RemoveComponent(typeid(T));
	}
	template <typename T>
	void SetComponent(T* ptr)
	{
		SetComponent(
			typeid(T),
			ptr,
			[](void* pp)->void
			{
				delete ((T*)pp);
			}
		);
	}
	KILL_COPY_CONSTRUCT(Actor)
};

class VENGINE_DLL_COMMON ActorSingleThread final : public vstd::IOperatorNewBase {
private:
	vstd::HashMap<vstd::Type, ActorPointer> hash;
	void* GetComponent(vstd::Type t) const;
	void RemoveComponent(vstd::Type t);
	void SetComponent(vstd::Type t, void* ptr, void (*disposer)(void*));

public:
	ActorSingleThread();
	ActorSingleThread(uint32_t initComponentCapacity);
	~ActorSingleThread();
	template<typename T>
	T* GetComponent() const {
		return reinterpret_cast<T*>(GetComponent(typeid(T)));
	}
	template<typename T>
	void RemoveComponent() {
		RemoveComponent(typeid(T));
	}
	template<typename T>
	void SetComponent(T* ptr) {
		SetComponent(
			typeid(T),
			ptr,
			[](void* pp) -> void {
				delete ((T*)pp);
			});
	}
	KILL_COPY_CONSTRUCT(ActorSingleThread)
};