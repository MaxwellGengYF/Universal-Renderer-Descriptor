#pragma once
#include <Common/MetaLib.h>
#include <variant>
namespace vstd {
namespace detail {
template<typename Func, typename PtrType>
constexpr static decltype(auto) NoArgs_FuncTable(GetVoidType_t<Func> *func) {
    using PureFunc = std::remove_cvref_t<Func>;
    PureFunc *realFunc = reinterpret_cast<PureFunc *>(func);
    return (std::forward<Func>(*realFunc)).template operator()<std::remove_reference_t<PtrType>>();
}
template<typename Func, typename... Args>
static decltype(auto) VisitVariant(
    Func &&func,
    size_t idx) {
    constexpr static auto table =
        {
            NoArgs_FuncTable<
                std::remove_reference_t<Func>,
                Args>...};
	return (table.begin()[idx])(&func);
}
}// namespace detail

template<typename T>
class VariantVisitor;

template<typename... Args>
class VariantVisitor<std::variant<Args...>> {
public:
    template<typename Func>
    void operator()(Func&& func, size_t idx) {
        assert(idx < sizeof...(Args));
        detail::VisitVariant<Func, Args...>(std::forward<Func>(func), idx);
    }
};
template<typename... Args>
class VariantVisitor<vstd::variant<Args...>> {
public:
	template<typename Func>
    void operator()(Func &&func, size_t idx) {
        assert(idx < sizeof...(Args));
        detail::VisitVariant<Func, Args...>(std::forward<Func>(func), idx);
    }
};
}// namespace vstd