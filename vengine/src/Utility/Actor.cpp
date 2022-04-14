
#include <Utility/Actor.h>
ActorPointer::~ActorPointer() {
	disposer(ptr);
}
void* Actor::GetComponent(vstd::Type t) const {
	std::lock_guard lck(mtx);
	auto ite = hash.Find(t);
	if (ite) return ite.Value().ptr;
	return nullptr;
}
void ActorPointer::operator=(ActorPointer const& p) {
	disposer(ptr);
	ptr = p.ptr;
	disposer = p.disposer;
}
void Actor::RemoveComponent(vstd::Type t) {
	std::lock_guard lck(mtx);
	hash.Remove(t);
}
void Actor::SetComponent(vstd::Type t, void* ptr, void (*disposer)(void*)) {
	std::lock_guard lck(mtx);
	auto&& ite = hash.Emplace(t).Value();
	ite.ptr = ptr;
	ite.disposer = disposer;
}
Actor::Actor() {}
Actor::Actor(uint32_t initComponentCapacity) : hash(initComponentCapacity) {}
Actor::~Actor() {}

void* ActorSingleThread::GetComponent(vstd::Type t) const {
	auto ite = hash.Find(t);
	if (ite) return ite.Value().ptr;
	return nullptr;
}
void ActorSingleThread::RemoveComponent(vstd::Type t) {
	hash.Remove(t);
}
void ActorSingleThread::SetComponent(vstd::Type t, void* ptr, void (*disposer)(void*)) {
	auto&& ite = hash.Emplace(t).Value();
	ite.ptr = ptr;
	ite.disposer = disposer;
}
ActorSingleThread::ActorSingleThread() {}
ActorSingleThread::ActorSingleThread(uint32_t initComponentCapacity) : hash(initComponentCapacity) {}
ActorSingleThread::~ActorSingleThread() {}