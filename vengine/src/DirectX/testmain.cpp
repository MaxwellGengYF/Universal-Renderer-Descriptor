
#define COMPILER
#ifdef COMPILER
#include <DirectX/compiler.inl>
#else
#include <DirectX/DllManager.h>
#include <DirectX/ShadowMaskBaker/GeoBuffer.h>
#include <DirectX/Runtime/Device.h>
#include <DirectX/Runtime/CommandQueue.h>
#include <DirectX/Runtime/CommandBuffer.h>
#include <DirectX/Runtime/CommandAllocator.h>
int main() {
	using namespace toolhub::directx;
	Device device;
	auto dbBase = DllManager::GetDatabase();
	auto gra = DllManager::GetGraphics();
	auto stbi_write_jpg = DllManager::GetGraphicsDll()->GetDLLFunc<int(char const*, int, int, int, const void*, int)>("stbi_write_jpg");
	GeoBuffer geo(&device);
	float3 positions[] = {
		float3(0, 0, 1),
		float3(0, 0.5f, 1),
		float3(1, 1, 1)};
	float2 uvs[] = {
		float2(0.5f, 0),
		float2(0, 1),
		float2(1, 1)};
	uint idx[] = {0, 1, 2};
	geo.InputData(
		positions,
		positions,
		nullptr,
		nullptr,
		uvs,
		3,
		idx,
		vstd::array_count(idx));
	CommandQueue queue(&device, nullptr, D3D12_COMMAND_LIST_TYPE_COMPUTE);
	auto alloc = queue.CreateAllocator();
	DefaultBuffer outputBuffer(&device, 1024 * 1024 * sizeof(float3));
	vstd::vector<float> result(1024 * 1024 * 3);
	auto dispatchCmd = alloc->GetBuffer();
	{
		ResourceStateTracker stateTracker;
		auto bf = dispatchCmd->Build();
		geo.Compute(
			outputBuffer,
			result.data(),
			bf,
			stateTracker);
	}
	queue.Complete(queue.Execute(std::move(alloc)));
	vstd::vector<vbyte> byteResult;
	byteResult.push_back_func(
		result.size(),
		[&](size_t i) {
			return (vbyte)clamp(result[i] * 255, 0, 255.99);
		});
	stbi_write_jpg("hdr_result.jpg", 1024, 1024, 3, byteResult.data(), 100);

	return 0;
}
#endif