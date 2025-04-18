#pragma once

#include "Material.h"
#include "MaterialIdentifier.h"

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Engine/Asset/AssetType.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Physics
{
	struct MaterialCache final : public Asset::Type<MaterialIdentifier, UniquePtr<Material>>
	{
		using BaseType = Type;

		MaterialCache(Asset::Manager& assetManager);
		virtual ~MaterialCache();

		[[nodiscard]] MaterialIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] MaterialIdentifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] Optional<Material*> FindMaterial(const MaterialIdentifier identifier) const;

		[[nodiscard]] Threading::JobBatch TryLoadMaterial(const MaterialIdentifier identifier, Asset::Manager& assetManager);
		[[nodiscard]] const Material& GetDefaultMaterial() const;
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<MaterialIdentifier> m_loadedMaterials;
		MaterialIdentifier m_defaultMaterialIdentifier;
	};
}
