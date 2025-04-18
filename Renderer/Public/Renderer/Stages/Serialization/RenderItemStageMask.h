#pragma once

#include "../RenderItemStageMask.h"
#include <Common/Asset/Guid.h>
#include <Common/Serialization/Guid.h>
#include <Renderer/Assets/Stage/StageCache.h>

namespace ngine::Rendering
{
	inline bool RenderItemStageMask::Serialize(const Serialization::Reader serializer, Rendering::StageCache& stageCache)
	{
		for (const Serialization::Reader stageGuidSerializer : serializer.GetArrayView())
		{
			Asset::Guid stageGuid;
			stageGuidSerializer.SerializeInPlace(stageGuid);

			const Rendering::SceneRenderStageIdentifier stageIdentifier = stageCache.FindOrRegisterAsset(stageGuid);
			Assert(stageIdentifier.IsValid());

			Set(stageIdentifier);
		}

		return true;
	}

	inline bool RenderItemStageMask::Serialize(Serialization::Writer serializer, const Rendering::StageCache& stageCache) const
	{
		if (BaseType::AreNoneSet())
		{
			return false;
		}

		using BitIndexType = typename BaseType::BitIndexType;
		const BitIndexType numSetBits = GetNumberOfSetBits();
		if (numSetBits == 0)
		{
			return false;
		}

		Serialization::Value& value = serializer.GetValue();
		value = Serialization::Value(rapidjson::Type::kArrayType);
		value.Reserve((rapidjson::SizeType)numSetBits, serializer.GetDocument().GetAllocator());

		IterateSetBits(
			[&stageCache, &value, &serializer](const BitIndexType index)
			{
				const Rendering::SceneRenderStageIdentifier stageIdentifier = Rendering::SceneRenderStageIdentifier::MakeFromValidIndex(index);
				const Guid stageGuid = stageCache.GetAssetGuid(stageIdentifier);
				if (stageCache.GetAssetData(stageIdentifier).m_flags.IsSet(Rendering::StageFlags::Transient))
				{
					return true;
				}

				Serialization::Value guidValue;
				Serialization::Writer stageWriter(guidValue, serializer.GetData());
				stageWriter.SerializeInPlace(stageGuid);

				value.PushBack(Move(guidValue), serializer.GetDocument().GetAllocator());
				return true;
			}
		);

		return true;
	}
}
