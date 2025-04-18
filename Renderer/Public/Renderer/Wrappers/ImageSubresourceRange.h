#pragma once

#include <Renderer/ImageAspectFlags.h>
#include <Renderer/Assets/Texture/MipRange.h>
#include <Renderer/Assets/Texture/ArrayRange.h>

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	struct ImageSubresourceRange
	{
		ImageSubresourceRange() = default;
		ImageSubresourceRange(
			const EnumFlags<ImageAspectFlags> aspectMask, const MipRange mipRange = {0, 1}, const ArrayRange arrayRange = {0, 1}
		)
			: m_aspectMask(aspectMask)
			, m_mipRange(mipRange)
			, m_arrayRange(arrayRange)
		{
		}

		[[nodiscard]] bool operator==(const ImageSubresourceRange other) const
		{
			return (m_aspectMask == other.m_aspectMask) & (m_mipRange == other.m_mipRange) & (m_arrayRange == other.m_arrayRange);
		}
		[[nodiscard]] bool operator!=(const ImageSubresourceRange other) const
		{
			return !operator==(other);
		}

		[[nodiscard]] constexpr bool Contains(const ImageSubresourceRange& other) const
		{
			return m_aspectMask.AreAllSet(other.m_aspectMask) && m_mipRange.Contains(other.m_mipRange) &&
			       m_arrayRange.Contains(other.m_arrayRange);
		}

		EnumFlags<ImageAspectFlags> m_aspectMask;
		MipRange m_mipRange;
		ArrayRange m_arrayRange;
	};
}
