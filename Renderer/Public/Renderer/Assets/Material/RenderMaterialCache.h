#pragma once

#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/UniquePtr.h>

#include <Renderer/Assets/Material/MaterialIdentifier.h>

namespace ngine::Rendering
{
	struct RenderMaterial;
	struct LogicalDevice;
	struct LogicalDevice;
	struct MaterialCache;
	struct DescriptorSetLayoutView;

	struct RenderMaterialCache
	{
		~RenderMaterialCache();

		void Destroy(LogicalDevice& logicalDevice, MaterialCache& materialCache);

		[[nodiscard]] RenderMaterial& FindOrLoad(MaterialCache& materialCache, const MaterialIdentifier);
	protected:
		TIdentifierArray<UniquePtr<RenderMaterial>, MaterialIdentifier> m_materials{Memory::Zeroed};
	};
}
