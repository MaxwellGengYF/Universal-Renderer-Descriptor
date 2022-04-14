#pragma once
#include <Common/Common.h>
#include <Utility/VGuid.h>
#include <Common/VObject.h>
namespace vstd::serde {
class ISerializeDatabase {
public:
	DECLARE_VENGINE_OVERRIDE_OPERATOR_NEW
	virtual void WriteData(Guid const& key, vstd::span<vbyte const> data) = 0;
	virtual vstd::span<vbyte const> ReadData(Guid const& key) = 0;
	virtual void Save() = 0;
	virtual ~ISerializeDatabase() {}
};

ISerializeDatabase* GetSimpleFileStreamer();

}// namespace vstd::serde