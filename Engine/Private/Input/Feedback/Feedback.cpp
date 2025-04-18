#include <Input/Feedback/Feedback.h>

#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
#include <CoreHaptics/CoreHaptics.h>
#include <UIKit/UIKit.h>
#endif

namespace ngine::Input
{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
	struct Feedback::Generators
	{
		UIImpactFeedbackGenerator* m_pImpactGenerator;
		UISelectionFeedbackGenerator* m_pSelectionGenerator;
		UINotificationFeedbackGenerator* m_pNotificationGenerator;
	};
#endif

	Feedback::Feedback()
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		: m_pGenerators(
				Memory::ConstructInPlace,
				Generators{
					[[UIImpactFeedbackGenerator alloc] init],
					[[UISelectionFeedbackGenerator alloc] init],
					[[UINotificationFeedbackGenerator alloc] init]
				}
			)
#endif
	{
	}

	Feedback::~Feedback()
	{
	}

	void Feedback::TriggerImpact([[maybe_unused]] float intensity)
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		[m_pGenerators->m_pImpactGenerator impactOccurredWithIntensity:intensity];
#endif
	}

	void Feedback::TriggerNotification([[maybe_unused]] Feedback::NotificationType type)
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		UINotificationFeedbackType iosType;
		switch (type)
		{
			case Feedback::NotificationType::Success:
				iosType = UINotificationFeedbackTypeSuccess;
				break;
			case Feedback::NotificationType::Warning:
				iosType = UINotificationFeedbackTypeWarning;
				break;
			case Feedback::NotificationType::Error:
				iosType = UINotificationFeedbackTypeError;
				break;
		}
		[m_pGenerators->m_pNotificationGenerator notificationOccurred:iosType];
#endif
	}

	void Feedback::TriggerSelection()
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		[m_pGenerators->m_pSelectionGenerator selectionChanged];
#endif
	}

	bool Feedback::DoesDeviceSupportHaptics() const
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		return CHHapticEngine.capabilitiesForHardware.supportsHaptics;
#else
		return false;
#endif
	}
}
