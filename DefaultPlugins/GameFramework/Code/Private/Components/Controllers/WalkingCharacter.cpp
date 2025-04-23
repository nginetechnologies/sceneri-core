#include "Components/Controllers/WalkingCharacter.h"
#include "Components/Camera/ThirdPersonCameraController.h"
#include "Components/AnimationControllers/ThirdPersonAnimationController.h"
#include "Components/AnimationControllers/FirstPersonAnimationController.h"
#include "Components/Camera/FirstPersonCameraController.h"
#include "Components/Camera/ThirdPersonCameraController.h"
#include "Components/Camera/PlanarCameraController.h"

#include "Engine/Input/ActionMap.h"
#include "Engine/Input/Actions/ActionMonitor.h"
#include "Tags.h"

#include <PhysicsCore/Components/CharacterBase.h>
#include <PhysicsCore/Components/CharacterComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>

#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/SpringComponent.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include <Animation/Animation.h>
#include <Animation/AnimationCache.h>
#include <Animation/Components/SkeletonComponent.h>
#include <Animation/Plugin.h>
#include <Animation/SamplingCache.h>

#include <Engine/Entity/InputComponent.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>

#include "Engine/Entity/ComponentType.h"
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/Mod.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Array.h>

#include <PhysicsCore/BroadPhaseLayer.h>
#include <Physics/Collision/RayCast.h>
#include "PhysicsCore/Layer.h"

#include <NetworkingCore/Components/BoundComponent.h>

#include <GameFramework/Components/Items/ProjectileWeapon.h>
#include <GameFramework/Components/Items/FirearmProperties.h>
#include <GameFramework/Components/Camera/FirstPersonCameraController.h>
#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/Player/Ammunition.h>
#include <Backend/Plugin.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAsset.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>

#include <Common/Math/NumericLimits.h>
#include <Common/Math/Rotation2D.h>
#include <Common/Math/Vector2/Sign.h>
#include <Common/Math/Vector2/Abs.h>

namespace
{
	using namespace ngine::Literals;

	constexpr ngine::Guid JumpActionGuid = "D2B5D801-01EE-43B4-A8BB-77797A1C8214"_guid;
	constexpr ngine::Guid SprintActionGuid = "9D3B1AE9-4757-4029-B23E-A564D0CC6D4B"_guid;
	constexpr ngine::Guid MoveActionGuid = "49899D2E-E735-464A-8D5C-3CD526AA8974"_guid;
	constexpr ngine::Guid MoveCameraActionGuid = "53DAE2CE-41C1-4524-A35F-2ADF812201D1"_guid;
	constexpr ngine::Guid MouseLookActionGuid = "467965DA-1724-4522-BE85-B936D5030887"_guid;
	constexpr ngine::Guid AttackActionGuid = "3888f0f7-8186-4a64-ba99-f00ba8087dab"_guid;
	constexpr ngine::Guid SecondaryActionGuid = "7d4b1398-7348-418e-bb0d-29889c69fd2e"_guid;
	constexpr ngine::Guid ReloadActionGuid = "5DF8BC49-6D19-4718-8B92-B820564EEB0E"_guid;
}

namespace ngine::GameFramework::Components
{
	WalkingCharacterBase::WalkingCharacterBase(const WalkingCharacterBase& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_jumpHeight(templateComponent.m_jumpHeight)
		, m_walkingSpeed(templateComponent.m_walkingSpeed)
		, m_runningSpeed(templateComponent.m_runningSpeed)
		, m_walkingTurnSpeed(templateComponent.m_walkingTurnSpeed)
		, m_runningTurnSpeed(templateComponent.m_runningTurnSpeed)
		, m_flags(templateComponent.m_flags)
		, m_footStepDelay(templateComponent.m_footStepDelay)
	{
		if (const Optional<ProjectileWeapon*> pTemplateProjectileWeapon = templateComponent.FindDataComponentOfType<ProjectileWeapon>(cloner.GetTemplateSceneRegistry()))
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<ProjectileWeapon>(
				*pTemplateProjectileWeapon,
				ProjectileWeapon::Cloner{jobBatch, *this, templateComponent, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
			);
			Assert(jobBatch.IsInvalid());
		}

		if (templateComponent.GetFireAudioAsset().IsValid())
		{
			SetFireAudioAsset(templateComponent.GetFireAudioAsset());
		}
		if (templateComponent.GetReloadAudioAsset().IsValid())
		{
			SetReloadAudioAsset(templateComponent.GetReloadAudioAsset());
		}
		if (templateComponent.GetImpactAudioAsset().IsValid())
		{
			SetImpactAudioAsset(templateComponent.GetImpactAudioAsset());
		}
		if (templateComponent.GetAimInAudioAsset().IsValid())
		{
			SetAimInAudioAsset(templateComponent.GetAimInAudioAsset());
		}
		if (templateComponent.GetCasingAudioAsset().IsValid())
		{
			SetCasingAudioAsset(templateComponent.GetCasingAudioAsset());
		}
		if (templateComponent.GetEmptyFireAudioAsset().IsValid())
		{
			SetEmptyFireAudioAsset(templateComponent.GetEmptyFireAudioAsset());
		}
		if (templateComponent.GetJumpAudioAsset().IsValid())
		{
			SetJumpAudioAsset(templateComponent.GetJumpAudioAsset());
		}
		if (templateComponent.GetLandAudioAsset().IsValid())
		{
			SetLandAudioAsset(templateComponent.GetLandAudioAsset());
		}
		if (templateComponent.GetWalkAudioAsset().IsValid())
		{
			SetWalkAudioAsset(templateComponent.GetWalkAudioAsset());
		}
	}

	WalkingCharacterBase::~WalkingCharacterBase()
	{
	}

	void WalkingCharacterBase::OnEnable()
	{
		OnEnableInternal();
	}

	void WalkingCharacterBase::OnDisable()
	{
		OnDisableInternal();
	}

	void WalkingCharacterBase::OnDestroying()
	{
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::PersistentUserStorage& persistentUserStorage = backend.GetGame().GetPersistentUserStorage();
		persistentUserStorage.OnDataChanged.Remove(this);

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMonitor*> pActionMonitor = pInputComponent->GetMonitor())
			{
				OnInputDisabled(*pActionMonitor);
			}

			pInputComponent->OnAssignedActionMonitor.Remove(this);
			pInputComponent->OnInputEnabled.Remove(this);
			pInputComponent->OnInputDisabled.Remove(this);
		}

		OnDisable();
	}

	void WalkingCharacterBase::OnInputDisabled(Input::ActionMonitor& /* actionMonitor */)
	{
		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				pActionMap->UnbindBinaryAction(m_jumpAction);
				pActionMap->UnbindBinaryAction(m_sprintAction);
				pActionMap->UnbindAxisAction(m_moveAction);
				pActionMap->UnbindAxisAction(m_moveCameraAction);
				pActionMap->UnbindAxisAction(m_mouseLookCameraAction);
			}
		}
	}

	void WalkingCharacterBase::OnInputAssigned(Input::ActionMonitor& /* actionMonitor */)
	{
		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				m_jumpAction = pActionMap->BindBinaryAction(
					JumpActionGuid,
					[this](const bool inputValue)
					{
						OnJumpAction(inputValue);
					}
				);
				m_sprintAction = pActionMap->BindBinaryAction(
					SprintActionGuid,
					[](const bool)
					{
					}
				);
				m_moveAction = pActionMap->BindAxisAction(
					MoveActionGuid,
					[](const Math::Vector3f)
					{
					}
				);
				m_moveCameraAction = pActionMap->BindAxisAction(
					MoveCameraActionGuid,
					[](const Math::Vector3f)
					{
					}
				);
				m_mouseLookCameraAction = pActionMap->BindDeltaAction(
					MouseLookActionGuid,
					[this](const Math::Vector2f deltaCoordinate)
					{
						m_mouseInput += deltaCoordinate;
					}
				);
			}
		}
	}

	void WalkingCharacterBase::OnCreated()
	{
		m_pCameraComponent = GetParentSceneComponent()->FindFirstChildOfTypeRecursive<Entity::CameraComponent>().Get();
		m_pCharacterPhysics = FindFirstChildImplementingTypeRecursive<Physics::CharacterBase>().Get();

		if (LIKELY(m_pCharacterPhysics != nullptr))
		{

			// Add player tag
			Optional<Entity::Data::Tags*> pTagComponent = m_pCharacterPhysics->FindDataComponentOfType<Entity::Data::Tags>();
			if (pTagComponent.IsInvalid())
			{
				pTagComponent = m_pCharacterPhysics->CreateDataComponent<Entity::Data::Tags>(
					Entity::Data::Tags::Initializer{Entity::Data::Component3D::DynamicInitializer{*m_pCharacterPhysics, GetSceneRegistry()}}
				);
			}

			pTagComponent->SetTag(
				m_pCharacterPhysics->GetIdentifier(),
				GetSceneRegistry(),
				System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid)
			);
		}

		if (LIKELY(m_pCameraComponent != nullptr))
		{
			m_pThirdPersonCameraController = m_pCameraComponent->FindDataComponentOfType<Camera::ThirdPerson>().Get();
		}

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			// Backwards compatibility for projects before action maps were introduced
			if (pInputComponent->GetActionMapAsset().GetAssetGuid().IsInvalid())
			{
				pInputComponent->SetActionMapAsset({"b62903bc-05c2-4198-a113-026ac074bb83"_guid, "6ff5d6e8-62ab-4c26-aa4b-05765922a68a"_guid});
			}

			pInputComponent->OnAssignedActionMonitor.Add(*this, &WalkingCharacterBase::OnInputAssigned);
			pInputComponent->OnInputEnabled.Add(*this, &WalkingCharacterBase::OnInputAssigned);
			pInputComponent->OnInputDisabled.Add(*this, &WalkingCharacterBase::OnInputDisabled);
		}

		if (LIKELY(m_pCharacterPhysics != nullptr))
		{
			if (const Optional<ngine::Animation::SkeletonComponent*> pSkeletonComponent = m_pCharacterPhysics->FindFirstChildOfTypeRecursive<ngine::Animation::SkeletonComponent>())
			{
				m_pSkeletonComponent = pSkeletonComponent;
			}
		}

		OnCreatedInternal();

		if (const Optional<Networking::Backend::Plugin*> pBackend = System::FindPlugin<Networking::Backend::Plugin>())
		{
			Networking::Backend::PersistentUserStorage& persistentUserStorage = pBackend->GetGame().GetPersistentUserStorage();

			static constexpr ConstStringView SettingCameraInvertXKey = "settings_camera_invertX";
			static constexpr ConstStringView SettingCameraInvertYKey = "settings_camera_invertY";

			m_flags.Set(Flags::CameraInvertX, persistentUserStorage.Find(SettingCameraInvertXKey).ToBool());
			m_flags.Set(Flags::CameraInvertY, persistentUserStorage.Find(SettingCameraInvertYKey).ToBool());

			persistentUserStorage.OnDataChanged.Add(
				this,
				[](WalkingCharacterBase& walkingCharacter, ConstStringView key, ConstUnicodeStringView value)
				{
					if (key == SettingCameraInvertXKey)
					{
						walkingCharacter.m_flags.Set(Flags::CameraInvertX, value.ToBool());
					}
					else if (key == SettingCameraInvertYKey)
					{
						walkingCharacter.m_flags.Set(Flags::CameraInvertY, value.ToBool());
					}
				}
			);
		}
	}

	void WalkingCharacterBase::OnJumpAction(const bool releaseButton)
	{
		if (releaseButton && m_pCharacterPhysics != nullptr && m_pCharacterPhysics->IsOnGround())
		{
			if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
			{
				if (pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
				{
					pBoundComponent->BroadcastToAllRemotes<&WalkingCharacterBase::OnRemoteClientJumped>(
						*this,
						GetSceneRegistry(),
						Network::Channel{0},
						JumpMessageData{}
					);
				}
			}

			if (m_pJumpSpotComponent)
			{
				m_pJumpSpotComponent->Play();
			}

			// TODO: Figure out what gravity constant we should use
			const float desiredForce = Math::Sqrt(2.f * 9.81f * m_jumpHeight);
			m_pCharacterPhysics->Jump(GetWorldRotation().TransformDirection({0, 0, desiredForce}));
		}
	}

	void WalkingCharacterBase::OnRemoteClientJumped(Network::Session::BoundComponent&, Network::LocalPeer&, const JumpMessageData)
	{
		if (m_pCharacterPhysics != nullptr)
		{
			if (m_pJumpSpotComponent)
			{
				m_pJumpSpotComponent->Play();
			}

			// TODO: Figure out what gravity constant we should use
			const float desiredForce = Math::Sqrt(2.f * 9.81f * m_jumpHeight);
			m_pCharacterPhysics->Jump(GetWorldRotation().TransformDirection({0, 0, desiredForce}));
		}
	}

	Math::Vector3f WalkingCharacterBase::GetRequestedWalk() const
	{
		return m_latestLocalState.m_requestedWalk;
	}

	void WalkingCharacterBase::SetRequestedWalk(const Math::Vector3f requestedWalk)
	{
		m_latestRemoteState.m_requestedWalk = requestedWalk;
	}

	Math::Quaternionf WalkingCharacterBase::GetCameraRotation() const
	{
		return m_latestLocalState.m_cameraRotation;
	}

	void WalkingCharacterBase::SetCameraRotation(const Math::Quaternionf rotation)
	{
		m_latestRemoteState.m_cameraRotation = rotation;
	}

	Math::Quaternionf WalkingCharacterBase::GetBodyRotation() const
	{
		return m_latestLocalState.m_bodyRotation;
	}

	void WalkingCharacterBase::SetBodyRotation(const Math::Quaternionf rotation)
	{
		m_latestRemoteState.m_bodyRotation = rotation;
	}

	bool WalkingCharacterBase::GetIsSprinting() const
	{
		return m_latestLocalState.m_isSprinting;
	}

	void WalkingCharacterBase::SetIsSprinting(const bool isSprinting)
	{
		m_latestRemoteState.m_isSprinting = isSprinting;
	}

	WalkingCharacterBase::InputState WalkingCharacterBase::GetLocalInputState()
	{
		InputState newState{m_latestLocalState};

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				newState.m_requestedWalk = pActionMap->GetAxisActionState(m_moveAction);
				newState.m_isSprinting = pActionMap->GetBinaryActionState(m_sprintAction) ||
				                         (Math::Abs(newState.m_requestedWalk.x) >= m_sprintAnalogThreshold ||
				                          Math::Abs(newState.m_requestedWalk.y) >= m_sprintAnalogThreshold);
				const bool changed = !newState.m_requestedWalk.IsEquivalentTo(m_latestLocalState.m_requestedWalk, 0.005f) ||
				                     (newState.m_requestedWalk.IsZeroExact() != m_latestLocalState.m_requestedWalk.IsZeroExact()) ||
				                     newState.m_isSprinting != m_latestLocalState.m_isSprinting;
				m_latestLocalState.m_requestedWalk = newState.m_requestedWalk;
				m_latestLocalState.m_isSprinting = newState.m_isSprinting;

				if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
				{
					if (pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
					{
						if (const Optional<Network::LocalClient*> pLocalClient = pBoundComponent->GetLocalClient(*this, GetSceneRegistry()))
						{
							// TODO: Vector2 & Vector3 compression
							if (changed)
							{
								pBoundComponent->InvalidateProperties<&WalkingCharacterBase::GetRequestedWalk, &WalkingCharacterBase::GetIsSprinting>(
									*this,
									GetSceneRegistry()
								);
							}
						}
					}
				}
			}
		}

		return newState;
	}
	WalkingCharacterBase::InputState
	WalkingCharacterBase::GetInputState(const InputState localInputState, const Time::Timestamp currentTime) const
	{
		InputState inputState{localInputState};

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
				{
					if (pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
					{
						if (const Optional<Network::LocalClient*> pLocalClient = pBoundComponent->GetLocalClient(*this, GetSceneRegistry()))
						{
							const Time::Durationd roundTripTime = pLocalClient->GetRoundTripTime();

							// Keep simulating pressed input until the half round trip time, in order to get closer to the server state
							const Time::Timestamp serverInputTime = currentTime - roundTripTime * 0.5;

							const auto findInputStateBeforeOrAtTime = [this](const Time::Timestamp timestamp) -> InputState
							{
								if (m_inputCircularBuffer.HasElements())
								{
									// Start from the second most recent item, iterate backwards
									const ArrayView<const InputState, uint8> entries{m_inputCircularBuffer.GetView()};
									const uint8 startIndex = m_inputCircularBuffer.GetLastIndex();
									for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
									{
										const InputState* it = Math::Wrap(
											((const InputState*)entries.begin() + startIndex) - i,
											(const InputState*)entries.begin(),
											(const InputState*)entries.end() - 1
										);
										const InputState& entry = *it;
										if (entry.m_timestamp <= timestamp)
										{
											return entry;
										}
									}
								}
								InputState state{m_latestLocalState};
								state.m_timestamp = timestamp;
								return state;
							};
							const InputState serverInputState = findInputStateBeforeOrAtTime(serverInputTime);
							inputState = localInputState;
							inputState.m_isSprinting = localInputState.m_isSprinting | serverInputState.m_isSprinting;
							for (uint8 i = 0; i < 3; ++i)
							{
								if (Math::IsEquivalentTo(localInputState.m_requestedWalk[i], 0.f, 0.01f) && serverInputState.m_requestedWalk[i] != 0)
								{
									inputState.m_requestedWalk[i] = serverInputState.m_requestedWalk[i];
								}
							}
						}
					}
				}
			}

			const bool isRemotePlayer = pInputComponent->GetMonitor().IsInvalid();
			if (isRemotePlayer)
			{
				inputState = m_latestRemoteState;
				inputState.m_timestamp = currentTime;
			}
		}
		return inputState;
	}

	void WalkingCharacterBase::UpdateAnimationState(
		const InputState localInputState, const GroundState previousGroundState, const GroundState newGroundState, const FrameTime deltaTime
	)
	{
		const float speedScale = GetWorldScale().y;
		const float runningSpeed = m_runningSpeed * speedScale;
		const float walkingSpeed = m_walkingSpeed * speedScale;

		if (!localInputState.m_requestedWalk.IsZero())
		{
			const float localSpeed = (localInputState.m_isSprinting ? runningSpeed : walkingSpeed);

			const float movementMomentum = Math::LinearInterpolate(m_movementMomentum, localSpeed, Math::Min(deltaTime * 5.f, 1.f));
			m_movementMomentum = movementMomentum;
		}
		else
		{
			m_movementMomentum = Math::LinearInterpolate(m_movementMomentum, 0.f, Math::Min(deltaTime * 5.f, 1.f));
		}

		if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
		{
			auto getMovementSpeedRatio = [this, runningSpeed, walkingSpeed](const uint8 animationCount) -> float
			{
				if (LIKELY(animationCount > 1))
				{
					const float slice = Math::MultiplicativeInverse((float)animationCount - 1.f);

					const float currentSpeed = m_movementMomentum;

					const float walkingRatio = Math::Min(currentSpeed / walkingSpeed, 1.f);
					const float runningRatio = Math::Max(currentSpeed - walkingSpeed, 0.f) / (runningSpeed - walkingSpeed);

					return walkingRatio * slice + runningRatio * slice;
				}
				else
				{
					return 0;
				}
			};

			if (Optional<Animation::Controller::ThirdPerson*> pThirdPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::ThirdPerson>())
			{
				pThirdPersonAnimationController->SetMoveBlendRatio(getMovementSpeedRatio(pThirdPersonAnimationController->GetMoveBlendEntryCount())
				);
				pThirdPersonAnimationController->SetTimeInAir(m_timeInAir);
			}
			else if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
			{
				pFirstPersonAnimationController->SetMoveBlendRatio(getMovementSpeedRatio(pFirstPersonAnimationController->GetMoveBlendEntryCount())
				);
				pFirstPersonAnimationController->SetTimeInAir(m_timeInAir);
			}
		}

		if (previousGroundState == GroundState::InAir && newGroundState == GroundState::OnGround && m_pLandSpotComponent)
		{
			m_pLandSpotComponent->Play();
		}

		// TODO: This should be synced with animation - workaround for FPS demo
		const float velocity = m_pCharacterPhysics->GetLinearVelocity().GetLength();
		float timerDelay = m_footStepDelay - (velocity * 0.1f);
		if (velocity > 0.5 && newGroundState == GroundState::OnGround && m_footStepTime > timerDelay)
		{
			m_footStepTime = 0;
			if (m_pWalkSpotComponent)
			{
				m_pWalkSpotComponent->Play();
			}
		}
		m_footStepTime += deltaTime;
		// ~TODO
	}

	void WalkingCharacterBase::UpdateGroundState(const GroundState newGroundState, const FrameTime deltaTime)
	{
		switch (newGroundState)
		{
			case GroundState::InAir:
			{
				Time::Durationf timeInAir = m_timeInAir;
				timeInAir = Time::Durationf::FromSeconds(Math::Max(timeInAir.GetSeconds(), 0.f));
				timeInAir += Time::Durationf::FromSeconds(deltaTime);
				m_timeInAir = timeInAir;
			}
			break;
			case GroundState::OnGround:
			{
				Time::Durationf timeInAir = m_timeInAir;
				timeInAir = Time::Durationf::FromSeconds(Math::Min(timeInAir.GetSeconds(), 0.f));
				timeInAir -= Time::Durationf::FromSeconds(deltaTime);
				m_timeInAir = timeInAir;
			}
			break;
		}

		m_currentState = newGroundState;
	}

	void WalkingCharacterBase::FixedPhysicsUpdate()
	{
		const Optional<Physics::Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const Time::Timestamp physicsTickTime = pPhysicsScene->GetTickTime();

		InputState localInputState;

		if (pPhysicsScene->IsRollingBack())
		{
			const ArrayView<InputState, uint8> entries{m_inputCircularBuffer.GetView()};
			const uint8 startIndex = m_inputCircularBuffer.GetLastIndex();
			const Optional<InputState*> pInputState = [timestamp = physicsTickTime, entries, startIndex]() -> Optional<InputState*>
			{
				for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
				{
					InputState* it =
						Math::Wrap(((InputState*)entries.begin() + startIndex) - i, (InputState*)entries.begin(), (InputState*)entries.end() - 1);
					InputState& entry = *it;
					if (entry.m_timestamp <= timestamp)
					{
						return entry;
					}
				}
				return Invalid;
			}();
			if (LIKELY(pInputState.IsValid()))
			{
				localInputState = *pInputState;
			}
			else
			{
				localInputState = m_latestLocalState;
			}
		}
		else
		{
			localInputState = GetLocalInputState();

			InputState& storedInputState = m_inputCircularBuffer.Emplace();
			storedInputState = localInputState;
			storedInputState.m_timestamp = physicsTickTime;
		}
		const InputState inputState = GetInputState(localInputState, physicsTickTime);

		const GroundState newGroundState = m_pCharacterPhysics->IsOnGround() ? GroundState::OnGround : GroundState::InAir;

		Math::Vector3f modifiedRequestedWalk = inputState.m_requestedWalk;

		if (m_flags.IsSet(Flags::LockToPlane))
		{
			const Math::Vector3f movementPlaneNormal = m_worldStartTransform.TransformDirection(m_movementPlaneNormal);
			modifiedRequestedWalk = inputState.m_requestedWalk.Project(movementPlaneNormal);
		}

		Math::Quaternionf currentCharacterRotation;

		const Optional<Physics::Data::Body*> pCharacterBody = m_pCharacterPhysics->FindDataComponentOfType<Physics::Data::Body>();
		if (LIKELY(pCharacterBody.IsValid()))
		{
			const Optional<Math::WorldTransform> bodyWorldTransform = pCharacterBody->GetWorldTransform(*pPhysicsScene);
			currentCharacterRotation = bodyWorldTransform.IsValid() ? bodyWorldTransform->GetRotationQuaternion()
			                                                        : m_pCharacterPhysics->GetWorldRotation();
		}
		else
		{
			currentCharacterRotation = m_pCharacterPhysics->GetWorldRotation();
		}

		Math::Vector3f velocityForwardDirection;
		if (m_flags.IsSet(Flags::LockToPlane))
		{
			velocityForwardDirection = modifiedRequestedWalk;
		}
		else if (m_flags.IsSet(Flags::CameraRelativeMovemementInput))
		{
			velocityForwardDirection = inputState.m_cameraRotation.TransformDirection(modifiedRequestedWalk);
			velocityForwardDirection = velocityForwardDirection.GetNormalizedSafe();

			// Handle case where we look down from a birds-eye view
			const Math::Vector3f cameraForward = inputState.m_cameraRotation.GetForwardColumn();
			if (cameraForward.Dot(currentCharacterRotation.GetUpColumn()) > 0.9)
			{
				velocityForwardDirection.z = velocityForwardDirection.y;
				velocityForwardDirection.y = 0;
			}
		}
		else
		{
			velocityForwardDirection = currentCharacterRotation.TransformDirection(modifiedRequestedWalk);
		}

		if (newGroundState == GroundState::OnGround)
		{
			velocityForwardDirection = velocityForwardDirection.Project(m_pCharacterPhysics->GetGroundNormal());
			velocityForwardDirection.z = Math::Min(velocityForwardDirection.z, 0.f);
			velocityForwardDirection = velocityForwardDirection.GetNormalizedSafe();
		}

		{
			const float speedScale = GetWorldScale().y;
			const float runningSpeed = m_runningSpeed * speedScale;
			const float walkingSpeed = m_walkingSpeed * speedScale;
			float speed = (inputState.m_isSprinting ? runningSpeed : walkingSpeed);

			m_pCharacterPhysics->SetVelocity(velocityForwardDirection * speed);
		}

		m_newState = newGroundState;
	}

	void WalkingCharacterBase::SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Guid assetGuid)
	{
		if (assetGuid.IsValid())
		{
			if (pComponent.IsInvalid())
			{
				Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
					*GetSceneRegistry().GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();

				pComponent = soundSpotSceneData.CreateInstance(Audio::SoundSpotComponent::Initializer(
					Entity::Component3D::Initializer{*this},
					{},
					assetGuid,
					Audio::Volume{100_percent},
					m_soundRadius
				));
			}
			else
			{
				pComponent->SetAudioAsset(Asset::Picker{assetGuid, Audio::AssetFormat.assetTypeGuid});
			}
		}
	}

	Asset::Picker WalkingCharacterBase::GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const
	{
		if (pComponent)
		{
			return pComponent->GetAudioAsset();
		}

		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetFireAudioAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			pProjectileWeapon->SetSoundAsset(*this, sceneRegistry, ProjectileWeapon::Sound::Fire, asset.GetAssetGuid(), m_soundRadius);
		}
	}

	Asset::Picker WalkingCharacterBase::GetFireAudioAsset() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			if (const Optional<Audio::SoundSpotComponent*> pSoundSpotComponent = pProjectileWeapon->GetSoundSoundComponent(ProjectileWeapon::Sound::Fire))
			{
				return GetAssetFromSoundSpot(pSoundSpotComponent);
			}
		}
		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetEmptyFireAudioAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			pProjectileWeapon->SetSoundAsset(*this, sceneRegistry, ProjectileWeapon::Sound::EmptyFire, asset.GetAssetGuid(), m_soundRadius);
		}
	}

	Asset::Picker WalkingCharacterBase::GetEmptyFireAudioAsset() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			if (const Optional<Audio::SoundSpotComponent*> pSoundSpotComponent = pProjectileWeapon->GetSoundSoundComponent(ProjectileWeapon::Sound::EmptyFire))
			{
				return GetAssetFromSoundSpot(pSoundSpotComponent);
			}
		}
		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetCasingAudioAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			pProjectileWeapon->SetSoundAsset(*this, sceneRegistry, ProjectileWeapon::Sound::Casing, asset.GetAssetGuid(), m_soundRadius);
		}
	}

	Asset::Picker WalkingCharacterBase::GetCasingAudioAsset() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			if (const Optional<Audio::SoundSpotComponent*> pSoundSpotComponent = pProjectileWeapon->GetSoundSoundComponent(ProjectileWeapon::Sound::Casing))
			{
				return GetAssetFromSoundSpot(pSoundSpotComponent);
			}
		}
		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetReloadAudioAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			pProjectileWeapon->SetSoundAsset(*this, sceneRegistry, ProjectileWeapon::Sound::Reload, asset.GetAssetGuid(), m_soundRadius);
		}
	}

	Asset::Picker WalkingCharacterBase::GetReloadAudioAsset() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			if (const Optional<Audio::SoundSpotComponent*> pSoundSpotComponent = pProjectileWeapon->GetSoundSoundComponent(ProjectileWeapon::Sound::Reload))
			{
				return GetAssetFromSoundSpot(pSoundSpotComponent);
			}
		}
		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetImpactAudioAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			Data::ProjectileProperties projectileProperties = pProjectileWeapon->GetProjectileProperties();
			projectileProperties.m_impactSoundAssetGuid = asset.GetAssetGuid();
			pProjectileWeapon->SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
		}
	}

	Asset::Picker WalkingCharacterBase::GetImpactAudioAsset() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		if (pProjectileWeapon.IsValid())
		{
			const Data::ProjectileProperties projectileProperties = pProjectileWeapon->GetProjectileProperties();
			return {projectileProperties.m_impactSoundAssetGuid, Audio::AssetFormat.assetTypeGuid};
		}
		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void WalkingCharacterBase::SetAimInAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pAimInSpotComponent, asset.GetAssetGuid());
	}

	Asset::Picker WalkingCharacterBase::GetAimInAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pAimInSpotComponent);
	}

	void WalkingCharacterBase::SetJumpAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pJumpSpotComponent, asset.GetAssetGuid());
	}

	Asset::Picker WalkingCharacterBase::GetJumpAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pJumpSpotComponent);
	}

	void WalkingCharacterBase::SetLandAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pLandSpotComponent, asset.GetAssetGuid());
	}

	Asset::Picker WalkingCharacterBase::GetLandAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pLandSpotComponent);
	}

	void WalkingCharacterBase::SetWalkAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pWalkSpotComponent, asset.GetAssetGuid());
	}

	Asset::Picker WalkingCharacterBase::GetWalkAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pWalkSpotComponent);
	}

	void WalkingCharacterBase::SetImpactDecalMaterialAssetGuid(const Asset::Picker asset)
	{
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			Data::ProjectileProperties projectileProperties = pProjectileWeapon->GetProjectileProperties();
			projectileProperties.m_impactDecalMaterialAssetGuid = asset.GetAssetGuid();
			pProjectileWeapon->SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
		}
	}

	Asset::Picker WalkingCharacterBase::GetImpactDecalMaterialAssetGuid() const
	{
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			Data::ProjectileProperties projectileProperties = pProjectileWeapon->GetProjectileProperties();
			return Asset::Picker{projectileProperties.m_impactDecalMaterialAssetGuid, MaterialInstanceAssetType::AssetFormat.assetTypeGuid};
		}
		else
		{
			return {};
		}
	}

	void WalkingCharacterBase::BeforePhysicsUpdate()
	{
		const FrameTime deltaTime{GetCurrentFrameTime()};
		const Optional<Physics::Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const InputState localInputState = GetLocalInputState();
		const InputState inputState = GetInputState(localInputState, pPhysicsScene->GetTickTime());

		const Optional<Physics::Data::Body*> pCharacterBody = m_pCharacterPhysics->FindDataComponentOfType<Physics::Data::Body>();

		Math::Quaternionf currentCharacterRotation;
		if (pCharacterBody.IsValid())
		{
			if (const Optional<Math::Quaternionf> quaternion = pCharacterBody->GetWorldRotation(*pPhysicsScene))
			{
				currentCharacterRotation = *quaternion;
			}
			else
			{
				currentCharacterRotation = m_pCharacterPhysics->GetWorldRotation();
			}
		}
		else
		{
			currentCharacterRotation = m_pCharacterPhysics->GetWorldRotation();
		}

		Math::Quaternionf newRotation{currentCharacterRotation};

		Math::Vector3f modifiedRequestedWalk = inputState.m_requestedWalk;

		if (m_flags.IsSet(Flags::RotateToMovement))
		{
			const Math::Vector3f movementDirection = inputState.m_cameraRotation.TransformDirection(modifiedRequestedWalk);
			if (movementDirection.GetLength() > 0)
			{
				const Math::Vector3f upDirection = currentCharacterRotation.GetUpColumn();
				const Math::Vector3f rightDirection = movementDirection.Cross(upDirection).GetNormalized();
				Math::Matrix3x3f newRotationMatrix(rightDirection, movementDirection, upDirection);
				newRotationMatrix.Orthonormalize();
				newRotation = Math::Quaternionf{newRotationMatrix};

				const float turnSpeedFactor = 6.f;
				const float turnSpeed = (inputState.m_isSprinting ? m_runningTurnSpeed : m_walkingTurnSpeed) * turnSpeedFactor;
				newRotation =
					Math::Quaternionf::SphericalInterpolation(currentCharacterRotation, newRotation, Math::Min(turnSpeed * deltaTime, 1.f));
				currentCharacterRotation = newRotation;
			}
		}

		if (pCharacterBody.IsValid())
		{
			pCharacterBody->SetWorldRotation(*pPhysicsScene, newRotation);
		}
		else
		{
			m_pCharacterPhysics->SetWorldRotation(newRotation);
		}

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				m_latestLocalState.m_bodyRotation = newRotation;
			}
		}
	}

	void WalkingCharacterBase::AfterPhysicsUpdate()
	{
		if (m_pCharacterPhysics != nullptr)
		{
			// Need to wait for the physics to settle before setting the camera controller position

			if (m_isFirstUpdate && m_pThirdPersonCameraController != nullptr)
			{
				const Math::WorldTransform& currentWorldTransform = m_pCharacterPhysics->GetWorldTransform();
				m_pThirdPersonCameraController->SetVerticalLocation(currentWorldTransform.GetLocation().z);
				m_pThirdPersonCameraController->SetTrackedComponent(*m_pCharacterPhysics);
			}

			if (m_isFirstUpdate && m_pCharacterPhysics != nullptr)
			{
				m_worldStartTransform = m_pCharacterPhysics->GetWorldTransform();
				m_isFirstUpdate = false;
			}

			if (m_flags.IsSet(Flags::LockToPlane) && !m_isFirstUpdate)
			{
				const Math::Vector3f movementPlaneNormal = m_worldStartTransform.TransformDirection(m_movementPlaneNormal);
				const Math::Vector3f offsetLocation = m_pCharacterPhysics->GetWorldLocation() - m_worldStartTransform.GetLocation();
				const Math::Vector3f lockedLocation = offsetLocation.Project(movementPlaneNormal);
				m_pCharacterPhysics->SetWorldLocation(m_worldStartTransform.GetLocation() + lockedLocation);
			}
		}

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				const Math::Vector3f inversionSettings{
					m_flags.IsSet(Flags::CameraInvertX) ? 1.f : -1.f,
					m_flags.IsSet(Flags::CameraInvertY) ? 1.f : -1.f,
					1.f
				};
				const Math::Vector3f cameraInput = pActionMap->GetAxisActionState(m_moveCameraAction) * inversionSettings;

				const float mouseSensitivity = 0.01f;
				const Math::Vector2f cameraMouseInput = m_mouseInput * mouseSensitivity * Math::Vector2f{inversionSettings.x, inversionSettings.y};
				m_mouseInput = Math::Zero;

				if (m_pCameraComponent != nullptr)
				{
					const Math::Quaternionf cameraRotation = m_pCameraComponent->GetWorldRotation();

					const bool changedCameraRotation = !cameraRotation.IsEquivalentTo(m_latestLocalState.m_cameraRotation);
					m_latestLocalState.m_cameraRotation = cameraRotation;

					if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
					{
						if (pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
						{
							if (const Optional<Network::LocalClient*> pLocalClient = pBoundComponent->GetLocalClient(*this, GetSceneRegistry()))
							{
								if (changedCameraRotation)
								{
									pBoundComponent->InvalidateProperties<&WalkingCharacterBase::GetCameraRotation>(*this, GetSceneRegistry());
								}
							}
						}
					}
				}
				m_cameraInput = {cameraInput.x + cameraMouseInput.x, cameraInput.y + cameraMouseInput.y};
			}
		}

		if (m_pThirdPersonCameraController != nullptr)
		{
			m_pThirdPersonCameraController->SetRotationalDirection(m_cameraInput);
		}

		const FrameTime deltaTime = GetCurrentFrameTime();

		const GroundState previousState = m_currentState;
		UpdateGroundState(m_newState, deltaTime);
		UpdateAnimationState(GetLocalInputState(), previousState, m_currentState, deltaTime);

		if (m_currentState == GroundState::OnGround && m_pThirdPersonCameraController != nullptr && m_pCharacterPhysics != nullptr)
		{
			const Math::WorldTransform& currentWorldTransform = m_pCharacterPhysics->GetWorldTransform();
			m_pThirdPersonCameraController->SetVerticalLocation(currentWorldTransform.GetLocation().z);
		}
	}

	[[maybe_unused]] const bool wasWalkingCharacterBaseRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<WalkingCharacterBase>>::Make());
	[[maybe_unused]] const bool wasWalkingCharacterBaseTypeRegistered = Reflection::Registry::RegisterType<WalkingCharacterBase>();

	GenericWalkingCharacter::GenericWalkingCharacter(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		m_flags |= Flags::CameraRelativeMovemementInput | Flags::RotateToMovement;
	}

	GenericWalkingCharacter::GenericWalkingCharacter(const GenericWalkingCharacter& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
		m_flags |= Flags::CameraRelativeMovemementInput | Flags::RotateToMovement;
	}

	void GenericWalkingCharacter::OnCreatedInternal()
	{
		if (LIKELY(m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr))
		{
			Entity::ComponentTypeSceneData<GenericWalkingCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<GenericWalkingCharacter>&>(*GetTypeSceneData());

			if (IsEnabled())
			{
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableFixedPhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}

			Optional<Entity::ComponentTypeSceneDataInterface*> pCharacterPhysicsSceneData = m_pCharacterPhysics->GetTypeSceneData();
			Assert(pCharacterPhysicsSceneData.IsValid());
			if (LIKELY(pCharacterPhysicsSceneData.IsValid()))
			{
				sceneData.GetFixedPhysicsUpdateStage()
					->AddSubsequentStage(*pCharacterPhysicsSceneData->GetFixedPhysicsUpdateStage(), GetSceneRegistry());
			}

			// Ensure that before & after physics update don't overlap with the camera update
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

				{
					Entity::ComponentTypeSceneData<Camera::ThirdPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::ThirdPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::FirstPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::FirstPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::Planar>& cameraSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Camera::Planar>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
			}
		}
	}

	void GenericWalkingCharacter::OnEnableInternal()
	{
		if (m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr)
		{
			Entity::ComponentTypeSceneData<GenericWalkingCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<GenericWalkingCharacter>&>(*GetTypeSceneData());
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void GenericWalkingCharacter::OnDisableInternal()
	{
		Entity::ComponentTypeSceneData<GenericWalkingCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<GenericWalkingCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	[[maybe_unused]] const bool wasGenericWalkingCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<GenericWalkingCharacter>>::Make());
	[[maybe_unused]] const bool wasGenericWalkingCharacterTypeRegistered = Reflection::Registry::RegisterType<GenericWalkingCharacter>();

	ThirdPersonCharacter::ThirdPersonCharacter(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		m_flags |= Flags::CameraRelativeMovemementInput | Flags::RotateToMovement;
	}

	ThirdPersonCharacter::ThirdPersonCharacter(const ThirdPersonCharacter& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
		m_flags |= Flags::CameraRelativeMovemementInput | Flags::RotateToMovement;
	}

	void ThirdPersonCharacter::OnCreatedInternal()
	{
		if (LIKELY(m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr))
		{
			Entity::ComponentTypeSceneData<ThirdPersonCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<ThirdPersonCharacter>&>(*GetTypeSceneData());

			if (IsEnabled())
			{
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableFixedPhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}

			Optional<Entity::ComponentTypeSceneDataInterface*> pCharacterPhysicsSceneData = m_pCharacterPhysics->GetTypeSceneData();
			Assert(pCharacterPhysicsSceneData.IsValid());
			if (LIKELY(pCharacterPhysicsSceneData.IsValid()))
			{
				sceneData.GetFixedPhysicsUpdateStage()
					->AddSubsequentStage(*pCharacterPhysicsSceneData->GetFixedPhysicsUpdateStage(), GetSceneRegistry());
			}

			// Ensure that before & after physics update don't overlap with the camera update
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

				{
					Entity::ComponentTypeSceneData<Camera::ThirdPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::ThirdPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::FirstPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::FirstPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::Planar>& cameraSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Camera::Planar>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
			}
		}
	}

	void ThirdPersonCharacter::OnEnableInternal()
	{
		if (m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr)
		{
			Entity::ComponentTypeSceneData<ThirdPersonCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<ThirdPersonCharacter>&>(*GetTypeSceneData());
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void ThirdPersonCharacter::OnDisableInternal()
	{
		Entity::ComponentTypeSceneData<ThirdPersonCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<ThirdPersonCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	[[maybe_unused]] const bool wasThirdPersonCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ThirdPersonCharacter>>::Make());
	[[maybe_unused]] const bool wasThirdPersonCharacterTypeRegistered = Reflection::Registry::RegisterType<ThirdPersonCharacter>();

	FirstPersonCharacter::FirstPersonCharacter(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		CreateDataComponent<ProjectileWeapon>(ProjectileWeapon::Initializer{
			Entity::Data::Component3D::DynamicInitializer{*this, initializer.GetSceneRegistry()},
			Data::FirearmProperties::MakeAutomaticRifle(),
			Data::ProjectileProperties::MakeAutomaticRifle()
		});
	}

	FirstPersonCharacter::FirstPersonCharacter(const FirstPersonCharacter& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
	}

	FirstPersonCharacter::FirstPersonCharacter(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		CreateDataComponent<ProjectileWeapon>(ProjectileWeapon::Initializer{
			Entity::Data::Component3D::DynamicInitializer{*this, deserializer.GetSceneRegistry()},
			Data::FirearmProperties::MakeAutomaticRifle(),
			Data::ProjectileProperties::MakeAutomaticRifle()
		});
	}

	void FirstPersonCharacter::OnCreatedInternal()
	{
		if (LIKELY(m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr))
		{
			Entity::ComponentTypeSceneData<FirstPersonCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<FirstPersonCharacter>&>(*GetTypeSceneData());

			if (IsEnabled())
			{
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableFixedPhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}

			Optional<Entity::ComponentTypeSceneDataInterface*> pCharacterPhysicsSceneData = m_pCharacterPhysics->GetTypeSceneData();
			Assert(pCharacterPhysicsSceneData.IsValid());
			if (LIKELY(pCharacterPhysicsSceneData.IsValid()))
			{
				sceneData.GetFixedPhysicsUpdateStage()
					->AddSubsequentStage(*pCharacterPhysicsSceneData->GetFixedPhysicsUpdateStage(), GetSceneRegistry());
			}

			// Ensure that before & after physics update don't overlap with the camera update
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

				{
					Entity::ComponentTypeSceneData<Camera::ThirdPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::ThirdPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::FirstPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::FirstPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::Planar>& cameraSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Camera::Planar>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
			}
		}

		if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
		{
			if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
			{
				pFirstPersonAnimationController->OnOneShotAnimationCompleted.Add(
					*this,
					[this](FirstPersonCharacter&, Animation::Controller::FirstPerson::OneShotAnimation oneShotAnimation)
					{
						using OneShotAnimation = Animation::Controller::FirstPerson::OneShotAnimation;

						switch (oneShotAnimation)
						{
							case OneShotAnimation::Reload:
								[[fallthrough]];
							case OneShotAnimation::ReloadEmpty:
								[[fallthrough]];
							case OneShotAnimation::AimReload:
								[[fallthrough]];
							case OneShotAnimation::AimReloadEmpty:
							{
								const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
								Assert(pProjectileWeapon.IsValid());
								if (LIKELY(pProjectileWeapon.IsValid()))
								{
									pProjectileWeapon->FinishReload(*this, *this);
								}
								break;
							}
							default:
								break;
						}
					}
				);
			}
		}

		if (const Optional<Ammunition*> pAmmunitionComponent = FindDataComponentOfType<Ammunition>())
		{
			const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
			Assert(pProjectileWeapon.IsValid());
			if (LIKELY(pProjectileWeapon.IsValid()))
			{
				const Data::FirearmProperties& firearmProperties = pProjectileWeapon->GetFirearmProperties();
				pAmmunitionComponent->SetMaximumAmmunition(firearmProperties.m_magazineCapacity);
				pAmmunitionComponent->Add(pProjectileWeapon->GetAmmunitionCount());
			}
		}
	}

	void FirstPersonCharacter::OnEnableInternal()
	{
		if (m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr)
		{
			Entity::ComponentTypeSceneData<FirstPersonCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<FirstPersonCharacter>&>(*GetTypeSceneData());
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void FirstPersonCharacter::OnDisableInternal()
	{
		Entity::ComponentTypeSceneData<FirstPersonCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<FirstPersonCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void FirstPersonCharacter::OnInputDisabled(Input::ActionMonitor& actionMonitor)
	{
		BaseType::OnInputDisabled(actionMonitor);

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				pActionMap->UnbindBinaryAction(m_attackAction);
				pActionMap->UnbindBinaryAction(m_secondaryAction);
			}
		}
	}

	void FirstPersonCharacter::OnInputAssigned(Input::ActionMonitor& actionMonitor)
	{
		BaseType::OnInputAssigned(actionMonitor);

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMap*> pActionMap = pInputComponent->GetActionMap())
			{
				m_attackAction = pActionMap->BindBinaryAction(
					AttackActionGuid,
					[this](const bool inputValue)
					{
						OnAttack(inputValue);
					}
				);
				m_secondaryAction = pActionMap->BindBinaryAction(
					SecondaryActionGuid,
					[this](const bool inputValue)
					{
						OnAim(inputValue);
					}
				);
				m_reloadAction = pActionMap->BindBinaryAction(
					ReloadActionGuid,
					[this](const bool inputValue)
					{
						OnReload(inputValue);
					}
				);
			}
		}
	}

	void FirstPersonCharacter::AfterPhysicsUpdate()
	{
		BaseType::AfterPhysicsUpdate();

		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			if (pProjectileWeapon->CanFire())
			{
				OnFire();
			}
		}
	}

	void FirstPersonCharacter::OnAttack(const bool isAttacking)
	{
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			if (pProjectileWeapon->IsReloading())
			{
				return;
			}

			if (isAttacking && pProjectileWeapon->ShouldAutoReload())
			{
				OnReload(true);
				return;
			}

			if (isAttacking)
			{
				pProjectileWeapon->HoldTrigger();
			}
			else
			{
				pProjectileWeapon->ReleaseTrigger();
			}
		}
	}

	void FirstPersonCharacter::OnAim(const bool isAiming)
	{
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			if (pProjectileWeapon->IsReloading())
			{
				return;
			}
		}

		if (m_pAimInSpotComponent)
		{
			m_pAimInSpotComponent->Play();
		}

		m_isAiming = isAiming;

		if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
		{
			if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
			{
				pFirstPersonAnimationController->SetAiming(isAiming);
			}
		}
	}

	void FirstPersonCharacter::OnReload(const bool isReloading)
	{
		if (isReloading)
		{
			const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
			Assert(pProjectileWeapon.IsValid());
			if (LIKELY(pProjectileWeapon.IsValid()))
			{
				pProjectileWeapon->StartReload();
			}

			if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
			{
				if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
				{
					pFirstPersonAnimationController->StartOneShot(
						m_isAiming ? Animation::Controller::FirstPerson::OneShotAnimation::AimReload
											 : Animation::Controller::FirstPerson::OneShotAnimation::Reload
					);
				}
			}
		}
	}

	void FirstPersonCharacter::OnFire()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);
		Assert(pProjectileWeapon.IsValid());
		if (LIKELY(pProjectileWeapon.IsValid()))
		{
			const Math::WorldCoordinate cameraLocation = m_pCameraComponent->GetWorldLocation();
			const Math::Vector3f cameraDirection = m_pCameraComponent->GetWorldRotation().GetForwardColumn();
			const Math::WorldLine cameraLine = {cameraLocation, cameraLocation + cameraDirection * 1000.0f};

			Array<JPH::BodyID, 1> bodiesFilter{};
			if (const Optional<Physics::Data::Body*> pCharacterBody = m_pCharacterPhysics->FindDataComponentOfType<Physics::Data::Body>(sceneRegistry))
			{
				JPH::BodyID bodyID = pCharacterBody->GetIdentifier();
				bodiesFilter[0] = bodyID;
			}

			switch (pProjectileWeapon->Fire(*this, *this, cameraLine, bodiesFilter))
			{
				case ProjectileWeapon::FireResult::OutOfAmmoSingle:
				{
					if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
					{
						if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
						{
							pFirstPersonAnimationController->StartOneShot(
								m_isAiming ? Animation::Controller::FirstPerson::OneShotAnimation::AimFireEmpty
													 : Animation::Controller::FirstPerson::OneShotAnimation::FireEmpty
							);
						}
					}
				}
				break;
				case ProjectileWeapon::FireResult::OutOfAmmoBurst:
				case ProjectileWeapon::FireResult::NotReadySingle:
				case ProjectileWeapon::FireResult::NotReadyBurst:
					break;
				case ProjectileWeapon::FireResult::Fired:
				{
					if (m_pSkeletonComponent.IsValid() && m_pSkeletonComponent->GetSkeletonInstance().IsValid())
					{
						if (Optional<Animation::Controller::FirstPerson*> pFirstPersonAnimationController = m_pSkeletonComponent->FindDataComponentOfType<Animation::Controller::FirstPerson>())
						{
							pFirstPersonAnimationController->StartOneShot(
								m_isAiming ? Animation::Controller::FirstPerson::OneShotAnimation::AimFire
													 : Animation::Controller::FirstPerson::OneShotAnimation::Fire
							);
						}
					}

					ApplyRecoil();
				}
				break;
			}
		}
	}

	void FirstPersonCharacter::ApplyRecoil()
	{
		if (Optional<GameFramework::Camera::FirstPerson*> pCameraController = FindFirstDataComponentOfTypeInChildrenRecursive<GameFramework::Camera::FirstPerson>())
		{
			const Optional<ProjectileWeapon*> pProjectileWeapon = FindDataComponentOfType<ProjectileWeapon>(GetSceneRegistry());
			Assert(pProjectileWeapon.IsValid());
			if (LIKELY(pProjectileWeapon.IsValid()))
			{
				const Data::FirearmProperties& firearmProperties = pProjectileWeapon->GetFirearmProperties();
				const float horizontalRecoil = firearmProperties.m_horizontalRecoilFactor;
				const float verticalRecoil = firearmProperties.m_verticalRecoilFactor;

				Math::Vector2f horizontalLimits{horizontalRecoil * -0.5f, horizontalRecoil * 0.5f};
				Math::Vector2f verticalLimits{0.0f, verticalRecoil};
				pCameraController->Shake(horizontalLimits, verticalLimits);
			}
		}
	}

	[[maybe_unused]] const bool wasFirstPersonCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FirstPersonCharacter>>::Make());
	[[maybe_unused]] const bool wasFirstPersonCharacterTypeRegistered = Reflection::Registry::RegisterType<FirstPersonCharacter>();

	RollingBallCharacter::RollingBallCharacter(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		m_flags |= Flags::CameraRelativeMovemementInput;
	}

	RollingBallCharacter::RollingBallCharacter(const RollingBallCharacter& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
		m_flags |= Flags::CameraRelativeMovemementInput;
	}

	void RollingBallCharacter::OnCreatedInternal()
	{
		if (LIKELY(m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr))
		{
			Entity::ComponentTypeSceneData<RollingBallCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<RollingBallCharacter>&>(*GetTypeSceneData());

			if (IsEnabled())
			{
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableFixedPhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}

			Optional<Entity::ComponentTypeSceneDataInterface*> pCharacterPhysicsSceneData = m_pCharacterPhysics->GetTypeSceneData();
			Assert(pCharacterPhysicsSceneData.IsValid());
			if (LIKELY(pCharacterPhysicsSceneData.IsValid()))
			{
				sceneData.GetFixedPhysicsUpdateStage()
					->AddSubsequentStage(*pCharacterPhysicsSceneData->GetFixedPhysicsUpdateStage(), GetSceneRegistry());
			}

			// Ensure that before & after physics update don't overlap with the camera update
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

				{
					Entity::ComponentTypeSceneData<Camera::ThirdPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::ThirdPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::FirstPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::FirstPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::Planar>& cameraSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Camera::Planar>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
			}
		}
	}

	void RollingBallCharacter::OnEnableInternal()
	{
		if (m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr)
		{
			Entity::ComponentTypeSceneData<RollingBallCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<RollingBallCharacter>&>(*GetTypeSceneData());
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void RollingBallCharacter::OnDisableInternal()
	{
		Entity::ComponentTypeSceneData<RollingBallCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RollingBallCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	[[maybe_unused]] const bool wasRollingBallCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RollingBallCharacter>>::Make());
	[[maybe_unused]] const bool wasRollingBallCharacterTypeRegistered = Reflection::Registry::RegisterType<RollingBallCharacter>();

	PlanarMovementCharacter::PlanarMovementCharacter(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		m_flags |= Flags::LockToPlane | Flags::RotateToMovement;
	}

	PlanarMovementCharacter::PlanarMovementCharacter(const PlanarMovementCharacter& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
		m_flags |= Flags::LockToPlane | Flags::RotateToMovement;
	}

	void PlanarMovementCharacter::OnCreatedInternal()
	{
		if (LIKELY(m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr))
		{
			Entity::ComponentTypeSceneData<PlanarMovementCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<PlanarMovementCharacter>&>(*GetTypeSceneData());

			if (IsEnabled())
			{
				sceneData.EnableBeforePhysicsUpdate(*this);
				sceneData.EnableFixedPhysicsUpdate(*this);
				sceneData.EnableAfterPhysicsUpdate(*this);
			}

			Optional<Entity::ComponentTypeSceneDataInterface*> pCharacterPhysicsSceneData = m_pCharacterPhysics->GetTypeSceneData();
			Assert(pCharacterPhysicsSceneData.IsValid());
			if (LIKELY(pCharacterPhysicsSceneData.IsValid()))
			{
				sceneData.GetFixedPhysicsUpdateStage()
					->AddSubsequentStage(*pCharacterPhysicsSceneData->GetFixedPhysicsUpdateStage(), GetSceneRegistry());
			}

			// Ensure that before & after physics update don't overlap with the camera update
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

				{
					Entity::ComponentTypeSceneData<Camera::ThirdPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::ThirdPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::FirstPerson>& cameraSceneData =
						*sceneRegistry.GetOrCreateComponentTypeData<Camera::FirstPerson>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
				{
					Entity::ComponentTypeSceneData<Camera::Planar>& cameraSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Camera::Planar>();
					sceneData.GetAfterPhysicsUpdateStage()->AddSubsequentStage(*cameraSceneData.GetAfterPhysicsUpdateStage(), sceneRegistry);
				}
			}
		}
	}

	void PlanarMovementCharacter::OnEnableInternal()
	{
		if (m_pCharacterPhysics != nullptr && m_pCameraComponent != nullptr)
		{
			Entity::ComponentTypeSceneData<PlanarMovementCharacter>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<PlanarMovementCharacter>&>(*GetTypeSceneData());
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void PlanarMovementCharacter::OnDisableInternal()
	{
		Entity::ComponentTypeSceneData<PlanarMovementCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<PlanarMovementCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	[[maybe_unused]] const bool wasPlanarMovementCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<PlanarMovementCharacter>>::Make());
	[[maybe_unused]] const bool wasPlanarMovementCharacterTypeRegistered = Reflection::Registry::RegisterType<PlanarMovementCharacter>();
}
