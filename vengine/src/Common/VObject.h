#pragma once
#include <VEngineConfig.h>
#include <atomic>
#include <mutex>
#include <Common/functional.h>
#include <Common/MetaLib.h>
#include <assert.h>
#include <Common/vector.h>
#include <Common/Memory.h>
#include <Common/Log.h>
#include <Common/spin_mutex.h>
#include <Common/HashMap.h>

class PtrLink;
namespace vstd{
class VObject;
}
namespace vstd {
class VENGINE_DLL_COMMON VObjectClass : public IOperatorNewBase {
protected:
	template<typename OT, typename T>
	void AddType() {
		allowCastClass.Emplace(typeid(T), [](VObject* vobj) -> void* {
			return static_cast<T*>(static_cast<OT*>(vobj));
		});
	}

	std::atomic<VObjectClass const*> baseLevel;
	spin_mutex setBaseLock;
	static void* M_GetCastPtr(VObjectClass const* cls, VObject* ptr, Type t);

public:
	VObjectClass* operator->() {
		return this;
	}
	template<typename SelfType>
	struct TypeAdder {
		template<typename... Types>
		static void AddTypes(VObjectClass* o) {
			o->allowCastClass.Emplace(typeid(SelfType), [](VObject* vobj) -> void* {
				return static_cast<SelfType*>(vobj);
			});
			if constexpr (sizeof...(Types) > 0) {
				auto&& v = {(o->AddType<SelfType, Types>(), 0)...};
			}
		}
	};
	HashMap<Type, funcPtr_t<void*(VObject*)>, hash<Type>, compare<Type>, VEngine_AllocType::Default> allowCastClass;
	VObjectClass();
	VObjectClass(Type type, funcPtr_t<void*(VObject*)>);
	~VObjectClass();
	VObjectClass* SetBase(VObjectClass const* base);
	template<typename T>
	static T* GetCastPtr(VObjectClass const* cls, VObject* ptr) {
		return reinterpret_cast<T*>(M_GetCastPtr(cls, ptr, typeid(T)));
	}
};

//__VA_ARGS__
#define REGIST_VOBJ_CLASS(CLSNAME, ...)                                 \
	static VObjectClass* Get##CLSNAME##Cls_() {                         \
		static VObjectClass vobj;                                       \
		VObjectClass::TypeAdder<CLSNAME>::AddTypes<__VA_ARGS__>(&vobj); \
		return &vobj;                                                   \
	}                                                                   \
	static VObjectClass* CLSNAME##Cls_ = Get##CLSNAME##Cls_();

#define SET_VOBJ_CLASS(CLSNAME) originClassDesc = CLSNAME##Cls_->SetBase(originClassDesc)

class VENGINE_DLL_COMMON VObject : public IOperatorNewBase {
	friend class PtrLink;

private:
	vector<function<void(VObject*)>> disposeFuncs;
	static std::atomic<size_t> CurrentID;
	size_t instanceID;

protected:
	VObjectClass const* originClassDesc = nullptr;
	VObject();

public:
	Type GetType() const noexcept {
		return typeid(*const_cast<VObject*>(this));
	}
	template<typename T>
	T* GetInterface() {
		return VObjectClass::GetCastPtr<T>(originClassDesc, this);
	}
	template<typename T>
	T const* GetInterface() const {
		return VObjectClass::GetCastPtr<T>(originClassDesc, const_cast<VObject*>(this));
	}
	template<typename T>
	size_t GetInterfaceOffset() const {
		return reinterpret_cast<size_t>(VObjectClass::GetCastPtr<T>(originClassDesc, reinterpret_cast<VObject*>(0x7fffffff))) - (size_t)0x7fffffff;
	}

	void AddEventBeforeDispose(function<void(VObject*)>&& func) noexcept;
	size_t GetInstanceID() const noexcept { return instanceID; }
	virtual ~VObject() noexcept;
	KILL_COPY_CONSTRUCT(VObject)
	VObject(VObject&& v) = delete;
};
}// namespace vstd
#include <Common/ObjectPtr.h>