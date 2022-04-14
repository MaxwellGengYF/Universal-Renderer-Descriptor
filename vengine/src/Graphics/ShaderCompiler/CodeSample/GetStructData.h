#pragma once
#include <Common/Common.h>
class GetStructData final
{
	vstd::HashMap<vstd::string_view, size_t> structSizes;
	vstd::string const& str;
	size_t maxSize = 0;
public:
	size_t GetSize(vstd::string const& str) const;
	size_t GetMaxSize() const
	{
		return maxSize;
	}
	GetStructData(vstd::string const& str);
	bool Parse();
	~GetStructData();
};