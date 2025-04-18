#pragma once

#include <Common/Math/Range.h>

namespace ngine::Rendering
{
	//! Represents the used range of mips, index based
	struct MipRange : private Math::Range<uint32>
	{
		using BaseType = Math::Range<uint32>;
		using UnitType = uint32;

		constexpr MipRange()
			: BaseType(BaseType::MakeStartToEnd(0, 0))
		{
		}
		constexpr MipRange(const uint32 index, const uint32 count)
			: BaseType(BaseType::MakeStartToEnd(index, index + Math::Max(count, 1u) - 1u))
		{
		}

		using BaseType::begin;
		using BaseType::end;

		[[nodiscard]] constexpr uint32 GetIndex() const
		{
			return BaseType::GetMinimum();
		}
		[[nodiscard]] constexpr uint32 GetCount() const
		{
			return BaseType::GetSize();
		}
		[[nodiscard]] constexpr uint32 GetEnd() const
		{
			return GetIndex() + GetSize();
		}

		[[nodiscard]] constexpr bool operator==(const MipRange& other) const
		{
			return BaseType::operator==(other);
		}
		[[nodiscard]] constexpr bool operator!=(const MipRange& other) const
		{
			return BaseType::operator!=(other);
		}

		[[nodiscard]] constexpr bool Contains(const MipRange& other) const
		{
			return BaseType::Contains(other);
		}
	};
}
