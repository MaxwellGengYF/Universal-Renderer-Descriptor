#include <Common/Common.h>
#include <Utility/FileUtility.h>
#include <Database/DatabaseInclude.h>
#include <Database/IJsonDatabase.h>
#include <Common/DynamicDLL.h>
#include <Utility/BinaryReader.h>
#include <Utility/MD5.h>
#include <taskflow/taskflow.hpp>
int main(int argc, char** argv) {
	tf::Executor tPool(std::thread::hardware_concurrency());
	vstd::vector<vstd::string> paths;
	auto readFileTask = [&] {
		tf::Taskflow flow;
		flow.emplace(
			[&] {
				for (auto&& i : vstd::range(1, argc)) {
					FileUtility::GetFiles(argv[i], [&](auto&& path) {
						paths.emplace_back(std::move(path));
					});
				}
			});
		return tPool.run(std::move(flow));
	}();
	DllFactoryLoader<toolhub::db::Database const> dll("VEngine_Database.dll", "Database_GetFactory");
	auto db = vstd::create_unique(dll()->CreateDatabase());
	vstd::optional<BinaryReader> reader;
	reader.New("file_record.bin");
	static thread_local vstd::vector<vbyte> binCache;
	auto avaliable = [&] {
		if (*reader) {
			binCache.resize(reader->GetLength());
			reader->Read((char*)binCache.data(), binCache.size());
			return db->GetRootNode()->Read(binCache, false);
		}
		return false;
	}();
	reader.Delete();
	auto newDict = db->CreateDict();
	auto&& root = db->GetRootNode();
	auto&& ReadBin = [&](vstd::string const& path) {
		BinaryReader reader(path);
		binCache.clear();
		binCache.resize(reader.GetLength());
		reader.Read((char*)binCache.data(), binCache.size());
	};

	vstd::spin_mutex mtx;
	auto ProcessPath = [&]<bool avaliable>(vstd::string const& path) {
		ReadBin(path);
		vstd::Guid md5 = vstd::MD5(binCache);
		auto guidOpt = root->Get(path).try_get<vstd::Guid>();
		if constexpr (avaliable) {
			if (!guidOpt || (*guidOpt != md5)) {
				auto f = fopen(path.c_str(), "wb");
				if (f) {
					fwrite(binCache.data(), binCache.size(), 1, f);
					fclose(f);
				}
			}
		} else {
			auto f = fopen(path.c_str(), "wb");
			if (f) {
				fwrite(binCache.data(), binCache.size(), 1, f);
				fclose(f);
			}
		}
		{
			std::lock_guard lck(mtx);
			newDict->Set(std::move(path), md5);
		}
	};
	readFileTask.wait();
	tf::Taskflow flow;
	auto emplaceFun = [&]<bool ava>() {
		flow.emplace_all(
			[&](size_t i) {
				auto&& path = paths[i];
				ProcessPath.operator()<ava>(path);
			},
			paths.size(),
			std::min<size_t>(paths.size(), std::thread::hardware_concurrency()));
	};
	if (avaliable) {
		emplaceFun.operator()<true>();
	} else {
		emplaceFun.operator()<false>();
	}
	tPool.run(std::move(flow)).wait();
	binCache.clear();
	newDict->Serialize(binCache);
	auto f = fopen("file_record.bin", "wb");
	if (f) {
		fwrite(binCache.data(), binCache.size(), 1, f);
		fclose(f);
	}
	return 0;
}