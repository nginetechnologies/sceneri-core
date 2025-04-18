#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Function/Event.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Threading/Jobs/TimerHandle.h>
#include <Common/Time/Stopwatch.h>
#include <Common/Time/Timestamp.h>
#include <Common/Math/Log2.h>
#include <Common/Math/Vector2.h>

#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/DeviceType.h>
#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/Devices/Mouse/MouseButton.h>
#include <Engine/Input/ForwardDeclarations/ScreenCoordinate.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Input
{
	struct Manager;

	struct MouseDeviceType final : public DeviceType
	{
		inline static constexpr Guid DeviceTypeGuid = "66EB037F-94AC-407C-A69C-A4DB55EA5A8D"_guid;

		MouseDeviceType(const DeviceTypeIdentifier identifier, Manager& manager);

		virtual void RestoreInputState(Monitor&, const InputIdentifier) const override
		{
		}

		[[nodiscard]] DeviceIdentifier
		GetOrRegisterInstance(const uintptr platformIdentifier, Manager& manager, Optional<Rendering::Window*> pSourceWindow);
		[[nodiscard]] DeviceIdentifier FindInstance(const uintptr platformIdentifier, Manager& manager);

		enum class ClickState : uint8
		{
			AwaitingPress,
			SentSingleClickStartPress,
			AwaitingSecondPress,
			SentDoubleClickStartPress
		};

		struct Mouse final : public DeviceInstance
		{
			Mouse(
				const DeviceIdentifier identifier,
				const DeviceTypeIdentifier typeIdentifier,
				Monitor* pActiveMonitor,
				const uintptr platformIdentifier
			)
				: DeviceInstance(identifier, typeIdentifier, pActiveMonitor)
				, m_platformIdentifier(platformIdentifier)
			{
			}
			~Mouse();

			uintptr m_platformIdentifier;
			EnumFlags<MouseButton> m_buttonStates;
			Threading::Atomic<ClickState> m_clickState = ClickState::AwaitingPress;
			Time::Timestamp m_lastClickTime;
			uint32 m_doubleClickJobIdentifier = 0;
			Monitor* m_pMonitorDuringLastPress = nullptr;
			Math::Vector2i m_previousCoordinates = Math::Zero;
			Threading::Atomic<bool> m_isScrolling{false};
			Time::Timestamp m_lastScrollTime;
			Threading::TimerHandle m_scheduledTimerHandle;
		};

		void OnMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate, Rendering::Window& window);
		void OnMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const Math::Vector2i deltaCoordinates, Rendering::Window& window);
		void OnPress(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const MouseButton button, Rendering::Window& window);
		void OnRelease(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const MouseButton button, Rendering::Window* pWindow);
		void OnPressCancelled(DeviceIdentifier deviceIdentifier, const MouseButton button, Rendering::Window* pWindow);
		void OnStartScroll(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const Math::Vector2i delta, Rendering::Window& window);
		void OnScroll(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const Math::Vector2i delta, Rendering::Window& window);
		void OnEndScroll(DeviceIdentifier deviceIdentifier, ScreenCoordinate, const Math::Vector2f velocity, Rendering::Window& window);
		void OnCancelScroll(DeviceIdentifier deviceIdentifier, ScreenCoordinate, Rendering::Window& window);

		[[nodiscard]] InputIdentifier GetMoveCursorInputIdentifier() const
		{
			return m_moveCursorInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetDraggingMotionInputIdentifier() const
		{
			return m_motionDragInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetHoveringMotionInputIdentifier() const
		{
			return m_motionHoverInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetButtonPressInputIdentifier(const MouseButton button) const
		{
			return m_buttonIdentifiers[GetButtonIndex(button)].m_press;
		}

		[[nodiscard]] InputIdentifier GetButtonReleaseInputIdentifier(const MouseButton button) const
		{
			return m_buttonIdentifiers[GetButtonIndex(button)].m_release;
		}

		[[nodiscard]] InputIdentifier GetScrollInputIdentifier() const
		{
			return m_scrollInputIdentifier;
		}

		[[nodiscard]] virtual InputIdentifier DeserializeDeviceInput(const Serialization::Reader&) const override;
	protected:
		[[nodiscard]] uint8 GetButtonIndex(const MouseButton button) const
		{
			return Math::Log2((uint8)button);
		}
		[[nodiscard]] static Time::Durationf GetMaximumDoubleClickDelay();
		[[nodiscard]] static Time::Durationf GetLongPressDelay();
		[[nodiscard]] bool IsWithinDoubleClickTime(const Time::Timestamp lastClickTime) const;
		[[nodiscard]] bool IsWithinScrollTime(const Time::Timestamp lastScrollTime) const;

		enum class ScrollState : uint8
		{
			Scrolling,
			End,
			Cancel
		};
		void OnScrollInternal(
			const ScrollState state,
			DeviceIdentifier deviceIdentifier,
			ScreenCoordinate,
			const Math::Vector2i delta,
			const Math::Vector2f velocity,
			Rendering::Window& window
		);
	protected:
		enum class Motion : uint8
		{
			MoveCursor,
			Hover,
			Drag,
			Scroll,
		};

		struct ButtonIdentifiers
		{
			InputIdentifier m_press;
			InputIdentifier m_release;
		};

		Manager& m_manager;

		InputIdentifier m_moveCursorInputIdentifier;
		InputIdentifier m_motionHoverInputIdentifier;
		InputIdentifier m_motionDragInputIdentifier;
		Array<ButtonIdentifiers, (uint8)MouseButton::Count> m_buttonIdentifiers;
		InputIdentifier m_scrollInputIdentifier;

		friend Array<ButtonIdentifiers, (uint8)MouseButton::Count> CreateButtonIdentifiers(Manager& manager);
	};
}
