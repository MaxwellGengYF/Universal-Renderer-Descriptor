#pragma once
#include "Common.h"
namespace vstd {
class VENGINE_DLL_COMMON StringBuilder final {
	vstd::vector<
		vstd::variant<
			vstd::string_view,
			vstd::string,
			char>,
		VEngine_AllocType::VEngine, 8>
		views;
	size_t size = 0;
	vstd::string* target;

public:
	StringBuilder& operator<<(vstd::string_view str);
	StringBuilder& operator<<(char str);
	StringBuilder& operator<<(vstd::string&& str);
	StringBuilder& operator<<(vstd::string const& str);
	StringBuilder(vstd::string* target);
	~StringBuilder();
	StringBuilder(StringBuilder const&) = delete;
	StringBuilder(StringBuilder&&) = delete;
};
}// namespace vstd