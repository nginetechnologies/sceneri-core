#include "Common/Math/Min.h"
#include <GameFramework/Components/Camera/FirstPersonCameraController.h>

#include <Backend/Plugin.h>

#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/InputComponent.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/InputIdentifier.h>

#include <Common/Math/Damping.h>
#include <Common/Math/MultiplicativeInverse.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector2/Exponential.h>
#include <Common/Math/Vector2/Min.h>
#include <Common/Math/Vector2/Abs.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/Random.h>
#include <Common/IO/Log.h>

namespace
{
	using namespace ngine::Literals;

	constexpr ngine::Guid MoveCameraActionGuid = "53DAE2CE-41C1-4524-A35F-2ADF812201D1"_guid;
	constexpr ngine::Guid MouseLookActionGuid = "467965DA-1724-4522-BE85-B936D5030887"_guid;

	constexpr ngine::ConstStringView SettingCameraInvertXKey = "settings_camera_invertX";
	constexpr ngine::ConstStringView SettingCameraInvertYKey = "settings_camera_invertY";
	constexpr ngine::ConstStringView SettingMouseLookSensitivityKey = "settings_mouselook_sensitivity";
	constexpr ngine::ConstStringView SettingGamepadSensitivityKey = "settings_gamepad_sensitivity";
}

namespace ngine::GameFramework::Camera
{
	FirstPerson::FirstPerson(const Deserializer& deserializer)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(deserializer.GetParent()))
	{
		LogWarningIf(
			!deserializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		m_pCameraComponent->SetController(*this);
	}

	FirstPerson::FirstPerson(const FirstPerson& templateComponent, const Cloner& cloner)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(cloner.GetParent()))
		, m_mouseSensitivity{templateComponent.m_mouseSensitivity}
		, m_flags{templateComponent.m_flags}
	{
		LogWarningIf(
			!cloner.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		m_pCameraComponent->SetController(*this);
	}

	FirstPerson::FirstPerson(Initializer&& initializer)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(initializer.GetParent()))
	{
		LogWarningIf(
			!initializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		m_pCameraComponent->SetController(*this);
	}

	void FirstPerson::OnCreated()
	{
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::PersistentUserStorage& persistentUserStorage = backend.GetGame().GetPersistentUserStorage();

		m_flags.Set(Flags::CameraInvertX, persistentUserStorage.Find(SettingCameraInvertXKey).ToBool());
		m_flags.Set(Flags::CameraInvertY, persistentUserStorage.Find(SettingCameraInvertYKey).ToBool());

		ConstUnicodeStringView mouseSensitivitySetting = persistentUserStorage.Find(SettingMouseLookSensitivityKey);
		if (!mouseSensitivitySetting.IsEmpty())
		{
			SetMouseSensitivity(Math::Vector2f{mouseSensitivitySetting.ToFloat()});
		}

		ConstUnicodeStringView gamepadSensitivitySetting = persistentUserStorage.Find(SettingGamepadSensitivityKey);
		if (!gamepadSensitivitySetting.IsEmpty())
		{
			SetGamepadSensitivity(Math::Vector2f{gamepadSensitivitySetting.ToFloat(), gamepadSensitivitySetting.ToFloat() * 0.5f});
		}

		persistentUserStorage.OnDataChanged.Add(
			this,
			[](FirstPerson& cameraController, ConstStringView key, ConstUnicodeStringView value)
			{
				if (key == SettingCameraInvertXKey)
				{
					cameraController.m_flags.Set(Flags::CameraInvertX, value.ToBool());
				}
				else if (key == SettingCameraInvertYKey)
				{
					cameraController.m_flags.Set(Flags::CameraInvertY, value.ToBool());
				}
				else if (key == SettingMouseLookSensitivityKey)
				{
					if (!value.IsEmpty())
					{
						cameraController.SetMouseSensitivity(Math::Vector2f{value.ToFloat()});
					}
				}
				else if (key == SettingGamepadSensitivityKey)
				{
					if (!value.IsEmpty())
					{
						cameraController.SetGamepadSensitivity(Math::Vector2f{value.ToFloat(), value.ToFloat() * 0.75f});
					}
				}
			}
		);
	}

	void FirstPerson::OnParentCreated(Entity::Component3D& parent)
	{
		if (Optional<Entity::InputComponent*> pInputComponent = parent.FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			pInputComponent->OnAssignedActionMonitor.Add(*this, &FirstPerson::OnInputAssigned);
			pInputComponent->OnInputEnabled.Add(*this, &FirstPerson::OnInputAssigned);
			pInputComponent->OnInputDisabled.Add(*this, &FirstPerson::OnInputDisabled);
		}
	}

	void FirstPerson::OnDestroying()
	{
		Entity::ComponentTypeSceneData<FirstPerson>& sceneData =
			*m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<FirstPerson>();
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);

		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::PersistentUserStorage& persistentUserStorage = backend.GetGame().GetPersistentUserStorage();
		persistentUserStorage.OnDataChanged.Remove(this);

		if (Optional<Entity::InputComponent*> pInputComponent = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			pInputComponent->OnAssignedActionMonitor.Remove(this);
			pInputComponent->OnInputEnabled.Remove(this);
			pInputComponent->OnInputDisabled.Remove(this);
		}
		if (m_pCameraComponent != nullptr)
		{
			m_pCameraComponent->RemoveController(*this);
		}
	}

	void FirstPerson::OnEnable()
	{
		if (Optional<Entity::InputComponent*> pInputComponent = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				Entity::ComponentTypeSceneData<FirstPerson>& sceneData =
					*m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<FirstPerson>();
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}
		}
	}

	void FirstPerson::OnDisable()
	{
		Entity::ComponentTypeSceneData<FirstPerson>& sceneData =
			*m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<FirstPerson>();
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void FirstPerson::BeforePhysicsUpdate()
	{
		if (Optional<Entity::InputComponent*> pInputComponent = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				const Math::Vector3f cameraInput = pActionMap->GetAxisActionState(m_moveCameraAction);
				const Math::Vector2f inversionSettings{
					m_flags.IsSet(Flags::CameraInvertX) ? -1.f : 1.f,
					m_flags.IsSet(Flags::CameraInvertY) ? 1.f : -1.f,
				};
				m_inputAngles += Math::Vector2f{cameraInput.x, cameraInput.y} * inversionSettings;
			}
		}
	}

	void FirstPerson::AfterPhysicsUpdate()
	{
		if (m_pCameraComponent != nullptr)
		{
			const FrameTime frameTime = Math::Min(m_pCameraComponent->GetCurrentFrameTime(), FrameTime{1_seconds});

			m_targetAngles -= m_mouseInput * m_mouseSensitivity;
			m_mouseInput = Math::Zero;

			const Math::Vector2f gamepadAnglesDelta = (m_inputAngles * Math::Abs(m_inputAngles)) *
			                                          Math::Min(m_gamepadSensitivity * frameTime, Math::Vector2f{1.f});
			m_inputAngles = Math::Zero;

			Math::Anglef targetPitchAngle = Math::Clamp<Math::Anglef>(
				Math::Anglef::FromRadians(m_targetAngles.y),
				Math::Anglef::FromDegrees(m_minPitchAngle),
				Math::Anglef::FromDegrees(m_maxPitchAngle)
			);
			m_targetAngles.y = targetPitchAngle.GetRadians();

			m_currentAngles = Math::LinearInterpolate(m_currentAngles, m_targetAngles, Math::Min(m_interpolationSpeed * frameTime, 1.0f));

			m_targetAngles -= gamepadAnglesDelta;
			m_currentAngles -= gamepadAnglesDelta;
			targetPitchAngle = Math::Clamp<Math::Anglef>(
				Math::Anglef::FromRadians(m_currentAngles.y),
				Math::Anglef::FromDegrees(m_minPitchAngle),
				Math::Anglef::FromDegrees(m_maxPitchAngle)
			);
			m_currentAngles.y = targetPitchAngle.GetRadians();

			const Math::Anglef yawAngle = Math::Anglef::FromRadians(m_currentAngles.x);
			const Math::Anglef pitchAngle = Math::Anglef::FromRadians(m_currentAngles.y);
			const Math::Quaternionf yawRotation = {Math::CreateRotationAroundAxis, yawAngle, Math::Up};
			const Math::Quaternionf pitchRotation = {Math::CreateRotationAroundAxis, pitchAngle, Math::Right};

			m_pCameraComponent->GetParent().SetRelativeRotation(yawRotation);
			m_pCameraComponent->SetRelativeRotation(pitchRotation);
		}
	}

	void FirstPerson::OnInputAssigned(Input::ActionMonitor& /*actionMonitor*/)
	{
		if (Optional<Entity::InputComponent*> pInputComponent = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				m_moveCameraAction = pActionMap->BindAxisAction(
					MoveCameraActionGuid,
					[](const Math::Vector3f)
					{
					}
				);

				m_mouseLookAction = pActionMap->BindDeltaAction(
					MouseLookActionGuid,
					[this](const Math::Vector2f deltaCoordinate)
					{
						const Math::Vector2f inversionSettings{
							m_flags.IsSet(Flags::CameraInvertX) ? -1.f : 1.f,
							m_flags.IsSet(Flags::CameraInvertY) ? -1.f : 1.f,
						};

						m_mouseInput += deltaCoordinate * inversionSettings;
					}
				);

				Entity::ComponentTypeSceneData<FirstPerson>& sceneData =
					*m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<FirstPerson>();
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}
		}
	}

	void FirstPerson::OnInputDisabled(Input::ActionMonitor& /*actionMonitor*/)
	{
		if (Optional<Entity::InputComponent*> pInputComponent = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Entity::InputComponent>())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				pActionMap->UnbindAxisAction(m_moveCameraAction);
				pActionMap->UnbindDeltaAction(m_mouseLookAction);
			}
		}
	}

	void FirstPerson::Shake(const Math::Vector2f& horizontalLimits, const Math::Vector2f& verticalLimits)
	{
		const float randomHorizontal = Math::Random(horizontalLimits.x, horizontalLimits.y);
		const float randomVertical = Math::Random(verticalLimits.x, verticalLimits.y);
		m_targetAngles += Math::Vector2f{randomHorizontal, randomVertical};
	}

	void FirstPerson::SetMouseSensitivity(const Math::Vector2f sensitivity)
	{
		LogWarningIf(sensitivity.IsZero(), "Cannot set mouse sensitivity to zero");
		if (!sensitivity.IsZero())
		{
			m_mouseSensitivity = sensitivity;
		}
	}

	void FirstPerson::SetGamepadSensitivity(const Math::Vector2f sensitivity)
	{
		LogWarningIf(sensitivity.IsZero(), "Cannot set mouse sensitivity to zero");
		if (!sensitivity.IsZero())
		{
			m_gamepadSensitivity = sensitivity;
		}
	}

	[[maybe_unused]] const bool wasFirstPersonCameraControllerTypeRegistered = Reflection::Registry::RegisterType<FirstPerson>();
	[[maybe_unused]] const bool wasFirstPersonCameraControllerComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FirstPerson>>::Make());
}
