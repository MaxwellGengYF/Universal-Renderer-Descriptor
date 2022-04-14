
#include <Graphics/SceneDescriptor/ITexture.h>
namespace toolhub::graphics {
class Texture final : public ITexture {
public:
	TextureData data;
	vstd::span<vbyte const> bin;
	TextureData GetConfigData() const override {
		return data;
	}
	void SetConfigData(TextureData const& data) override {
		this->data = data;
	}
	vstd::span<vbyte const> GetData() const override {
		return bin;
	}
	void SetData(vstd::span<vbyte const> data) override {
		bin = data;
	}
	size_t GetPixelSize() const override {
		auto GetUncompFormat = [&](uint3 size, UnCompressedFormat format) {
			size_t result = [&] {
				switch (format.format) {
					case PixelFormat::UInt32:
					case PixelFormat::SInt32:
					case PixelFormat::Float32:
						return 4;
					case PixelFormat::UInt16:
					case PixelFormat::SInt16:
					case PixelFormat::UNorm16:
					case PixelFormat::SNorm16:
					case PixelFormat::Float16:
						return 2;
					case PixelFormat::UInt8:
					case PixelFormat::SInt8:
					case PixelFormat::UNorm8:
					case PixelFormat::SNorm8:
					case PixelFormat::Float8:
						return 1;
					default:
						return 0;
				}
			}() * format.channelCount;
			return result * size.x * size.y * size.z;
		};
		auto GetCompFormat = [&](uint3 size, CompressedFormat format) {
			size_t chunkSize = [&] {
				switch (format) {
				case CompressedFormat::BC1:
						return 1;
				case CompressedFormat::BC5:
				case CompressedFormat::BC6H:
				case CompressedFormat::BC7:
					return 2;
			} }();
			return chunkSize * (size.x / 4) * (size.y / 4);
		};
		uint3 sz = data.size;
		for (auto i : vstd::range(data.mipCount)) {
			auto d = vstd::create_disposer([&] {
				sz.x /= 2;
				sz.y /= 2;
				sz.z /= 2;
			});
			return data.format.multi_visit_or(
				size_t(0),
				[&](auto&& f) {
					return GetUncompFormat(sz, f);
				},
				[&](auto&& f) {
					return GetCompFormat(sz, f);
				});
		}
	}
	Tag GetTag() const override {
		return Tag::Texture;
	}
	VSTD_SELF_PTR
};
}// namespace toolhub::graphics