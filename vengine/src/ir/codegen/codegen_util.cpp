#include <ir/codegen/codegen_util.h>
namespace toolhub::ir {
namespace detail {
template<typename T>
struct PrintType;
template<>
struct PrintType<float> {
	static void Print(float v, vstd::string& str) {
		vstd::to_string(v, str);
	}
};
template<>
struct PrintType<int32> {
	static void Print(int32 v, vstd::string& str) {
		vstd::to_string(v, str);
	}
};
template<>
struct PrintType<uint> {
	static void Print(uint v, vstd::string& str) {
		vstd::to_string(v, str);
	}
};
template<>
struct PrintType<bool> {
	static void Print(bool v, vstd::string& str) {
		str << (v ? "true"_sv : "false"_sv);
	}
};

template<typename T, size_t i>
struct PrintType<Vector<T, i>> {
	static void Print(Vector<T, i> t, vstd::string& str) {
		for (auto idx : vstd::range(i)) {
			vstd::to_string(t[idx], str);
			if (idx < i - 1) {
				str << ',';
			}
		}
	}
};
template<size_t i>
struct PrintType<Matrix<i>> {
	static void Print(Matrix<i> t, vstd::string& str) {
		size_t idx = 0;
		for (auto c : vstd::range(i))
			for (auto r : vstd::range(i)) {
				vstd::to_string(t.cols[c][r], str);
				if (idx < (i * i - 1)) {
					str << ',';
				}
				++idx;
			}
	}
};
}// namespace detail
void CodegenUtil::PrintConstArray(ConstantVar const& var, vstd::string& str) {
	var.data.visit([&](auto span) {
		using Type = std::remove_cvref_t<decltype(span)>::value_type;
		for (auto&& i : span) {
			detail::PrintType<Type>::Print(i, str);
			str << ',';
		}
		if (!str.empty()) {
			str.erase(str.end() - 1);
		}
	});
}
}// namespace toolhub::ir