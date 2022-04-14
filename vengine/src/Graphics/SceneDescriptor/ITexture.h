#pragma once
#include <Graphics/SceneDescriptor/IResource.h>
namespace toolhub::graphics {
class ITexture : public IResource {
public:
	enum class PixelFormat : vbyte {
		UInt32,
		SInt32,
		Float32,
		UInt16,
		SInt16,
		Float16,
		UNorm16,
		SNorm16,
		UInt8,
		SInt8,
		Float8,
		UNorm8,
		SNorm8
	};
	enum class CompressedFormat : vbyte {
		BC1,
		BC5,
		BC6H,
		BC7
		//TODO: ASTC
	};
	struct UnCompressedFormat {
		PixelFormat format;
		vbyte channelCount;
	};
	struct TextureData {
		uint3 size;
		vstd::variant<UnCompressedFormat, CompressedFormat> format;
		uint mipCount;
	};
	virtual TextureData GetConfigData() const = 0;
	virtual void SetConfigData(TextureData const& data) = 0;
	virtual vstd::span<vbyte const> GetData() const = 0;
	virtual void SetData(vstd::span<vbyte const> data) = 0;
	virtual size_t GetPixelSize() const = 0;
};
}// namespace toolhub::graphics