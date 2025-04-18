#pragma once

#include <Common/Function/Function.h>
#include <Common/Math/Angle.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/Time/Stopwatch.h>

namespace ngine::Input
{
	struct Gesture3DMovementAction final : public Action
	{
		Function<void(const ScreenCoordinate), 24> OnStartRotateCameraPlane{[](const ScreenCoordinate)
		                                                                    {
																																				}};
		Function<void(const Math::Anglef, const Math::RotationalSpeedf), 24> OnRotateCameraPlane{
			[](const Math::Anglef, const Math::RotationalSpeedf)
			{
			}
		};
		Function<void(const Math::RotationalSpeedf velocity), 24> OnStopRotateCameraPlane{[](const Math::RotationalSpeedf)
		                                                                                  {
																																											}};
		Function<void(const ScreenCoordinate, const float scale), 24> OnStartPinch{[](const ScreenCoordinate, const float)
		                                                                           {
																																							 }};
		Function<void(const ScreenCoordinate, const float scale), 24> OnPinchMotion{[](const ScreenCoordinate, const float)
		                                                                            {
																																								}};
		Function<void(const ScreenCoordinate), 24> OnEndPinch{[](const ScreenCoordinate)
		                                                      {
																													}};
		Function<void(), 24> OnCancelPinch{[]()
		                                   {
																			 }};
		Function<void(const ScreenCoordinate), 24> OnStartPanCameraLocation{[](const ScreenCoordinate)
		                                                                    {
																																				}};
		Function<void(const ScreenCoordinate, const Math::Vector2i delta), 24> OnPanCameraLocation{
			[](const ScreenCoordinate, const Math::Vector2i)
			{
			}
		};
		Function<void(const Math::Vector2f velocity), 24> OnStopPanCameraLocation{[](const Math::Vector2f)
		                                                                          {
																																							}};
		Function<void(const Math::Vector2i delta), 24> OnRotateCamera{[](const Math::Vector2i)
		                                                              {
																																	}};
		Function<void(const Math::Vector2f velocity), 24> OnStopRotateCamera{[](const Math::Vector2f)
		                                                                     {
																																				 }};
		Function<void(const ScreenCoordinate), 24> OnStartOrbitCamera{[](const ScreenCoordinate)
		                                                              {
																																	}};
		Function<void(const Math::Vector2i delta), 24> OnOrbitCamera{[](const Math::Vector2i)
		                                                             {
																																 }};
		Function<void(const Math::Vector2f velocity), 24> OnStopOrbitCamera{[](const Math::Vector2f)
		                                                                    {
																																				}};

		inline static constexpr Time::Durationf GracePeriodDuration = 0.2_seconds;

		virtual void On2DSurfaceStartPinchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override
		{
			OnStartPinch(coordinate, scale);

			m_initialPinchScale = scale;
		}

		inline static constexpr float MaximumGracePeriodScaling = 0.1f;

		virtual void On2DSurfacePinchMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override
		{
			OnPinchMotion(coordinate, scale);
		}

		virtual void On2DSurfaceStopPinchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnEndPinch(coordinate);
		}

		virtual void On2DSurfaceCancelPinchInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnCancelPinch();
		}

		void OnPanFingerCountChanged(const uint8 fingerCount, const ScreenCoordinate coordinate)
		{
			m_switchGestureCoordinate = coordinate;
			m_lastPanFingerCount = fingerCount;
			switch (fingerCount)
			{
				case 1:
				{
					switch (m_state)
					{
						case State::PanningCameraLocation:
							break;
						case State::None:
						case State::OrbitingCameraGracePeriod:
						case State::OrbitingCamera:
						case State::RotatingCameraPlane:
						case State::RotatingCameraPlaneGracePeriod:
							SetState(State::PanningCameraLocation);
							OnStartPanCameraLocation(coordinate);
							break;
					}
				}
				break;
				case 2:
				{
					switch (m_state)
					{
						case State::OrbitingCamera:
						case State::OrbitingCameraGracePeriod:
							Assert(false);
							break;
						case State::None:
						case State::PanningCameraLocation:
						case State::RotatingCameraPlaneGracePeriod:
							SetState(State::OrbitingCameraGracePeriod);
							break;
						case State::RotatingCameraPlane:
							break;
					}
				}
				break;
			}
		}

		void OnPanFingerMotion(const ScreenCoordinate coordinate, const uint8 fingerCount)
		{
			if (m_previousPanCoordinate.IsInvalid())
			{
				m_previousPanCoordinate = coordinate;
			}

			if (m_lastPanFingerCount != fingerCount)
			{
				if (fingerCount < m_lastPanFingerCount)
				{
					m_previousPanCoordinate = coordinate;
				}

				OnPanFingerCountChanged(fingerCount, *m_previousPanCoordinate);
				return;
			}

			const Math::Vector2i deltaCoordinate = Math::Vector2i(coordinate) - Math::Vector2i(*m_previousPanCoordinate);
			m_previousPanCoordinate = coordinate;

			switch (m_state)
			{
				case State::None:
					break;
				case State::PanningCameraLocation:
					OnPanCameraLocation(coordinate, deltaCoordinate);
					break;
				case State::OrbitingCameraGracePeriod:
				{
					if (m_stateChangeStopwatch.GetElapsedTime() > GracePeriodDuration)
					{
						SetState(State::OrbitingCamera);
						OnStartOrbitCamera(*m_switchGestureCoordinate);
					}
				}
				break;
				case State::OrbitingCamera:
					OnOrbitCamera(deltaCoordinate);
					break;
				case State::RotatingCameraPlane:
				case State::RotatingCameraPlaneGracePeriod:
					break;
			}
		}

		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 radius
		) override
		{
			m_previousPanCoordinate = Invalid;
			OnPanFingerCountChanged(fingers.GetSize(), coordinate);
		}

		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			OnPanFingerMotion(coordinate, fingers.GetSize());
		}

		virtual void On2DSurfaceStopPanInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity
		) override
		{
			OnStopPan(velocity);
		}

		virtual void On2DSurfaceCancelPanInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnEndPan();
		}

		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2i deltaCoordinate
		) override
		{
			OnPanFingerMotion(coordinate, 1);
		}

		virtual void On2DSurfaceStartPressInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate coordinate, [[maybe_unused]] const uint8 numRepeats
		) override
		{
			OnPanFingerCountChanged(1, coordinate);
		}

		virtual void On2DSurfaceStopPressInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const uint8 numRepeats
		) override
		{
			OnStopPan(Math::Zero);
		}

		virtual void On2DSurfaceCancelPressInput(const DeviceIdentifier, const InputIdentifier) override
		{
			OnEndPan();
		}

		void OnStopPan(const Math::Vector2f velocity)
		{
			switch (m_state)
			{
				case State::PanningCameraLocation:
					OnStopPanCameraLocation(velocity);
					break;
				case State::OrbitingCameraGracePeriod:
				case State::OrbitingCamera:
					OnStopOrbitCamera(velocity);
					break;
				case State::None:
				case State::RotatingCameraPlane:
				case State::RotatingCameraPlaneGracePeriod:
					break;
			}

			/*if (const Optional<ViewportCameraController*> pViewportCameraController =
			m_viewportWidget.GetSceneView().GetActiveCameraComponent().FindDataComponentOfType<ViewportCameraController>())
			{
			  const Math::Vector2f deltaRatio = (Math::Vector2f)velocity;

			  switch(m_lastPanFingerCount)
			  {
			    case 1:
			    pViewportCameraController->m_panVelocity.x = -deltaRatio.x;
			    pViewportCameraController->m_panVelocity.y = deltaRatio.y;
			    pViewportCameraController->m_panVelocity = Math::Zero;
			    break;
			    case 2:
			    pViewportCameraController->m_rotationVelocity = deltaRatio;
			    break;
			    case 3:
			    pViewportCameraController->m_orbitRotationVelocity = deltaRatio;
			    break;
			  }
			}*/

			m_lastPanFingerCount = 0;

			OnEndPan();
		}

		void OnEndPan()
		{
			m_switchGestureCoordinate = Invalid;
			m_lastPanFingerCount = 0;
			switch (m_state)
			{
				case State::PanningCameraLocation:
				case State::OrbitingCameraGracePeriod:
				case State::OrbitingCamera:
					SetState(State::None);
					break;
				case State::None:
				case State::RotatingCameraPlane:
				case State::RotatingCameraPlaneGracePeriod:
					break;
			}
		}

		inline static constexpr bool EnableRotationAroundPlane = false;

		virtual void On2DSurfaceStartRotateInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			if constexpr (EnableRotationAroundPlane)
			{
				if (m_switchGestureCoordinate.IsValid())
				{
					OnStartRotateCameraPlane(*m_switchGestureCoordinate);
				}
				else
				{
					OnStartRotateCameraPlane(coordinate);
				}

				switch (m_state)
				{
					case State::None:
					case State::PanningCameraLocation:
						SetState(State::RotatingCameraPlaneGracePeriod);
						break;
					case State::OrbitingCameraGracePeriod:
					case State::OrbitingCamera:
						break;
					case State::RotatingCameraPlane:
					case State::RotatingCameraPlaneGracePeriod:
						Assert(false);
						break;
				}
			}
		}

		inline static constexpr Math::Anglef MaximumGracePeriodRotationAngle = 5_degrees;

		virtual void On2DSurfaceRotateMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef angle,
			const Math::RotationalSpeedf velocity
		) override
		{
			if constexpr (EnableRotationAroundPlane)
			{
				switch (m_state)
				{
					case State::None:
					case State::PanningCameraLocation:
					case State::OrbitingCamera:
						break;
					case State::OrbitingCameraGracePeriod:
					{
						if (angle > MaximumGracePeriodRotationAngle)
						{
							SetState(State::RotatingCameraPlane);
						}
					}
					break;
					case State::RotatingCameraPlaneGracePeriod:
					{
						if (m_stateChangeStopwatch.GetElapsedTime() > GracePeriodDuration || angle > MaximumGracePeriodRotationAngle)
						{
							SetState(State::RotatingCameraPlane);
						}
					}
					break;
					case State::RotatingCameraPlane:
						OnRotateCameraPlane(angle, velocity);
						break;
				}
			}
		}

		virtual void On2DSurfaceStopRotateInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef,
			const Math::RotationalSpeedf velocity
		) override
		{
			if constexpr (EnableRotationAroundPlane)
			{
				switch (m_state)
				{
					case State::None:
					case State::PanningCameraLocation:
					case State::OrbitingCameraGracePeriod:
					case State::OrbitingCamera:
						break;
					case State::RotatingCameraPlane:
					case State::RotatingCameraPlaneGracePeriod:
						SetState(State::None);
						OnStopRotateCameraPlane(velocity);
						break;
				}
			}
		}

		virtual void On2DSurfaceCancelRotateInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			if constexpr (EnableRotationAroundPlane)
			{
				switch (m_state)
				{
					case State::None:
					case State::PanningCameraLocation:
					case State::OrbitingCameraGracePeriod:
					case State::OrbitingCamera:
						break;
					case State::RotatingCameraPlane:
					case State::RotatingCameraPlaneGracePeriod:
						SetState(State::None);
						break;
				}
			}
		}

		enum class State : uint8
		{
			None,
			PanningCameraLocation,
			//! State where we can rotate the camera, but also allow switching to RotatingCameraPlane within this grace period
			OrbitingCameraGracePeriod,
			OrbitingCamera,
			//! State where we can rotate the camera around a point, but also allow switching to OrbitingCamera within this grace
			//! period
			RotatingCameraPlaneGracePeriod,
			RotatingCameraPlane
		};

		void SetState(const State state)
		{
			m_state = state;
			m_stateChangeStopwatch.Restart();
		}
	protected:
		uint8 m_lastPanFingerCount = 0;
		Optional<ScreenCoordinate> m_previousPanCoordinate = Invalid;
		Optional<ScreenCoordinate> m_switchGestureCoordinate = Invalid;

		State m_state{State::None};
		Time::Stopwatch m_stateChangeStopwatch;
		float m_initialPinchScale = 1.f;
	};
}
