#include "Assets/Texture/RenderTargetCache.h"
#include "Assets/Texture/TextureCache.h"

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering
{
	TextureIdentifier RenderTargetCache::FindOrRegisterRenderTargetFromTemplateIdentifier(
		TextureCache& textureCache, const RenderTargetTemplateIdentifier templateIdentifier
	)
	{
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			auto it = m_identifierLookupMap.Find(templateIdentifier);
			if (it != m_identifierLookupMap.end())
			{
				return it->second;
			}
		}

		Threading::UniqueLock lock(m_identifierLookupMapMutex);
		auto it = m_identifierLookupMap.Find(templateIdentifier);
		if (it != m_identifierLookupMap.end())
		{
			return it->second;
		}

		const TextureIdentifier identifier = textureCache.RegisterProceduralRenderTargetAsset();
		m_identifierLookupMap.Emplace(RenderTargetTemplateIdentifier(templateIdentifier), TextureIdentifier(identifier));
		return identifier;
	}

	TextureIdentifier RenderTargetCache::FindRenderTargetFromTemplateIdentifier(const RenderTargetTemplateIdentifier templateIdentifier)
	{
		Threading::SharedLock lock(m_identifierLookupMapMutex);
		auto it = m_identifierLookupMap.Find(templateIdentifier);
		if (it != m_identifierLookupMap.end())
		{
			return it->second;
		}
		return {};
	}
}
