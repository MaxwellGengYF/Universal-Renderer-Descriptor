
#include "Common/MetaLib.h"
#include "Common/span.h"
#include <ir/statement_name.h>
#include <Common/functional.h>
#include <ir/kernel.h>
#include <ir/var.h>
#include <ir/statement.h>
#include <ir/command_recorder.h>
#include <ir/parser.h>
namespace luisa::ir {
static bool If(StatementName& s, StatementName::FuncCall const& funcPack) {
	auto&& recorder = s.recorder;
	auto scope = recorder.PushStack();
	scope->tag = CommandRecorder::ScopeTag::If;
	return true;
}
static bool EndIf(StatementName& s, StatementName::FuncCall const& funcPack) {
	auto&& recorder = s.recorder;
	recorder.PopStack();
	return true;
}
bool StatementName::BuiltInFunc(CallOp callOp, FuncCall const& funcPack) {
	auto stmt = recorder.objAlloc->Allocate<BuiltinCallStmt>();
	stmt->dst = {parser->GetType(funcPack.ret.typeName, funcPack.ret.typeArrSize), recorder.TryGetVar(funcPack.ret.varName)};
	if (!stmt->dst.var) return false;
	auto argSpan =
		recorder.objAlloc
			->AllocateSpan<Argument>(
				funcPack.args.size(),
				[](vstd::span<Argument> sp) {})
			->span();
	for (auto i : vstd::range(argSpan.size())) {
		auto&& argDesc = funcPack.args[i];
		argSpan[i] = {parser->GetType(argDesc.typeName, argDesc.typeArrSize), recorder.TryGetVar(argDesc.varName)};
		if (!argSpan[i]) return false;
	}
	stmt->args = argSpan;
	return true;
}
bool StatementName::BinaryOpCall(BinaryOp op, FuncCall const& funcPack) { return true; }
bool StatementName::UnaryOpCall(UnaryOp op, FuncCall const& funcPack) { return true; }
void StatementName::CustomInit() {
}
// clang-format off
//Builtin
static bool Fall(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ALL, funcPack); }
static bool Fany(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ANY, funcPack); }
static bool Fselect(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SELECT, funcPack); }
static bool Fclamp(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::CLAMP, funcPack); }
static bool Flerp(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LERP, funcPack); }
static bool Fstep(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::STEP, funcPack); }
static bool Fabs(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ABS, funcPack); }
static bool Fmin(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MIN, funcPack); }
static bool Fmax(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAX, funcPack); }
static bool Fclz(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::CLZ, funcPack); }
static bool Fctz(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::CTZ, funcPack); }
static bool Fpopcount(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::POPCOUNT, funcPack); }
static bool Freverse(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::REVERSE, funcPack); }
static bool Fisinf(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ISINF, funcPack); }
static bool Fisnan(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ISNAN, funcPack); }
static bool Facos(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ACOS, funcPack); }
static bool Facosh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ACOSH, funcPack); }
static bool Fasin(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ASIN, funcPack); }
static bool Fasinh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ASINH, funcPack); }
static bool Fatan(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATAN, funcPack); }
static bool Fatan2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATAN2, funcPack); }
static bool Fatanh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATANH, funcPack); }
static bool Fcos(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::COS, funcPack); }
static bool Fcosh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::COSH, funcPack); }
static bool Fsin(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SIN, funcPack); }
static bool Fsinh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SINH, funcPack); }
static bool Ftan(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TAN, funcPack); }
static bool Ftanh(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TANH, funcPack); }
static bool Fexp(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::EXP, funcPack); }
static bool Fexp2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::EXP2, funcPack); }
static bool Fexp10(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::EXP10, funcPack); }
static bool Flog(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LOG, funcPack); }
static bool Flog2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LOG2, funcPack); }
static bool Flog10(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LOG10, funcPack); }
static bool Fpow(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::POW, funcPack); }
static bool Fsqrt(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SQRT, funcPack); }
static bool Frsqrt(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::RSQRT, funcPack); }
static bool Fceil(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::CEIL, funcPack); }
static bool Ffloor(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::FLOOR, funcPack); }
static bool Ffract(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::FRACT, funcPack); }
static bool Ftrunc(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TRUNC, funcPack); }
static bool Fround(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ROUND, funcPack); }
static bool Ffma(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::FMA, funcPack); }
static bool Fcopysign(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::COPYSIGN, funcPack); }
static bool Fcross(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::CROSS, funcPack); }
static bool Fdot(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::DOT, funcPack); }
static bool Flength(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LENGTH, funcPack); }
static bool Flength_squared(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::LENGTH_SQUARED, funcPack); }
static bool Fnormalize(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::NORMALIZE, funcPack); }
static bool Ffaceforward(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::FACEFORWARD, funcPack); }
static bool Fdeterminant(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::DETERMINANT, funcPack); }
static bool Ftranspose(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TRANSPOSE, funcPack); }
static bool Finverse(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::INVERSE, funcPack); }
static bool Fsynchronize_block(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SYNCHRONIZE_BLOCK, funcPack); }
static bool Fatomic_exchange(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_EXCHANGE, funcPack); }
static bool Fatomic_compare_exchange(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_COMPARE_EXCHANGE, funcPack); }
static bool Fatomic_fetch_add(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_ADD, funcPack); }
static bool Fatomic_fetch_sub(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_SUB, funcPack); }
static bool Fatomic_fetch_and(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_AND, funcPack); }
static bool Fatomic_fetch_or(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_OR, funcPack); }
static bool Fatomic_fetch_xor(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_XOR, funcPack); }
static bool Fatomic_fetch_min(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_MIN, funcPack); }
static bool Fatomic_fetch_max(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ATOMIC_FETCH_MAX, funcPack); }
static bool Fbuffer_read(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BUFFER_READ, funcPack); }
static bool Fbuffer_write(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BUFFER_WRITE, funcPack); }
static bool Ftexture_read(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TEXTURE_READ, funcPack); }
static bool Ftexture_write(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TEXTURE_WRITE, funcPack); }
static bool Fbindless_texture2d_sample(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_SAMPLE, funcPack); }
static bool Fbindless_texture2d_sample_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_SAMPLE_LEVEL, funcPack); }
static bool Fbindless_texture2d_sample_grad(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD, funcPack); }
static bool Fbindless_texture3d_sample(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_SAMPLE, funcPack); }
static bool Fbindless_texture3d_sample_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_SAMPLE_LEVEL, funcPack); }
static bool Fbindless_texture3d_sample_grad(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD, funcPack); }
static bool Fbindless_texture2d_read(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_READ, funcPack); }
static bool Fbindless_texture3d_read(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_READ, funcPack); }
static bool Fbindless_texture2d_read_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_READ_LEVEL, funcPack); }
static bool Fbindless_texture3d_read_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_READ_LEVEL, funcPack); }
static bool Fbindless_texture2d_size(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_SIZE, funcPack); }
static bool Fbindless_texture3d_size(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_SIZE, funcPack); }
static bool Fbindless_texture2d_size_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE2D_SIZE_LEVEL, funcPack); }
static bool Fbindless_texture3d_size_level(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_TEXTURE3D_SIZE_LEVEL, funcPack); }
static bool Fbindless_buffer_read(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::BINDLESS_BUFFER_READ, funcPack); }
static bool Fmake_bool2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_BOOL2, funcPack); }
static bool Fmake_bool3(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_BOOL3, funcPack); }
static bool Fmake_bool4(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_BOOL4, funcPack); }
static bool Fmake_int2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_INT2, funcPack); }
static bool Fmake_int3(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_INT3, funcPack); }
static bool Fmake_int4(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_INT4, funcPack); }
static bool Fmake_uint2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_UINT2, funcPack); }
static bool Fmake_uint3(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_UINT3, funcPack); }
static bool Fmake_uint4(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_UINT4, funcPack); }
static bool Fmake_float2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT2, funcPack); }
static bool Fmake_float3(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT3, funcPack); }
static bool Fmake_float4(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT4, funcPack); }
static bool Fmake_float2x2(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT2X2, funcPack); }
static bool Fmake_float3x3(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT3X3, funcPack); }
static bool Fmake_float4x4(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::MAKE_FLOAT4X4, funcPack); }
static bool Fassume(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::ASSUME, funcPack); }
static bool Funreachable(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::UNREACHABLE, funcPack); }
static bool Finstance_to_world_matrix(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::INSTANCE_TO_WORLD_MATRIX, funcPack); }
static bool Fset_accel_tranform(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SET_ACCEL_TRANFORM, funcPack); }
static bool Fset_accel_visibility(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SET_ACCEL_VISIBILITY, funcPack); }
static bool Fset_accel_transform_visibility(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::SET_ACCEL_TRANSFORM_VISIBILITY, funcPack); }
static bool Ftrace_closest(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TRACE_CLOSEST, funcPack); }
static bool Ftrace_any(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BuiltInFunc(CallOp::TRACE_ANY, funcPack); }
// Binary
static bool Fadd(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::ADD, funcPack); }
static bool Fsub(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::SUB, funcPack); }
static bool Fmul(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::MUL, funcPack); }
static bool Fdiv(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::DIV, funcPack); }
static bool Fmod(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::MOD, funcPack); }
static bool Fbit_and(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::BIT_AND, funcPack); }
static bool Fbit_or(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::BIT_OR, funcPack); }
static bool Fbit_xor(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::BIT_XOR, funcPack); }
static bool Fshl(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::SHL, funcPack); }
static bool Fshr(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::SHR, funcPack); }
static bool Fand(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::AND, funcPack); }
static bool For(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::OR, funcPack); }
static bool Fless(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::LESS, funcPack); }
static bool Fgreater(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::GREATER, funcPack); }
static bool Fless_equal(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::LESS_EQUAL, funcPack); }
static bool Fgreater_equal(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::GREATER_EQUAL, funcPack); }
static bool Fequal(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::EQUAL, funcPack); }
static bool Fnot_equal(StatementName& s, StatementName::FuncCall const& funcPack) { return s.BinaryOpCall(BinaryOp::NOT_EQUAL, funcPack); }
// Unary
static bool Fplus(StatementName& s, StatementName::FuncCall const& funcPack) { return s.UnaryOpCall(UnaryOp::PLUS, funcPack); }
static bool Fminus(StatementName& s, StatementName::FuncCall const& funcPack) { return s.UnaryOpCall(UnaryOp::MINUS, funcPack); }
static bool Fnot(StatementName& s, StatementName::FuncCall const& funcPack) { return s.UnaryOpCall(UnaryOp::NOT, funcPack); }
static bool Fbit_not(StatementName& s, StatementName::FuncCall const& funcPack) { return s.UnaryOpCall(UnaryOp::BIT_NOT, funcPack); }

void StatementName::Init() {
#define REGIST_FUNC(FUNC, NAME) funcs.Emplace(#NAME##_sv, FUNC)
#define REGIST_NAME(TYPE, ENUM, NAME) TYPE.Emplace(ENUM, #NAME##_sv);
	//Builtin
	REGIST_FUNC(If, if);
	REGIST_FUNC(EndIf, endif);
	REGIST_FUNC(Fall, all);
	REGIST_FUNC(Fany, any);
	REGIST_FUNC(Fselect, select);
	REGIST_FUNC(Fclamp, clamp);
	REGIST_FUNC(Flerp, lerp);
	REGIST_FUNC(Fstep, step);
	REGIST_FUNC(Fabs, abs);
	REGIST_FUNC(Fmin, min);
	REGIST_FUNC(Fmax, max);
	REGIST_FUNC(Fclz, clz);
	REGIST_FUNC(Fctz, ctz);
	REGIST_FUNC(Fpopcount, popcount);
	REGIST_FUNC(Freverse, reverse);
	REGIST_FUNC(Fisinf, isinf);
	REGIST_FUNC(Fisnan, isnan);
	REGIST_FUNC(Facos, acos);
	REGIST_FUNC(Facosh, acosh);
	REGIST_FUNC(Fasin, asin);
	REGIST_FUNC(Fasinh, asinh);
	REGIST_FUNC(Fatan, atan);
	REGIST_FUNC(Fatan2, atan2);
	REGIST_FUNC(Fatanh, atanh);
	REGIST_FUNC(Fcos, cos);
	REGIST_FUNC(Fcosh, cosh);
	REGIST_FUNC(Fsin, sin);
	REGIST_FUNC(Fsinh, sinh);
	REGIST_FUNC(Ftan, tan);
	REGIST_FUNC(Ftanh, tanh);
	REGIST_FUNC(Fexp, exp);
	REGIST_FUNC(Fexp2, exp2);
	REGIST_FUNC(Fexp10, exp10);
	REGIST_FUNC(Flog, log);
	REGIST_FUNC(Flog2, log2);
	REGIST_FUNC(Flog10, log10);
	REGIST_FUNC(Fpow, pow);
	REGIST_FUNC(Fsqrt, sqrt);
	REGIST_FUNC(Frsqrt, rsqrt);
	REGIST_FUNC(Fceil, ceil);
	REGIST_FUNC(Ffloor, floor);
	REGIST_FUNC(Ffract, fract);
	REGIST_FUNC(Ftrunc, trunc);
	REGIST_FUNC(Fround, round);
	REGIST_FUNC(Ffma, fma);
	REGIST_FUNC(Fcopysign, copysign);
	REGIST_FUNC(Fcross, cross);
	REGIST_FUNC(Fdot, dot);
	REGIST_FUNC(Flength, length);
	REGIST_FUNC(Flength_squared, length_squared);
	REGIST_FUNC(Fnormalize, normalize);
	REGIST_FUNC(Ffaceforward, faceforward);
	REGIST_FUNC(Fdeterminant, determinant);
	REGIST_FUNC(Ftranspose, transpose);
	REGIST_FUNC(Finverse, inverse);
	REGIST_FUNC(Fsynchronize_block, synchronize_block);
	REGIST_FUNC(Fatomic_exchange, atomic_exchange);
	REGIST_FUNC(Fatomic_compare_exchange, atomic_compare_exchange);
	REGIST_FUNC(Fatomic_fetch_add, atomic_fetch_add);
	REGIST_FUNC(Fatomic_fetch_sub, atomic_fetch_sub);
	REGIST_FUNC(Fatomic_fetch_and, atomic_fetch_and);
	REGIST_FUNC(Fatomic_fetch_or, atomic_fetch_or);
	REGIST_FUNC(Fatomic_fetch_xor, atomic_fetch_xor);
	REGIST_FUNC(Fatomic_fetch_min, atomic_fetch_min);
	REGIST_FUNC(Fatomic_fetch_max, atomic_fetch_max);
	REGIST_FUNC(Fbuffer_read, buffer_read);
	REGIST_FUNC(Fbuffer_write, buffer_write);
	REGIST_FUNC(Ftexture_read, texture_read);
	REGIST_FUNC(Ftexture_write, texture_write);
	REGIST_FUNC(Fbindless_texture2d_sample, bindless_texture2d_sample);
	REGIST_FUNC(Fbindless_texture2d_sample_level, bindless_texture2d_sample_level);
	REGIST_FUNC(Fbindless_texture2d_sample_grad, bindless_texture2d_sample_grad);
	REGIST_FUNC(Fbindless_texture3d_sample, bindless_texture3d_sample);
	REGIST_FUNC(Fbindless_texture3d_sample_level, bindless_texture3d_sample_level);
	REGIST_FUNC(Fbindless_texture3d_sample_grad, bindless_texture3d_sample_grad);
	REGIST_FUNC(Fbindless_texture2d_read, bindless_texture2d_read);
	REGIST_FUNC(Fbindless_texture3d_read, bindless_texture3d_read);
	REGIST_FUNC(Fbindless_texture2d_read_level, bindless_texture2d_read_level);
	REGIST_FUNC(Fbindless_texture3d_read_level, bindless_texture3d_read_level);
	REGIST_FUNC(Fbindless_texture2d_size, bindless_texture2d_size);
	REGIST_FUNC(Fbindless_texture3d_size, bindless_texture3d_size);
	REGIST_FUNC(Fbindless_texture2d_size_level, bindless_texture2d_size_level);
	REGIST_FUNC(Fbindless_texture3d_size_level, bindless_texture3d_size_level);
	REGIST_FUNC(Fbindless_buffer_read, bindless_buffer_read);
	REGIST_FUNC(Fmake_bool2, make_bool2);
	REGIST_FUNC(Fmake_bool3, make_bool3);
	REGIST_FUNC(Fmake_bool4, make_bool4);
	REGIST_FUNC(Fmake_int2, make_int2);
	REGIST_FUNC(Fmake_int3, make_int3);
	REGIST_FUNC(Fmake_int4, make_int4);
	REGIST_FUNC(Fmake_uint2, make_uint2);
	REGIST_FUNC(Fmake_uint3, make_uint3);
	REGIST_FUNC(Fmake_uint4, make_uint4);
	REGIST_FUNC(Fmake_float2, make_float2);
	REGIST_FUNC(Fmake_float3, make_float3);
	REGIST_FUNC(Fmake_float4, make_float4);
	REGIST_FUNC(Fmake_float2x2, make_float2x2);
	REGIST_FUNC(Fmake_float3x3, make_float3x3);
	REGIST_FUNC(Fmake_float4x4, make_float4x4);
	REGIST_FUNC(Fassume, assume);
	REGIST_FUNC(Funreachable, unreachable);
	REGIST_FUNC(Finstance_to_world_matrix, instance_to_world_matrix);
	REGIST_FUNC(Fset_accel_tranform, set_accel_tranform);
	REGIST_FUNC(Fset_accel_visibility, set_accel_visibility);
	REGIST_FUNC(Fset_accel_transform_visibility, set_accel_transform_visibility);
	REGIST_FUNC(Ftrace_closest, trace_closest);
	REGIST_FUNC(Ftrace_any, trace_any);
	//Binary Op
	REGIST_FUNC(Fadd, add);
	REGIST_FUNC(Fsub, sub);
	REGIST_FUNC(Fmul, mul);
	REGIST_FUNC(Fdiv, div);
	REGIST_FUNC(Fmod, mod);
	REGIST_FUNC(Fbit_and, bit_and);
	REGIST_FUNC(Fbit_or, bit_or);
	REGIST_FUNC(Fbit_xor, bit_xor);
	REGIST_FUNC(Fshl, shl);
	REGIST_FUNC(Fshr, shr);
	REGIST_FUNC(Fand, and);
	REGIST_FUNC(For, or);
	REGIST_FUNC(Fless, less);
	REGIST_FUNC(Fgreater, greater);
	REGIST_FUNC(Fless_equal, less_equal);
	REGIST_FUNC(Fgreater_equal, greater_equal);
	REGIST_FUNC(Fequal, equal);
	REGIST_FUNC(Fnot_equal, not_equal);
	// Unary
	REGIST_FUNC(Fplus, plus);
	REGIST_FUNC(Fminus, minus);
	REGIST_FUNC(Fnot, not);
	REGIST_FUNC(Fbit_not, bit_not);

	REGIST_NAME(callMap, CallOp::ALL, all);
	REGIST_NAME(callMap, CallOp::ANY, any);
	REGIST_NAME(callMap, CallOp::SELECT, select);
	REGIST_NAME(callMap, CallOp::CLAMP, clamp);
	REGIST_NAME(callMap, CallOp::LERP, lerp);
	REGIST_NAME(callMap, CallOp::STEP, step);
	REGIST_NAME(callMap, CallOp::ABS, abs);
	REGIST_NAME(callMap, CallOp::MIN, min);
	REGIST_NAME(callMap, CallOp::MAX, max);
	REGIST_NAME(callMap, CallOp::CLZ, clz);
	REGIST_NAME(callMap, CallOp::CTZ, ctz);
	REGIST_NAME(callMap, CallOp::POPCOUNT, popcount);
	REGIST_NAME(callMap, CallOp::REVERSE, reverse);
	REGIST_NAME(callMap, CallOp::ISINF, isinf);
	REGIST_NAME(callMap, CallOp::ISNAN, isnan);
	REGIST_NAME(callMap, CallOp::ACOS, acos);
	REGIST_NAME(callMap, CallOp::ACOSH, acosh);
	REGIST_NAME(callMap, CallOp::ASIN, asin);
	REGIST_NAME(callMap, CallOp::ASINH, asinh);
	REGIST_NAME(callMap, CallOp::ATAN, atan);
	REGIST_NAME(callMap, CallOp::ATAN2, atan2);
	REGIST_NAME(callMap, CallOp::ATANH, atanh);
	REGIST_NAME(callMap, CallOp::COS, cos);
	REGIST_NAME(callMap, CallOp::COSH, cosh);
	REGIST_NAME(callMap, CallOp::SIN, sin);
	REGIST_NAME(callMap, CallOp::SINH, sinh);
	REGIST_NAME(callMap, CallOp::TAN, tan);
	REGIST_NAME(callMap, CallOp::TANH, tanh);
	REGIST_NAME(callMap, CallOp::EXP, exp);
	REGIST_NAME(callMap, CallOp::EXP2, exp2);
	REGIST_NAME(callMap, CallOp::EXP10, exp10);
	REGIST_NAME(callMap, CallOp::LOG, log);
	REGIST_NAME(callMap, CallOp::LOG2, log2);
	REGIST_NAME(callMap, CallOp::LOG10, log10);
	REGIST_NAME(callMap, CallOp::POW, pow);
	REGIST_NAME(callMap, CallOp::SQRT, sqrt);
	REGIST_NAME(callMap, CallOp::RSQRT, rsqrt);
	REGIST_NAME(callMap, CallOp::CEIL, ceil);
	REGIST_NAME(callMap, CallOp::FLOOR, floor);
	REGIST_NAME(callMap, CallOp::FRACT, fract);
	REGIST_NAME(callMap, CallOp::TRUNC, trunc);
	REGIST_NAME(callMap, CallOp::ROUND, round);
	REGIST_NAME(callMap, CallOp::FMA, fma);
	REGIST_NAME(callMap, CallOp::COPYSIGN, copysign);
	REGIST_NAME(callMap, CallOp::CROSS, cross);
	REGIST_NAME(callMap, CallOp::DOT, dot);
	REGIST_NAME(callMap, CallOp::LENGTH, length);
	REGIST_NAME(callMap, CallOp::LENGTH_SQUARED, length_squared);
	REGIST_NAME(callMap, CallOp::NORMALIZE, normalize);
	REGIST_NAME(callMap, CallOp::FACEFORWARD, faceforward);
	REGIST_NAME(callMap, CallOp::DETERMINANT, determinant);
	REGIST_NAME(callMap, CallOp::TRANSPOSE, transpose);
	REGIST_NAME(callMap, CallOp::INVERSE, inverse);
	REGIST_NAME(callMap, CallOp::SYNCHRONIZE_BLOCK, synchronize_block);
	REGIST_NAME(callMap, CallOp::ATOMIC_EXCHANGE, atomic_exchange);
	REGIST_NAME(callMap, CallOp::ATOMIC_COMPARE_EXCHANGE, atomic_compare_exchange);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_ADD, atomic_fetch_add);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_SUB, atomic_fetch_sub);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_AND, atomic_fetch_and);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_OR, atomic_fetch_or);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_XOR, atomic_fetch_xor);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_MIN, atomic_fetch_min);
	REGIST_NAME(callMap, CallOp::ATOMIC_FETCH_MAX, atomic_fetch_max);
	REGIST_NAME(callMap, CallOp::BUFFER_READ, buffer_read);
	REGIST_NAME(callMap, CallOp::BUFFER_WRITE, buffer_write);
	REGIST_NAME(callMap, CallOp::TEXTURE_READ, texture_read);
	REGIST_NAME(callMap, CallOp::TEXTURE_WRITE, texture_write);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_SAMPLE, bindless_texture2d_sample);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_SAMPLE_LEVEL, bindless_texture2d_sample_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD, bindless_texture2d_sample_grad);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_SAMPLE, bindless_texture3d_sample);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_SAMPLE_LEVEL, bindless_texture3d_sample_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD, bindless_texture3d_sample_grad);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_READ, bindless_texture2d_read);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_READ, bindless_texture3d_read);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_READ_LEVEL, bindless_texture2d_read_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_READ_LEVEL, bindless_texture3d_read_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_SIZE, bindless_texture2d_size);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_SIZE, bindless_texture3d_size);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE2D_SIZE_LEVEL, bindless_texture2d_size_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_TEXTURE3D_SIZE_LEVEL, bindless_texture3d_size_level);
	REGIST_NAME(callMap, CallOp::BINDLESS_BUFFER_READ, bindless_buffer_read);
	REGIST_NAME(callMap, CallOp::MAKE_BOOL2, make_bool2);
	REGIST_NAME(callMap, CallOp::MAKE_BOOL3, make_bool3);
	REGIST_NAME(callMap, CallOp::MAKE_BOOL4, make_bool4);
	REGIST_NAME(callMap, CallOp::MAKE_INT2, make_int2);
	REGIST_NAME(callMap, CallOp::MAKE_INT3, make_int3);
	REGIST_NAME(callMap, CallOp::MAKE_INT4, make_int4);
	REGIST_NAME(callMap, CallOp::MAKE_UINT2, make_uint2);
	REGIST_NAME(callMap, CallOp::MAKE_UINT3, make_uint3);
	REGIST_NAME(callMap, CallOp::MAKE_UINT4, make_uint4);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT2, make_float2);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT3, make_float3);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT4, make_float4);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT2X2, make_float2x2);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT3X3, make_float3x3);
	REGIST_NAME(callMap, CallOp::MAKE_FLOAT4X4, make_float4x4);
	REGIST_NAME(callMap, CallOp::ASSUME, assume);
	REGIST_NAME(callMap, CallOp::UNREACHABLE, unreachable);
	REGIST_NAME(callMap, CallOp::INSTANCE_TO_WORLD_MATRIX, instance_to_world_matrix);
	REGIST_NAME(callMap, CallOp::SET_ACCEL_TRANFORM, set_accel_tranform);
	REGIST_NAME(callMap, CallOp::SET_ACCEL_VISIBILITY, set_accel_visibility);
	REGIST_NAME(callMap, CallOp::SET_ACCEL_TRANSFORM_VISIBILITY, set_accel_transform_visibility);
	REGIST_NAME(callMap, CallOp::TRACE_CLOSEST, trace_closest);
	REGIST_NAME(callMap, CallOp::TRACE_ANY, trace_any);
	REGIST_NAME(binaryMap, BinaryOp::ADD, add);
	REGIST_NAME(binaryMap, BinaryOp::SUB, sub);
	REGIST_NAME(binaryMap, BinaryOp::MUL, mul);
	REGIST_NAME(binaryMap, BinaryOp::DIV, div);
	REGIST_NAME(binaryMap, BinaryOp::MOD, mod);
	REGIST_NAME(binaryMap, BinaryOp::BIT_AND, bit_and);
	REGIST_NAME(binaryMap, BinaryOp::BIT_OR, bit_or);
	REGIST_NAME(binaryMap, BinaryOp::BIT_XOR, bit_xor);
	REGIST_NAME(binaryMap, BinaryOp::SHL, shl);
	REGIST_NAME(binaryMap, BinaryOp::SHR, shr);
	REGIST_NAME(binaryMap, BinaryOp::AND, and);
	REGIST_NAME(binaryMap, BinaryOp::OR, or);
	REGIST_NAME(binaryMap, BinaryOp::LESS, less);
	REGIST_NAME(binaryMap, BinaryOp::GREATER, greater);
	REGIST_NAME(binaryMap, BinaryOp::LESS_EQUAL, less_equal);
	REGIST_NAME(binaryMap, BinaryOp::GREATER_EQUAL, greater_equal);
	REGIST_NAME(binaryMap, BinaryOp::EQUAL, equal);
	REGIST_NAME(binaryMap, BinaryOp::NOT_EQUAL, not_equal);
	REGIST_NAME(unaryMap, UnaryOp::BIT_NOT, bit_not);
	REGIST_NAME(unaryMap, UnaryOp::MINUS, minus);
	REGIST_NAME(unaryMap, UnaryOp::NOT, not);
	REGIST_NAME(unaryMap, UnaryOp::PLUS, plus);
	CustomInit();
#undef REGIST_FUNC
#undef REGIST_NAME
	// clang-format on
}
}// namespace luisa::ir