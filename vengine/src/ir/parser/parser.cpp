
#include <ir/parser/type_descriptor.h>
#include <ir/parser/parser_utility.h>
#include <ir/parser/parser.h>
#include <Common/functional.h>
namespace toolhub::ir {
#define JUMP_SPACE glb.space.GetNext(ite)
enum class CharState : uint8_t {
	None,
	Word,
	Number,
	Comma,
	LeftRound,
	RightRound,
	LeftBrace,
	RightBrace,
	Colon,
	LeftBracket,
	RightBracket,
	And
};
enum class ParserState : uint8_t {
	Struct,
	Const,
	Shared,
	Function
};
static bool IsKeyword(CharState state) {
	return state == CharState::Word || state == CharState::Number;
}
class ParserGlobalData {
public:
	std::mutex mtx;
	StateRecorder<bool> space;
	StateRecorder<CharState> charTypes;
	vstd::HashMap<vstd::string_view, ParserState> parserState;
	bool inited = false;
	static void InitSpace(typename StateRecorder<bool>::ArrayType& ptr) {
		ptr[' '] = true;
		ptr['\t'] = true;
		ptr['\r'] = true;
		ptr['\n'] = true;
	}
	static void InitChar(CharState* ptr) {
		ptr['_'] = CharState::Word;
		for (auto&& i : vstd::ptr_range(ptr + 'a', ptr + 'z' + 1)) {
			i = CharState::Word;
		}
		for (auto&& i : vstd::ptr_range(ptr + 'A', ptr + 'Z' + 1)) {
			i = CharState::Word;
		}
		for (auto i : vstd::range(10)) {
			ptr[i + 48] = CharState::Number;
		}
		ptr['-'] = CharState::Number;
		ptr['.'] = CharState::Number;
		ptr[','] = CharState::Comma;
		ptr['('] = CharState::LeftRound;
		ptr[')'] = CharState::RightRound;
		ptr['{'] = CharState::LeftBrace;
		ptr['}'] = CharState::RightBrace;
		ptr[':'] = CharState::Colon;
		ptr['['] = CharState::LeftBracket;
		ptr[']'] = CharState::RightBracket;
		ptr['&'] = CharState::And;
	}
	void Init() {
		std::lock_guard lck(mtx);
		if (inited) return;
		inited = true;
		space.Init(InitSpace);
		charTypes.Init(InitChar);
		parserState.Emplace("struct", ParserState::Struct);
		parserState.Emplace("def", ParserState::Function);
		parserState.Emplace("const", ParserState::Const);
		parserState.Emplace("shared", ParserState::Shared);
	}
};
static ParserGlobalData glb;
Parser::Parser() {
	glb.Init();
	stateName.Init();
	stateName.recorder = &recorder;
}

bool Parser::ParseType(TypeDescriptor& result, char const*& ite) {
	auto func = [&ite, this]<bool parseSubType>(auto& func, TypeDescriptor& result) -> bool {
		if (glb.charTypes.Get(*ite) == CharState::Word) {
			result = {glb.charTypes.GetNext(ite, IsKeyword), 0};
		} else {
			errorMsg = "Invalid type in struct!";
			return false;
		}
		JUMP_SPACE;
		if constexpr (parseSubType) {
			if (*ite == '<') {
				++ite;
				JUMP_SPACE;
				result.subType = vstd::create_unique(new TypeDescriptor());
				if (!func.template operator()<false>(func, *result.subType)) return false;
				JUMP_SPACE;
				if (*ite != '>') {
					errorMsg = "require '>'";
					return false;
				}
				++ite;
				JUMP_SPACE;
			}
		}
		if (*ite == '[') {
			++ite;
			JUMP_SPACE;
			if (glb.charTypes.Get(*ite) != CharState::Number) {
				errorMsg = "array's size must be valid number";
				return false;
			}
			auto arraySizeStr = glb.charTypes.GetNext(ite, CharState::Number);
			auto arraySize = StringUtil::StringToNumber(arraySizeStr).get_or<int64>(-1);
			if (arraySize < 0) {
				errorMsg = "array's size must be unsigned integer";
				return false;
			}
			result.typeArrSize = arraySize;
			JUMP_SPACE;
			if (*ite != ']') {
				errorMsg = "require ']'";
				return false;
			}
			++ite;
		}
		return true;
	};
	return func.operator()<true>(func, result);
}
Type const* Parser::GetType(
	TypeDescriptor const& desc) {
	auto func = [this]<bool subType>(auto& func, TypeDescriptor const& desc) -> Type const* {
		auto GetMember = [&](vstd::HashCache<vstd::string_view> const& typeName) -> Type const* {
			auto member = Kernel::GetBuiltinType(typeName);
			if (member != nullptr)
				return member;
			auto customIte = customTypes.Find(typeName);
			if (customIte)
				return customIte.Value();
			return nullptr;
		};
		auto&& typeArraySize = desc.typeArrSize;
		if (typeArraySize == 0) {
			if constexpr (subType) {
				if (desc.subType != nullptr) {
					vstd::string tempStr;
					tempStr << desc.typeName << '(' << desc.subType->typeName;
					auto customIte = customTypes.Find(tempStr);
					if (customIte)
						return customIte.Value();
					auto parentIte = Kernel::GetBuiltinType(desc.typeName);
					if (!parentIte) {
						std::cout << desc.typeName << '\n';
						return nullptr;
					}
					auto subType = func.template operator()<false>(func, *desc.subType);
					Type* newType = kernel->allocator.Allocate<Type>(*parentIte);
					newType->element = subType;
					return newType;
				}
			}
			vstd::HashCache<vstd::string_view> typeName = desc.typeName;
			auto member = GetMember(typeName);
			if (!member) {
				errorMsg << "Unknown type: " << *typeName.key;
				return nullptr;
			}
			return member;

		} else {
			if constexpr (subType) {
				if (desc.subType != nullptr) return nullptr;
			}
			vstd::HashCache<vstd::string_view> typeName = desc.typeName;
			vstd::string arrayName;
			arrayName << *typeName.key << '[' << vstd::to_string(typeArraySize) << ']';
			auto customIte = customTypes.Find(arrayName);
			if (customIte) {
				return customIte.Value();
			}
			auto element = GetMember(typeName);
			if (!element) {
				errorMsg << "Unknown type: " << *typeName.key;
				return nullptr;
			}
			auto arrayType = kernel->allocator.Allocate<Type>();
			arrayType->tag = Type::Tag::Array;
			arrayType->size = element->size * typeArraySize;
			arrayType->dimension = typeArraySize;
			arrayType->alignment = element->alignment;
			arrayType->element = element;
			customTypes.Emplace(std::move(arrayName), arrayType);
			return arrayType;
		}
	};
	return func.operator()<true>(func, desc);
}

bool Parser::ParseStruct(char const*& ite) {
	JUMP_SPACE;
	// align number
	if (glb.charTypes.Get(*ite) != CharState::Number) {
		errorMsg = "Invalid syntax: require number after \"struct\"";
		return false;
	}
	auto alignNumStr = glb.charTypes.GetNext(ite, CharState::Number);
	auto alignSize = StringUtil::StringToNumber(alignNumStr).get_or<int64>(-1);
	if (alignSize < 0) {
		errorMsg = "align number must be integer and larger than zero!";
		return false;
	}
	JUMP_SPACE;
	if (!IsKeyword(glb.charTypes.Get(*ite))) {
		errorMsg = "Invalid syntax: struct name can only be number or word!";
		return false;
	}
	// struct name
	auto structName = glb.charTypes.GetNext(ite, IsKeyword);
	JUMP_SPACE;
	if (*ite != '{') {
		errorMsg = "struct body must start with '{'!";
		return false;
	}
	++ite;
	vstd::vector<TypeDescriptor> typeStrv;
	while (true) {
		JUMP_SPACE;
		if (*ite == '}') {
			++ite;
			break;
		}
		auto& value = typeStrv.emplace_back();
		if (!ParseType(value, ite)) return false;
	}
	Type* structType = kernel->allocator.Allocate<Type>();
	auto structMembers = kernel->allocator.AllocateSpan<Type const*>(typeStrv.size(), [](vstd::span<Type const*> sp) {})->ptr();
	structType->tag = Type::Tag::Structure;
	structType->members = {structMembers,
						   typeStrv.size()};
	structType->alignment = alignSize;
	auto CalcAlign = [](uint64 value, uint64 align) {
		return (value + (align - 1)) & ~(align - 1);
	};
	for (auto&& i : structType->members) {
	}
	size_t size = 0;
	for (auto i : vstd::range(typeStrv.size())) {
		auto type = GetType(typeStrv[i]);
		if (!type) return false;
		structMembers[i] = type;
		size = CalcAlign(size, type->alignment);
	}
	structType->size = std::max<size_t>(1, size);
	customTypes.Emplace(structName, structType);
	// analysis types
	return true;
}
bool Parser::ParseFunction(char const*& ite) {
	JUMP_SPACE;
	if (glb.charTypes.Get(*ite) != CharState::Word) {
		errorMsg = "Invalid syntax: require name after \"def\"";
		return false;
	}
	auto funcName = glb.charTypes.GetNext(ite, IsKeyword);
	JUMP_SPACE;
	if (*ite != '(') {
		errorMsg = "require '(' after function";
		return false;
	}
	++ite;
	// parse argument
	functionVars.clear();
	for (bool loop = true; loop;) {
		JUMP_SPACE;
		auto state = glb.charTypes.Get(*ite);
		switch (state) {
			case CharState::Word:
			case CharState::Number: {
				auto argName = glb.charTypes.GetNext(ite, IsKeyword);
				JUMP_SPACE;
				if (*ite != ':') {
					errorMsg = "require argument's type";
					return false;
				}
				++ite;
				JUMP_SPACE;
				TypeDescriptor typeDesc;
				ParseType(typeDesc, ite);
				bool argIsRef;
				if (*ite == '&') {
					argIsRef = true;
					++ite;
				} else
					argIsRef = false;
				JUMP_SPACE;
				auto argType = GetType(typeDesc);
				if (!argType) {
					errorMsg << "Invalid argument type: " << typeDesc.typeName;
					return false;
				}
				Var* argVar;
				if (argIsRef) {
					argVar = kernel->allocator.Allocate<RefVar>();
				} else {
					argVar = kernel->allocator.Allocate<VariableVar>();
				}
				argVar->type = argType;
				argVar->usage = Usage::Read;
				functionVars.emplace_back(argName, argVar);
				if (*ite == ',') {
					++ite;
				}
			} break;
			case CharState::RightRound:
				++ite;
				loop = false;
				break;
			default:
				errorMsg = "Invalid argument";
				return false;
		}
	};
	JUMP_SPACE;
	TypeDescriptor returnTypeName;
	if (*ite == ':') {
		++ite;
		JUMP_SPACE;
		if (!IsKeyword(glb.charTypes.Get(*ite))) {
			errorMsg = "Invalid return type";
			return false;
		}
		if (!ParseType(returnTypeName, ite)) {
			return false;
		}
		JUMP_SPACE;
	}
	if (*ite != '{') {
		errorMsg = "require '{' after function return type";
		return false;
	}
	++ite;
	recorder.AddArguments(functionVars);
	for (bool loop = true; loop;) {
		JUMP_SPACE;
		switch (glb.charTypes.Get(*ite)) {
			case CharState::Word:
			case CharState::Number: {
				vstd::vector<vstd::string_view> argNames;
				vstd::string_view returnName;
				TypeDescriptor returnNameType;
				vstd::string_view stateName;
				if (!ParseStatement(ite, returnName, returnNameType, stateName, argNames)) {
					return false;
				}
				// TODO: process statement

			} break;
			case CharState::RightBrace:
				++ite;
				loop = false;
				break;
			default:
				errorMsg = "require function declare";
				return false;
		}
	}
	auto callable = stateName.AddCustomFunc(std::move(returnTypeName), funcName, functionVars, recorder.GetStatements());
	if (!callable)
		return false;
	recorder.ClearFunction();
	if (funcName == "main"_sv)
		kernel->mainCallable = callable;
	else
		kernel->callables.emplace_back(callable);
	return true;
}
bool Parser::ParseStatement(
	char const*& ite,
	vstd::string_view& returnName,
	TypeDescriptor& returnTypeName,
	vstd::string_view& stateName,
	vstd::vector<vstd::string_view>& argNames) {
	if (!IsKeyword(glb.charTypes.Get(*ite))) {
		errorMsg = "require statemenet name";
		return false;
	}
	stateName = glb.charTypes.GetNext(ite, IsKeyword);
	returnName = {};
	returnTypeName = TypeDescriptor({}, 0);
	JUMP_SPACE;
	switch (*ite) {
		case '=': {
			returnName = stateName;
			++ite;
			JUMP_SPACE;
			stateName = glb.charTypes.GetNext(ite, IsKeyword);
			JUMP_SPACE;
			if (*ite != '(') {
				errorMsg = "require '(' after statement";
				return false;
			}
		} break;
		case '(':
			break;
		default: {
			errorMsg = "require '(' after statement";
			return false;
		}
	}

	++ite;
	for (bool loop = true; loop;) {
		JUMP_SPACE;
		if (*ite == ')') {
			++ite;
			loop = false;
			break;
		} else if (!IsKeyword(glb.charTypes.Get(*ite))) {
			errorMsg = "Invalid statement arguments";
			return false;
		}
		auto argName = glb.charTypes.GetNext(ite, IsKeyword);
		JUMP_SPACE;
		argNames.emplace_back(argName);
		JUMP_SPACE;
		switch (*ite) {
			case ')':
				++ite;
				loop = false;
				break;
			case ',':
				++ite;
				break;
			default:
				errorMsg = "Invalid statement argument";
				return false;
		}
		if (*ite == ',') {
			++ite;
			break;
		}
	}
	if (!returnName.empty()) {
		JUMP_SPACE;
		if (*ite != ':') {
			errorMsg = "require func call type";
			return false;
		}
		++ite;
		JUMP_SPACE;
		if (!ParseType(returnTypeName, ite)) {
			errorMsg = "error return type";
			return false;
		}
	}
	VarDescriptor varDesc(returnName, std::move(returnTypeName));
	bool result = this->stateName.Run(stateName, std::move(varDesc), argNames);
	if (!result) {
		errorMsg = "illegal statement";
	}
	return result;
}

bool Parser::ParseConst(char const*& ite) {
	JUMP_SPACE;
	TypeDescriptor typeName;
	if (!ParseType(typeName, ite))
		return false;
	auto type = GetType(typeName);
	if (!type) return false;
	JUMP_SPACE;
	if (!IsKeyword(glb.charTypes.Get(*ite))) {
		errorMsg = "require const value name";
		return false;
	}
	auto varName = glb.charTypes.GetNext(ite, IsKeyword);
	JUMP_SPACE;
	if (*ite != '{') {
		errorMsg = "require '{'";
		return false;
	}
	++ite;
	vstd::vector<vstd::variant<int64, double>, VEngine_AllocType::VEngine, 1> arrayResult;
	bool containedFloat = false;
	for (bool loop = true; loop;) {
		JUMP_SPACE;
		if (glb.charTypes.Get(*ite) != CharState::Number) {
			errorMsg = "require number";
			return false;
		}
		auto num = StringUtil::StringToNumber(glb.charTypes.GetNext(ite, CharState::Number));
		if (!num.valid()) {
			errorMsg = "illegal number";
			return false;
		}
		if (num.IsTypeOf<double>()) {
			containedFloat = true;
		}
		arrayResult.emplace_back(num);
		JUMP_SPACE;
		switch (*ite) {
			case '}':
				loop = false;
				++ite;
				break;
			case ',':
				++ite;
				break;
			default:
				errorMsg << "illegal syntax: '" << *ite << '\'';
				return false;
		}
	}
	if (arrayResult.size() > typeName.typeArrSize) {
		errorMsg = "literal value more than expected";
		return false;
	}
	auto result = kernel->allocator.Allocate<ConstantVar>();
	kernel->constants.emplace_back(result);
	recorder.globalVarMap.Emplace(varName, result);
	auto Set = [&](size_t idx, auto& result) {
		arrayResult[idx].visit([&](auto&& v) {
			result = static_cast<std::remove_cvref_t<decltype(result)>>(v);
		});
	};
	auto SetSpan = [&]<typename T>() {
		auto sp = kernel->allocator.AllocateSpan<T>(arrayResult.size(), [](auto sp) {})->span();
		for (auto i : vstd::range(sp.size())) {
			Set(i, sp[i]);
		}
		result->data = sp;
	};
	auto SetSpanVector = [&]<typename Vec>(size_t dim) {
		auto sp = kernel->allocator.AllocateSpan<Vec>(arrayResult.size() / dim, [](auto sp) {})->span();
		size_t idx = 0;
		for (auto i : vstd::range(sp.size())) {
			float* ptr = reinterpret_cast<float*>(sp.data() + i);
			for (auto v : vstd::range(dim)) {
				Set(idx, ptr[v]);
				++idx;
			}
		}
		result->data = sp;
	};
	auto SetSpanMatrix = [&]<typename Mat>(size_t dim) {
		auto sp = kernel->allocator.AllocateSpan<Mat>(arrayResult.size() / (dim * dim), [](auto sp) {})->span();
		size_t idx = 0;
		for (auto i : vstd::range(sp.size())) {
			for (auto c : vstd::range(dim)) {
				float* ptr = reinterpret_cast<float*>(&sp[i].cols[c]);
				for (auto v : vstd::range(dim)) {
					Set(idx, ptr[v]);
					++idx;
				}
			}
		}
		result->data = sp;
	};
	result->type = type;
	if (type->tag == Type::Tag::Array)
		type = type->element;
	switch (type->tag) {
		case Type::Tag::Float:
			SetSpan.operator()<float>();
			break;
		case Type::Tag::Int:
			SetSpan.operator()<int>();
			break;
		case Type::Tag::UInt:
			SetSpan.operator()<uint>();
			break;
		case Type::Tag::Bool:
			SetSpan.operator()<bool>();
			break;
		case Type::Tag::Vector:
			switch (type->dimension) {
				case 2:
					SetSpanVector.operator()<float2>(2);
					break;
				case 3:
					SetSpanVector.operator()<float3>(3);
					break;
				case 4:
					SetSpanVector.operator()<float4>(4);
					break;
			}
			break;
		case Type::Tag::Matrix:
			switch (type->dimension) {
				case 2:
					SetSpanMatrix.operator()<float2x2>(2);
					break;
				case 3:
					SetSpanMatrix.operator()<float3x3>(3);
					break;
				case 4:
					SetSpanMatrix.operator()<float4x4>(4);
					break;
			}
			break;
	}
	return true;
}
bool Parser::ParseGroupShared(char const*& ite) { return true; }
vstd::variant<
	vstd::unique_ptr<Kernel>,
	vstd::string>
Parser::Parse(vstd::string const& str) {
	auto CollectKernel = [&] {
		auto allocTypes = kernel->allocator.AllocateSpan<Type const*>(customTypes.size(), [](auto d) {});
		kernel->types = allocTypes->span();
		auto typePtr = allocTypes->ptr();
		for (auto&& i : customTypes) {
			(*typePtr) = i.second;
			typePtr++;
		}
	};
	errorMsg = vstd::string();
	char const* ite = str.data();
	kernel = vstd::create_unique(new Kernel());
	stateName.parser = this;
	recorder.objAlloc = &kernel->allocator;
	stateName.objAlloc = &kernel->allocator;
	customTypes.Clear();
	recorder.Reset();
	varIndex = 0;
	while (ite) {
		JUMP_SPACE;
		if (!*ite) {
			CollectKernel();
			return std::move(kernel);
		}
		switch (glb.charTypes.Get(*ite)) {
			case CharState::Word: {
				auto word = glb.charTypes.GetNext(ite, CharState::Word);
				auto parseIte = glb.parserState.Find(word);
				auto LoadError = [&] {
					errorMsg << "Invalid syntax: " << word;
				};
				if (!parseIte) {
					LoadError();
					return std::move(errorMsg);
				}
#define TIF(FUNC) \
	if (!(FUNC)) { return std::move(errorMsg); }
				switch (parseIte.Value()) {
					case ParserState::Const:
						TIF(ParseConst(ite));
						break;
					case ParserState::Function:
						TIF(ParseFunction(ite));
						break;
					case ParserState::Shared:
						TIF(ParseGroupShared(ite));
						break;
					case ParserState::Struct:
						TIF(ParseStruct(ite));
						break;
					default:
						LoadError();
						return std::move(errorMsg);
				}
			} break;
#undef TIF
			default: {
				errorMsg << "Invalid syntax: " << *ite;
				return std::move(errorMsg);
			} break;
		}
	}
	CollectKernel();
	return std::move(kernel);
}
}// namespace toolhub::ir
