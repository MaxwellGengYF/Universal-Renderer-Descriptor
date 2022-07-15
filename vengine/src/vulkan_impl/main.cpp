#include <components/device.h>
#include <Utility/BinaryReader.h>
#include <shader/compute_shader.h>
#include <shader/shader_code.h>
#include <gpu_collection/buffer.h>
#include <shader/descriptor_pool.h>
#include <shader/descriptorset_manager.h>
#include <runtime/command_pool.h>
#include <runtime/frame_resource.h>
#include <runtime/res_state_tracker.h>
#include <gpu_collection/texture.h>
#include <rtx/mesh.h>
#include <rtx/accel.h>
#include <rtx/query.h>
#include <dxc/dxc_util.h>
using namespace toolhub::vk;
namespace toolhub::vk {
void TestBuffer(Device const* device, vstd::span<vbyte const> block);
void TestBindless(Device const* device, vstd::span<vbyte const> block);
void TestRayQuery(Device const* device, vstd::span<vbyte const> block);
}// namespace toolhub::vk
void CompileAndTest(
	Device const* device,
	vstd::string const& shaderName,
	vstd::function<void(Device const* device, vstd::span<vbyte const> block)> const& func) {
	vstd::string shaderCode;
	{
		BinaryReader reader(shaderName);
		shaderCode = reader.ReadToString();
	}
	toolhub::directx::DXShaderCompiler compiler;
	auto compResult = compiler.CompileCompute(
		shaderCode,
		true);
	auto errMsg = compResult.try_get<vstd::string>();
	if (errMsg) {
		std::cout << "Compile error: " << *errMsg << '\n';
		return;
	}
	auto&& byteCode = compResult.get<0>();
	vstd::span<vbyte const> resultArr(
		byteCode->GetBufferPtr(),
		byteCode->GetBufferSize());
	func(device, resultArr);
}
int main() {
	auto instance = Device::InitVkInstance();
	auto device = Device::CreateDevice(instance, nullptr, 0);
	CompileAndTest(device, "test_rayquery.compute", TestRayQuery);

	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}