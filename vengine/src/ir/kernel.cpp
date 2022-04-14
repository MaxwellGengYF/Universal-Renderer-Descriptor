
#include <ir/kernel.h>
namespace luisa::ir {
namespace detail {
class KernelAllocator : public vstd::IOperatorNewBase {
public:
	vstd::HashMap<vstd::string, Type> types;
	KernelAllocator() {
		auto&& boolType = types.Emplace("bool").Value();
		boolType.tag = Type::Tag::Bool;
		boolType.size = 1;
		boolType.alignment = 1;
		boolType.dimension = 1;
		auto&& floatType = types.Emplace("float").Value();
		floatType.tag = Type::Tag::Float;
		floatType.size = 4;
		floatType.alignment = 4;
		floatType.dimension = 1;
		auto&& intType = types.Emplace("int").Value();
		intType.tag = Type::Tag::Int;
		intType.size = 4;
		intType.alignment = 4;
		intType.dimension = 1;
		auto&& uintType = types.Emplace("uint").Value();
		uintType.tag = Type::Tag::UInt;
		uintType.size = 4;
		uintType.alignment = 4;
		uintType.dimension = 1;
		Type* allTypes[] = {
			&boolType,
			&floatType,
			&uintType,
			&intType};
		char const* typeNames[] = {
			"bool",
			"float",
			"uint",
			"int"};
		int32 alignment[] = {0, 1, 2, 4, 4};
		for (auto ele : vstd::range(4))
			for (auto i : vstd::range(2, 5)) {
				vstd::string vecName;
				vecName << typeNames[ele] << vstd::to_string(i);
				auto&& vec = types.Emplace(std::move(vecName)).Value();
				auto eleType = allTypes[ele];
				vec.alignment = eleType->alignment * alignment[i];
				vec.size = eleType->size * alignment[i];
				vec.dimension = i;
				vec.element = eleType;
				vec.tag = Type::Tag::Vector;
			}
		for (auto i : vstd::range(2, 5)) {
			vstd::string vecName("float"_sv);
			auto num = vstd::to_string(i);
			vecName << num << 'x' << num;
			auto&& mat = types.Emplace(std::move(vecName)).Value();
			mat.alignment = alignment[i] * 4;
			mat.size = alignment[i] * 4;
			mat.dimension = i;
			mat.element = &floatType;
			mat.tag = Type::Tag::Matrix;
		}
	}
};
static vstd::unique_ptr<KernelAllocator> alloc;
static std::mutex allocatorMtx;
static void InitKernelAllocator() {
	std::lock_guard lck(allocatorMtx);
	if (!alloc)
		alloc = vstd::create_unique(new KernelAllocator());
}
}// namespace detail
Type const* Kernel::GetBuiltinType(vstd::HashCache<vstd::string_view> const& description) {
	auto ite = detail::alloc->types.Find(description);
	if (!ite) return nullptr;
	return &ite.Value();
}

Kernel::Kernel()
	: allocator(65536) {
	detail::InitKernelAllocator();
}
Kernel::~Kernel() {
	detail::InitKernelAllocator();
}
}// namespace luisa::ir