#include <ir/codegen/ir_codegen.h>
#include <ir/codegen/codegen_util.h>
namespace toolhub::ir {
void IRCodegen::PrintType(Type const* t, vstd::string& str) {
	if (t->tag != Type::Tag::Structure) return;
	// Check struct
	for (auto&& ele : t->members) {
		if (ele->tag == Type::Tag::Structure) {
			if (!typeNames.Find(ele)) PrintType(ele, str);
		}
	}
	vstd::string typeName;
	typeName << 'T' << vstd::to_string(typeNames.Size());
	str << "struct "_sv << vstd::to_string(t->alignment) << ' ' << typeName << "{\n";
	typeNames.Emplace(t, std::move(typeName));
	for (auto&& ele : t->members) {
		GetTypeName(ele, str);
		str << '\n';
	}
	str << "}\n";
}
IRCodegen::~IRCodegen() {}
void IRCodegen::GetTypeName(Type const* t, vstd::string& str) {
	switch (t->tag) {
		case Type::Tag::Float:
			str << "float"_sv;
			break;
		case Type::Tag::Int:
			str << "int"_sv;
			break;
		case Type::Tag::UInt:
			str << "uint"_sv;
			break;
		case Type::Tag::Bool:
			str << "bool"_sv;
			break;
		case Type::Tag::Vector:
			GetTypeName(t->element, str);
			vstd::to_string(t->dimension, str);
			break;
		case Type::Tag::Matrix: {
			GetTypeName(t->element, str);
			auto dim = vstd::to_string(t->dimension);
			str << dim << 'x' << dim;
		} break;
		case Type::Tag::Array:
			GetTypeName(t->element, str);
			str << '[' << vstd::to_string(t->dimension) << ']';
			break;
		case Type::Tag::Structure: {
			auto ite = typeNames.Find(t);
			if (ite) {
				str << ite.Value();
			}
		} break;
		case Type::Tag::Buffer: {
			str << "Buffer<";
			GetTypeName(t->element, str);
			str << '>';
		} break;
	}
}
vstd::string IRCodegen::Gen(Kernel const& kernel) {
	vstd::string str;
	for (auto&& t : kernel.types) {
		PrintType(t, str);
	}
	PrintConst(kernel.constants, str);
	size_t callableIdx = 0;
	for (auto&& c : kernel.callables) {
		PrintFunc(*c, str, callableIdx);
		callableIdx++;
	}
	return str;
}
void IRCodegen::PrintConst(vstd::span<ConstantVar const* const> vars, vstd::string& str) {
	size_t idx = 0;
	for (auto&& i : vars) {
		str << "const "_sv;
		GetTypeName(i->type, str);
		str << " c"_sv << vstd::to_string(idx) << " {"_sv;
		CodegenUtil::PrintConstArray(*i, str);
		str << "}\n";
		constIndices.Emplace(i, idx);
		++idx;
	}
}
void IRCodegen::PrintStatement(Statement const* stmt, vstd::string& str) {
	auto endPtrint = vstd::create_disposer([&] { str << '\n'; });
	auto PrintRet = [&](auto&& stmt) {
		if (stmt->dst) {
			PrintVar(stmt->dst, str);
			str << '=';
		}
	};
	auto PrintDstType = [&](auto&& stmt) {
		if (stmt->dst) {
			str << ':';
			GetTypeName(stmt->dst->type, str);
		}
	};
	auto PrintArgs = [&](vstd::span<Var const* const> vars) {
		str << '(';
		for (auto i : vstd::range(vars.size())) {
			PrintVar(vars[i], str);
			if (i < vars.size() - 1) str << ',';
		}
		str << ')';
	};
	auto PrintCondition = [&](vstd::variant<Var const*, bool> const& condition) {
		condition.multi_visit(
			[&](auto&& v) {
				PrintVar(v, str);
			},
			[&](auto&& b) {
				str << (b ? "true" : "false");
			});
	};
	//TODO
	switch (stmt->tag()) {
		case Statement::Tag::CustomCall: {
			auto customStmt = static_cast<CustomCallStmt const*>(stmt);
			PrintRet(customStmt);
			str << "callable_" << vstd::to_string(customStmt->callableIndex);
			PrintArgs(customStmt->args);
			PrintDstType(customStmt);
		} break;
		case Statement::Tag::BuiltinCall: {
			auto builtInStmt = static_cast<BuiltinCallStmt const*>(stmt);
			PrintRet(builtInStmt);
			str << callOpNames[static_cast<size_t>(builtInStmt->op)];
			PrintArgs(builtInStmt->args);
			PrintDstType(builtInStmt);
		} break;
		case Statement::Tag::Binary: {
			auto binStmt = static_cast<BinaryStmt const*>(stmt);
			PrintRet(binStmt);
			str << binOpNames[static_cast<size_t>(binStmt->op)];
			PrintArgs(std::initializer_list<Var const*>{binStmt->lhs, binStmt->rhs});
			PrintDstType(binStmt);
		} break;
		case Statement::Tag::Unary: {
			auto unaStmt = static_cast<UnaryStmt const*>(stmt);
			PrintRet(unaStmt);
			str << unaOpNames[static_cast<size_t>(unaStmt->op)];
			PrintVar(unaStmt->lhs, str);
			PrintDstType(unaStmt);
		} break;
		case Statement::Tag::Break: {
			str << "break()";
		} break;
		case Statement::Tag::Continue: {
			str << "continue()";
		} break;
		case Statement::Tag::Access: {
			auto accessStmt = static_cast<AccessStmt const*>(stmt);
			PrintRet(accessStmt);
			str << "get_index(";
			PrintVar(accessStmt->src, str);
			str << ',';
			accessStmt->index.multi_visit(
				[&](auto&& v) { PrintVar(v, str); },
				[&](auto&& v) { str << vstd::to_string(v); });
			str << ')';
			PrintDstType(accessStmt);
		} break;
		case Statement::Tag::If: {
			auto ifStmt = static_cast<IfStmt const*>(stmt);
			str << "if(";
			PrintCondition(ifStmt->condition);
			str << ")\n";
			for (auto&& i : ifStmt->trueField) {
				PrintStatement(i, str);
			}
			if (!ifStmt->falseField.empty()) {
				str << "else()\n";
				for (auto&& i : ifStmt->falseField) {
					PrintStatement(i, str);
				}
			}
			str << "endif()";
		} break;
		case Statement::Tag::Loop: {
			auto loopStmt = static_cast<LoopStmt const*>(stmt);
			str << "while(";
			PrintCondition(loopStmt->condition);
			str << ")\n";
			for (auto&& i : loopStmt->commands) {
				PrintStatement(i, str);
			}
			str << "endwhile()";
		} break;
		case Statement::Tag::Member: {
			auto memberStmt = static_cast<MemberStmt const*>(stmt);
			PrintRet(memberStmt);
			str << "get_member(";
			PrintVar(memberStmt->src, str);
			str << ',';
			for (auto i : vstd::range(memberStmt->indices.size())) {
				str << vstd::to_string(memberStmt->indices[i]);
				if (i < memberStmt->indices.size() - 1) str << ',';
			}
			str << ')';
			PrintDstType(memberStmt);
		} break;
		case Statement::Tag::Return: {
			auto retStmt = static_cast<ReturnStmt const*>(stmt);
			if (!retStmt->retValue)
				str << "return()";
			else {
				str << "return(";
				PrintVar(retStmt->retValue, str);
				str << ')';
			}

		} break;
		case Statement::Tag::Switch: {
			auto switchStmt = static_cast<SwitchStmt const*>(stmt);
			str << "switch(";
			PrintVar(switchStmt->condition, str);
			str << ")\n";
			for(auto&& c : switchStmt->cases){
				str << "case(" << vstd::to_string(c.first) << ")\n";
				for(auto&& s : c.second){
					PrintStatement(s, str);
				}
				str << "endcase()\n";
			}
			str << "endswitch()";
		} break;
	}
}
void IRCodegen::PrintVar(Var const* var, vstd::string& str) {
	switch (var->tag()) {
		case Var::Tag::Constant: {
			auto ite = constIndices.Find(var);
			if (ite) str << 'c' << vstd::to_string((ite.Value()));
			else {
				VEngine_Log("constant not found!");
				VENGINE_EXIT;
			}
		} break;
		case Var::Tag::Literal: {
			CodegenUtil::PrintLiteralValue(static_cast<LiteralVar const*>(var)->literalValue, str);
		} break;
		case Var::Tag::Variable:
		case Var::Tag::Ref: {
			size_t mapIdx = varMap.Size();
			auto ite = varMap.Emplace(var, vstd::MakeLazyEval([&] {
										  return mapIdx;
									  }));
			mapIdx = ite.Value();
			str << 'v' << vstd::to_string(mapIdx);
		} break;
	}
}

void IRCodegen::PrintFunc(Callable const& callable, vstd::string& str, size_t index) {
	varMap.Clear();
	str << "def callable_"_sv;
	vstd::to_string(index, str);
	str << '(';
	for (auto a : vstd::range(callable.arguments.size())) {
		auto arg = callable.arguments[a];
		PrintVar(arg, str);
		str << ':';
		GetTypeName(arg->type, str);
		if (arg->tag() == Var::Tag::Ref)
			str << '&';
		if (a < callable.arguments.size() - 1) str << ',';
	}
	if (callable.retType) {
		str << "):";
		GetTypeName(callable.retType, str);
	} else {
		str << ')';
	}
	str << "{\n";
	for (auto&& state : callable.statements) {
		PrintStatement(state, str);
	}
	str << "}\n";
}
template<typename Op>
void IRCodegen::RegistState(Op op, vstd::string_view strv) {
	if constexpr (std::is_same_v<Op, CallOp>) {
		callOpNames[static_cast<size_t>(op)] = strv;
	} else if constexpr (std::is_same_v<Op, BinaryOp>) {
		binOpNames[static_cast<size_t>(op)] = strv;
	} else if constexpr (std::is_same_v<Op, UnaryOp>) {
		unaOpNames[static_cast<size_t>(op)] = strv;
	}
}
#define REGIST_NAME(OP, STR) RegistState(OP, #STR##_sv)
void IRCodegen::InitStatement() {
	callOpNames.resize(CallOpCount);
	binOpNames.resize(BinaryOpCount);
	unaOpNames.resize(UnaryOpCount);
	REGIST_NAME(CallOp::ALL, all);
	REGIST_NAME(CallOp::ANY, any);
	REGIST_NAME(CallOp::SELECT, select);
	REGIST_NAME(CallOp::CLAMP, clamp);
	REGIST_NAME(CallOp::LERP, lerp);
	REGIST_NAME(CallOp::STEP, step);
	REGIST_NAME(CallOp::ABS, abs);
	REGIST_NAME(CallOp::MIN, min);
	REGIST_NAME(CallOp::MAX, max);
	REGIST_NAME(CallOp::CLZ, clz);
	REGIST_NAME(CallOp::CTZ, ctz);
	REGIST_NAME(CallOp::POPCOUNT, popcount);
	REGIST_NAME(CallOp::REVERSE, reverse);
	REGIST_NAME(CallOp::ISINF, isinf);
	REGIST_NAME(CallOp::ISNAN, isnan);
	REGIST_NAME(CallOp::ACOS, acos);
	REGIST_NAME(CallOp::ACOSH, acosh);
	REGIST_NAME(CallOp::ASIN, asin);
	REGIST_NAME(CallOp::ASINH, asinh);
	REGIST_NAME(CallOp::ATAN, atan);
	REGIST_NAME(CallOp::ATAN2, atan2);
	REGIST_NAME(CallOp::ATANH, atanh);
	REGIST_NAME(CallOp::COS, cos);
	REGIST_NAME(CallOp::COSH, cosh);
	REGIST_NAME(CallOp::SIN, sin);
	REGIST_NAME(CallOp::SINH, sinh);
	REGIST_NAME(CallOp::TAN, tan);
	REGIST_NAME(CallOp::TANH, tanh);
	REGIST_NAME(CallOp::EXP, exp);
	REGIST_NAME(CallOp::EXP2, exp2);
	REGIST_NAME(CallOp::EXP10, exp10);
	REGIST_NAME(CallOp::LOG, log);
	REGIST_NAME(CallOp::LOG2, log2);
	REGIST_NAME(CallOp::LOG10, log10);
	REGIST_NAME(CallOp::POW, pow);
	REGIST_NAME(CallOp::SQRT, sqrt);
	REGIST_NAME(CallOp::RSQRT, rsqrt);
	REGIST_NAME(CallOp::CEIL, ceil);
	REGIST_NAME(CallOp::FLOOR, floor);
	REGIST_NAME(CallOp::FRACT, fract);
	REGIST_NAME(CallOp::TRUNC, trunc);
	REGIST_NAME(CallOp::ROUND, round);
	REGIST_NAME(CallOp::FMA, fma);
	REGIST_NAME(CallOp::COPYSIGN, copysign);
	REGIST_NAME(CallOp::CROSS, cross);
	REGIST_NAME(CallOp::DOT, dot);
	REGIST_NAME(CallOp::LENGTH, length);
	REGIST_NAME(CallOp::LENGTH_SQUARED, length_squared);
	REGIST_NAME(CallOp::NORMALIZE, normalize);
	REGIST_NAME(CallOp::FACEFORWARD, faceforward);
	REGIST_NAME(CallOp::DETERMINANT, determinant);
	REGIST_NAME(CallOp::TRANSPOSE, transpose);
	REGIST_NAME(CallOp::INVERSE, inverse);
	REGIST_NAME(CallOp::SYNCHRONIZE_BLOCK, synchronize_block);
	REGIST_NAME(CallOp::ATOMIC_EXCHANGE, atomic_exchange);
	REGIST_NAME(CallOp::ATOMIC_COMPARE_EXCHANGE, atomic_compare_exchange);
	REGIST_NAME(CallOp::ATOMIC_FETCH_ADD, atomic_fetch_add);
	REGIST_NAME(CallOp::ATOMIC_FETCH_SUB, atomic_fetch_sub);
	REGIST_NAME(CallOp::ATOMIC_FETCH_AND, atomic_fetch_and);
	REGIST_NAME(CallOp::ATOMIC_FETCH_OR, atomic_fetch_or);
	REGIST_NAME(CallOp::ATOMIC_FETCH_XOR, atomic_fetch_xor);
	REGIST_NAME(CallOp::ATOMIC_FETCH_MIN, atomic_fetch_min);
	REGIST_NAME(CallOp::ATOMIC_FETCH_MAX, atomic_fetch_max);
	REGIST_NAME(CallOp::BUFFER_READ, buffer_read);
	REGIST_NAME(CallOp::BUFFER_WRITE, buffer_write);
	REGIST_NAME(CallOp::TEXTURE_READ, texture_read);
	REGIST_NAME(CallOp::TEXTURE_WRITE, texture_write);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_SAMPLE, bindless_texture2d_sample);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_SAMPLE_LEVEL, bindless_texture2d_sample_level);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_SAMPLE_GRAD, bindless_texture2d_sample_grad);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_SAMPLE, bindless_texture3d_sample);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_SAMPLE_LEVEL, bindless_texture3d_sample_level);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_SAMPLE_GRAD, bindless_texture3d_sample_grad);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_READ, bindless_texture2d_read);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_READ, bindless_texture3d_read);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_READ_LEVEL, bindless_texture2d_read_level);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_READ_LEVEL, bindless_texture3d_read_level);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_SIZE, bindless_texture2d_size);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_SIZE, bindless_texture3d_size);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE2D_SIZE_LEVEL, bindless_texture2d_size_level);
	REGIST_NAME(CallOp::BINDLESS_TEXTURE3D_SIZE_LEVEL, bindless_texture3d_size_level);
	REGIST_NAME(CallOp::BINDLESS_BUFFER_READ, bindless_buffer_read);
	REGIST_NAME(CallOp::MAKE_BOOL2, make_bool2);
	REGIST_NAME(CallOp::MAKE_BOOL3, make_bool3);
	REGIST_NAME(CallOp::MAKE_BOOL4, make_bool4);
	REGIST_NAME(CallOp::MAKE_INT2, make_int2);
	REGIST_NAME(CallOp::MAKE_INT3, make_int3);
	REGIST_NAME(CallOp::MAKE_INT4, make_int4);
	REGIST_NAME(CallOp::MAKE_UINT2, make_uint2);
	REGIST_NAME(CallOp::MAKE_UINT3, make_uint3);
	REGIST_NAME(CallOp::MAKE_UINT4, make_uint4);
	REGIST_NAME(CallOp::MAKE_FLOAT2, make_float2);
	REGIST_NAME(CallOp::MAKE_FLOAT3, make_float3);
	REGIST_NAME(CallOp::MAKE_FLOAT4, make_float4);
	REGIST_NAME(CallOp::MAKE_FLOAT2X2, make_float2x2);
	REGIST_NAME(CallOp::MAKE_FLOAT3X3, make_float3x3);
	REGIST_NAME(CallOp::MAKE_FLOAT4X4, make_float4x4);
	REGIST_NAME(CallOp::ASSUME, assume);
	REGIST_NAME(CallOp::UNREACHABLE, unreachable);
	REGIST_NAME(CallOp::INSTANCE_TO_WORLD_MATRIX, instance_to_world_matrix);
	REGIST_NAME(CallOp::SET_ACCEL_TRANFORM, set_accel_tranform);
	REGIST_NAME(CallOp::SET_ACCEL_VISIBILITY, set_accel_visibility);
	REGIST_NAME(CallOp::SET_ACCEL_TRANSFORM_VISIBILITY, set_accel_transform_visibility);
	REGIST_NAME(CallOp::TRACE_CLOSEST, trace_closest);
	REGIST_NAME(CallOp::TRACE_ANY, trace_any);
	REGIST_NAME(BinaryOp::ADD, add);
	REGIST_NAME(BinaryOp::SUB, sub);
	REGIST_NAME(BinaryOp::MUL, mul);
	REGIST_NAME(BinaryOp::DIV, div);
	REGIST_NAME(BinaryOp::MOD, mod);
	REGIST_NAME(BinaryOp::BIT_AND, bit_and);
	REGIST_NAME(BinaryOp::BIT_OR, bit_or);
	REGIST_NAME(BinaryOp::BIT_XOR, bit_xor);
	REGIST_NAME(BinaryOp::SHL, shl);
	REGIST_NAME(BinaryOp::SHR, shr);
	REGIST_NAME(BinaryOp::AND, and);
	REGIST_NAME(BinaryOp::OR, or);
	REGIST_NAME(BinaryOp::LESS, less);
	REGIST_NAME(BinaryOp::GREATER, greater);
	REGIST_NAME(BinaryOp::LESS_EQUAL, less_equal);
	REGIST_NAME(BinaryOp::GREATER_EQUAL, greater_equal);
	REGIST_NAME(BinaryOp::EQUAL, equal);
	REGIST_NAME(BinaryOp::NOT_EQUAL, not_equal);
	REGIST_NAME(UnaryOp::BIT_NOT, bit_not);
	REGIST_NAME(UnaryOp::MINUS, minus);
	REGIST_NAME(UnaryOp::NOT, not );
	REGIST_NAME(UnaryOp::PLUS, plus);
}
#undef REGIST_NAME
}// namespace toolhub::ir