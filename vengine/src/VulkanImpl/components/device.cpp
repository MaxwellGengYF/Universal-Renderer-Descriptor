#include <components/device.h>
namespace toolhub::vk {
//TODO
Device::Device() {}
Device::~Device() {}
void PipelineCachePrefixHeader::Init(Device const* device) {
	magic = PSO_MAGIC_NUM;
	vendorID = device->deviceProperties.vendorID;
	deviceID = device->deviceProperties.deviceID;
	driverVersion = device->deviceProperties.driverVersion;
	memcpy(uuid, device->deviceProperties.pipelineCacheUUID, VK_UUID_SIZE);
}
bool PipelineCachePrefixHeader::operator==(PipelineCachePrefixHeader const& v) const {
	return memcmp(this, &v, sizeof(PipelineCachePrefixHeader)) == 0;
}
}// namespace toolhub::vk