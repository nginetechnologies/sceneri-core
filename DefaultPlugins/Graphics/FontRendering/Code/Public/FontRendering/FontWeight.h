#pragma once

#include <FontRendering/Point.h>
#include <Common/Guid.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Font
{
	struct Weight
	{
		inline static constexpr Guid TypeGuid = "{c155cf77-57d2-4276-9cbd-ba82f8afb916}"_guid;

		constexpr explicit Weight(const uint16 value)
			: m_value(value)
		{
		}

		[[nodiscard]] bool operator==(const Weight& other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] bool operator!=(const Weight& other) const
		{
			return m_value != other.m_value;
		}

		[[nodiscard]] uint16 GetValue() const
		{
			return m_value;
		}

		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer) const;
	protected:
		uint16 m_value;
	};
}
