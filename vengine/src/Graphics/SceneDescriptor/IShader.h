#pragma once
#include <Common/Common.h>
#include <Graphics/SceneDescriptor/IResource.h>
#include <Graphics/IByteBlob.h>
namespace toolhub::graphics {
class IShader : public IResource {
public:
	enum class PropertyType : vbyte {
		CBuffer,
		Tex1D,
		RWTex1D,
		Tex2D,
		RWTex2D,
		Tex3D,
		RWTex3D,
		CubeMap,
		Buffer,
		RWBuffer,
		Accel
	};
	struct Property {
		vstd::string name;
		uint64 arrSize;
		PropertyType type;
		uint registIdx;
		uint spaceIdx;
	};
	vstd::variant<
		vstd::unique_ptr<IByteBlob>,
		vstd::span<vbyte const>>
		binBytes;
	vstd::span<vbyte const> GetSpan() const {
		return binBytes.multi_visit_or(
			vstd::span<vbyte const>(static_cast<vbyte const*>(nullptr), size_t(0)),
			[&](auto&& a) -> vstd::span<vbyte const> {
				return {a->GetBufferPtr(), a->GetBufferSize()};
			},
			[&](auto&& a) {
				return a;
			});
	}
	vstd::vector<Property> properties;
	uint shaderModel = 10;
	virtual ~IShader() = default;

protected:
	vstd::unique_ptr<db::IJsonDict> SaveProperty(db::IJsonDatabase* db) const;
	void ReadProperty(db::IJsonDict const* arr);
	void ReadBin(vstd::span<vbyte const> bin);
};

}// namespace toolhub::graphics