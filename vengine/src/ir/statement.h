#pragma once
#include "VEngineConfig.h"
#include "ir/var.h"
#include <ir/allocatable.h>
#include <Common/Common.h>
namespace luisa::ir {

enum struct CallOp : uint32_t {
	ALL,
	ANY,

	SELECT,
	CLAMP,
	LERP,
	STEP,

	ABS,
	MIN,
	MAX,

	CLZ,
	CTZ,
	POPCOUNT,
	REVERSE,

	ISINF,
	ISNAN,

	ACOS,
	ACOSH,
	ASIN,
	ASINH,
	ATAN,
	ATAN2,
	ATANH,

	COS,
	COSH,
	SIN,
	SINH,
	TAN,
	TANH,

	EXP,
	EXP2,
	EXP10,
	LOG,
	LOG2,
	LOG10,
	POW,

	SQRT,
	RSQRT,

	CEIL,
	FLOOR,
	FRACT,
	TRUNC,
	ROUND,

	FMA,
	COPYSIGN,

	CROSS,
	DOT,
	LENGTH,
	LENGTH_SQUARED,
	NORMALIZE,
	FACEFORWARD,

	DETERMINANT,
	TRANSPOSE,
	INVERSE,

	SYNCHRONIZE_BLOCK,

	ATOMIC_EXCHANGE,		/// [(atomic_ref, desired) -> old]: stores desired, returns old.
	ATOMIC_COMPARE_EXCHANGE,/// [(atomic_ref, expected, desired) -> old]: stores (old == expected ? desired : old), returns old.
	ATOMIC_FETCH_ADD,		/// [(atomic_ref, val) -> old]: stores (old + val), returns old.
	ATOMIC_FETCH_SUB,		/// [(atomic_ref, val) -> old]: stores (old - val), returns old.
	ATOMIC_FETCH_AND,		/// [(atomic_ref, val) -> old]: stores (old & val), returns old.
	ATOMIC_FETCH_OR,		/// [(atomic_ref, val) -> old]: stores (old | val), returns old.
	ATOMIC_FETCH_XOR,		/// [(atomic_ref, val) -> old]: stores (old ^ val), returns old.
	ATOMIC_FETCH_MIN,		/// [(atomic_ref, val) -> old]: stores min(old, val), returns old.
	ATOMIC_FETCH_MAX,		/// [(atomic_ref, val) -> old]: stores max(old, val), returns old.

	BUFFER_READ,  /// [(buffer, index) -> value]: reads the index-th element in buffer
	BUFFER_WRITE, /// [(buffer, index, value) -> void]: writes value into the index-th element of buffer
	TEXTURE_READ, /// [(texture, coord) -> value]
	TEXTURE_WRITE,/// [(texture, coord, value) -> void]

	BINDLESS_TEXTURE2D_SAMPLE,		//(bindless_array, index: uint, uv: float2): float4
	BINDLESS_TEXTURE2D_SAMPLE_LEVEL,//(bindless_array, index: uint, uv: float2, level: float): float4
	BINDLESS_TEXTURE2D_SAMPLE_GRAD, //(bindless_array, index: uint, uv: float2, ddx: float2, ddy: float2): float4
	BINDLESS_TEXTURE3D_SAMPLE,		//(bindless_array, index: uint, uv: float3): float4
	BINDLESS_TEXTURE3D_SAMPLE_LEVEL,//(bindless_array, index: uint, uv: float3, level: float): float4
	BINDLESS_TEXTURE3D_SAMPLE_GRAD, //(bindless_array, index: uint, uv: float3, ddx: float3, ddy: float3): float4
	BINDLESS_TEXTURE2D_READ,		//(bindless_array, index: uint, coord: uint2): float4
	BINDLESS_TEXTURE3D_READ,		//(bindless_array, index: uint, coord: uint3): float4
	BINDLESS_TEXTURE2D_READ_LEVEL,	//(bindless_array, index: uint, coord: uint2, level: uint): float4
	BINDLESS_TEXTURE3D_READ_LEVEL,	//(bindless_array, index: uint, coord: uint3, level: uint): float4
	BINDLESS_TEXTURE2D_SIZE,		//(bindless_array, index: uint): uint2
	BINDLESS_TEXTURE3D_SIZE,		//(bindless_array, index: uint): uint3
	BINDLESS_TEXTURE2D_SIZE_LEVEL,	//(bindless_array, index: uint, level: uint): uint2
	BINDLESS_TEXTURE3D_SIZE_LEVEL,	//(bindless_array, index: uint, level: uint): uint3

	BINDLESS_BUFFER_READ,//(bindless_array, index: uint): expr->type()

	MAKE_BOOL2,
	MAKE_BOOL3,
	MAKE_BOOL4,
	MAKE_INT2,
	MAKE_INT3,
	MAKE_INT4,
	MAKE_UINT2,
	MAKE_UINT3,
	MAKE_UINT4,
	MAKE_FLOAT2,
	MAKE_FLOAT3,
	MAKE_FLOAT4,

	MAKE_FLOAT2X2,
	MAKE_FLOAT3X3,
	MAKE_FLOAT4X4,

	// optimization hints
	ASSUME,
	UNREACHABLE,

	INSTANCE_TO_WORLD_MATRIX,
	SET_ACCEL_TRANFORM,
	SET_ACCEL_VISIBILITY,
	SET_ACCEL_TRANSFORM_VISIBILITY,
	TRACE_CLOSEST,
	TRACE_ANY
};
enum struct UnaryOp : uint32_t {
	PLUS,
	MINUS,	// +x, -x
	NOT,	// !x
	BIT_NOT,// ~x
};

/**
 * @brief Enum of binary operations
 * 
 */
enum struct BinaryOp : uint32_t {

	// arithmetic
	ADD,
	SUB,
	MUL,
	DIV,
	MOD,
	BIT_AND,
	BIT_OR,
	BIT_XOR,
	SHL,
	SHR,
	AND,
	OR,

	// relational
	LESS,
	GREATER,
	LESS_EQUAL,
	GREATER_EQUAL,
	EQUAL,
	NOT_EQUAL
};
struct Var;
struct Argument {
	Type const* type = nullptr;
	Var const* var = nullptr;
	Argument(
		Type const* t,
		Var const* v)
		: type(t),
		  var(v) {}
	Argument() {}
	operator bool() const{
		return var == nullptr || type == nullptr;
	}
};
struct LiteralVar;
struct Statement : public Allocatable {
	enum class Tag : uint8_t {
		Unary,
		Binary,
		Return,
		Break,
		Continue,
		BuiltinCall,
		CustomCall,
		Member,
		Access,
		If,
		Loop,
		Switch
	};
	virtual Tag tag() const = 0;
	virtual ~Statement() = default;
};
enum class ReadWrite : bool {
	Read,
	Write
};
struct UnaryStmt : public Statement {
	Var const* dst;
	Argument lhs;
	UnaryOp op;
	Tag tag() const override { return Tag::Unary; }
};
struct BinaryStmt : public Statement {
	Var const* dst;
	Argument lhs;
	Argument rhs;
	BinaryOp op;
	Tag tag() const override { return Tag::Binary; }
};

struct ReturnStmt : public Statement {
	Tag tag() const override { return Tag::Return; }
};
struct BreakStmt : public Statement {
	Tag tag() const override { return Tag::Break; }
};
struct ContinueStmt : public Statement {
	Tag tag() const override { return Tag::Continue; }
};
struct BuiltinCallStmt : public Statement {
	CallOp op;
	Argument dst;
	vstd::span<Argument> args;
	Tag tag() const override { return Tag::BuiltinCall; }
};
struct CustomCallStmt : public Statement {
	Argument dst;
	uint64 callableIndex;
	vstd::span<Argument> args;
	Tag tag() const override { return Tag::CustomCall; }
};
struct MemberStmt : public Statement {
	Var const* dst;
	Var const* src;
	uint64 indices;
	ReadWrite rwState;
	Tag tag() const override { return Tag::Member; }
};
struct AccessStmt : public Statement {
	Var const* dst;
	Var const* src;
	Var const* indices;
	ReadWrite rwState;
	Tag tag() const override { return Tag::Access; }
};
struct IfStmt : public Statement {
	Var const* condition;
	vstd::span<Statement const*> trueField;
	vstd::span<Statement const*> falseField;
	Tag tag() const override { return Tag::If; }
};
struct LoopStmt : public Statement {
	Var const* condition;
	vstd::span<Statement const*> commands;
	Tag tag() const override { return Tag::Loop; }
};

struct SwitchStmt : public Statement {
	vstd::HashMap<int32_t, vstd::span<Statement const*>> cases;
	Tag tag() const override { return Tag::Switch; }
};
}// namespace luisa::ir