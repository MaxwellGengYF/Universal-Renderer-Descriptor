#include <types/bind_desriptor.h>
#include <components/texture.h>
#include <components/buffer.h>
namespace toolhub::vk {
BindDescriptor BindResource::GetDescriptor() const {
	return res.visit_or(
		BindDescriptor{},
		[&](auto&& v) {
			return v->GetDescriptor(offset, size);
		});
}

};// namespace toolhub::vk