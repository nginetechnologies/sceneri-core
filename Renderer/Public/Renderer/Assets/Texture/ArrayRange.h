#pragma once

#include <Common/Math/Range.h>

namespace ngine::Rendering
{
	//! Represents the used range of texture / render target array levels, index based
	struct ArrayRange : private Math::Range<uint32>
	{
		using BaseType = Math::Range<uint32>;
		using UnitType = uint32;

		ArrayRange()
			: BaseType(BaseType::MakeStartToEnd(0, 0))
		{
		}
		ArrayRange(const uint32 index, const uint32 count)
			: BaseType(BaseType::MakeStartToEnd(index, index + Math::Max(count, 1u) - 1u))
		{
		}

		using BaseType::begin;
		using BaseType::end;

		[[nodiscard]] uint32 GetIndex() const
		{
			return BaseType::GetMinimum();
		}
		[[nodiscard]] uint32 GetCount() const
		{
			return BaseType::GetSize();
		}
		[[nodiscard]] uint32 GetEnd() const
		{
			return GetIndex() + GetSize();
		}

		[[nodiscard]] constexpr bool operator==(const ArrayRange& other) const
		{
			return BaseType::operator==(other);
		}
		[[nodiscard]] constexpr bool operator!=(const ArrayRange& other) const
		{
			return BaseType::operator!=(other);
		}

		[[nodiscard]] constexpr bool Contains(const ArrayRange& other) const
		{
			return BaseType::Contains(other);
		}
	};
}
