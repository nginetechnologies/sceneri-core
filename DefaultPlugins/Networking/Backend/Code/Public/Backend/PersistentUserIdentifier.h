#pragma once

#include <Common/Math/Hash.h>

namespace ngine::Networking::Backend
{
	struct PersistentUserIdentifier
	{
		using Type = uint64;

		constexpr PersistentUserIdentifier() = default;
		constexpr PersistentUserIdentifier(const Type identifier)
			: m_identifier(identifier)
		{
		}

		[[nodiscard]] constexpr bool operator==(const PersistentUserIdentifier other) const
		{
			return m_identifier == other.m_identifier;
		}
		[[nodiscard]] constexpr bool IsValid() const
		{
			return m_identifier > 0;
		}
		[[nodiscard]] constexpr Type Get() const
		{
			return m_identifier;
		}

		struct Hash
		{
			size operator()(const PersistentUserIdentifier& identifier) const
			{
				return Math::Hash(identifier.m_identifier);
			}
		};
	private:
		Type m_identifier{0};
	};
}
