#pragma once

#include <Common/Math/PowerOfTwo.h>
#include <Common/Math/Log2.h>
#include <Common/Math/Min.h>
#include <Common/Math/Ceil.h>
#include <Common/Math/Floor.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/PowerOfTwo.h>
#include <Common/Memory/CountBits.h>
#include <Common/Memory/Endian.h>

#include "MipRange.h"

namespace ngine::Rendering
{
	//! Represents a mask containing used mips
	struct MipMask
	{
		using StoredType = uint16;

		constexpr MipMask() = default;

		explicit constexpr MipMask(const StoredType mask)
			: m_mask(mask)
		{
		}

		[[nodiscard]] constexpr bool AreAnySet() const
		{
			return m_mask != 0;
		}
		[[nodiscard]] constexpr bool IsEmpty() const
		{
			return m_mask == 0;
		}

		[[nodiscard]] StoredType GetSize() const
		{
			return Memory::GetNumberOfSetBits(m_mask);
		}

		[[nodiscard]] constexpr StoredType GetValue() const
		{
			return m_mask;
		}

		[[nodiscard]] Memory::BitIndex<StoredType> GetFirstIndex() const
		{
			return Memory::GetFirstSetIndex(m_mask);
		}
		[[nodiscard]] Memory::BitIndex<StoredType> GetLastIndex() const
		{
			return Memory::GetLastSetIndex(m_mask);
		}

		[[nodiscard]] constexpr bool operator==(const MipMask other) const
		{
			return m_mask == other.m_mask;
		}
		[[nodiscard]] constexpr bool operator!=(const MipMask other) const
		{
			return m_mask != other.m_mask;
		}
		[[nodiscard]] constexpr MipMask operator&(const MipMask other) const
		{
			return MipMask{StoredType(m_mask & other.m_mask)};
		}
		constexpr MipMask& operator&=(const MipMask other)
		{
			m_mask = StoredType(m_mask & other.m_mask);
			return *this;
		}
		[[nodiscard]] constexpr MipMask operator|(const MipMask other) const
		{
			return MipMask{StoredType(m_mask | other.m_mask)};
		}
		constexpr MipMask& operator|=(const MipMask other)
		{
			m_mask = StoredType(m_mask | other.m_mask);
			return *this;
		}
		[[nodiscard]] constexpr MipMask operator>>(const StoredType offset) const
		{
			return MipMask{StoredType(m_mask >> offset)};
		}
		constexpr MipMask& operator>>=(const StoredType offset)
		{
			m_mask >>= offset;
			return *this;
		}
		[[nodiscard]] constexpr MipMask operator<<(const StoredType offset) const
		{
			return MipMask{StoredType(m_mask << offset)};
		}
		constexpr MipMask& operator<<=(const StoredType offset)
		{
			m_mask <<= offset;
			return *this;
		}
		[[nodiscard]] constexpr MipMask operator~() const
		{
			return MipMask{StoredType(~m_mask)};
		}

		//! Gets the mask as a continguous range
		[[nodiscard]] MipRange GetRange(uint16 totalMipCount) const
		{
			StoredType mask = m_mask;
			if (mask & 1)
			{
				const StoredType contiguousSetBitsCount = (StoredType)Memory::GetNumberOfTrailingZeros((StoredType)~mask);
				const uint16 firstIndex = totalMipCount - contiguousSetBitsCount;
				return {firstIndex, StoredType(contiguousSetBitsCount - 1)};
			}
			else if (mask != 0)
			{
				// Skip leading zeroes
				StoredType skippedBitCount = (StoredType)Memory::GetNumberOfTrailingZeros(mask);
				mask >>= skippedBitCount;

				Assert(mask & 1);
				const StoredType contiguousSetBitsCount = (StoredType)Memory::GetNumberOfTrailingZeros((StoredType)~mask);
				totalMipCount -= Math::Min(totalMipCount, skippedBitCount);
				return {StoredType(totalMipCount - Math::Min(totalMipCount, contiguousSetBitsCount)), StoredType(contiguousSetBitsCount - 1)};
			}
			else
			{
				return MipRange{0, 0};
			}
		}

		[[nodiscard]] static constexpr MipMask FromIndex(StoredType index)
		{
			return MipMask{StoredType(1u << index)};
		}

		[[nodiscard]] static MipMask FromRange(const MipRange range)
		{
			return MipMask{
				StoredType(Memory::SetBitRange(Math::Range<StoredType>::Make((StoredType)range.GetIndex(), (StoredType)range.GetCount())))
			};
		}

		[[nodiscard]] static MipMask FromSizeAllToLargest(const uint32 size)
		{
			if (Math::IsPowerOfTwo(size))
			{
				// Exact match
				return FromRange(MipRange{0, Math::Log2(size) + 1});
			}
			// Return largest
			return FromRange(MipRange{0, (StoredType)Math::Ceil(Math::Log2((float)size))});
		}
		[[nodiscard]] static MipMask FromSizeAllToLargest(const Math::Vector2ui size)
		{
			return FromSizeAllToLargest(Math::Min(size.x, size.y));
		}
		[[nodiscard]] static MipMask FromSizeAllToLargest(const Math::Vector3ui size)
		{
			return FromSizeAllToLargest(Math::Min(size.x, size.y, size.z));
		}

		[[nodiscard]] static MipMask FromSizeAllToSmallest(const uint32 size)
		{
			if (Math::IsPowerOfTwo(size))
			{
				// Exact match
				return FromRange(MipRange{0, Math::Log2(size) + 1});
			}
			// Return smallest
			return FromRange(MipRange{0, (StoredType)Math::Floor(Math::Log2((float)size))});
		}
		[[nodiscard]] static MipMask FromSizeAllToSmallest(const Math::Vector2ui size)
		{
			return FromSizeAllToSmallest(Math::Min(size.x, size.y));
		}

		//! Returns the exact or closest (rounding up) mip that would be necessary to render at the specified resolution
		[[nodiscard]] static constexpr MipMask FromSizeToLargest(const Math::Vector2ui size)
		{
			if (Math::IsPowerOfTwo(size.x) & Math::IsPowerOfTwo(size.y))
			{
				// Exact match
				StoredType index = (StoredType)Math::Log2(Math::Min(size.x, size.y));
				return MipMask{static_cast<StoredType>(1u << index)};
			}

			// Return largest
			StoredType index = (StoredType)Math::Ceil(Math::Log2((float)Math::Min(size.x, size.y)));
			return MipMask{static_cast<StoredType>(1u << index)};
		}

		//! Returns the exact or closest (rounding down) mip that would be necessary to render at the specified resolution
		[[nodiscard]] static constexpr MipMask FromSizeToSmallest(const Math::Vector2ui size)
		{
			if (Math::IsPowerOfTwo(size.x) & Math::IsPowerOfTwo(size.y))
			{
				// Exact match
				StoredType index = (StoredType)Math::Log2(Math::Min(size.x, size.y));
				return MipMask{static_cast<StoredType>(1u << index)};
			}

			// Return smallest
			StoredType index = (StoredType)Math::Floor(Math::Log2((float)Math::Min(size.x, size.y)));
			return MipMask{static_cast<StoredType>(1u << index)};
		}

		//! Returns the one or two mips that would be necessary to render at the specified resolution with blending
		[[nodiscard]] static constexpr MipMask FromSizeBlended(const Math::Vector2ui size)
		{
			if (Math::IsPowerOfTwo(size.x) & Math::IsPowerOfTwo(size.y))
			{
				// Exact mask, only return one mip
				StoredType index = (StoredType)Math::Log2(Math::Min(size.x, size.y));
				return MipMask{static_cast<StoredType>(1u << index)};
			}

			StoredType index = (StoredType)Math::Ceil(Math::Log2((float)Math::Min(size.x, size.y)));
			StoredType mask = static_cast<StoredType>(1 << index);
			// Also include the lower mip to allow blending
			mask |= mask >> 1;
			return MipMask{mask};
		}
	protected:
		StoredType m_mask{0};
	};

	inline static constexpr MipMask AllMips{Math::NumericLimits<MipMask::StoredType>::Max};
}
