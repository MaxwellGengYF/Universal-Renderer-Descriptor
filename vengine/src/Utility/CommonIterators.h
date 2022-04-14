#pragma once
//#include <Common/Common.h>
//namespace vstd {
//template<typename T, bool forward>
//struct VectorIEnumerable : public IEnumerable<T> {
//private:
//	using PureT = std::remove_reference_t<T>;
//	PureT* beg;
//	PureT* ed;
//
//public:
//	template<typename VectorType>
//	VectorIEnumerable(VectorType const& vec) {
//		beg = vec.data();
//		ed = vec.data() + vec.size();
//	}
//	VectorIEnumerable(VectorIEnumerable const&) = default;
//	VectorIEnumerable(VectorIEnumerable&&) = default;
//	VectorIEnumerable(VectorIEnumerable& v) : VectorIEnumerable((VectorIEnumerable const&)v) {}
//	VectorIEnumerable(VectorIEnumerable const&& v) : VectorIEnumerable(v) {}
//
//	T GetValue() override {
//		if constexpr (forward)
//			return std::forward<T>(*beg);
//		else
//			return *beg;
//	}
//	bool End() override {
//		return beg == ed;
//	}	
//	void GetNext() override {
//		++beg;
//	}
//	void Dispose() override {
//		delete this;
//	}
//	optional<size_t> Length() override {
//		return ed - beg;
//	}
//};
//template<typename T, VEngine_AllocType allocType, size_t stackCount>
//decltype(auto) GetVectorIEnumerable(vector<T, allocType, stackCount> const& vec) {
//	return VectorIEnumerable<T, false>(vec);
//}
//template<typename T>
//decltype(auto) GetSpanIEnumerable(vstd::span<T> vec) {
//	return VectorIEnumerable<T, false>(vec);
//}
//template<typename T>
//decltype(auto) GetInitListIEnumerable(std::initializer_list<T const> const& lst) {
//	return VectorIEnumerable<T const, false>(lst);
//}
//template<typename T, VEngine_AllocType allocType, size_t stackCount>
//decltype(auto) GetVectorIEnumerable_Move(vector<T, allocType, stackCount> const& vec) {
//	return VectorIEnumerable<T, true>(vec);
//}
//template<typename T>
//decltype(auto) GetSpanIEnumerable_Move(vstd::span<T> vec) {
//	return VectorIEnumerable<T, true>(vec);
//}
//
//}// namespace vstd