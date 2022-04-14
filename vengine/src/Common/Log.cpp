
#include <Common/Log.h>
#include <mutex>
#include <cstdio>
namespace LogGlobal {
static bool isInitialized = false;
static std::mutex mtx;
}// namespace LogGlobal
void VEngine_Log(vstd::string_view const& chunk) {
	std::cout << chunk;
	return;
	using namespace LogGlobal;
	std::lock_guard<decltype(mtx)> lck(mtx);
	FILE* file = nullptr;
	if (!isInitialized) {
		isInitialized = true;
		file = fopen("LoggerFile.log", "w");
		if (file) {
			vstd::string_view chunk = "This is a log file from last run: \n";
			fwrite(chunk.data(), chunk.size(), 1, file);
		}
	} else {
		file = fopen("LoggerFile.log", "a+");
	}
	if (file) {
		fwrite(chunk.data(), chunk.size(), 1, file);
		fclose(file);
	}
}
void VEngine_Log(vstd::string_view const* chunk, size_t chunkCount) {
	for (auto i : vstd::ptr_range(chunk, chunk + chunkCount))
		std::cout << i;
	return;
	using namespace LogGlobal;
	std::lock_guard<decltype(mtx)> lck(mtx);
	FILE* file = nullptr;
	if (!isInitialized) {
		isInitialized = true;
		file = fopen("LoggerFile.log", "w");
		if (file) {
			vstd::string_view chunk = "This is a log file from last run: \n";
			fwrite(chunk.data(), chunk.size(), 1, file);
		}
	} else {
		file = fopen("LoggerFile.log", "a+");
	}
	if (file) {
		for (size_t i = 0; i < chunkCount; ++i)
			fwrite(chunk[i].data(), chunk[i].size(), 1, file);
		fclose(file);
	}
}
void VEngine_Log(std::initializer_list<vstd::string_view> const& initList) {
	VEngine_Log(initList.begin(), initList.size());
}
void VEngine_Log(std::type_info const& t) {
	VEngine_Log(
		{t.name(),
		 " runtime error! Usually did mistake operation, like vstd::optional\n"});
}

void VEngine_Log(char const* chunk) {
	VEngine_Log(vstd::string_view(chunk));
}

void VEngine_Log_PureVirtual(vstd::Type tarType) {
	vstd::string d;
	d.append("Try call pure virtual function in ").append(tarType.name()).append("\n");
	VEngine_Log(d);
	VENGINE_EXIT;
}
