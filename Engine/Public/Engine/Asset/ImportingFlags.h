#pragma once

namespace ngine::Asset
{
	enum class ImportingFlags : uint8
	{
		//! Saves the imported state to disk
		SaveToDisk = 1 << 0,
		FullHierarchy = 1 << 1
	};

	ENUM_FLAG_OPERATORS(ImportingFlags);
}
