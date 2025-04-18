#include <Renderer/Assets/Material/RenderMaterialCache.h>
#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/MaterialCache.h>

#include <Renderer/Devices/LogicalDeviceView.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Rendering
{
	RenderMaterialCache::~RenderMaterialCache() = default;

	void RenderMaterialCache::Destroy(LogicalDevice& logicalDevice, MaterialCache& materialCache)
	{
		for (UniquePtr<RenderMaterial>& pMaterial : materialCache.GetValidElementView(m_materials.GetView()))
		{
			if (pMaterial.IsValid())
			{
				pMaterial->Destroy(logicalDevice);
			}
		}
	}

	RenderMaterial& RenderMaterialCache::FindOrLoad(MaterialCache& materialCache, const MaterialIdentifier materialIdentifier)
	{
		UniquePtr<RenderMaterial>& renderMaterial = m_materials[materialIdentifier];
		if (renderMaterial.IsValid())
		{
			return *renderMaterial;
		}

		renderMaterial.CreateInPlace(*this, *materialCache.GetMaterial(materialIdentifier));

		return *renderMaterial;
	}
}
