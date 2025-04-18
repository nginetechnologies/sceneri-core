#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Widgets
{
	enum class OpenDocumentFlags : uint8
	{
		EnableEditing = 1 << 0,
		// Clones the specified document into a new one
		Clone = 1 << 1,
		// Loads the specified remote document into a local one, retaining the same unique identifiers
		CopyToLocal = 1 << 2,
		//! Indicates that we should fit the document asset to view after assets have been loaded
		//! This is used to ensure the asset is visible and fills the screen
		FitToView = 1 << 3,
		//! Indicates whether this document is being loaded for thumbnail generation
		//! Used to determine whether to remove the background
		IsThumbnailGeneration = 1 << 4,
		SkipResolveMissingAssets = 1 << 5
	};
	ENUM_FLAG_OPERATORS(OpenDocumentFlags);
}
