#include <ir/serialize/ser_context.h>
#include <Utility/MathLib.h>
namespace toolhub::ir {
uint64 SerContext::AddSerObj(void const* ser, SerFunc func) {
	auto ite = map.Find(ser);
	if (ite) return ite.Value().index;
	auto idx = index++;
	ite = map.Emplace(ser, PlaceHolder{idx, 0});
	auto&& frontVec = vec[swapChain];
	frontVec.emplace_back(Command{ser, func, ite});
	return idx;
}
void SerContext::Clear() {
	index = 0;
	map.Clear();
	serResult.clear();
	for (auto&& i : vec)
		i.clear();
}
bool SerContext::UpdateCmd() {
	swapChain = !swapChain;
	auto&& backVec = vec[!swapChain];
	if (backVec.empty()) return false;
	for (auto&& i : backVec) {
		i.idx.Value().offset = serResult.size();
		i.SerFunc(i.ptr, *this);
	}
	return true;
}
void SerContext::ProcessSerialize() {
	while (UpdateCmd()) {}
	auto size = MathLib::CalcAlign(serResult.size(), alignof(uint64));
	*reinterpret_cast<uint64*>(serResult.data()) = size;
	serResult.resize(size + map.Size() * sizeof(uint64));
	uint64* ptr = reinterpret_cast<uint64*>(serResult.data() + size);
	for (auto&& i : map) {
		ptr[i.second.index] = i.second.offset;
	}
}
SerContext::~SerContext() {
}
}// namespace toolhub::ir