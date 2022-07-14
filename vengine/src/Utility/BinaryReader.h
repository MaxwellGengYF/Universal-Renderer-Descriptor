#pragma once
#include <Common/Common.h>
class VENGINE_DLL_COMMON BinaryReader : public vstd::IOperatorNewBase {
private:
	struct FileSystemData {
		FILE* globalIfs;
		std::mutex* readMtx;
		uint64 offset;
	};
	bool isAvaliable = true;
	union {
		FileSystemData packageData;
		FILE* ifs;
	};
	uint64 length;
	uint64 currentPos;

public:
	BinaryReader(vstd::string const& path);
	void Read(void* ptr, uint64 len);
	vstd::string ReadToString();
	vstd::vector<vbyte> Read(bool addNullEnd = false);
	operator bool() const {
		return isAvaliable;
	}
	bool operator!() const {
		return !operator bool();
	}
	void SetPos(uint64 pos);
	uint64 GetPos() const {
		return currentPos;
	}
	uint64 GetLength() const {
		return length;
	}
	~BinaryReader();

	KILL_COPY_CONSTRUCT(BinaryReader)
};