

#include <Utility/StringUtility.h>
#include <Utility/CommonIterators.h>
#include <Network/NetworkInclude.h>
#include <Utility/FunctionExecutor.h>
namespace vstd {
void FunctionExecutor::Dispose() {
	delete this;
}
void FunctionExecutor::Reset() {
	funcMap.Clear();
}
void FunctionExecutor::AddFunctor(vstd::string name, FuncType func) {
	funcMap.Emplace(std::move(name), std::move(func));
}

FunctionExecutor::FunctionExecutor() {
	keywords.Emplace("true", KeyWord::True);
	keywords.Emplace("false", KeyWord::False);
	keywords.Emplace("null", KeyWord::Null);
}
char const* FunctionExecutor::GetNextChar(char const* ptr, char const* end) const {
	auto&& s = recorders.spaces;
	while (ptr != end) {
		if (!s[*ptr]) return ptr;
		ptr++;
	}
	return ptr;
}
template<char beginEnd>
bool FunctionExecutor::GetString(char const*& ptr, char const* const end, vstd::string& str) const {
	ptr++;
	char const* start = ptr;
	bool isSlash = false;
	size_t sz = 31;
	size_t idx = 0;
	str.resize(sz + 1);
	auto push = [&](char c) {
		if (idx >= sz) {
			sz = sz * 2 + 1;
			str.resize(sz + 1);
		}
		str[idx++] = c;
	};
	auto func = [&]() {
		while (true) {
			if (isSlash) {
				isSlash = false;
				switch (*ptr) {
					case '\\':
						push('\\');
						break;
					case 't':
						push('\t');
						break;
					case 'r':
						push('\r');
						break;
					case 'n':
						push('\n');
						break;
					case '\'':
						push('\'');
						break;
					case '\"':
						push('\"');
						break;
				}
			} else {
				switch (*ptr) {
					case '\\':

						isSlash = true;
						break;
					case beginEnd:
						return true;
					default:
						push(*ptr);
						break;
				}
			}
			++ptr;
			if (ptr == end) {
				return false;
			}
		}
		return true;
	};
	if (!func()) return false;
	++ptr;
	str.resize(idx);
	return true;
}
bool FunctionExecutor::GetElementVector(
	char const*& ptr,
	char const* const end,
	vstd::vector<vstd::unique_ptr<Element>>& elements) const {
	++ptr;
	while (true) {
		ptr = GetNextChar(ptr, end);
		if (ptr == end) return false;
		if (*ptr == ')') {
			return true;
		}
		auto var = ParseElement(ptr, end);
		if (!var.valid())
			return false;
		elements.emplace_back(new Element(std::move(var)));
		ptr = GetNextChar(ptr, end);
		switch (*ptr) {
			case ')':
				ptr++;
				return true;
			case ',': {
				ptr++;
				continue;
			}
			default:
				return false;
		}
	}
	return false;
}
FunctionExecutor::Element::VariantType FunctionExecutor::ParseElement(char const*& ptr, char const* const end) const {
	if (recorders.pureNumCheck[*ptr]) {//Number
		char const* start = ptr;
		do {
			++ptr;
			if (ptr == end) {
				break;
			}
		} while (recorders.numCheck[*ptr]);
		auto numStr = vstd::string_view(start, ptr);
		auto value = StringUtil::StringToNumber(numStr);
		switch (value.GetType()) {
			case 0:
				return value.template get<0>();
			case 1:
				return value.template get<1>();
			default:
				return {};
		}
	} else if (recorders.keywordStates[*ptr]) {//Bool
		char const* start = ptr;
		++ptr;
		while (recorders.keywordStates[*ptr]) {
			++ptr;
			if (ptr == end) {
				break;
			}
		}
		auto ite = keywords.Find(vstd::string_view(start, ptr));
		if (!ite)
			return {};
		switch (ite.Value()) {
			case KeyWord::True:
				return true;
			case KeyWord::False:
				return false;
			case KeyWord::Null:
				return nullptr;
		}
	} else {
		switch (*ptr) {
			case '$': {
				ptr++;
				char const* start = ptr;
				while (recorders.guidCheck[*ptr]) {
					++ptr;
					if (ptr == end) {
						break;
					}
				}
				auto guidStr = vstd::string_view(start, ptr);
				auto guid = vstd::Guid::TryParseGuid(guidStr);
				if (!guid) {
					return {};
				} else
					return *guid;
			} break;
			case '\'': {
				vstd::string str;
				if (!GetString<'\''>(ptr, end, str))
					return {};
				return str;
			} break;
			case '"': {
				vstd::string str;
				if (!GetString<'"'>(ptr, end, str))
					return {};
				return str;
			} break;
			case '(': {
				vstd::vector<vstd::unique_ptr<Element>> elements;
				if (!GetElementVector(ptr, end, elements))
					return {};
				return elements;
			} break;
		}
	}
	return {};
}
vstd::string FunctionExecutor::Execute(vstd::string_view cmd) {
	auto ptr = cmd.data();

	auto work = [&]() -> vstd::string {
		auto funcIte = funcMap.Find(vstd::string_view(cmd.data(), ptr));
		if (!funcIte) {
			return "e2";
		}
		auto var = ParseElement(ptr, cmd.data() + cmd.size());
		if (!var.valid()) {
			return "e1";
		}
		return funcIte.Value()(std::move(var));
	};
	while (ptr != cmd.data() + cmd.size()) {
		if (*ptr == '(') {
			return work();
		}
		ptr++;
	}
	return "e0";
}

}// namespace vstd