#include <ir/api/statement.h>
namespace toolhub::ir {
namespace detail {
static bool IsTypeOrEleTag(Type::Tag tag, Type const* type) {
	if (type->tag == tag) return true;
	else if (type->tag == Type::Tag::Vector) {
		if (type->element->tag == tag) return true;
	}
	return false;
}
}// namespace detail
bool UnaryStmt::valid() const {
	if (!dst || !lhs) return false;
	auto type = lhs->type;
	auto IsType = [&](Type::Tag tag) { return detail::IsTypeOrEleTag(tag, type); };
	if (op == UnaryOp::CAST) {
		if (dst->type == lhs->type) return false;
		//TODO: type cast valid-check
		return true;
	} else {
		if (dst->type != lhs->type) return false;
		switch (op) {
			case UnaryOp::NOT:
				return IsType(Type::Tag::Bool);
			case UnaryOp::BIT_NOT:
				return IsType(Type::Tag::Bool) || IsType(Type::Tag::Int) || IsType(Type::Tag::UInt);
			case UnaryOp::PLUS:
				return IsType(Type::Tag::Float) || IsType(Type::Tag::Int) || IsType(Type::Tag::UInt);
			case UnaryOp::MINUS:
				return IsType(Type::Tag::Float) || IsType(Type::Tag::Int) || IsType(Type::Tag::UInt);
		}
		return false;
	}
}
bool BinaryStmt::valid() const {
	if (!dst || !lhs || !rhs || (lhs->type != rhs->type)) return false;
	auto type = lhs->type;
	auto IsType = [&](Type::Tag tag) { return detail::IsTypeOrEleTag(tag, type); };
	switch (op) {
		case BinaryOp::ADD:
		case BinaryOp::SUB:
		case BinaryOp::MUL:
		case BinaryOp::DIV:
		case BinaryOp::MOD: {
			if (dst->type != lhs->type) return false;
			return !IsType(Type::Tag::Bool);
		}
		case BinaryOp::BIT_AND:
		case BinaryOp::BIT_OR:
		case BinaryOp::BIT_XOR:
		case BinaryOp::SHL:
		case BinaryOp::SHR: {
			if (dst->type != lhs->type) return false;
			return !IsType(Type::Tag::Float);
		}
		case BinaryOp::AND:
		case BinaryOp::OR: {
			if (dst->type != lhs->type) return false;
			return IsType(Type::Tag::Bool);
		}
		case BinaryOp::LESS:
		case BinaryOp::GREATER:
		case BinaryOp::LESS_EQUAL:
		case BinaryOp::GREATER_EQUAL:
		case BinaryOp::EQUAL:
		case BinaryOp::NOT_EQUAL: {
			if (type->tag == Type::Tag::Vector) {
				return (dst->type->tag == Type::Tag::Vector && dst->type->element->tag == Type::Tag::Bool && dst->type->dimension == type->dimension);
			} else {
				return (dst->type->tag == Type::Tag::Bool);
			}
		}
	}
	return false;
}
bool ReturnStmt::valid() const {
	return retValue;
}
bool BreakStmt::valid() const { return true; }
bool ContinueStmt::valid() const { return true; }
bool BuiltinCallStmt::valid() const {
	//TODO
	return true;
}
bool CustomCallStmt::valid() const {
	return true;
}
bool MemberStmt::valid() const {
	//TODO
	return true;
}
bool AccessStmt::valid() const {
	return true;
}
bool IfStmt::valid() const {
	return condition->type->tag == Type::Tag::Bool;
}
bool LoopStmt::valid() const {
	return condition->type->tag == Type::Tag::Bool;
}
bool SwitchStmt::valid() const {
	return condition->type->tag == Type::Tag::Bool;
}
}// namespace toolhub::ir