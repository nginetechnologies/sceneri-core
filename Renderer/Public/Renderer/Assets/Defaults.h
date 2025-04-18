#pragma once

#include <Common/Asset/Guid.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>

namespace ngine::Rendering::Constants
{
	inline static constexpr Asset::Guid DefaultMaterialInstanceAssetGuid = "{7962add3-c63a-6075-42d3-517305ba3b0b}"_asset;
	inline static constexpr Asset::Guid DefaultMeshAssetGuid = ngine::Primitives::SphereMeshAssetGuid;
	inline static constexpr Asset::Guid DefaultMeshSceneAssetGuid = ngine::Primitives::SphereMeshSceneAssetGuid;

	inline static constexpr Asset::Guid DefaultCubemapIrradianceAssetGuid = "04884085-d1a3-4f51-a699-28314554510e"_asset;
	inline static constexpr Asset::Guid DefaultCubemapPrefilteredAssetGuid = "82f0d28c-a03b-49d4-a269-cdabe3769b38"_asset;
}
