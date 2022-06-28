#include <vulkan_impl/components/device.h>
#include <Utility/BinaryReader.h>
#include <vulkan_impl/shader/compute_shader.h>
#include <vulkan_impl/shader/shader_code.h>
#include <vulkan_impl/gpu_collection/buffer.h>
#include <vulkan_impl/shader/descriptor_pool.h>
#include <vulkan_impl/shader/descriptorset_manager.h>
#include <vulkan_impl/runtime/command_pool.h>
#include <vulkan_impl/runtime/frame_resource.h>
#include <vulkan_impl/runtime/res_state_tracker.h>
#include <vulkan_impl/gpu_collection/texture.h>
#include <vulkan_impl/rtx/mesh.h>
#include <vulkan_impl/rtx/accel.h>
#include <dxc/dxc_util.h>
#include <vulkan_impl/rtx/query.h>
using namespace toolhub::vk;
namespace toolhub::vk {
void TestBuffer(Device const* device, vstd::span<vbyte const> block);
void TestBindless(Device const* device, vstd::span<vbyte const> block);
}// namespace toolhub::vk
void CompileAndTest(
	Device const* device,
	vstd::string const& shaderName,
	vstd::move_only_func<void(Device const* device, vstd::span<vbyte const> block)> const& func) {
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
	CompileAndTest(device, "test_bindless.compute", TestBindless);

	delete device;
	vkDestroyInstance(instance, Device::Allocator());
	std::cout << "finished\n";
	return 0;
}