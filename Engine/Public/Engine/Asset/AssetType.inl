#pragma once

#include "AssetType.h"

#include <Engine/Asset/AssetManager.h>

namespace ngine::Asset
{
	template<typename IdentifierType, typename InstanceDataType>
	inline void Type<IdentifierType, InstanceDataType>::RegisterAssetModifiedCallback([[maybe_unused]] Manager& manager)
	{
#if TRACK_ASSET_FILE_CHANGES
		manager.RegisterAssetModifiedCallback(
			[this](const Guid assetGuid, const IO::PathView filePath)
			{
				const IdentifierType identifier = FindIdentifier(assetGuid);
				if (identifier.IsValid())
				{
					if (m_reloadingAssets.Set(identifier))
					{
						OnAssetModified(assetGuid, identifier, filePath);
					}
				}
			}
		);
#endif
	}

	template<typename IdentifierType, typename InstanceDataType>
	inline Type<IdentifierType, InstanceDataType>::~Type()
	{
		for (const typename decltype(m_identifierLookupMap)::PairType& pair : m_identifierLookupMap)
		{
			m_storedData.Destroy(pair.second);
		}
	}
}
