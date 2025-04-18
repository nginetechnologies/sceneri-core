#pragma once

#include "Format.h"
#include "Swizzle.h"

#include <Common/Platform/ForceInline.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector2/Max.h>
#include <Common/Math/Vector2/Ceil.h>
#include <Common/Math/Vector3.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class FormatFlags : uint32
	{
		SRGB = 1 << 0,
		Normalized = 1 << 1,
		Scaled = 1 << 2,
		Unsigned = 1 << 3,
		Signed = 1 << 4,
		Integer = 1 << 5,
		Float = 1 << 6,
		Depth = 1 << 7,
		Stencil = 1 << 8,
		DepthStencil = Depth | Stencil,
		Swizzle = 1 << 9,
		LuminanceAlpha = 1 << 10,
		Packed8 = 1 << 11,
		Packed16 = 1 << 12,
		Packed32 = 1 << 13,
		ASTC = 1 << 14,
		BC = 1 << 15,
		HDR = 1 << 16,
		Compressed = ASTC | BC,
		Packed = Packed8 | Packed16 | Packed32
	};
	ENUM_FLAG_OPERATORS(FormatFlags);

	struct FormatInfo
	{
		[[nodiscard]] PURE_LOCALS_AND_POINTERS constexpr uint32 GetBitsPerPixel() const
		{
			return m_blockDataSize * 8 / m_blockExtent.GetComponentLength();
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Math::Vector2ui GetBlockCount(const Math::Vector2ui extent) const
		{
			const Math::Vector2ui blockExtent{m_blockExtent.x, m_blockExtent.y};
			return Math::Max(extent + blockExtent - Math::Vector2ui{1}, blockExtent) / blockExtent;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS uint32 GetBytesPerRow(const uint32 width) const
		{
			return (uint32)Math::Ceil((float)width / (float)m_blockExtent.x) * m_blockDataSize;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS uint32 GetBytesPerColumn(const uint32 height) const
		{
			return (uint32)Math::Ceil((float)height / (float)m_blockExtent.y) * m_blockDataSize;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Math::Vector2ui GetBytesPerDimension(const Math::Vector2ui size) const
		{
			const Math::Vector2ui blockCount = (Math::Vector2ui
			)Math::Ceil((Math::Vector2f)size / (Math::Vector2f)Math::Vector2ui{m_blockExtent.x, m_blockExtent.y});
			return blockCount * Math::Vector2ui{m_blockDataSize};
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS uint32 GetBytesPerLayer(const Math::Vector2ui size) const
		{
			const Math::Vector2ui blockCount = (Math::Vector2ui
			)Math::Ceil((Math::Vector2f)size / (Math::Vector2f)Math::Vector2ui{m_blockExtent.x, m_blockExtent.y});
			return blockCount.GetComponentLength() * m_blockDataSize;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS constexpr uint8 GetBlockDataSizePerChannel() const
		{
			return m_blockDataSize / m_componentCount;
		}

		uint8 m_blockDataSize;
		Math::TVector3<uint8> m_blockExtent;
		uint8 m_componentCount;
		Array<Swizzle, 4> m_swizzle;
		EnumFlags<FormatFlags> m_flags;
	};

	[[nodiscard]] PURE_NOSTATICS constexpr FormatInfo GetFormatInfo(const Format format)
	{
		switch (format)
		{
			case Format::Invalid:
				return FormatInfo{0, {0, 0, 0}, 0, {Swizzle::Zero, Swizzle::Zero, Swizzle::Zero, Swizzle::Zero}, FormatFlags{}};
			case Format::R4G4_UNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Packed8 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R4G4B4A4_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::B4G4R4A4_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R5G6B5_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::B5G6R5_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R5G5B5A1_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::B5G5R5A1_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::A1R5G5B5_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Alpha, Swizzle::Red, Swizzle::Green, Swizzle::Blue},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R8_UNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R8_SNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R8_USCALED:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				}; // ,
			case Format::R8_SSCALED:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R8_UINT:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R8_SINT:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R8_SRGB:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB
				};

			case Format::R8G8_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R8G8_SNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R8G8_USCALED:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R8G8_SSCALED:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R8G8_UINT:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R8G8_SINT:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R8G8_SRGB:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::SRGB
				};

			case Format::R8G8B8_UNORM:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R8G8B8_SNORM:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R8G8B8_USCALED:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R8G8B8_SSCALED:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R8G8B8_UINT:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R8G8B8_SINT:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R8G8B8_SRGB:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB
				};

			case Format::B8G8R8_UNORM_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8_SNORM_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8_USCALED_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8_SSCALED_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8_UINT_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8_SINT_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8_SRGB_PACK8:
				return FormatInfo{
					3,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB | FormatFlags::Swizzle
				};

			case Format::R8G8B8A8_UNORM_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R8G8B8A8_SNORM_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R8G8B8A8_USCALED_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R8G8B8A8_SSCALED_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R8G8B8A8_UINT_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R8G8B8A8_SINT_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R8G8B8A8_SRGB_PACK8:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB
				};

			case Format::B8G8R8A8_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_SNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_USCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_SSCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_SINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed | FormatFlags::Swizzle
				};
			case Format::B8G8R8A8_SRGB:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB | FormatFlags::Swizzle
				};

			case Format::R8G8B8A8_UNORM_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_SNORM_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_USCALED_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_SSCALED_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_UINT_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_SINT_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed | FormatFlags::Swizzle | FormatFlags::Packed32
				};
			case Format::R8G8B8A8_SRGB_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::SRGB | FormatFlags::Swizzle | FormatFlags::Packed32
				};

			case Format::R10G10B10A2_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32
				};
			case Format::R10G10B10A2_SNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::Packed32
				};
			case Format::R10G10B10A2_USCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned | FormatFlags::Packed32
				};
			case Format::R10G10B10A2_SSCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed | FormatFlags::Packed32
				};
			case Format::R10G10B10A2_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned | FormatFlags::Packed32
				};
			case Format::R10G10B10A2_SINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed | FormatFlags::Packed32
				};

			case Format::B10G10R10A2_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A2_SNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A2_USCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A2_SSCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A2_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A2_SINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed | FormatFlags::Packed32 | FormatFlags::Swizzle
				};

			case Format::R16_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R16_SNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R16_USCALED:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R16_SSCALED:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R16_UINT:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R16_SINT:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R16_SFLOAT:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R16G16_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R16G16_SNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R16G16_USCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R16G16_SSCALED:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R16G16_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R16G16_SINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R16G16_SFLOAT:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R16G16B16_UNORM:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R16G16B16_SNORM:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R16G16B16_USCALED:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R16G16B16_SSCALED:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R16G16B16_UINT:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R16G16B16_SINT:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R16G16B16_SFLOAT:
				return FormatInfo{
					6,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R16G16B16A16_UNORM:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::R16G16B16A16_SNORM:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Signed
				};
			case Format::R16G16B16A16_USCALED:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Unsigned
				};
			case Format::R16G16B16A16_SSCALED:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Scaled | FormatFlags::Signed
				};
			case Format::R16G16B16A16_UINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R16G16B16A16_SINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R16G16B16A16_SFLOAT:
				return FormatInfo{
					8,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R32_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R32_SINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R32_SFLOAT:
				return FormatInfo{
					4,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R32G32_UINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R32G32_SINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R32G32_SFLOAT:
				return FormatInfo{
					8,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R32G32B32_UINT:
				return FormatInfo{
					12,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R32G32B32_SINT:
				return FormatInfo{
					12,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R32G32B32_SFLOAT:
				return FormatInfo{
					12,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R32G32B32A32_UINT:
				return FormatInfo{
					16,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R32G32B32A32_SINT:
				return FormatInfo{
					16,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R32G32B32A32_SFLOAT:
				return FormatInfo{
					16,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR
				};

			case Format::R64_UINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R64_SINT:
				return FormatInfo{
					8,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R64_SFLOAT:
				return FormatInfo{
					8,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed
				};

			case Format::R64G64_UINT:
				return FormatInfo{
					16,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R64G64_SINT:
				return FormatInfo{
					16,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R64G64_SFLOAT:
				return FormatInfo{
					16,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed
				};

			case Format::R64G64B64_UINT:
				return FormatInfo{
					24,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R64G64B64_SINT:
				return FormatInfo{
					24,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R64G64B64_SFLOAT:
				return FormatInfo{
					24,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed
				};

			case Format::R64G64B64A64_UINT:
				return FormatInfo{
					32,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Unsigned
				};
			case Format::R64G64B64A64_SINT:
				return FormatInfo{
					32,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Integer | FormatFlags::Signed
				};
			case Format::R64G64B64A64_SFLOAT:
				return FormatInfo{
					32,
					{1, 1, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Float | FormatFlags::Signed
				};

			case Format::R11G11B10_UFLOAT:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Packed32 | FormatFlags::Float | FormatFlags::Signed
				};
			case Format::R9G9B9E5_UFLOAT:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Packed32 | FormatFlags::Float | FormatFlags::Unsigned
				};

			case Format::D16_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Integer
				};
			case Format::D24_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Integer
				};
			case Format::D32_SFLOAT:
				return FormatInfo{
					4,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Float
				};
			case Format::S8_UINT:
				return FormatInfo{1, {1, 1, 1}, 1, {Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha}, FormatFlags::Stencil};
			case Format::D16_UNORM_S8_UINT:
				return FormatInfo{
					3,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Integer | FormatFlags::Stencil
				};
			case Format::D24_UNORM_S8_UINT:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Integer | FormatFlags::Stencil
				};
			case Format::D32_SFLOAT_S8_UINT:
				return FormatInfo{
					5,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Depth | FormatFlags::Float | FormatFlags::Stencil
				};

			case Format::BC1_RGB_UNORM:
				return FormatInfo{
					8,
					{4, 4, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC1_RGB_SRGB:
				return FormatInfo{
					8,
					{4, 4, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC1_RGBA_UNORM:
				return FormatInfo{
					8,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC1_RGBA_SRGB:
				return FormatInfo{
					8,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC2_RGBA_UNORM:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC2_RGBA_SRGB:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC3_RGBA_UNORM:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC3_RGBA_SRGB:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC4_R_UNORM:
				return FormatInfo{
					8,
					{4, 4, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC4_R_SNORM:
				return FormatInfo{
					8,
					{4, 4, 1},
					1,
					{Swizzle::Red, Swizzle::Zero, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::BC
				};
			case Format::BC5_RG_UNORM:
				return FormatInfo{
					16,
					{4, 4, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC5_RG_SNORM:
				return FormatInfo{
					16,
					{4, 4, 1},
					2,
					{Swizzle::Red, Swizzle::Green, Swizzle::Zero, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Signed | FormatFlags::BC
				};
			case Format::BC6H_RGB_UFLOAT:
				return FormatInfo{
					16,
					{4, 4, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Float | FormatFlags::Unsigned | FormatFlags::HDR | FormatFlags::BC
				};
			case Format::BC6H_RGB_SFLOAT:
				return FormatInfo{
					16,
					{4, 4, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Float | FormatFlags::Signed | FormatFlags::HDR | FormatFlags::BC
				};
			case Format::BC7_RGBA_UNORM:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};
			case Format::BC7_RGBA_SRGB:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::BC
				};

			case Format::ASTC_4X4_LDR:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_4X4_SRGB:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_5X4_LDR:
				return FormatInfo{
					16,
					{5, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_5X4_SRGB:
				return FormatInfo{
					16,
					{5, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_5X5_LDR:
				return FormatInfo{
					16,
					{5, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_5X5_SRGB:
				return FormatInfo{
					16,
					{5, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_6X5_LDR:
				return FormatInfo{
					16,
					{6, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_6X5_SRGB:
				return FormatInfo{
					16,
					{6, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_6X6_LDR:
				return FormatInfo{
					16,
					{6, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_6X6_SRGB:
				return FormatInfo{
					16,
					{6, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X5_LDR:
				return FormatInfo{
					16,
					{8, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X5_SRGB:
				return FormatInfo{
					16,
					{8, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X6_LDR:
				return FormatInfo{
					16,
					{8, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X6_SRGB:
				return FormatInfo{
					16,
					{8, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X8_LDR:
				return FormatInfo{
					16,
					{8, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_8X8_SRGB:
				return FormatInfo{
					16,
					{8, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X5_LDR:
				return FormatInfo{
					16,
					{10, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X5_SRGB:
				return FormatInfo{
					16,
					{10, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X6_LDR:
				return FormatInfo{
					16,
					{10, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X6_SRGB:
				return FormatInfo{
					16,
					{10, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X8_LDR:
				return FormatInfo{
					16,
					{10, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X8_SRGB:
				return FormatInfo{
					16,
					{10, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X10_LDR:
				return FormatInfo{
					16,
					{10, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_10X10_SRGB:
				return FormatInfo{
					16,
					{10, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_12X10_LDR:
				return FormatInfo{
					16,
					{12, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_12X10_SRGB:
				return FormatInfo{
					16,
					{12, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_12X12_LDR:
				return FormatInfo{
					16,
					{12, 12, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};
			case Format::ASTC_12X12_SRGB:
				return FormatInfo{
					16,
					{12, 12, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::SRGB | FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC
				};

			case Format::ASTC_4X4_HDR:
				return FormatInfo{
					16,
					{4, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_5X4_HDR:
				return FormatInfo{
					16,
					{5, 4, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_5X5_HDR:
				return FormatInfo{
					16,
					{5, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_6X5_HDR:
				return FormatInfo{
					16,
					{6, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_6X6_HDR:
				return FormatInfo{
					16,
					{6, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_8X5_HDR:
				return FormatInfo{
					16,
					{8, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_8X6_HDR:
				return FormatInfo{
					16,
					{8, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_8X8_HDR:
				return FormatInfo{
					16,
					{8, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_10X5_HDR:
				return FormatInfo{
					16,
					{10, 5, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_10X6_HDR:
				return FormatInfo{
					16,
					{10, 6, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_10X8_HDR:
				return FormatInfo{
					16,
					{10, 8, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_10X10_HDR:
				return FormatInfo{
					16,
					{10, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_12X10_HDR:
				return FormatInfo{
					16,
					{12, 10, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};
			case Format::ASTC_12X12_HDR:
				return FormatInfo{
					16,
					{12, 12, 1},
					4,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::ASTC | FormatFlags::HDR
				};

			case Format::L8_UNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Red, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};
			case Format::A8_UNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					1,
					{Swizzle::Zero, Swizzle::Zero, Swizzle::Zero, Swizzle::Red},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};
			case Format::L8A8_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Red, Swizzle::Red, Swizzle::Green},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};
			case Format::L16_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Red, Swizzle::Red, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};
			case Format::A16_UNORM:
				return FormatInfo{
					2,
					{1, 1, 1},
					1,
					{Swizzle::Zero, Swizzle::Zero, Swizzle::Zero, Swizzle::Red},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};
			case Format::L16A16_UNORM:
				return FormatInfo{
					4,
					{1, 1, 1},
					2,
					{Swizzle::Red, Swizzle::Red, Swizzle::Red, Swizzle::Green},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::LuminanceAlpha
				};

			case Format::B8G8R8_UNORM_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Swizzle
				};
			case Format::B8G8R8_SRGB_PACK32:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::One},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Swizzle | FormatFlags::SRGB
				};

			case Format::R3G3B2_UNORM:
				return FormatInfo{
					1,
					{1, 1, 1},
					3,
					{Swizzle::Red, Swizzle::Green, Swizzle::Blue, Swizzle::One},
					FormatFlags::Packed8 | FormatFlags::Normalized | FormatFlags::Unsigned
				};

			case Format::B10G10R10_XR:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A10_XR:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle
				};
			case Format::B10G10R10A10_XR_SRGB:
				return FormatInfo{
					4,
					{1, 1, 1},
					4,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle | FormatFlags::SRGB
				};
			case Format::B10G10R10_XR_SRGB:
				return FormatInfo{
					4,
					{1, 1, 1},
					3,
					{Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha},
					FormatFlags::Normalized | FormatFlags::Unsigned | FormatFlags::Packed32 | FormatFlags::Swizzle | FormatFlags::SRGB
				};

			case Format::YCbCr_G8_B8R8_2PLANE_420_UNORM:
				return FormatInfo{4, {1, 1, 1}, 3, {Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha}, FormatFlags{}};
			case Format::YCbCr_G8_B8R8_2PLANE_422_UNORM:
				return FormatInfo{4, {1, 1, 1}, 3, {Swizzle::Blue, Swizzle::Green, Swizzle::Red, Swizzle::Alpha}, FormatFlags{}};

			case Format::A1B5G5R5_UNORM_PACK16:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Alpha, Swizzle::Blue, Swizzle::Green, Swizzle::Red},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::A4R4G4B4_UNORM_PACK16:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Alpha, Swizzle::Red, Swizzle::Green, Swizzle::Blue},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
			case Format::A4B4G4R4_UNORM_PACK16:
				return FormatInfo{
					2,
					{1, 1, 1},
					4,
					{Swizzle::Alpha, Swizzle::Blue, Swizzle::Green, Swizzle::Red},
					FormatFlags::Packed16 | FormatFlags::Normalized | FormatFlags::Unsigned
				};
		}

		ExpectUnreachable();
	}
}
