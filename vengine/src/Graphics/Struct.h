#pragma once
#include <Common/Common.h>
#include <Common/DXMath/DXMath.h>
#include <Utility/VGuid.h>
namespace toolhub::db {
class IJsonDict;
}
namespace toolhub {
template<typename T>
struct Span {
	using type = T;
	T* ptr;
	uint64 count;
};
template<>
struct Span<char> {
	using type = char;
	char const* ptr;
	uint64 count;
	operator vstd::string_view() const {
		return vstd::string_view(ptr, count);
	}
	Span() {}
	Span(char const* ptr, uint64 v)
		: ptr(ptr), count(v) {}
	Span(vstd::string const& str) 
	: ptr(str.data()), count(str.size()) {

	}
	Span(vstd::string_view const& str)
		: ptr(str.data()), count(str.size()) {
	}
};
struct MeshData {
	Span<float3> position;
	Span<float3> normal;
	Span<float4> tangent;
	Span<float2> uv[8];
	Span<uint4> skinIndices;
	Span<float4> skinWeights;
};
struct Component {
	vstd::StackObject<vstd::Guid> guid;
	db::IJsonDict* dict;
};
struct Transform {
	vstd::StackObject<vstd::Guid> guid;
	float3 position;
	float4 rotation;
	float3 localScale;
	vstd::StackObject<vstd::Guid>* child;
	uint64 childCount;
};
struct GameObject {
	vstd::StackObject<vstd::Guid> guid;
	Component* comps;
	uint64 compCount;
};
}// namespace toolhub::unity