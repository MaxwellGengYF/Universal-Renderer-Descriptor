#pragma once
#include <type_traits>
namespace vstd {
template<typename T>
struct MemberFunc;

template<typename Class, typename Ret, typename... Args>
class MemberFunc<Ret (Class::*)(Args...)> {
	using FuncPtrType = Ret (Class::*)(Args...);
	FuncPtrType f;

public:
	using RetType = Ret;
	using ClassType = Class;
	MemberFunc(FuncPtrType f)
		: f(f) {}
	Ret operator()(Class* ptr, Args... args) const {
		return (ptr->*f)(std::forward<Args>(args)...);
	}
	Ret operator()(Class& ptr, Args... args) const {
		return (ptr.*f)(std::forward<Args>(args)...);
	}
};
template<typename Class, typename Ret, typename... Args>
class MemberFunc<Ret (Class::*)(Args...) const> {
	using FuncPtrType = Ret (Class::*)(Args...) const;
	FuncPtrType f;

public:
	using RetType = Ret;
	using ClassType = Class const;
	MemberFunc(FuncPtrType f)
		: f(f) {}
	Ret operator()(ClassType* ptr, Args... args) const {
		return (ptr->*f)(std::forward<Args>(args)...);
	}
	Ret operator()(ClassType& ptr, Args... args) const {
		return (ptr.*f)(std::forward<Args>(args)...);
	}
};
template<typename T>
class BindMemberFunc {
	MemberFunc<T> func;
	using ClassType = typename MemberFunc<T>::ClassType;
	using RetType = typename MemberFunc<T>::RetType;
	ClassType* cls;

public:
	BindMemberFunc(ClassType* cls, T func)
		: func(func),
		  cls(cls) {}
	template<typename... Args>
	RetType operator()(Args&&... args) const {
		return func(cls, std::forward<Args>(args)...);
	}
};
template<typename T>
MemberFunc<T> MakeMemberFunc(T ptr) {
	return {ptr};
}

template<typename Class, typename T>
BindMemberFunc<T> MakeBindMemberFunc(Class* cls, T ptr) {
	return {cls, ptr};
}

}// namespace vstd