#pragma once

#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	using FrameIndex = uint8;
	using FrameMask = uint8;
	struct FrameImageId
	{
		FrameImageId() = default;
		explicit FrameImageId(const FrameIndex id)
			: m_id(id)
		{
		}
		[[nodiscard]] explicit operator FrameIndex() const
		{
			return m_id;
		}
		[[nodiscard]] bool operator==(const FrameImageId other) const
		{
			return m_id == other.m_id;
		}
	private:
		FrameIndex m_id = 0;
	};
}
