#pragma once

#include <Widgets/Style/Size.h>
#include <Widgets/PositionType.h>

namespace ngine::Widgets
{
	struct WidgetPositionInfo
	{
		inline static constexpr Guid TypeGuid = "1508d946-db30-4dbe-af82-a0bbcaf76081"_guid;

		[[nodiscard]] bool operator==(const WidgetPositionInfo& other) const
		{
			return position == other.position && positionType == other.positionType;
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer writer) const;

		Style::Size position;
		PositionType positionType;
	};
}
