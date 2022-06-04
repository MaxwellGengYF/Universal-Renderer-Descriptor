#pragma once
#include "VEngineConfig.h"
#include <ir/api/var.h>
#include <ir/api/allocatable.h>
#include <Common/Common.h>
namespace toolhub::ir {

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
	TRACE_ANY,
};
static constexpr size_t CallOpCount = static_cast<size_t>(CallOp::TRACE_ANY) + 1;
enum struct UnaryOp : uint32_t {
	PLUS,
	MINUS,	// +x, -x
	NOT,	// !x
	BIT_NOT,// ~x
	CAST, //(float)x
};
static constexpr size_t UnaryOpCount = static_cast<size_t>(UnaryOp::CAST) + 1;
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
static constexpr size_t BinaryOpCount = static_cast<size_t>(BinaryOp::NOT_EQUAL) + 1;

struct Var;
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
	virtual bool valid() const = 0;
	virtual Tag tag() const = 0;
	virtual ~Statement() = default;
};
enum class ReadWrite : bool {
	Read,
	Write
};
struct UnaryStmt : public Statement {
	Var const* dst;
	Var const* lhs;
	UnaryOp op;
	Tag tag() const override { return Tag::Unary; }
	bool valid() const override;
};
struct BinaryStmt : public Statement {
	Var const* dst;
	Var const* lhs;
	Var const* rhs;
	BinaryOp op;
	Tag tag() const override { return Tag::Binary; }
	bool valid() const override;
};

struct ReturnStmt : public Statement {
	Var const* retValue;
	Tag tag() const override { return Tag::Return; }
	bool valid() const override;
};
struct BreakStmt : public Statement {
	Tag tag() const override { return Tag::Break; }
	bool valid() const override;
};
struct ContinueStmt : public Statement {
	Tag tag() const override { return Tag::Continue; }
	bool valid() const override;
};
struct BuiltinCallStmt : public Statement {
	CallOp op;
	Var const* dst;
	vstd::span<Var const*> args;
	Tag tag() const override { return Tag::BuiltinCall; }
	bool valid() const override;
};
struct CustomCallStmt : public Statement {
	Var const* dst;
	size_t callableIndex;
	vstd::span<Var const*> args;
	Tag tag() const override { return Tag::CustomCall; }
	bool valid() const override;
};
struct MemberStmt : public Statement {
	Var const* dst;
	Var const* src;
	vstd::span<uint64> indices;
	ReadWrite rwState;
	Tag tag() const override { return Tag::Member; }
	bool valid() const override;
};
struct AccessStmt : public Statement {
	Var const* dst;
	Var const* src;
	vstd::variant<Var const*, uint64> index;
	ReadWrite rwState;
	Tag tag() const override { return Tag::Access; }
	bool valid() const override;
};
struct IfStmt : public Statement {
	Var const* condition;
	vstd::vector<Statement const*> trueField;
	vstd::vector<Statement const*> falseField;
	Tag tag() const override { return Tag::If; }
	bool valid() const override;
};
struct LoopStmt : public Statement {
	Var const* condition;
	vstd::vector<Statement const*> commands;
	Tag tag() const override { return Tag::Loop; }
	bool valid() const override;
};
struct SwitchStmt : public Statement {
	using Case = std::pair<int32_t, vstd::vector<Statement const*>>;
	Var const* condition;
	vstd::vector<Case> cases;
	Tag tag() const override { return Tag::Switch; }
	bool valid() const override;
};
}// namespace toolhub::ir