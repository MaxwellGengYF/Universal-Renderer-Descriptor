#pragma once
#include <Common/Common.h>
#include <ir/basic_types.h>>
#include <ir/allocatable.h>
namespace luisa::ir {
using basic_types = std::tuple<
	bool, float, int, uint,
	bool2, float2, int2, uint2,
	bool3, float3, int3, uint3,
	bool4, float4, int4, uint4,
	float2x2, float3x3, float4x4>;
namespace detail {
template<typename T>
struct make_literal_value {
	static_assert(always_false_v<T>);
};

template<typename... T>
struct make_literal_value<std::tuple<T...>> {
	using type = vstd::variant<T...>;
};

template<typename T>
using make_literal_value_t = typename make_literal_value<T>::type;
template<typename T>
struct constant_data_view {
	static_assert(always_false_v<T>);
};

template<typename... T>
struct constant_data_view<std::tuple<T...>> {
	using type = vstd::variant<vstd::span<const T>...>;
};

template<typename T>
using constant_data_view_t = typename constant_data_view<T>::type;
}// namespace detail
struct Type {
	enum class Tag : uint8_t {
		Bool,
		Float,
		Int,
		UInt,

		Vector,
		Matrix,

		Array,
		Structure,

		Buffer,
		Texture,
		BindlessArray,
		Accel,
	};
	Tag tag;
	size_t size = 0;
	size_t alignment = 0;
	uint dimension = 0;
	Type const* element = nullptr;
	vstd::span<Type const*> members;
};
enum class Usage : vbyte {
	Read,
	ReadWrite
};
struct Var : public Allocatable {
	enum class Tag : uint8_t {
		Literal,
		Constant,
		Variable,
		Ref
	};
	Type const* type;
	uint64_t index;
	Usage usage;
	virtual Tag tag() const = 0;
	virtual ~Var() = default;
};
struct LiteralVar : public Var {
	using Value = detail::make_literal_value_t<basic_types>;
	Value literalValue;
	Tag tag() const override { return Tag::Literal; }
};
struct ConstantVar : public Var {
	using Data = detail::constant_data_view_t<basic_types>;
	Data data;
	Tag tag() const override { return Tag::Constant; }
};
struct VariableVar : public Var {
	Tag tag() const override { return Tag::Variable; }
};
struct RefVar : public Var {
	Tag tag() const override { return Tag::Ref; }
};
}// namespace luisa::ir