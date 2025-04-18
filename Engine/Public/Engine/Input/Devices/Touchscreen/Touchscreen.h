#pragma once

#include <Engine/Input/GestureRecognizer.h>
#include <Engine/Input/DeviceType.h>
#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/Devices/Touchscreen/FingerIdentifier.h>
#include <Engine/Input/ScreenCoordinate.h>

#include <Common/Memory/Containers/Vector.h>

#include <Common/Math/ForwardDeclarations/Ratio.h>
#include <Common/Math/ForwardDeclarations/Radius.h>
#include <Common/Math/ForwardDeclarations/Angle.h>
#include <Common/Math/ForwardDeclarations/RotationalSpeed.h>
#include <Common/Function/Event.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Input
{
	struct Manager;

	struct TouchscreenDeviceType final : public DeviceType
	{
		inline static constexpr Guid DeviceTypeGuid = "D2CE3217-92A0-4826-AB19-C7748CB86445"_guid;

		TouchscreenDeviceType(const DeviceTypeIdentifier identifier, Manager& manager);
		virtual void RestoreInputState(Monitor&, const InputIdentifier) const override
		{
		}

		[[nodiscard]] DeviceIdentifier
		GetOrRegisterInstance(const int64 sdlIdentifier, Manager& manager, Optional<Rendering::Window*> pSourceWindow);

		void OnStartTouch(TouchDescriptor touch, DeviceIdentifier, Rendering::Window&);
		void OnStopTouch(TouchDescriptor touch, DeviceIdentifier, Rendering::Window&);
		void OnMotion(TouchDescriptor touch, DeviceIdentifier, Rendering::Window&);
		void OnCancelTouch(DeviceIdentifier deviceIdentifier, FingerIdentifier fingerIdentifier, Rendering::Window& window);

		void OnStartTap(
			DeviceIdentifier,
			ScreenCoordinate,
			uint16 touchRadius,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnStopTap(
			DeviceIdentifier,
			ScreenCoordinate,
			uint16 touchRadius,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnCancelTap(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);
		void OnDoubleTap(
			DeviceIdentifier,
			ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			uint16 touchRadius,
			Rendering::Window& window
		);

		void OnStartLongPress(
			DeviceIdentifier,
			ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			uint16 touchRadius,
			Rendering::Window& window
		);
		void OnStopLongPress(
			DeviceIdentifier,
			ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			uint16 touchRadius,
			Rendering::Window& window
		);
		void OnLongPressMotion(
			DeviceIdentifier,
			ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			uint16 touchRadius,
			Rendering::Window& window
		);
		void OnCancelLongPress(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);

		void OnStartPan(
			DeviceIdentifier,
			ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Math::Vector2f velocity,
			uint16 touchRadius,
			Rendering::Window& window
		);
		void OnStopPan(
			DeviceIdentifier,
			ScreenCoordinate,
			Math::Vector2f velocity,
			const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
			Rendering::Window& window
		);
		void OnPanMotion(
			DeviceIdentifier,
			ScreenCoordinate,
			Math::Vector2f velocity,
			uint16 touchRadius,
			const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
			Rendering::Window& window
		);
		void OnCancelPan(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);
		void OnStartPinch(
			DeviceIdentifier,
			ScreenCoordinate,
			const float scale,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnStopPinch(DeviceIdentifier, ScreenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);
		void OnPinchMotion(
			DeviceIdentifier,
			ScreenCoordinate,
			const float scale,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnCancelPinch(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);
		void
		OnStartRotate(DeviceIdentifier, ScreenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);
		void OnStopRotate(
			DeviceIdentifier,
			ScreenCoordinate,
			const Math::Anglef angle,
			const Math::RotationalSpeedf velocity,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnRotateMotion(
			DeviceIdentifier,
			ScreenCoordinate,
			const Math::Anglef angle,
			const Math::RotationalSpeedf velocity,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			Rendering::Window& window
		);
		void OnCancelRotate(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window);

		[[nodiscard]] InputIdentifier GetStartTouchInputIdentifier() const
		{
			return m_startTouchInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetStopTouchInputIdentifier() const
		{
			return m_stopTouchInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetMotionInputIdentifier() const
		{
			return m_motionInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetTapInputIdentifier() const
		{
			return m_tapInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetDoubleTapInputIdentifier() const
		{
			return m_doubleTapInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetLongPressInputIdentifier() const
		{
			return m_longPressInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetPanInputIdentifier() const
		{
			return m_panInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetPinchInputIdentifier() const
		{
			return m_pinchInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetRotateInputIdentifier() const
		{
			return m_rotateInputIdentifier;
		}

		struct Touchscreen final : public DeviceInstance
		{
			Touchscreen(
				const DeviceIdentifier identifier,
				const DeviceTypeIdentifier typeIdentifier,
				Monitor* pActiveMonitor,
				const int64 sdlIdentifier,
				const ScreenCoordinate size
			)
				: DeviceInstance(identifier, typeIdentifier, pActiveMonitor)
				, m_sdlIdentifier(sdlIdentifier)
				, m_size(size)
			{
			}

			void SetActiveMonitor(Monitor* pMonitor, const DeviceType& deviceType) = delete;
			void SetActiveMonitor(Monitor& monitor, const DeviceType& deviceType) = delete;

			void SetActiveMonitor(Monitor& monitor, const DeviceType&, FingerIdentifier fingerIdentifier)
			{
				auto it = m_fingerInputMonitors.Find(fingerIdentifier);
				if (it != m_fingerInputMonitors.end())
				{
					it->second = monitor;
				}
				else
				{
					m_fingerInputMonitors.Emplace(fingerIdentifier, &monitor);
				}
			}
			void SetActiveMonitor(Monitor* pMonitor, const DeviceType& deviceType, const FingerIdentifier fingerIdentifier)
			{
				if (pMonitor != nullptr)
				{
					SetActiveMonitor(*pMonitor, deviceType, fingerIdentifier);
				}
				else
				{
					auto it = m_fingerInputMonitors.Find(fingerIdentifier);
					if (it != m_fingerInputMonitors.end())
					{
						m_fingerInputMonitors.Remove(it);
					}
				}
			}

			[[nodiscard]] Optional<Monitor*> GetActiveMonitor(const FingerIdentifier fingerIdentifier) const
			{
				auto it = m_fingerInputMonitors.Find(fingerIdentifier);
				if (it != m_fingerInputMonitors.end())
				{
					return it->second;
				}
				else
				{
					return Invalid;
				}
			}

			[[nodiscard]] Optional<Monitor*> GetMonitorDuringLastPress(const FingerIdentifier fingerIdentifier) const
			{
				auto it = m_lastPressFingerInputMonitors.Find(fingerIdentifier);
				if (it != m_lastPressFingerInputMonitors.end())
				{
					return it->second;
				}
				else
				{
					return Invalid;
				}
			}

			void SetMonitorDuringLastPress(const FingerIdentifier fingerIdentifier, Optional<Monitor*> pMonitor)
			{
				auto it = m_lastPressFingerInputMonitors.Find(fingerIdentifier);
				if (pMonitor != nullptr)
				{
					if (it != m_lastPressFingerInputMonitors.end())
					{
						it->second = pMonitor;
					}
					else
					{
						m_lastPressFingerInputMonitors.Emplace(fingerIdentifier, pMonitor);
					}
				}
				else if (it != m_fingerInputMonitors.end())
				{
					m_lastPressFingerInputMonitors.Remove(it);
				}
			}

			int64 m_sdlIdentifier;
			UnorderedMap<FingerIdentifier, Optional<Monitor*>> m_lastPressFingerInputMonitors;
			UnorderedMap<FingerIdentifier, Optional<Monitor*>> m_fingerInputMonitors;
			ScreenCoordinate m_size;
		};

		Event<void(void*, Monitor& newMonitor), 24> OnMonitorChanged;

		[[nodiscard]] virtual InputIdentifier DeserializeDeviceInput(const Serialization::Reader&) const override;
	protected:
		Manager& m_manager;

		GestureRecognizer m_gestureRecognizer;

		InputIdentifier m_startTouchInputIdentifier;
		InputIdentifier m_stopTouchInputIdentifier;
		InputIdentifier m_motionInputIdentifier;
		InputIdentifier m_tapInputIdentifier;
		InputIdentifier m_doubleTapInputIdentifier;
		InputIdentifier m_longPressInputIdentifier;
		InputIdentifier m_panInputIdentifier;
		InputIdentifier m_pinchInputIdentifier;
		InputIdentifier m_rotateInputIdentifier;
	};
}
