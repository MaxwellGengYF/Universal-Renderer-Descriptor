#pragma once
#include <Graphics/FilePacker.h>
#include <Common/DXMath/DXMath.h>
namespace toolhub::graphics {

class IResource : public vstd::ISelfPtr {
protected:
	vstd::optional<vstd::Guid> guid;
	IResource() {}
	void SaveBase(db::IJsonDict* dict) const;
	void SetBase(db::IJsonDict const* dict);
	void CreateGuid();

public:
	virtual ~IResource() = default;
	bool Loaded() const { return guid; }
	vstd::Guid GetGuid() const {
		if (guid) return *guid;
		return vstd::Guid(false);
	}
	enum class Tag : vbyte {
		Undefined,
		Texture,
		Mesh,
		ComputeShader,
		RTShader
	};
	virtual Tag GetTag() const = 0;
	virtual void Save(vstd::vector<vbyte>& data, db::IJsonDatabase* db) = 0;
};
}// namespace toolhub::graphics