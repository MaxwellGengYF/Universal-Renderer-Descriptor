#include <types/buffer_view.h>
#include <types/tex_view.h>
#include <components/gpu_collection.h>
namespace toolhub::vk {
class Buffer;
class CommandBuffer;
class Texture;
enum class BufferReadState : vbyte {
	ComputeOrCopy,
	IndirectArgs,
	Accel,
};
enum class BufferWriteState : vbyte {
	Compute,
	Copy,
	Accel,
};
enum class TextureWriteState : vbyte {
	Compute,
	Copy
};
enum class ResourceUsage : vbyte {
	ComputeOrCopy,
	AccelBuild
};
class ResStateTracker : public Resource {
	struct Range {
		size_t min;
		size_t max;
		VkAccessFlagBits accessFlag;
		VkPipelineStageFlagBits stage;
		Range() {}
		Range(size_t value, VkAccessFlagBits accessFlag, VkPipelineStageFlagBits stage)
			: accessFlag(accessFlag), stage(stage) {
			min = value;
			max = value + 1;
		}
		Range(size_t min, size_t size, VkAccessFlagBits accessFlag, VkPipelineStageFlagBits stage)
			: min(min), max(size + min),
			  accessFlag(accessFlag), stage(stage) {}
		bool collide(Range const& r) const {
			return min < r.max && r.min < max;
		}
	};
	struct Barriers {
		vstd::small_vector<VkBufferMemoryBarrier> bufferBarriers;
		vstd::small_vector<VkImageMemoryBarrier> imageBarriers;
	};
	using StagePair = std::pair<VkPipelineStageFlagBits, VkPipelineStageFlagBits>;
	vstd::HashMap<StagePair, Barriers> collectedBarriers;
	vstd::HashMap<GPUCollection const*, vstd::small_vector<Range>> readMap;
	vstd::HashMap<GPUCollection const*, vstd::small_vector<Range>> writeMap;
	vstd::HashMap<std::pair<Texture const*, uint>> texWriteMap;

	void MarkRange(
		BufferView const& bufferView,
		vstd::small_vector<Range>& vec, VkAccessFlagBits dstFlag, VkPipelineStageFlagBits dstStage);
	void MarkTexture(
		Texture const* tex,
		uint mipLevel,
		VkAccessFlagBits dstFlag);
	// void MarkRange(
	// 	TexView const& bufferView,
	// 	vstd::small_vector<Range>& vec, VkAccessFlagBits dstFlag, VkPipelineStageFlagBits dstStage);

public:
	void Reset();
	void MarkBufferWrite(BufferView const& bufferView, BufferWriteState type, ResourceUsage usage);
	void MarkBufferRead(BufferView const& bufferView, BufferReadState type, ResourceUsage usage);
	void MarkTextureWrite(Texture const* tex, uint targetMip, TextureWriteState type);
	void MarkTextureRead(TexView const& tex);

	void Execute(CommandBuffer* cb);
	ResStateTracker(Device const* device);
	~ResStateTracker();

	//TODO
	//void MarkBindlessRead(BindlessArray const& arr);
};
}// namespace toolhub::vk