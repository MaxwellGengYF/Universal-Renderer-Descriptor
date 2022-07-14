
#include <Utility/BinaryReader.h>
BinaryReader::BinaryReader(vstd::string const& path) {
	currentPos = 0;
	ifs = fopen(path.c_str(), "rb");
	isAvaliable = ifs;
	if (isAvaliable) {
		fseek(ifs, 0, SEEK_END);
		length = ftell(ifs);
		fseek(ifs, 0, SEEK_SET);
	} else {
		length = 0;
	}
}
void BinaryReader::Read(void* ptr, uint64 len) {
	if (!isAvaliable) return;
	uint64 targetEnd = currentPos + len;
	if (targetEnd > length) {
		targetEnd = length;
		len = targetEnd - currentPos;
	}
	uint64 lastPos = currentPos;
	currentPos = targetEnd;
	if (len == 0) return;
	fseek(ifs, lastPos, SEEK_SET);
	fread(ptr, len, 1, ifs);
}
vstd::string BinaryReader::ReadToString() {
	vstd::string str;
	str.resize(length);
	Read(str.data(), str.size());
	return str;
}
vstd::vector<vbyte> BinaryReader::Read(bool addNullEnd) {
	if (!isAvaliable) return vstd::vector<vbyte>();
	auto len = length;
	uint64 targetEnd = currentPos + len;
	if (targetEnd > length) {
		targetEnd = length;
		len = targetEnd - currentPos;
	}
	if (len == 0) {
		if (addNullEnd)
			return vstd::vector<vbyte>({0});
		else
			return vstd::vector<vbyte>();
	}
	vstd::vector<vbyte> result;
	result.resize(addNullEnd ? (len + 1) : len);
	uint64 lastPos = currentPos;
	currentPos = targetEnd;

	fseek(ifs, lastPos, SEEK_SET);
	fread(result.data(), len, 1, ifs);
	return result;
}

void BinaryReader::SetPos(uint64 pos) {
	if (!isAvaliable) return;
	if (pos > length) pos = length;
	currentPos = pos;
}
BinaryReader::~BinaryReader() {
	if (!isAvaliable) return;
	fclose(ifs);
}
