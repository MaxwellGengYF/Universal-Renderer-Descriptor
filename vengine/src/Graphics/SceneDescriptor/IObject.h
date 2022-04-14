#pragma once
#include <Common/Common.h>
namespace toolhub {
class Object : public vstd::ISelfPtr {
public:
	enum class Tag {
		Undefined,
		Entity,
		Component
	};
	virtual Tag GetTag() const { return Tag::Undefined; }
	virtual ~Object() = default;
};
}// namespace toolhub