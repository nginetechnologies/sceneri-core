#pragma once

#include <Common/Memory/Containers/StringView.h>

namespace ngine::Audio
{
	enum class InitializationResult : uint8
	{
		Failed,
		Success
	};

	struct SystemInterface
	{
	public:
		virtual ~SystemInterface()
		{
		}

		[[nodiscard]] virtual InitializationResult Initialize() = 0;
		virtual void Shutdown() = 0;

		[[nodiscard]] virtual ConstStringView GetDeviceName() const = 0;
	};
}
