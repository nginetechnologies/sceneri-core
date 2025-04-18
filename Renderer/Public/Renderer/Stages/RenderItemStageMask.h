#pragma once

#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Rendering
{
	struct StageCache;

	struct RenderItemStageMask : public IdentifierMask<SceneRenderStageIdentifier>
	{
		using BaseType = IdentifierMask<SceneRenderStageIdentifier>;
		using BaseType::BaseType;
		using BaseType::operator=;

		bool Serialize(const Serialization::Reader, Rendering::StageCache& stageCache);
		bool Serialize(Serialization::Writer, const Rendering::StageCache& stageCache) const;
	};
}
