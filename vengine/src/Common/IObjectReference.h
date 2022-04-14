#pragma once
#include <Common/MetaLib.h>
class vstd::VObject;
class IObjectReference {
public:
	template<typename T>
	int64 GetVObjectPtrOffset() const {
		size_t thisPtr = reinterpret_cast<size_t>(static_cast<vstd::PureType_t<T> const*>(this));
		size_t vobjPtr = reinterpret_cast<size_t>(GetVObjectPtr());
		return static_cast<int64>(thisPtr - vobjPtr);
	}
	virtual vstd::VObject* GetVObjectPtr() = 0;
	virtual vstd::VObject const* GetVObjectPtr() const = 0;
};

#define VENGINE_IOBJREF_OVERRIDE          \
	vstd::VObject* GetVObjectPtr() override {             \
		return this;                                \
	}                                               \
	vstd::VObject const* GetVObjectPtr() const override { \
		return this;                                \
	}