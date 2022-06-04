#include <Common/Common.h>
namespace toolhub {
void MoveChannel(vstd::span<vstd::string_view const>);
}// namespace toolhub
int main(int argc, char* argv[]) {
    using namespace toolhub;
	vstd::HashMap<vstd::string_view, vstd::funcPtr_t<void(vstd::span<vstd::string_view const>)>> cmdFuncs;
	cmdFuncs.Emplace("move_channel", MoveChannel);
	auto func = cmdFuncs.Find(argv[1]);
	if (!func) return 1;
	vstd::vector<vstd::string_view> cmds;
	cmds.push_back_func(argc - 2, [&](size_t i) {
		return argv[i + 2];
	});
	func.Value()(cmds);
	return 0;
}