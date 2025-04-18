#pragma once

#include <Renderer/Stages/RenderItemStageMask.h>

namespace ngine::Rendering
{
	struct RenderItemStagesProperty
	{
		inline static constexpr Guid TypeGuid = "{7E3ACDD5-7D38-4EAD-B66A-9DA0C3698C90}"_guid;

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer) const;

		[[nodiscard]] inline bool operator==(const RenderItemStagesProperty& other) const
		{
			return m_mask == other.m_mask;
		}
		[[nodiscard]] inline bool operator!=(const RenderItemStagesProperty& other) const
		{
			return m_mask != other.m_mask;
		}

		RenderItemStageMask m_mask;
		StageCache& m_stageCache;
	};
}
