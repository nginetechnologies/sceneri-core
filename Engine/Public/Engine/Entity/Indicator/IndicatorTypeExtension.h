#pragma once

#include <Common/Guid.h>
#include <Common/EnumFlags.h>

namespace ngine::Entity
{
	struct IndicatorTypeExtension final : public Reflection::ExtensionInterface
	{
		inline static constexpr Guid TypeGuid = "{FE599213-64C4-478F-97B4-954ECCFA8DF9}"_guid;

		enum class Flags : uint8
		{
			RequiresGhost = 1 << 0
		};

		constexpr IndicatorTypeExtension(
			const Guid indicatorComponentGuid, const Guid indicatorIconGuid = {}, const EnumFlags<Flags> flags = {}
		)
			: ExtensionInterface{TypeGuid}
			, m_indicatorComponentGuid(indicatorComponentGuid)
			, m_indicatorIconGuid(indicatorIconGuid)
			, m_flags(flags)
		{
		}
		constexpr IndicatorTypeExtension(const EnumFlags<Flags> flags)
			: ExtensionInterface{TypeGuid}
			, m_flags(flags)
		{
		}

		[[nodiscard]] constexpr bool HasCustomIndicator() const
		{
			return m_indicatorComponentGuid.IsValid();
		}
		[[nodiscard]] constexpr Guid GetGuid() const
		{
			return m_indicatorComponentGuid;
		}
		[[nodiscard]] constexpr Guid GetIndicatorIconGuid() const
		{
			return m_indicatorIconGuid;
		}

		[[nodiscard]] constexpr EnumFlags<Flags> GetFlags() const
		{
			return m_flags;
		}
	private:
		Guid m_indicatorComponentGuid;
		Guid m_indicatorIconGuid;
		EnumFlags<Flags> m_flags;
	};
}
