#pragma once
#include <Common/Common.h>
#include <Utility/BinaryReader.h>
struct ProfileResult {
	vstd::string name;
	uint64 frameIndex;
	uint64 nanoSecondCount;
	template<typename A, typename B, typename C>
	ProfileResult(
		A&& a,
		B&& b,
		C&& c)
		: name(std::forward<A>(a)),
		  frameIndex(std::forward<B>(b)),
		  nanoSecondCount(std::forward<C>(c)) {}
};
class Profiler {
public:
	using TimeClock = std::chrono::time_point<std::chrono::high_resolution_clock>;
	uint64 curFrame = 0;
	vstd::HashMap<vstd::string, std::pair<TimeClock, TimeClock>> records;
	vstd::vector<ProfileResult> results;
	void UpdateFrame(uint64 curFrame) {
		this->curFrame = curFrame;
	}
	void BeginSample(wchar_t const* name, size_t nameCount) {
		vstd::string str;
		str.resize(nameCount);
		for (auto i : vstd::range(nameCount)) {
			str[i] = name[i];
		}
		auto ite = records.Emplace(std::move(str));
		ite.Value().first = std::chrono::high_resolution_clock::now();
	}
	void EndSample(wchar_t const* name, size_t nameCount) {
		auto now = std::chrono::high_resolution_clock::now();
		
		vstd::string str;
		str.resize(nameCount);
		for (auto i : vstd::range(nameCount)) {
			str[i] = name[i];
		}
		auto ite = records.Find(str);
		if (ite)
			ite.Value().second = now;
	}
	void FinishProfile() {
		for (auto&& i : records) {
			results.emplace_back(
				std::move(i.first),
				curFrame,
				size_t((i.second.second - i.second.first).count()));
		}
		records.Clear();
	}
	void Print(wchar_t const* name, size_t nameCount) {
		vstd::string str;
		str.resize(nameCount);
		for (auto i : vstd::range(nameCount)) {
			str[i] = name[i];
		}
		vstd::string r;
		struct Range {
			vstd::string& r;
			Range(vstd::string& r) : r(r) {
				r << "{\n";
			}
			~Range() { r << "}\n"; }
		};
		std::sort(results.begin(), results.end(), [](auto&& a, auto&& b) {
			if (a.frameIndex < b.frameIndex) return true;
			else if (a.frameIndex > b.frameIndex)
				return false;
			else return a.nanoSecondCount > b.nanoSecondCount;
		});
		{
			Range out(r);
			size_t sz = 0;
			for (auto&& i : results) {
				r << '"'
				  << "Frame_"_sv
				  << vstd::to_string(i.frameIndex)
				  <<'_'
				  << i.name << "\": " << vstd::to_string(i.nanoSecondCount);
				if (sz != results.size() - 1) {
					r << ",\n"_sv;
				} else {
					r << '\n';
				}
				sz++;
			}
		}
		auto f = fopen(str.data(), "w");
		if (f) {
			auto d = vstd::create_disposer([&] { fclose(f); });
			fwrite(r.data(), r.size(), 1, f);
		}
	}
};
static Profiler pro;
VENGINE_UNITY_EXTERN void UpdateFrame(uint64 f) {
	pro.UpdateFrame(f);
}
VENGINE_UNITY_EXTERN void BeginSample(wchar_t const* name, size_t nameCount) {
	pro.BeginSample(name, nameCount);
}
VENGINE_UNITY_EXTERN void EndSample(wchar_t const* name, size_t nameCount) {
	pro.EndSample(name, nameCount);
}
VENGINE_UNITY_EXTERN void FinishProfile() {
	pro.FinishProfile();
}
VENGINE_UNITY_EXTERN void Print(wchar_t const* name, size_t nameCount) {
	pro.Print(name, nameCount);
}