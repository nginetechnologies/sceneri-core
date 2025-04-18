#pragma once

namespace ngine::Network
{
	struct Channel
	{
		constexpr Channel(const uint8 channel)
			: m_channel(channel)
		{
		}

		[[nodiscard]] constexpr uint8 Get() const
		{
			return m_channel;
		}
	protected:
		uint8 m_channel;
	};
}
