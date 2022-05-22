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
	}
}
vstd::string IRCodegen::Gen(Kernel const& kernel) {
	vstd::string str;
	for (auto&& t : kernel.types) {
		PrintType(t, str);
	}
	PrintConst(kernel.constants, str);
	return str;
}
void IRCodegen::PrintConst(vstd::span<ConstantVar const> vars, vstd::string& str) {
	size_t idx = 0;
	for(auto&& i : vars){
		str << "const "_sv;
		GetTypeName(i.type, str);
		str << " c"_sv << vstd::to_string(idx) << " {"_sv;
		CodegenUtil::PrintConstArray(i, str);
		str << '}';
	}
}

}// namespace toolhub::ir