#pragma once
#include <Graphics/ShaderCompiler/CodeSample/SeparateRules.h>
#include <Common/Common.h>
#include <Common/functional.h>
#include <Graphics/ShaderCompiler/CodeSample/UsefulSyntax.h>
#include <Utility/StringUtility.h>
#include <Graphics/ShaderCompiler/CodeSample/CodeCompileLibrary.h>
#include <Common/BitVector.h>
#include <Graphics/SceneDescriptor/IShader.h>
using IShader = toolhub::graphics::IShader;
class RegisterProcessor {
public:
	struct ShaderProperty {
		std::optional<uint32_t> arrayLength;
		vstd::string subTypeName;
		vstd::string typeName;
		vstd::string varName;
	};
	vstd::vector<std::pair<vstd::string_view, bool>> strs;
	vstd::HashMap<char, std::pair<uint32_t, uint32_t>> registerPool;
	vstd::vector<IShader::Property> registerResults;

	//Cache Feild
	//Variables
	vstd::string originString;
	vstd::unique_ptr<ISeparateRule> commentRule;
	vstd::string finalCode;
	void GenerateBaseChunk(vstd::string const& path) {
		StringUtil::ReadStringFromFile(path, originString);
	}
	bool GetVariableData(vstd::string_view const& str, ShaderProperty& var) const {
		WordSyntax typeName(true);
		CoverAreaSyntax cover('<', '>', false);
		WordSyntax varName(true);
		CoverAreaSyntax arrName('[', ']', false);
		CutAreaSyntax cut(':', false);
		WordSyntax registerKeyWord(true);
		SyntaxBase base;
		base.AddSubSyntax(&typeName);
		base.AddSubSyntax(&cover);
		base.AddSubSyntax(&varName);
		base.AddSubSyntax(&arrName);
		base.AddSubSyntax(&cut);

		cut.AddSubSyntax(&registerKeyWord);
		base.SyntaxClear();
		if (!base.SyntaxMatch(str)) {
			return false;
		}

		var.typeName = std::move(typeName.word);
		var.subTypeName = std::move(cover.insideString);
		var.varName = std::move(varName.word);
		if (arrName.IsCaptured()) {
			auto num = StringUtil::StringToNumber(var.subTypeName);
			var.arrayLength = num.visit_or(
				std::numeric_limits<uint>::max(),
				[&](auto&& v) -> uint {
					return v;
				});

		} else
			var.arrayLength.reset();
		if (cut.IsCaptured()) {
			if (registerKeyWord.word != "register")
				return false;
		}
		return true;
	}
	bool ProcessWord(char sign, vstd::string_view const& sentence, vstd::string& codeDest, IShader::PropertyType valueType) {
		ShaderProperty var;
		if (!GetVariableData(sentence, var)) return false;
		uint32_t regisIndex = 0;
		uint32_t spaceIndex = 0;
		{
			vstd::string result;
			result.reserve(64);
			auto add = [](auto&& str, auto&& chunk) -> void {
				str.append(chunk.data(), chunk.size());
			};
			add(result, var.typeName);
			if (var.subTypeName.size() > 0) {
				result += '<';
				add(result, var.subTypeName);
				result += '>';
			}
			result += ' ';
			add(result, var.varName);
			auto regiIte = registerPool.Emplace(sign, std::pair<uint32_t, uint32_t>{0, 0});

			if (var.arrayLength.has_value()) {
				auto v = var.arrayLength.value();
				result += '[';
				if (v != -1) {
					result += vstd::to_string(var.arrayLength.value());
				}
				result += ']';
				regisIndex = 0;
				spaceIndex = ++regiIte.Value().second;
			} else {
				regisIndex = regiIte.Value().first++;
				spaceIndex = 0;
			}
			result += " : register(";
			result += sign;
			result += vstd::to_string(regisIndex);
			if (spaceIndex > 0) {
				result += ", space";
				result += vstd::to_string(spaceIndex);
			}
			result += ')';

			codeDest += result;
		}
		registerResults.push_back(IShader::Property{
			.name = vstd::string(var.varName),
			.arrSize = (uint64)(var.arrayLength.has_value() ? var.arrayLength.value() : uint(0)),
			.type = valueType,
			.registIdx = regisIndex,
			.spaceIdx = spaceIndex});
		return true;
	}
	struct Range {
		char left;
		char right;
		int32_t rangeLevel = 0;
		Range(char left, char right) : left(left), right(right) {}
		void Iterate(char const* ptr) {
			if (*ptr == left) rangeLevel++;
			else if (*ptr == right)
				rangeLevel--;
		}
	};
	void ProcessCode(vstd::BitVector const& inRangeVec, char const* startPtr, char const* end, vstd::string& codeDest) {
		vstd::string_view word;
		char const* start = startPtr;
		char const* wordStart = startPtr;

		char last = 0;
		int x = 0;
		for (char const* ite = start; ite < end;) {
			last = *ite;
			if (!PreloadData::preloadData.IsWord[*ite]) {
				size_t ii = ite - startPtr;
				if (inRangeVec[ii] && (ite - wordStart > 0)) {
					word = vstd::string_view(wordStart, ite);
					bool catched = true;
					char sign = ' ';
					auto value = (IShader::PropertyType)KeywordStruct::hlslTypeSeparatedKeyword.IsKeyWord(word);

					switch (value) {
						case IShader::PropertyType::CBuffer:
							sign = 'b';
							break;
						case IShader::PropertyType::Tex1D:
						case IShader::PropertyType::Tex2D:
						case IShader::PropertyType::Tex3D:
						case IShader::PropertyType::CubeMap:
						case IShader::PropertyType::Buffer:
						case IShader::PropertyType::Accel:
							sign = 't';
							break;
						case IShader::PropertyType::RWTex1D:
						case IShader::PropertyType::RWTex2D:
						case IShader::PropertyType::RWTex3D:
						case IShader::PropertyType::RWBuffer:
							sign = 'u';
							break;
						default:
							catched = false;
					}
					//Start Working on sentence Here
					if (catched) {
						if (wordStart - start > 0) {
							codeDest.append(start, size_t(wordStart - start));
						}
						for (; ite < end; ++ite) {
							if (*ite == '{' || *ite == ';') {
								vstd::string_view sentence(wordStart, ite);
								if (!ProcessWord(sign, sentence, codeDest, value)) {
									codeDest.append(sentence.data(), sentence.size());
								}
								start = ite;
								break;
							}
						}
					}
				}
				wordStart = ite + 1;
			}
			++ite;
		}
		if ((int64_t)(wordStart - start) > 0) {
			codeDest.append(start, (int64_t)(wordStart - start));
		}
		if ((int64_t)(end - wordStart) > 0) {
			codeDest.append(wordStart, (int64_t)(end - wordStart));
		}
	}
	//Separate Code and Comments
	static void GetSeparateDataFromString(vstd::string& originString, vstd::vector<std::pair<vstd::string_view, bool>>& strs) {
		char* lastAvaliable = originString.data();
		auto SaveLast = [&](char* ava, char* i, bool isComment) -> void {
			auto&& str = strs.emplace_back();
			if ((int64_t)(i - ava) > 0) {
				str.first = vstd::string_view(ava, (int64_t)(i - ava));
				str.second = isComment;
			}
		};
		char* end = originString.data() + originString.size();
		for (char* start = originString.data(); start < end - 1; ++start) {
			char c = *start;
			if (c == '\0') {
				end = start;
				break;
			}
			if (StringUtil::CompareCharArray(start, "//", 2)) {
				SaveLast(lastAvaliable, start, false);
				char* commentStart = start;
				for (; start < end; ++start) {
					if (*start == '\n' || *start == '\0') {
						break;
					}
				}
				SaveLast(commentStart, start, true);
				lastAvaliable = start;
			} else if (StringUtil::CompareCharArray(start, "/*", 2)) {
				SaveLast(lastAvaliable, start, false);
				char* commentStart = start;
				for (; start < end - 1; ++start) {
					if (*start == '\0') break;
					if (StringUtil::CompareCharArray(start, "*/", 2)) {
						start += 2;
						break;
					}
				}
				SaveLast(commentStart, start, true);
				lastAvaliable = start;
			} else if (*start == '\"') {
				SaveLast(lastAvaliable, start, false);
				char* commentStart = start;
				start++;
				for (; start < end; ++start) {
					if (start[-1] != '\\' && (*start == '\"')) {
						start += 1;
						break;
					}
				}
				SaveLast(commentStart, start, true);
				lastAvaliable = start;
			}
		}
		SaveLast(lastAvaliable, end, false);
	}

	RegisterProcessor(vstd::string&& data)
		: originString(std::move(data)) {
		GetSeparateDataFromString(originString, strs);
	}
	void GenerateCode() {
		finalCode.clear();
		vstd::BitVector vec;
		Range r[] =
			{
				Range('{', '}'),
				Range('[', ']'),
				Range('(', ')'),
				Range('<', '>')};
		for (auto& i : strs) {
			if (!i.second) {
				vec.Clear();
				vec.Resize(i.first.size());
				char const* d = i.first.data();
				bool isGlobal = true;

				for (uint64 count = 0; count < i.first.size(); ++count) {
					isGlobal = true;
					for (auto& a : r) {
						if (a.rangeLevel > 0)
							isGlobal = false;
						a.Iterate(d + count);
					}
					if (r[0].rangeLevel > 0 || r[2].rangeLevel > 0) {
						r[1].rangeLevel = 0;
						r[3].rangeLevel = 0;
					}
					vec[count] = isGlobal;
				}
				ProcessCode(vec, i.first.data(), i.first.data() + i.first.size(), finalCode);
			} else
				finalCode.append(i.first.data(), i.first.size());
		}
	}
};