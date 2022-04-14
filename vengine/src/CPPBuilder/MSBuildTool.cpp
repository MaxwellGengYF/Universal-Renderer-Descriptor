vengine_cppbuilder

#include <VEngineConfig.h>
#include <fstream>
#include <ctime>
#include <Utility/BinaryReader.h>
#include <Database/IJsonObject.h>
#include <Database/DatabaseInclude.h>
#include <JobSystem/ThreadPool.h>
#include <Common/DynamicDLL.h>
using namespace toolhub::db;
VENGINE_UNITY_EXTERN bool compile_msbuild(IJsonDatabase* db) {
	vstd::string postCall = ".vcxproj\" -property:Configuration=";
	auto rt = db->GetRootNode();
	auto configure = rt->Get("Configuration").template try_get<vstd::string_view>();
	if (!configure)
		return false;
	auto platform = rt->Get("Platform").template try_get<vstd::string_view>();
	if (!platform)
		return false;
	vstd::vector<vbyte> buildToolFolder;
	{
		BinaryReader rd("MSBuildPath.txt");
		if (!rd)
			return false;
		buildToolFolder = rd.Read();
	}
	auto buildToolSubDir = rt->Get("BuildToolSubDir").template try_get<vstd::string_view>();
	if (!buildToolSubDir)
		return false;
	auto projDict = rt->Get("Projects").template try_get<IJsonDict*>();
	if (!projDict)
		return false;
	postCall << *configure
			 << ",Platform="
			 << *platform
			 << '\n';
	vstd::string preCall = "\"";
	preCall << vstd::string_view(reinterpret_cast<char const*>(buildToolFolder.data()), buildToolFolder.size())
			<< *buildToolSubDir
			<< "\" \"";
	ThreadPool tPool(std::thread::hardware_concurrency());
	std::atomic_uint jsonValue = 0;
	auto compileFunc = [&](char const* name) {
		uint v = jsonValue++;
		vstd::string fileName = "__temp_TEMP_";
		fileName += vstd::to_string(v);
		fileName += ".cmd";
		{
			std::ofstream ofs(fileName.c_str());
			vstd::string data = preCall;
			data << name
				 << postCall;
			ofs.write(data.data(), data.size());
		}
		system(fileName.c_str());
		remove(fileName.c_str());
	};
	vstd::HashMap<vstd::string_view, ThreadTaskHandle<false>> projDepends;
	auto finalJob = tPool.GetFence<true>();
	uint64* ptr = reinterpret_cast<uint64*>(*projDict);
	for (auto kv : **projDict) {
		vstd::string_view* k = kv.key.template try_get<vstd::string_view>();
		if (!k) {
			return false;
		}
		finalJob.AddDepend(projDepends.Emplace(
										  *k,
										  tPool.GetTask<false>(
											  [&, key = *k]() {
												  compileFunc(key.data());
											  }))
							   .Value());
	}
	for (auto kv : **projDict) {
		auto&& job = projDepends.Find(kv.key.force_get<vstd::string_view>()).Value();
		IJsonArray** arr = kv.value.template try_get<IJsonArray*>();
		if (!arr) continue;
		for (auto value : **arr) {
			vstd::string_view* depName = value.template try_get<vstd::string_view>();
			auto ite = projDepends.Find(*depName);
			if (ite) {
				job.AddDepend(ite.Value());
			}
		}
	}

	auto start = clock();
	finalJob.Complete();
	auto end = clock();
	double endtime = (double)(end - start) / CLOCKS_PER_SEC;
	std::cout << "Compile Total time: " << endtime << " second\n";
}