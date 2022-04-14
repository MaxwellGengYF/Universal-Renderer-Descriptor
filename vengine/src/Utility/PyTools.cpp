
#include <VEngineConfig.h>
#ifdef VENGINE_PYTHON_SUPPORT
#include <Common/Common.h>
#include <JobSystem/ThreadPool.h>
#include <Utility/FileUtility.h>
#include <Utility/StringUtility.h>
#include <Utility/QuickSort.h>
namespace vepy {
vstd::optional<ThreadPool> tPool;

vstd::vector<vstd::string> paths;
vstd::vector<vstd::string> headerPaths;
vstd::optional<vstd::HashMap<vstd::string, void>> ignoreMap;
vstd::optional<vstd::HashMap<vstd::string, void>> avaliableExt;
vstd::optional<vstd::HashMap<vstd::string, vstd::vector<vstd::string>>> packPathes;
VENGINE_UNITY_EXTERN void Py_InitFileSys() {
	tPool.New(std::thread::hardware_concurrency());
	ignoreMap.New();
	avaliableExt.New();
	packPathes.New();
	printf("DLL Load Success!\n");
}

VENGINE_UNITY_EXTERN void Py_AddIgnorePath(
	char const* path) {
	ignoreMap->Emplace(StringUtil::ToLower(path));
}

VENGINE_UNITY_EXTERN void Py_AddExtension(
	char const* ext) {
	avaliableExt->Emplace(ext);
}

void ExecuteFileSys() {
	paths.clear();
	headerPaths.clear();
	auto GetPath = [](auto&& str) {
		if (str.size() < 2) return vstd::string();
		return vstd::string(str.begin() + 2, str.end());
	};
	FileUtility::GetFiles(
		".",
		[&](vstd::string&& str) { paths.emplace_back(GetPath(str)); },
		[&](vstd::string const& str, bool isFile) {
			auto newStr = GetPath(str);
			if (isFile) {
				for (auto i = newStr.end(); i != newStr.begin(); i--) {
					if (*(i - 1) == '.') {
						auto ext = vstd::string_view(i, newStr.end());
						if (ext == "h"_sv || ext == "hpp"_sv) {
							headerPaths.emplace_back(newStr);
						}
						return static_cast<bool>(avaliableExt->Find(ext));
					}
				}
				return false;
			} else {
				if (ignoreMap->Find(StringUtil::ToLower(vstd::string_view(newStr)))) {
					return false;
				}
			}
			return true;
		});
	std::mutex mtx;
	auto getPathJob = tPool->GetParallelTask<true>(
		[&](size_t i) {
			vstd::vector<char> vec(513);
			vec[512] = 0;
			std::ifstream ifs(paths[i].c_str());
			ifs.getline(vec.data(), 512);
			ifs.close();
			size_t lenStr = strlen(vec.data());
			auto IsEmpty = [](char c) {
				switch (c) {
					case '\t':
					case '\n':
					case '\r':
					case ' ':
						return true;
					default:
						return false;
				}
			};
			auto GetCharacter = [&](vstd::string_view filePath) {
				//Cull Empty
				auto Default = []() {
					return vstd::optional<
						std::tuple<vstd::string_view,
								   vstd::string_view>>();
				};
				if (filePath.size() == 0) {
					return Default();
				}
				auto GetNextWord = [&](char const* cur, char const* end, bool isEmpty) {
					for (; cur != end; ++cur) {
						if (IsEmpty(*cur) == isEmpty)
							break;
					}
					return cur;
				};

				auto firstNonEmpty = GetNextWord(filePath.data(), filePath.data() + filePath.size(), false);
				auto firstEmpty = GetNextWord(firstNonEmpty, filePath.data() + filePath.size(), true);
				if (firstNonEmpty == firstEmpty)
					return Default();

				return vstd::optional<
						   std::tuple<vstd::string_view,
									  vstd::string_view>>()
					.New(std::make_tuple(
						vstd::string_view(firstNonEmpty, firstEmpty),
						vstd::string_view(firstEmpty, filePath.data() + filePath.size())));
			};
			auto v = GetCharacter(vstd::string_view(vec.data(), lenStr));
			if (!v || std::get<0>(*v) != "#pragma"_sv) {
				return;
			}
			v = GetCharacter(std::get<1>(*v));
			if (!v || !StringUtil::EqualIgnoreCapital(std::get<0>(*v), "vengine_package"_sv))
				return;
			v = GetCharacter(std::get<1>(*v));
			if (!v) return;
			size_t ed = lenStr;
			for (auto i : vstd::range(lenStr - 1, -1, -1)) {
				if (IsEmpty(vec[i])) {
					ed = i;
				} else
					break;
			}
			auto pkgName = StringUtil::ToLower(vstd::string_view(std::get<0>(*v).data(), vec.data() + ed));
			{
				std::lock_guard lck(mtx);
				auto ite = packPathes->Emplace(
					std::move(pkgName));
				ite.Value().emplace_back(std::move(paths[i]));
			}
		},
		paths.size(), 1);
	getPathJob.Complete();
	vstd::vector<vstd::vector<vstd::string>*> strs;
	strs.reserve(packPathes->size());
	for (auto&& i : (*packPathes)) {
		strs.push_back(&i.second);
	}
	auto sortPathJob = tPool->GetParallelTask<true>(
		[&](size_t i) {
			QuicksortStackCustomCompare<vstd::string>(
				strs[i]->data(),
				[](vstd::string const& a, vstd::string const& b) {
					auto sz = std::min<size_t>(a.size(), b.size());

					for (auto i : vstd::range(sz)) {
						if (a[i] > b[i])
							return 1;
						else if (a[i] < b[i])
							return -1;
					}
					if (a.size() > b.size()) return 1;
					else if (a.size() == b.size())
						return 0;
					return -1;
				},
				0, strs[i]->size() - 1);
		},
		strs.size(), 1);
	sortPathJob.Complete();
}
vstd::optional<ThreadTaskHandle<true>> task;
VENGINE_UNITY_EXTERN void Py_ExecuteFileSys() {

	task = tPool->GetTask<true>([]() { ExecuteFileSys(); });
	task->Execute();
}
vstd::vector<vstd::string>* vecPtr = nullptr;

VENGINE_UNITY_EXTERN void Py_SetPackageName(char const* name) {
	vstd::string lowName = name;
	StringUtil::ToLower(lowName);
	task->Complete();
	auto ite = packPathes->Find(lowName);
	vecPtr = (ite) ? &ite.Value() : nullptr;
}

VENGINE_UNITY_EXTERN uint Py_PathSize() {
	return vecPtr ? vecPtr->size() : 0;
}
VENGINE_UNITY_EXTERN uint Py_HeaderSize() {
	return headerPaths.size();
}
VENGINE_UNITY_EXTERN char const* Py_GetHeaderPath(uint v) {
	return headerPaths[v].c_str();
}
VENGINE_UNITY_EXTERN char const* Py_GetPath(uint v) {
	return vecPtr ? (*vecPtr)[v].c_str() : nullptr;
}

VENGINE_UNITY_EXTERN void Py_DisposeFileSys() {
	tPool.Delete();
	printf("DLL Dispose Success!\n");
}
}// namespace vepy
#endif