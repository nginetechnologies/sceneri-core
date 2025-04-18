#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Guid.h>
#include <Common/Reflection/Extension.h>

namespace ngine::Asset
{
	struct EditorTypeExtension final : public Reflection::ExtensionInterface
	{
		inline static constexpr Guid TypeGuid = "594eeafd-202d-4d9c-b24e-e02fbd147815"_guid;

		constexpr EditorTypeExtension(
			const Guid documentTypeGuid, const Guid documentAssetGuid, const Guid documentContextMenuGuid, const Guid documentContextMenuEntryGuid
		)
			: ExtensionInterface{TypeGuid}
			, m_documentTypeGuid(documentTypeGuid)
			, m_documentAssetGuid(documentAssetGuid)
			, m_documentContextMenuGuid(documentContextMenuGuid)
			, m_documentContextMenuEntryGuid(documentContextMenuEntryGuid)
		{
		}

		//! Guid of the widget document that can preview / edit this asset type
		Guid m_documentTypeGuid;
		//! Guids of the asset containing a document that can preview / edit this asset type
		Guid m_documentAssetGuid;
		Guid m_documentContextMenuGuid;
		Guid m_documentContextMenuEntryGuid;
	};
}
