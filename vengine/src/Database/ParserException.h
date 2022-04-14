#pragma once
#include <Common/Common.h>
namespace toolhub::db {
struct ParsingException {
	vstd::string message;
	ParsingException() {}
	ParsingException(vstd::string&& msg)
		: message(std::move(msg)) {}
};
}// namespace toolhub::db