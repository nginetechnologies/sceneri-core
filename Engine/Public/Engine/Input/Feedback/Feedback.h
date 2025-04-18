#pragma once

#include <Common/Memory/UniqueRef.h>

namespace ngine::Input
{
	struct Feedback
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		struct Generators;
#endif

		enum class NotificationType
		{
			Success,
			Error,
			Warning
		};

		Feedback();
		~Feedback();

		void TriggerNotification(NotificationType type);
		void TriggerImpact(float intensity = 0.5f);
		void TriggerSelection();

		[[nodiscard]] bool DoesDeviceSupportHaptics() const;
	private:
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		UniqueRef<Generators> m_pGenerators;
#endif
	};
} // namespace ngine::Input
