#pragma once

#include <Common/Function/Event.h>
#include <Common/Math/Vector2.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>

#include <Renderer/ImageAspectFlags.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/RenderTargetTemplateIdentifier.h>

#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Rendering
{
	struct TextureCache;

	struct RenderTargetCache final
	{
		[[nodiscard]] TextureIdentifier
		FindOrRegisterRenderTargetFromTemplateIdentifier(TextureCache& textureCache, const RenderTargetTemplateIdentifier templateIdentifier);
		[[nodiscard]] TextureIdentifier FindRenderTargetFromTemplateIdentifier(const RenderTargetTemplateIdentifier templateIdentifier);
	protected:
		Threading::SharedMutex m_identifierLookupMapMutex;
		UnorderedMap<RenderTargetTemplateIdentifier, TextureIdentifier, RenderTargetTemplateIdentifier::Hash> m_identifierLookupMap;
	};
}
