#include "VirtualController/VirtualController.h"
#include "VirtualController/ControllerMappingDataComponent.h"

#include <Engine/Threading/JobManager.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include <Widgets/RootWidget.h>
#include <Widgets/WidgetFlags.h>
#include <Widgets/ToolWindow.h>

#include <Common/Memory/Optional.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Reflection/Registry.inl>
#include <Engine/Entity/HierarchyComponent.inl>

namespace ngine::GameFramework
{
	void VirtualController::Initialize(Widgets::Widget& parentWidget)
	{
		LoadWidgets(parentWidget);

		SetupController(parentWidget);

		ConnectEventCallbacks();

		m_touchAction.OnStartTouch.Bind(*this, &VirtualController::OnStartTouch);
		m_touchAction.OnMoveTouch.Bind(*this, &VirtualController::OnMoveTouch);
		m_touchAction.OnStopTouch.Bind(*this, &VirtualController::OnStopTouch);
		m_touchAction.OnCancelTouch.Bind(*this, &VirtualController::OnCancelTouch);
	}

	void VirtualController::Destroy()
	{
		if (IsValid())
		{
			m_touchAction.OnStartTouch.Unbind();
			m_touchAction.OnMoveTouch.Unbind();
			m_touchAction.OnStopTouch.Unbind();
			m_touchAction.OnCancelTouch.Unbind();

			DisconnectEventCallbacks();

			m_buttonInstances.Clear();
			m_axisInstances.Clear();
			m_moduleInstances.Clear();

			if (m_pGamepad)
			{
				UnhookTouchInput();

				m_pGamepad->Destroy(m_pGamepad->GetSceneRegistry());
				m_pGamepad = nullptr;
			}
			m_pGamepadActionMonitor = nullptr;

			// TODO: Unregister?
			m_deviceIdentifier = {};
		}
	}

	void VirtualController::Configure(const Input::ActionMonitor& actionMonitor)
	{
		if (m_pGamepadActionMonitor != &actionMonitor)
		{
			m_pGamepadActionMonitor = actionMonitor;
			SetupModuleMapping();
			if (m_shouldBeVisible)
			{
				Show();
			}
		}
	}

	void VirtualController::Show()
	{
		m_shouldBeVisible = true;
		if (IsValid())
		{
			m_pGamepad->MakeVisible();
		}
	}

	void VirtualController::Hide()
	{
		m_shouldBeVisible = false;
		if (IsValid())
		{
			m_pGamepad->Hide();
		}
	}

	bool VirtualController::IsValid() const
	{
		return m_pGamepad.IsValid() && m_deviceIdentifier.IsValid();
	}

	void VirtualController::OnStartTouch(
		Input::DeviceIdentifier,
		Input::FingerIdentifier fingerIdentifier,
		ScreenCoordinate screenCoordinate,
		Math::Ratiof pressureRatio,
		uint16 touchRadius
	)
	{
		Rendering::ToolWindow& window = *m_pGamepad->GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidget = window.GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			bool wasUsed{false};

			auto buttonIt = m_buttonInstances.Find(uintptr(pWidget));
			if (buttonIt != m_buttonInstances.end())
			{
				OnButtonDown(buttonIt->second);
				wasUsed = true;
			}

			auto analogIt = m_analogInstances.Find(uintptr(pWidget));
			if (analogIt != m_analogInstances.end())
			{
				OnAnalogInput(analogIt->second, pressureRatio);
				wasUsed = true;
			}

			auto axisIt = m_axisInstances.Find(uintptr(pWidget));
			if (axisIt != m_axisInstances.end())
			{
				OnAxisInput(axisIt->second, *pWidget, screenCoordinate);
				wasUsed = true;
			}

			if (wasUsed)
			{
				m_activeWidgets.Emplace(fingerIdentifier, uintptr(pWidget));
			}
		}
	}

	void VirtualController::OnMoveTouch(
		Input::DeviceIdentifier,
		Input::FingerIdentifier fingerIdentifier,
		ScreenCoordinate screenCoordinate,
		Math::Vector2i,
		Math::Ratiof pressureRatio,
		uint16 touchRadius
	)
	{
		Widgets::Widget* pWidget{nullptr};
		auto widgetIt = m_activeWidgets.Find(fingerIdentifier);
		if (widgetIt != m_activeWidgets.end())
		{
			pWidget = (Widgets::Widget*)widgetIt->second;
		}
		else
		{
			Rendering::ToolWindow& window = *m_pGamepad->GetOwningWindow();
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			pWidget = window.GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius);
			m_activeWidgets.Emplace(fingerIdentifier, uintptr(pWidget));
		}

		if (pWidget)
		{
			auto analogIt = m_analogInstances.Find(uintptr(pWidget));
			if (analogIt != m_analogInstances.end())
			{
				OnAnalogInput(analogIt->second, pressureRatio);
			}

			auto axisIt = m_axisInstances.Find(uintptr(pWidget));
			if (axisIt != m_axisInstances.end())
			{
				OnAxisInput(axisIt->second, *pWidget, screenCoordinate);
			}
		}
	}

	void VirtualController::OnStopTouch(
		Input::DeviceIdentifier, Input::FingerIdentifier fingerIdentifier, ScreenCoordinate screenCoordinate, Math::Ratiof, uint16 touchRadius
	)
	{
		Widgets::Widget* pWidget{nullptr};
		auto widgetIt = m_activeWidgets.Find(fingerIdentifier);
		if (widgetIt != m_activeWidgets.end())
		{
			pWidget = (Widgets::Widget*)widgetIt->second;
			m_activeWidgets.Remove(widgetIt);
		}
		else
		{
			Rendering::ToolWindow& window = *m_pGamepad->GetOwningWindow();
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			pWidget = window.GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius);
		}

		if (pWidget)
		{
			auto buttonIt = m_buttonInstances.Find(uintptr(pWidget));
			if (buttonIt != m_buttonInstances.end())
			{
				OnButtonUp(buttonIt->second);
			}

			auto analogIt = m_analogInstances.Find(uintptr(pWidget));
			if (analogIt != m_analogInstances.end())
			{
				OnAnalogInput(analogIt->second, 0.f);
			}

			auto axisIt = m_axisInstances.Find(uintptr(pWidget));
			if (axisIt != m_axisInstances.end())
			{
				ResetInputAxis(axisIt->second);
			}
		}
	}

	void VirtualController::OnCancelTouch(Input::DeviceIdentifier, const Input::FingerIdentifier fingerIdentifier)
	{
		Widgets::Widget* pWidget{nullptr};
		auto widgetIt = m_activeWidgets.Find(fingerIdentifier);
		if (widgetIt != m_activeWidgets.end())
		{
			pWidget = (Widgets::Widget*)widgetIt->second;
			m_activeWidgets.Remove(widgetIt);
		}

		if (pWidget)
		{
			auto buttonIt = m_buttonInstances.Find(uintptr(pWidget));
			if (buttonIt != m_buttonInstances.end())
			{
				OnButtonCancel(buttonIt->second);
			}

			auto analogIt = m_analogInstances.Find(uintptr(pWidget));
			if (analogIt != m_analogInstances.end())
			{
				OnAnalogInput(analogIt->second, 0.f);
			}

			auto axisIt = m_axisInstances.Find(uintptr(pWidget));
			if (axisIt != m_axisInstances.end())
			{
				ResetInputAxis(axisIt->second);
			}
		}
	}

	void VirtualController::SetupController(Widgets::Widget& parentWidget)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		m_deviceIdentifier = gamepadDeviceType.GetOrRegisterInstance(uintptr(this), inputManager, parentWidget.GetOwningWindow());
	}

	void VirtualController::OnButtonDown(Input::GamepadInput::Button button)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		gamepadDeviceType.OnButtonDown(m_deviceIdentifier, button);
	}

	void VirtualController::OnButtonUp(Input::GamepadInput::Button button)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		gamepadDeviceType.OnButtonUp(m_deviceIdentifier, button);
	}

	void VirtualController::OnButtonCancel(Input::GamepadInput::Button button)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		gamepadDeviceType.OnButtonCancel(m_deviceIdentifier, button);
	}

	void VirtualController::OnAnalogInput(Input::GamepadInput::Analog analogInput, float value)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		gamepadDeviceType.OnAnalogInput(m_deviceIdentifier, analogInput, value);
	}

	void VirtualController::OnAxisInput(Input::GamepadInput::Axis axis, Widgets::Widget& axisWidget, ScreenCoordinate coordinate)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		const Widgets::Widget* pPadWidget = axisWidget.HasChildren() ? &axisWidget : static_cast<Widgets::Widget*>(&axisWidget.GetParent());
		Widgets::Widget* pThumbstickWidget = axisWidget.HasChildren() ? static_cast<Widgets::Widget*>(&axisWidget.GetChild(0)) : &axisWidget;

		const Math::Vector2i padPosition = (Math::Vector2i)pPadWidget->GetPosition();
		const Math::Vector2i padSize = pPadWidget->GetSize();
		const Math::Vector2i padCenter = padPosition + (padSize >> 1);

		const Math::Vector2i thumbstickPosition = (Math::Vector2i)pThumbstickWidget->GetPosition();
		const Math::Vector2i thumbstickSize = pThumbstickWidget->GetSize();
		const Math::Vector2i thumbstickHalfSize = (thumbstickSize >> 1);

		const float radius = float(padSize.x - thumbstickSize.x) * 0.5f;

		Math::Vector2f axisValue(float(int32(coordinate.x) - padCenter.x), float(int32(coordinate.y) - padCenter.y));
		const float distance = axisValue.GetLength();
		const float scale = Math::Min(distance, radius) / distance;
		axisValue *= scale;

		const Math::Vector2i newPosition = padCenter + Math::Vector2i(int32(axisValue.x), int32(axisValue.y)) - thumbstickHalfSize;
		axisValue /= radius; // map to -1 : 1
		axisValue.y *= -1.f; // flip axis as up is expected to be positive

		gamepadDeviceType.OnAxisInput(m_deviceIdentifier, axis, axisValue);

		if (thumbstickPosition != newPosition)
		{
			pThumbstickWidget->Reposition((WindowCoordinate)newPosition);
		}
	}

	void VirtualController::ResetInputAxis(Input::GamepadInput::Axis axis)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		SetActiveMonitor(gamepadDeviceType);

		gamepadDeviceType.OnAxisInput(m_deviceIdentifier, axis, Math::Vector2f(Math::Zero));

		Widgets::Widget* pPadWidget = nullptr;
		Widgets::Widget* pThumbstickWidget = nullptr;
		for (auto axisInstance : m_axisInstances)
		{
			if (axisInstance.second == axis)
			{
				Widgets::Widget* pAxisWidget = reinterpret_cast<Widgets::Widget*>(axisInstance.first);
				pPadWidget = pAxisWidget->HasChildren() ? pAxisWidget : static_cast<Widgets::Widget*>(&pAxisWidget->GetParent());
				pThumbstickWidget = pAxisWidget->HasChildren() ? static_cast<Widgets::Widget*>(&pAxisWidget->GetChild(0)) : pAxisWidget;
				break;
			}
		}

		if (pPadWidget && pThumbstickWidget)
		{
			const Math::Vector2i padSize = pPadWidget->GetSize();
			const Math::Vector2i thumbstickSize = pThumbstickWidget->GetSize();
			const Math::Vector2i thumbstickPosition = (Math::Vector2i)pThumbstickWidget->GetPosition();
			const Math::Vector2i zeroPosition = (Math::Vector2i)pPadWidget->GetPosition() + (padSize >> 1) - (thumbstickSize >> 1);

			if (thumbstickPosition != zeroPosition)
			{
				pThumbstickWidget->Reposition((WindowCoordinate)zeroPosition);
			}
		}
	}

	void VirtualController::LoadWidgets(Widgets::Widget& parentWidget)
	{
		Threading::JobBatch
			jobBatch =
				Widgets::
					Widget::
						Deserialize(
							GamepadBaseWidgetAssetGuid,
							parentWidget.GetSceneRegistry(),
							parentWidget,
							[this](const Optional<Widgets::Widget*> pGamepadBase)
							{
								if (pGamepadBase)
								{
									pGamepadBase->Hide();

									m_pGamepad = pGamepadBase;
									HookTouchInput(static_cast<Input::ActionMonitor&>(*m_pGamepad->GetOwningWindow()->GetFocusedInputMonitor()));

									Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

									Optional<Widgets::Widget*> pModuleSlotLeft;
									Optional<Widgets::Widget*> pModuleSlotRight;
									for (Widgets::Widget& childWidget : pGamepadBase->GetChildren())
									{
										if (const Optional<Entity::Data::Tags*> pWidgetTags = childWidget.FindDataComponentOfType<Entity::Data::Tags>(childWidget.GetSceneRegistry()))
										{
											if (pWidgetTags->HasTag(tagRegistry.FindOrRegister(LeftModuleSlotTagGuid)))
											{
												pModuleSlotLeft = childWidget;
											}
											else if (pWidgetTags->HasTag(tagRegistry.FindOrRegister(RightModuleSlotTagGuid)))
											{
												pModuleSlotRight = childWidget;
											}
										}
									}

									Entity::SceneRegistry& sceneRegistry = pGamepadBase->GetSceneRegistry();

									Threading::JobBatch batch;

									const uint16 size = s_GamepadModuleMappings.GetSize();
									for (uint16 index = 0; index < size; ++index)
									{
										const GamepadModuleMapping& gamepadModuleMapping = s_GamepadModuleMappings[uint8(index)];

										Optional<Widgets::Widget*> pModuleSlot;
										if (gamepadModuleMapping.position == GamepadModulePosition::Left)
										{
											pModuleSlot = pModuleSlotLeft;
										}
										else if (gamepadModuleMapping.position == GamepadModulePosition::Right)
										{
											pModuleSlot = pModuleSlotRight;
										}
										else if (gamepadModuleMapping.position == GamepadModulePosition::RightExtension)
										{
											pModuleSlot = pModuleSlotRight;
										}

										if (pModuleSlot.IsInvalid())
										{
											continue;
										}

										Threading::JobBatch childJobBatch = Widgets::Widget::Deserialize(
											gamepadModuleMapping.assetGuid,
											sceneRegistry,
											pModuleSlot,
											[this, index, &sceneRegistry](const Optional<Widgets::Widget*> pGamepadWidget)
											{
												if (pGamepadWidget)
												{
													pGamepadWidget->Hide();
													m_moduleInstances.EmplaceBack(GamepadModuleInstance{pGamepadWidget, index});

													pGamepadWidget->IterateDataComponentsOfTypeInChildrenRecursive<Data::ControllerMappingDataComponent>(
														sceneRegistry,
														[this, index](Entity::Component2D& owner, Data::ControllerMappingDataComponent& controllerMapping)
														{
															Vector<Input::GamepadInput::Button> buttons = controllerMapping.GetButtons();
															for (Input::GamepadInput::Button button : buttons)
															{
																m_buttonInstances.Emplace(uintptr(&owner), button);
															}

															if (controllerMapping.HasAxisInput())
															{
																Input::GamepadInput::Axis axis = s_GamepadModuleMappings[uint8(index)].position ==
									                                                   GamepadModulePosition::Left
									                                                 ? Input::GamepadInput::Axis::LeftThumbstick
									                                                 : Input::GamepadInput::Axis::RightThumbstick;
																m_axisInstances.Emplace(uintptr(&owner), axis);
															}

															if (controllerMapping.HasLeftTrigger())
															{
																m_analogInstances.Emplace(uintptr(&owner), Input::GamepadInput::Analog::LeftTrigger);
															}

															if (controllerMapping.HasRightTrigger())
															{
																m_analogInstances.Emplace(uintptr(&owner), Input::GamepadInput::Analog::RightTrigger);
															}

															return Memory::CallbackResult::Continue;
														}
													);
												}
											},
											Invalid
										);
										{
											batch.QueueAsNewFinishedStage(childJobBatch);
										}
									}

									Threading::Job& finishedJob = Threading::CreateCallback(
										[this](Threading::JobRunnerThread&) mutable
										{
											if (m_shouldBeVisible && m_pGamepadActionMonitor)
											{
												SetupModuleMapping();
												Show();
											}
										},
										Threading::JobPriority::UserInterfaceLoading
									);
									batch.QueueAsNewFinishedStage(finishedJob);

									System::Get<Threading::JobManager>().Queue(batch, Threading::JobPriority::UserInterfaceAction);
								}
							},
							Invalid
						);
		if (jobBatch.IsValid())
		{
			System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::UserInterfaceLoading);
		}
	}

	void VirtualController::EnableModule(GamepadModulePosition position, GamepadModuleType type)
	{
		for (GamepadModuleInstance& moduleInstance : m_moduleInstances)
		{
			const GamepadModuleMapping& mapping = s_GamepadModuleMappings[uint8(moduleInstance.mappingIndex)];
			if (mapping.position == position)
			{
				if (mapping.type == type)
				{
					if (moduleInstance.pWidget)
					{
						moduleInstance.pWidget->MakeVisible();
					}
				}
			}
		}
	}

	void VirtualController::DisableModules()
	{
		for (GamepadModuleInstance& moduleInstance : m_moduleInstances)
		{
			if (moduleInstance.pWidget)
			{
				moduleInstance.pWidget->Hide();
			}
		}
	}

	void VirtualController::HookTouchInput(Input::ActionMonitor& actionMonitor)
	{
		m_pTouchActionMonitor = actionMonitor;

		Input::Manager& inputManager = System::Get<Input::Manager>();

		Input::TouchscreenDeviceType& touchscreenDeviceType =
			inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
		Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier()
		);

		m_touchAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
		m_touchAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
		m_touchAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetTapInputIdentifier());
		m_touchAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetLongPressInputIdentifier());
		m_touchAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetStartTouchInputIdentifier());
		m_touchAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetStopTouchInputIdentifier());
		m_touchAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetMotionInputIdentifier());
	}

	void VirtualController::UnhookTouchInput()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();

		Input::TouchscreenDeviceType& touchscreenDeviceType =
			inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
		Input::MouseDeviceType& mouseDeviceType = inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier()
		);

		if (m_pTouchActionMonitor.IsValid())
		{
			Input::ActionMonitor& actionMonitor = const_cast<Input::ActionMonitor&>(*m_pTouchActionMonitor);
			m_touchAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_touchAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_touchAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetTapInputIdentifier());
			m_touchAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetLongPressInputIdentifier());
			m_touchAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetStartTouchInputIdentifier());
			m_touchAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetStopTouchInputIdentifier());
			m_touchAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetMotionInputIdentifier());

			m_pTouchActionMonitor = nullptr;
		}
	}

	void VirtualController::UpdateTouchInputHook(Input::Monitor& monitor)
	{
		UnhookTouchInput();
		HookTouchInput(static_cast<Input::ActionMonitor&>(monitor));
	}

	void VirtualController::ConnectEventCallbacks()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.OnMonitorAssignedEnd.Add(
			*this,
			[](VirtualController& controller, const Input::ActionMonitor& actionMonitor)
			{
				controller.Show();
				controller.Configure(actionMonitor);
			}
		);

		gamepadDeviceType.OnInputEnabledEvent.Add(
			*this,
			[](VirtualController& controller, const Input::ActionMonitor& actionMonitor)
			{
				controller.Show();
				controller.Configure(actionMonitor);
			}
		);

		gamepadDeviceType.OnInputDisabledEvent.Add(
			*this,
			[](VirtualController& controller, const Input::ActionMonitor&)
			{
				controller.Hide();
			}
		);

		Input::TouchscreenDeviceType& touchscreenDeviceType =
			inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

		touchscreenDeviceType.OnMonitorChanged.Add(*this, &VirtualController::UpdateTouchInputHook);
	}

	void VirtualController::DisconnectEventCallbacks()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();

		Input::TouchscreenDeviceType& touchscreenDeviceType =
			inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
		touchscreenDeviceType.OnMonitorChanged.Remove(this);

		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.OnMonitorAssignedEnd.Remove(this);
		gamepadDeviceType.OnInputEnabledEvent.Remove(this);
		gamepadDeviceType.OnInputDisabledEvent.Remove(this);
	}

	void VirtualController::SetupModuleMapping()
	{
		Input::ActionMonitor::BoundInputActions::ConstRestrictedView boundInputActions = m_pGamepadActionMonitor->GetBoundActions();
		auto isActionBound = [boundInputActions](Input::InputIdentifier input)
		{
			return boundInputActions[input].GetSize() > 0;
		};

		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		DisableModules();

		// Left
		if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Axis::LeftThumbstick)))
		{
			EnableModule(GamepadModulePosition::Left, GamepadModuleType::Joystick);
			ResetInputAxis(Input::GamepadInput::Axis::LeftThumbstick);
		}
		else if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadLeft)) ||
				isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadRight)))
		{
			if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadUp)) ||
					isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadDown)))
			{
				EnableModule(GamepadModulePosition::Left, GamepadModuleType::Arrows);
			}
			else
			{
				EnableModule(GamepadModulePosition::Left, GamepadModuleType::LeftRight);
			}
		}
		else if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadUp)) ||
					isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::DirectionPadDown)))
		{
			EnableModule(GamepadModulePosition::Left, GamepadModuleType::Arrows);
		}

		// Right
		if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Axis::RightThumbstick)))
		{
			EnableModule(GamepadModulePosition::Right, GamepadModuleType::Joystick);
			ResetInputAxis(Input::GamepadInput::Axis::RightThumbstick);

			if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::A)))
			{
				if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::B)))
				{
					EnableModule(GamepadModulePosition::RightExtension, GamepadModuleType::ExtensionButtonAB);
				}
				else
				{
					EnableModule(GamepadModulePosition::RightExtension, GamepadModuleType::ExtensionButtonA);
				}
			}
			else if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::B)))
			{
				Assert(false, "Showing A when not used!");
				EnableModule(GamepadModulePosition::RightExtension, GamepadModuleType::ExtensionButtonA);
			}
		}
		else
		{
			if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::LeftTrigger)) || isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::RightTrigger)))
			{
				EnableModule(GamepadModulePosition::Right, GamepadModuleType::Analog);
			}
			else
			{
				if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::A)))
				{
					if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::B)))
					{
						if (isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::X)) || isActionBound(gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::Y)))
						{
							EnableModule(GamepadModulePosition::Right, GamepadModuleType::ButtonABXY);
						}
						else
						{
							EnableModule(GamepadModulePosition::Right, GamepadModuleType::ButtonAB);
						}
					}
					else
					{
						EnableModule(GamepadModulePosition::Right, GamepadModuleType::Button);
					}
				}
			}
		}
	}

	void VirtualController::SetActiveMonitor(Input::GamepadDeviceType& gamepadDeviceType)
	{
		// touch screen input resets the active monitor to the one that controls touch (e.g. ToolWindow)
		Input::Gamepad& gamepad = gamepadDeviceType.GetGamepad(m_deviceIdentifier);
		if (gamepad.GetActiveMonitor().Get() != m_pGamepadActionMonitor.Get())
		{
			gamepad.SetActiveMonitor(const_cast<Input::ActionMonitor&>(*m_pGamepadActionMonitor), gamepadDeviceType);
		}
	}
}
