
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
	vstd::HashMap<vstd::string_view, vstd::function<void(Type*)>> resTypeFuncs;
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
		resTypeFuncs.Emplace("Texture", [](Type* t) {
			t->tag = Type::Tag::Texture;
		});
		resTypeFuncs.Emplace("Buffer", [](Type* t) {
			t->tag = Type::Tag::Buffer;
		});
	}
};
static ParserGlobalData glb;
Parser::Parser() {
	glb.Init();
	stateName.Init();
	stateName.recorder = &recorder;
}
Type const* Parser::FindType(vstd::string_view name) {
	vstd::HashCache<vstd::string_view> nameKey(name);
	auto builtinType = Kernel::GetBuiltinType(nameKey);
	if (builtinType) return builtinType;
	auto ite = customTypes.Find(nameKey);
	if (!ite) return nullptr;
	return ite.Value();
}
bool Parser::ParseType(TypeDescriptor& result, char const*& ite) {
	if (glb.charTypes.Get(*ite) == CharState::Word) {
		result = {glb.charTypes.GetNext(ite, IsKeyword), 0};
	} else {
		errorMsg = "Invalid type in struct!";
		return false;
	}
	JUMP_SPACE;
	if (glb.charTypes.Get(*ite) == CharState::LeftBracket) {
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
}
Type const* Parser::GetType(vstd::string_view typeNameStr, size_t arraySize) {
	auto GetMember = [&](vstd::HashCache<vstd::string_view> const& typeName) -> Type const* {
		auto member = Kernel::GetBuiltinType(typeName);
		if (member != nullptr)
			return member;
		auto customIte = customTypes.Find(typeName);
		if (customIte)
			return customIte.Value();
		auto splitIte = StringUtil::Split(typeNameStr, '_').begin();
		if (splitIte == vstd::IteEndTag()) return nullptr;
		auto ite = glb.resTypeFuncs.Find(*splitIte);
		auto tp = kernel->allocator.Allocate<Type>();
		if (ite) {
			ite.Value()(tp);
		}
		++splitIte;
		if (splitIte == vstd::IteEndTag()) return nullptr;
		tp->element = GetType(*splitIte, 0);
		++splitIte;
		if (splitIte != vstd::IteEndTag()) return nullptr;
		return tp;
	};
	vstd::HashCache<vstd::string_view> typeName = typeNameStr;
	auto&& typeArraySize = arraySize;
	if (typeArraySize == 0) {
		auto member = GetMember(typeName);
		if (!member) {
			errorMsg << "Unknown type: " << *typeName.key;
			return nullptr;
		}
		return member;
	} else {
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
	vstd::vector<std::pair<vstd::string_view, size_t>> typeStrv;
	for (bool loop = true; loop;) {
		JUMP_SPACE;
		switch (glb.charTypes.Get(*ite)) {
			case CharState::RightBrace:
				++ite;
				loop = false;
				break;
			case CharState::LeftBracket: {
				if (typeStrv.empty()) {
					errorMsg = "Invalid syntax: '['"_sv;
					return false;
				}
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
				(typeStrv.end() - 1)->second = arraySize;
				JUMP_SPACE;
				if (*ite != ']') {
					errorMsg = "require ']'";
					return false;
				}
				++ite;
			} break;
			case CharState::Word: {
				typeStrv.emplace_back(glb.charTypes.GetNext(ite, IsKeyword), 0);
			} break;
			default: {
				errorMsg = "Invalid type in struct!";
				return false;
			}
		}
	}
	Type* structType = kernel->allocator.Allocate<Type>();
	auto structMembers = kernel->allocator.AllocateSpan<Type const*>(typeStrv.size(), [](vstd::span<Type const*> sp) {})->ptr();
	structType->tag = Type::Tag::Structure;
	structType->members = {structMembers,
						   typeStrv.size()};
	structType->alignment = alignSize;

	for (auto i : vstd::range(typeStrv.size())) {
		auto type = GetType(typeStrv[i].first, typeStrv[i].second);
		if (!type) return false;
		structMembers[i] = type;
	}
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
				if (!IsKeyword(glb.charTypes.Get(*ite))) {
					errorMsg = "require argument type name";
					return false;
				}
				auto argTypeName = glb.charTypes.GetNext(ite, IsKeyword);
				bool argIsRef;
				if (*ite == '&') {
					argIsRef = true;
					++ite;
				} else
					argIsRef = false;
				JUMP_SPACE;
				auto argType = FindType(argTypeName);
				if (!argType) {
					errorMsg << "Invalid argument type: " << argTypeName;
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
	auto callable = stateName.AddCustomFunc(returnTypeName, funcName, functionVars, recorder.GetStatements());
	if (!callable)
		return false;
	recorder.ClearFunction();
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
		if (!ParseType(returnTypeName, ite))
			return false;
	}
	VarDescriptor varDesc(returnName, returnTypeName);
	bool result = this->stateName.Run(stateName, varDesc, argNames);
	return result;
}

bool Parser::ParseConst(char const*& ite) {
	JUMP_SPACE;
	TypeDescriptor typeName;
	if (!ParseType(typeName, ite))
		return false;
	auto type = GetType(typeName.typeName, typeName.typeArrSize);
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
