

#include <Graphics/ShaderCompiler/CodeSample/CodeCompileLibrary.h>
#include <io.h>
PreloadData PreloadData::preloadData;

PreloadData::PreloadData() {
	for (char c = 'a'; c <= 'z'; ++c) {
		IsWord[(uint8_t)c] = 1;
	}
	for (char c = 'A'; c <= 'Z'; ++c) {
		IsWord[(uint8_t)c] = 1;
	}
	for (char c = '0'; c <= '9'; ++c) {
		IsWord[(uint8_t)c] = 1;
	}
	IsWord[(uint8_t)'.'] = 1;
	IsWord[(uint8_t)'_'] = 1;
	IsSpace[(uint8_t)'\t'] = 1;
	IsSpace[(uint8_t)' '] = 1;
	IsSpace[(uint8_t)'\r'] = 1;
	IsSpace[(uint8_t)'\n'] = 1;
	IsSpace[(uint8_t)'\\'] = 1;
	auto SetOperator = [&](char const* c) -> void {
		for (; *c; ++c) {
			IsOperator[(uint8_t)*c] = 1;
		}
	};
	SetOperator(
		"!@#$%^&*()_+-=~`{}|:\"<>?[]\\;\',./");
}

CharCutState SplitSpace::operator()(char c) {
	if (PreloadData::preloadData.IsSpace[(uint8_t)c]) {
		isSpace = true;
		return CharCutState::Cull;
	} else {
		if (isSpace) {
			isSpace = false;
			return CharCutState::Different;
		} else {
			return CharCutState::Same;
		}
	}
}

CharCutState SplitCode::operator()(char c) {
	if (PreloadData::preloadData.IsWord[(uint8_t)c]) {
		if (!isWord) {
			isWord = true;
			return CharCutState::Different;
		} else {
			return CharCutState::Same;
		}
	} else if (PreloadData::preloadData.IsSpace[(uint8_t)c]) {
		isWord = false;
		return CharCutState::Cull;
	}
	isWord = false;
	return CharCutState::Different;
}
CharCutState SplitMacro::operator()(char c) {
	if (PreloadData::preloadData.IsWord[(uint8_t)c]) {
		if (state == IsWord)
			return CharCutState::Same;
		else {
			state = IsWord;
			return CharCutState::Different;
		}
	} else if (PreloadData::preloadData.IsSpace[(uint8_t)c]) {
		state = IsOther;
		return CharCutState::Cull;
	} else if (PreloadData::preloadData.IsOperator[(uint8_t)c]) {
		if (state == IsOperator)
			return CharCutState::Same;
		else {
			state = IsOperator;
			return CharCutState::Different;
		}
	}
	state = IsOther;
	return CharCutState::Different;
}
KeywordStruct::KeywordStruct() {}
uint KeywordStruct::IsKeyWord(vstd::string const& word) const {
	auto ite = keyword_chunk.Find(word);
	if (!ite) return -1;
	return ite.Value();
}
uint KeywordStruct::IsKeyWord(vstd::string_view const& word) const {
	auto ite = keyword_chunk.Find(word);
	if (!ite) return -1;
	return ite.Value();
}

void KeywordStruct::Add(std::initializer_list<char const*> lst) {
	for (auto& j : lst) {
		keyword_chunk.Emplace(j, value);
	}
	value++;
}

KeywordStruct KeywordStruct::hlslTypeKeyword;
KeywordStruct KeywordStruct::hlslTypeSeparatedKeyword;

struct KeywordStruct_Adder {
	KeywordStruct_Adder() {
		KeywordStruct::hlslTypeKeyword.Add(
			{"cbuffer",
			 "ConstantBuffer"});
		KeywordStruct::hlslTypeKeyword.Add(
			{"Texture1D",
			 "Texture1DArray",
			 "Texture2D",
			 "Texture2DArray",
			 "Texture3D",
			 "TextureCube",
			 "Buffer",
			 "StructuredBuffer",
			 "ByteAddressBuffer",
			 "RaytracingAccelerationStructure"});
		KeywordStruct::hlslTypeKeyword.Add(
			{"RWTexture1D",
			 "RWTexture1DArray",
			 "RWTexture2D",
			 "RWTexture2DArray",
			 "RWTexture3D",
			 "RWBuffer",
			 "RWStructuredBuffer",
			 "RWByteAddressBuffer"});

		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"cbuffer",
			 "ConstantBuffer"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"Texture1D",
			 "Texture1DArray"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"RWTexture1D",
			 "RWTexture1DArray"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"Texture2D",
			 "Texture2DArray"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"RWTexture2D",
			 "RWTexture2DArray"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"Texture3D"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"RWTexture3D"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"TextureCube"});

		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"Buffer",
			 "StructuredBuffer",
			 "ByteAddressBuffer"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"RWBuffer",
			 "RWStructuredBuffer",
			 "RWByteAddressBuffer"});
		KeywordStruct::hlslTypeSeparatedKeyword.Add(
			{"RaytracingAccelerationStructure"});
	}
};
static KeywordStruct_Adder keywordStruct_Adder;