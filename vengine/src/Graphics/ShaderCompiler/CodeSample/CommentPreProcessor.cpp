

#include <Graphics/ShaderCompiler/CodeSample/CommentPreProcessor.h>
#include <Utility/StringUtility.h>

bool CommentPreProcessor::PreProcess(vstd::string& source, vstd::string& dest) {
	dest.clear();
	dest.resize(source.size());
	char* destPtr = dest.data();
	size_t destSize = 0;
	auto PushBack = [&](char c) -> void {
		destPtr[destSize] = c;
		destSize++;
	};
	enum State {
		Looking,
		CommentStar,
		CommentLine,
		String
	};
	State state = Looking;
	char const* lineComment = "//";
	char const* starComment = "/*";
	char const* starEndComment = "*/";
	bool isInString = false;
	char const* beg = source.data();
	char const* end = beg + source.size();
	bool lastIsSpace = false;
	while (beg < end) {
		switch (state) {
			case Looking:
				for (; beg < end; beg++) {
					if (StringUtil::IsCharSpace(*beg)) {
						if (lastIsSpace) continue;
						lastIsSpace = true;
						PushBack(' ');
					} else {
						lastIsSpace = false;
						PushBack(*beg);
					}
					if (*beg == '\"') {
						state = State::String;
						beg++;
						break;
					} else if ((*(uint16_t*)lineComment) == (*(uint16_t*)beg)) {
						destSize--;
						state = State::CommentLine;
						beg += 2;
						break;
					} else if ((*(uint16_t*)starComment) == (*(uint16_t*)beg)) {
						destSize--;
						state = State::CommentStar;
						beg += 2;
						break;
					}
				}
				break;
			case CommentLine:
				for (; beg < end; beg++) {
					if (*beg == '\n') {
						beg++;
						state = State::Looking;
						if (!lastIsSpace) {
							PushBack(' ');
							lastIsSpace = true;
						}
						break;
					}
				}
				break;
			case CommentStar:
				for (; beg < end; beg++) {
					if ((*(uint16_t*)starEndComment) == (*(uint16_t*)beg)) {
						beg += 2;
						state = State::Looking;
						break;
					}
				}
				break;
			case String:
				for (; beg < end; beg++) {
					PushBack(*beg);
					if (*beg == '\\') {
						beg++;
						continue;
					}
					if (*beg == '\"') {
						beg++;
						state = State::Looking;
						break;
					}
				}
				break;
		}
	}
	dest.resize(destSize);
	return true;
}

void CommentPreProcessor::Execute(vstd::string const& source, vstd::function<void(vstd::string_view)> const& func) {
	char const* lastPart = source.data();

	enum State {
		Looking,
		CommentStar,
		CommentLine,
		String
	};
	State state = Looking;
	char const* lineComment = "//";
	char const* starComment = "/*";
	char const* starEndComment = "*/";
	bool isInString = false;
	char const* beg = source.data();
	char const* end = beg + source.size();
	auto ExecuteFunc = [&](char const* curPtr) -> void {
		func(vstd::string_view(lastPart, curPtr));
	};
	while (beg < end) {
		switch (state) {
			case Looking:
				for (; beg < end; beg++) {
					if (*beg == '\"') {
						state = State::String;
						beg++;
						break;
					} else if ((*(uint16_t*)lineComment) == (*(uint16_t*)beg)) {
						ExecuteFunc(beg);
						state = State::CommentLine;
						beg += 2;
						break;
					} else if ((*(uint16_t*)starComment) == (*(uint16_t*)beg)) {
						ExecuteFunc(beg);
						state = State::CommentStar;
						beg += 2;
						break;
					}
				}
				break;
			case CommentLine:
				for (; beg < end; beg++) {
					if (*beg == '\n') {
						state = State::Looking;
						lastPart = beg;
						beg++;

						break;
					}
				}
				break;
			case CommentStar:
				for (; beg < end; beg++) {
					if ((*(uint16_t*)starEndComment) == (*(uint16_t*)beg)) {
						beg += 2;
						lastPart = beg;
						state = State::Looking;
						break;
					}
				}
				break;
			case String:
				for (; beg < end; beg++) {
					if (*beg == '\\') {
						beg++;
						continue;
					}
					if (*beg == '\"') {
						beg++;
						state = State::Looking;
						break;
					}
				}
				break;
		}
	}
	if (lastPart != end) {
		ExecuteFunc(end);

	}
}
