#pragma once

#include <Widgets/Style/Size.h>

namespace ngine::Widgets
{
	struct WidgetSizeInfo
	{
		inline static constexpr Guid TypeGuid = "c28b5ea3-cbf8-4569-a854-b88fade3bdfb"_guid;

		[[nodiscard]] bool operator==(const WidgetSizeInfo& other) const
		{
			return size == other.size && growthFactor == other.growthFactor && growthFactor == other.growthFactor;
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer writer) const;

		Style::Size size;
		float growthFactor;

		enum class GrowthDirection : uint8
		{
			Horizontal,
			Vertical,
			None
		};
		GrowthDirection growthDirection;
	};
}
