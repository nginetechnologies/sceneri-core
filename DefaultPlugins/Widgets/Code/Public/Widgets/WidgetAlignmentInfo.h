#pragma once

#include <Widgets/Alignment.h>

namespace ngine::Widgets
{
	struct WidgetAlignmentInfo
	{
		inline static constexpr Guid TypeGuid = "6b165158-3088-472b-b233-38635e75e44a"_guid;

		[[nodiscard]] bool operator==(const WidgetAlignmentInfo& other) const
		{
			return x == other.x && y == other.y;
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer writer) const;

		Alignment x;
		Alignment y;
	};
}
