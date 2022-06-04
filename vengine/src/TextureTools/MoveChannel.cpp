#include <Common/Common.h>
#include <TextureTools/stb_data.h>
#include <stb/stb_image_write.h>
#include <glm/Include.h>
#include <Common/functional.h>
using namespace glm;
namespace toolhub {

void MoveChannel(vstd::span<vstd::string_view const> cmds) {
	if (cmds.size() < 3) return;
	int xCount, yCount, channel;
	StbData input = stbi_load(cmds[0].data(), &xCount, &yCount, &channel, 4);
	if (cmds[1].size() != 1) return;
	uint pixelOffset = 0;
	auto GetOffset = [](char cmd) {
		switch (cmd) {
			case 'r':
			case 'x':
				return 0;
			case 'g':
			case 'y':
				return 1;
			case 'b':
			case 'z':
				return 2;
			case 'a':
			case 'w':
				return 3;
		}
		return 0;
	};
	vstd::vector<char> result(xCount * yCount * 4);
	memset(result.data(), 255, result.byte_size());
	auto inputOffset = GetOffset(cmds[1][0]);
	for (auto c : cmds[2]) {
		auto outputOffset = GetOffset(c);
		for (auto i : vstd::range(xCount * yCount)) {
			result[i * 4 + outputOffset] = input.ptr[i * 4 + inputOffset];
		}
	}
    auto dstPath = vstd::string(cmds[0]) + "_dst";
    stbi_write_png(dstPath.c_str(), xCount, yCount, 4, result.data(), 0);
}
}// namespace toolhub