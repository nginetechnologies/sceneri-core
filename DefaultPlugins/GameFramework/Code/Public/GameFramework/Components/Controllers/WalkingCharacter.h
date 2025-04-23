#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Input/ActionHandle.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Math/ClampedValue.h>
#include <Common/Time/Duration.h>
#include <Common/Time/Timestamp.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/CircularBuffer.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine
{
	namespace Physics
	{
		struct CharacterBase;

		namespace Data
		{
			struct Scene;
		}
	}

	namespace Animation
	{
		struct SkeletonComponent;
	}

	namespace Entity
	{
		struct CameraComponent;
	}

	namespace Input
	{
		struct ActionMonitor;
	}

	namespace GameFramework
	{
		namespace Camera
		{
			struct ThirdPerson;
		}
	}

	namespace Network
	{
		struct LocalPeer;
		struct LocalHost;
		struct LocalClient;

		namespace Session
		{
			struct BoundComponent;
		}
	}
}

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::GameFramework::Components
{
	struct GenericWalkingCharacter;

	// TODO: Remove when we can replicate this in visual scripting
	struct WalkingCharacterBase : public Entity::Component3D
	{
		inline static constexpr Guid TypeGuid = "{10BC2A29-2131-447A-8A66-43742C106BDF}"_guid;
		enum class Flags : uint8
		{
			CameraRelativeMovemementInput = 1 << 0,
			CameraInvertX = 1 << 1,
			CameraInvertY = 1 << 2,
			LockToPlane = 1 << 3,
			RotateToMovement = 1 << 4
		};

		enum class GroundState : uint8
		{
			OnGround,
			InAir,
		};

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 6>;

		using Component3D::Component3D;
		using Component3D::Serialize;

		WalkingCharacterBase(const WalkingCharacterBase& templateComponent, const Cloner& cloner);
		virtual ~WalkingCharacterBase();

		void OnEnable();
		void OnDisable();

		void OnDestroying();
		void OnCreated();

		void BeforePhysicsUpdate();
		void FixedPhysicsUpdate();
		void AfterPhysicsUpdate();

		void SetFireAudioAsset(const Asset::Picker asset);
		Asset::Picker GetFireAudioAsset() const;

		void SetEmptyFireAudioAsset(const Asset::Picker asset);
		Asset::Picker GetEmptyFireAudioAsset() const;

		void SetCasingAudioAsset(const Asset::Picker asset);
		Asset::Picker GetCasingAudioAsset() const;

		void SetReloadAudioAsset(const Asset::Picker asset);
		Asset::Picker GetReloadAudioAsset() const;

		void SetImpactAudioAsset(const Asset::Picker asset);
		Asset::Picker GetImpactAudioAsset() const;

		void SetAimInAudioAsset(const Asset::Picker asset);
		Asset::Picker GetAimInAudioAsset() const;

		void SetJumpAudioAsset(const Asset::Picker asset);
		Asset::Picker GetJumpAudioAsset() const;

		void SetLandAudioAsset(const Asset::Picker asset);
		Asset::Picker GetLandAudioAsset() const;

		void SetWalkAudioAsset(const Asset::Picker asset);
		Asset::Picker GetWalkAudioAsset() const;

		void SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Guid asset);
		Asset::Picker GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const;

		void SetImpactDecalMaterialAssetGuid(const Asset::Picker asset);
		Asset::Picker GetImpactDecalMaterialAssetGuid() const;

		virtual void OnInputAssigned(Input::ActionMonitor& actionMonitor);
		virtual void OnInputDisabled(Input::ActionMonitor& actionMonitor);
	protected:
		void OnJumpAction(const bool);

		struct JumpMessageData
		{
		};
		void OnRemoteClientJumped(Network::Session::BoundComponent&, Network::LocalPeer&, const JumpMessageData message);

		virtual void OnCreatedInternal() = 0;
		virtual void OnEnableInternal() = 0;
		virtual void OnDisableInternal() = 0;

		[[nodiscard]] Math::Vector3f GetRequestedWalk() const;
		void SetRequestedWalk(const Math::Vector3f requestedWalk);
		[[nodiscard]] Math::Quaternionf GetCameraRotation() const;
		void SetCameraRotation(const Math::Quaternionf rotation);
		[[nodiscard]] Math::Quaternionf GetBodyRotation() const;
		void SetBodyRotation(const Math::Quaternionf rotation);
		[[nodiscard]] bool GetIsSprinting() const;
		void SetIsSprinting(const bool isSprinting);

		struct InputState
		{
			// TODO: Compress Vector3 & Vector2
			Math::Vector3f m_requestedWalk{Math::Zero};
			Math::Quaternionf m_cameraRotation{Math::Identity};
			Math::Quaternionf m_bodyRotation{Math::Identity};
			bool m_isSprinting{false};
			Time::Timestamp m_timestamp;
		};
		[[nodiscard]] InputState GetLocalInputState();
		[[nodiscard]] InputState GetInputState(const InputState localInputState, const Time::Timestamp currentTime) const;

		void UpdateAnimationState(
			const InputState localInputState, const GroundState previousGroundState, const GroundState newGroundState, const FrameTime deltaTime
		);
		void UpdateGroundState(const GroundState newGroundState, const FrameTime deltaTime);
	protected:
		friend struct Reflection::ReflectedType<GameFramework::Components::WalkingCharacterBase>;
		friend struct Reflection::ReflectedType<GameFramework::Components::GenericWalkingCharacter>;

		Physics::CharacterBase* m_pCharacterPhysics = nullptr;
		Optional<Animation::SkeletonComponent*> m_pSkeletonComponent = nullptr;
		Camera::ThirdPerson* m_pThirdPersonCameraController = nullptr;
		Entity::CameraComponent* m_pCameraComponent = nullptr;

		Input::ActionHandle m_jumpAction;
		Input::ActionHandle m_sprintAction;
		Input::ActionHandle m_moveAction;
		Input::ActionHandle m_moveCameraAction;
		Input::ActionHandle m_mouseLookCameraAction;

		// Store one second's worth of ticks in history, given 60 tick rate
		inline static constexpr uint8 InputHistorySize = 60;
		mutable FixedCircularBuffer<InputState, InputHistorySize> m_inputCircularBuffer;

		InputState m_latestLocalState;
		InputState m_latestRemoteState;

		Math::Vector2f m_cameraInput{Math::Zero};
		Math::Vector2f m_mouseInput{Math::Zero};

		GroundState m_newState;
		GroundState m_currentState;
		bool m_isFirstUpdate{true};

		Math::ClampedValuef m_jumpHeight = {2.f, 0.f, 1000.f};
		Math::ClampedValuef m_walkingSpeed = {3.5f, 0.f, 1000.f};
		Math::ClampedValuef m_runningSpeed = {5.f, 0.f, 1000.f};
		Math::ClampedValuef m_walkingTurnSpeed = {3.f, 0.f, 1000.f};
		Math::ClampedValuef m_runningTurnSpeed = {3.5f, 0.f, 1000.f};
		Math::Ratiof m_sprintAnalogThreshold = 0.75f;

		// Temporary until PlaneConstraint can be used instead
		Math::WorldTransform m_worldStartTransform = Math::Identity;
		Math::Vector3f m_movementPlaneNormal = Math::Forward;

		Time::Durationf m_timeInAir = 0_seconds;
		float m_movementMomentum = 0.f;

		EnumFlags<Flags> m_flags;
		Optional<Audio::SoundSpotComponent*> m_pAimInSpotComponent;
		Optional<Audio::SoundSpotComponent*> m_pJumpSpotComponent;
		Optional<Audio::SoundSpotComponent*> m_pLandSpotComponent;
		Optional<Audio::SoundSpotComponent*> m_pWalkSpotComponent;
		Math::Radiusf m_soundRadius{5_meters};
		float m_footStepDelay = 1.0f;
		float m_footStepTime = 0;
	};

	ENUM_FLAG_OPERATORS(WalkingCharacterBase::Flags);

	// TODO: Remove when we can replicate this in visual scripting
	// This is the controller we used in projects before creating players in editor
	struct GenericWalkingCharacter final : public WalkingCharacterBase
	{
		inline static constexpr Guid TypeGuid = "d4e4838b-b9dc-4788-8e33-7316ffef1693"_guid;
		using BaseType = WalkingCharacterBase;

		using BaseType::BaseType;
		GenericWalkingCharacter(Initializer&& initializer);
		GenericWalkingCharacter(const GenericWalkingCharacter& templateComponent, const Cloner& cloner);
	protected:
		virtual void OnCreatedInternal() override;
		virtual void OnEnableInternal() override;
		virtual void OnDisableInternal() override;
	};

	// TODO: Remove when we can replicate this in visual scripting
	struct ThirdPersonCharacter final : public WalkingCharacterBase
	{
		inline static constexpr Guid TypeGuid = "{183DEA3D-534F-47EA-B025-03FDAC1D17E9}"_guid;
		using BaseType = WalkingCharacterBase;

		using BaseType::BaseType;
		ThirdPersonCharacter(Initializer&& initializer);
		ThirdPersonCharacter(const ThirdPersonCharacter& templateComponent, const Cloner& cloner);
	protected:
		virtual void OnCreatedInternal() override;
		virtual void OnEnableInternal() override;
		virtual void OnDisableInternal() override;
	};

	// TODO: Remove when we can replicate this in visual scripting
	struct FirstPersonCharacter final : public WalkingCharacterBase
	{
		inline static constexpr Guid TypeGuid = "{833C816F-01AE-4BC4-BF0B-0264BECDF14E}"_guid;
		using BaseType = WalkingCharacterBase;

		using BaseType::BaseType;
		FirstPersonCharacter(Initializer&& initializer);
		FirstPersonCharacter(const FirstPersonCharacter& templateComponent, const Cloner& cloner);
		FirstPersonCharacter(const Deserializer& deserializer);

		void AfterPhysicsUpdate();
	protected:
		friend struct Reflection::ReflectedType<FirstPersonCharacter>;

		virtual void OnCreatedInternal() override;
		virtual void OnEnableInternal() override;
		virtual void OnDisableInternal() override;
		virtual void OnInputAssigned(Input::ActionMonitor& actionMonitor) override;
		virtual void OnInputDisabled(Input::ActionMonitor& actionMonitor) override;

		void OnAttack(const bool);
		void OnAim(const bool);
		void OnReload(const bool);

		void OnFire();
		void ApplyRecoil();

		Input::ActionHandle m_attackAction;
		Input::ActionHandle m_secondaryAction;
		Input::ActionHandle m_reloadAction;

		bool m_isAiming{false};
	};

	// TODO: Remove when we can replicate this in visual scripting
	struct RollingBallCharacter final : public WalkingCharacterBase
	{
		inline static constexpr Guid TypeGuid = "{E715A6D3-716D-4C73-AB83-C29AC03BCC41}"_guid;
		using BaseType = WalkingCharacterBase;

		using BaseType::BaseType;
		RollingBallCharacter(Initializer&& initializer);
		RollingBallCharacter(const RollingBallCharacter& templateComponent, const Cloner& cloner);
	protected:
		virtual void OnCreatedInternal() override;
		virtual void OnEnableInternal() override;
		virtual void OnDisableInternal() override;
	};

	// TODO: Remove when we can replicate this in visual scripting
	struct PlanarMovementCharacter final : public WalkingCharacterBase
	{
		inline static constexpr Guid TypeGuid = "{1d6b8500-6e41-45ba-bee7-156cf8d92b3e}"_guid;
		using BaseType = WalkingCharacterBase;

		using BaseType::BaseType;
		PlanarMovementCharacter(const PlanarMovementCharacter& templateComponent, const Cloner& cloner);
		PlanarMovementCharacter(Initializer&& initializer);
	protected:
		virtual void OnCreatedInternal() override;
		virtual void OnEnableInternal() override;
		virtual void OnDisableInternal() override;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Components::WalkingCharacterBase>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::WalkingCharacterBase>(
			GameFramework::Components::WalkingCharacterBase::TypeGuid,
			MAKE_UNICODE_LITERAL("Walking Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Jump Height"),
					"jumpHeight",
					"{18FCB4AB-DAB6-4D7A-A171-A5F02E0CB3CA}"_guid,
					MAKE_UNICODE_LITERAL("Jump"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_jumpHeight
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Walking Speed"),
					"walkingSpeed",
					"{D58FEA54-815F-45EF-ABFB-FA3E2582BE5D}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_walkingSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Running Speed"),
					"runningSpeed",
					"{342D426D-2A3C-4A10-B23A-6D9552B9DB99}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_runningSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Walking Turn Speed"),
					"walkingTurnSpeed",
					"{DF82C6ED-8249-4E17-91FF-4EDA4CBE7372}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_walkingTurnSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Running Turn Speed"),
					"runningTurnSpeed",
					"{5571A616-B333-4BCB-AC42-EC0162A1C576}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_runningTurnSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Sprint Analog Input Threshold"),
					"sprintAnalogThreshold",
					"{785E31EC-8850-43FB-98FA-C81849015488}"_guid,
					MAKE_UNICODE_LITERAL("Input"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_sprintAnalogThreshold
				},
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Shoot Decal"),
					"material_shoot_decal",
					"{0E0D1824-8C80-46A9-8D6C-90D0EEA1E8B0}"_guid,
					MAKE_UNICODE_LITERAL("Material"),
					&GameFramework::Components::WalkingCharacterBase::SetImpactDecalMaterialAssetGuid,
					&GameFramework::Components::WalkingCharacterBase::GetImpactDecalMaterialAssetGuid
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Fire Sound"),
					"audio_fire",
					"{252E4AD1-91BB-420D-9C2F-93FDA91BB913}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetFireAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetFireAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Empty Fire Sound"),
					"audio_fireempty",
					"{84A2897C-74C7-4121-9118-A0B7BF049DDD}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetEmptyFireAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetEmptyFireAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Casing Sound"),
					"audio_casing",
					"{EA6DD2C0-9FC4-470C-8467-5BE969E17D36}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetCasingAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetCasingAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Reload Sound"),
					"audio_reload",
					"{79E69902-34A2-475B-9178-97912A515AD3}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetReloadAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetReloadAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Impact Sound"),
					"audio_impact",
					"{689B1D7C-AA44-4DB0-9560-E9D28E4C55C6}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetImpactAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetImpactAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Aim In Sound"),
					"audio_aimin",
					"{FAE2809B-F0C3-4728-B726-CAC1A3668BE0}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetAimInAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetAimInAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Jump Sound"),
					"audio_jump",
					"{6B975B7D-955C-4ABC-A7C0-FD4CC91D6464}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetJumpAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetJumpAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Land Sound"),
					"audio_land",
					"{42DE60FC-DBDE-4FA5-8EA0-80D0DDAD8FED}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetLandAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetLandAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Walk Sound"),
					"audio_walk",
					"{295887D1-5D0E-491D-AD95-E0FADB4CF612}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::SetWalkAudioAsset,
					&GameFramework::Components::WalkingCharacterBase::GetWalkAudioAsset
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"audio_radius",
					"{B322D052-1899-43DA-9B54-D6569CAF329C}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::m_soundRadius
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Foot Step Delay"),
					"audio_footStepDelay",
					"{9569ACC1-D79F-4C1D-B76E-687A3A3886C4}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Components::WalkingCharacterBase::m_footStepDelay
				),
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Planar Movement Plane Normal"),
					"movementPlaneNormal",
					"{F6A6CD10-C13C-4FDB-BD38-37819B39D1E0}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_movementPlaneNormal
				},
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Requested Walk"),
					"requestedWalk",
					"{0B551FAA-62FF-456B-9996-D90D66D5671A}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateClientToHost |
						Reflection::PropertyFlags::PropagateClientToClient,
					&GameFramework::Components::WalkingCharacterBase::SetRequestedWalk,
					&GameFramework::Components::WalkingCharacterBase::GetRequestedWalk
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Camera Rotation"),
					"cameraRotation",
					"be58159e-5401-4e3c-94f4-4ed64d496dfd"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateClientToHost |
						Reflection::PropertyFlags::PropagateClientToClient,
					&GameFramework::Components::WalkingCharacterBase::SetCameraRotation,
					&GameFramework::Components::WalkingCharacterBase::GetCameraRotation
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Body Rotation"),
					"bodyRotation",
					"08f6e9af-2eb0-4bb5-b5e3-e2c052c52088"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateClientToHost |
						Reflection::PropertyFlags::PropagateClientToClient,
					&GameFramework::Components::WalkingCharacterBase::SetBodyRotation,
					&GameFramework::Components::WalkingCharacterBase::GetBodyRotation
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Is Sprinting"),
					"isSprinting",
					"{2A397B62-5A58-4810-B6A9-3223C3E751E3}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateClientToHost |
						Reflection::PropertyFlags::PropagateClientToClient,
					&GameFramework::Components::WalkingCharacterBase::SetIsSprinting,
					&GameFramework::Components::WalkingCharacterBase::GetIsSprinting
				)
			},
			Functions{Function{
				"{E96E6B28-4551-4859-961F-9E0650C3A06E}"_guid,
				MAKE_UNICODE_LITERAL("On Client Jumped"),
				&GameFramework::Components::WalkingCharacterBase::OnRemoteClientJumped,
				FunctionFlags::ClientToHost | FunctionFlags::ClientToClient,
				Reflection::ReturnType{},
				Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
				Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
				Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("message")}
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Components::GenericWalkingCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::GenericWalkingCharacter>(
			GameFramework::Components::GenericWalkingCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Generic Controller"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			// Duplicating property definitions for backwards compatibility
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Jump Height"),
					"jumpHeight",
					"{0E19A39F-95A2-41C4-911B-9364B2A4102D}"_guid,
					MAKE_UNICODE_LITERAL("Jump"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_jumpHeight
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Walking Speed"),
					"walkingSpeed",
					"{54C209B8-784E-42B9-967B-7340F47CBBBD}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_walkingSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Running Speed"),
					"runningSpeed",
					"{87A0D038-38B6-4800-B85F-B26468A1CAF5}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_runningSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Walking Turn Speed"),
					"walkingTurnSpeed",
					"{5B7528E5-244F-4F48-82A6-02677060D8A6}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_walkingTurnSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Running Turn Speed"),
					"runningTurnSpeed",
					"{8A9FA926-BB0A-4925-8D5F-A31DE359918A}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_runningTurnSpeed
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Sprint Analog Input Threshold"),
					"sprintAnalogThreshold",
					"{73C9376F-6677-408D-99F5-2D2E6CFDB340}"_guid,
					MAKE_UNICODE_LITERAL("Input"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_sprintAnalogThreshold
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Planar Movement Plane Normal"),
					"movementPlaneNormal",
					"{A5CDB030-A7FF-4F6C-A897-AA4EC3D75107}"_guid,
					MAKE_UNICODE_LITERAL("Movement"),
					Reflection::PropertyFlags{},
					&GameFramework::Components::WalkingCharacterBase::m_movementPlaneNormal
				}
			}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Components::ThirdPersonCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::ThirdPersonCharacter>(
			GameFramework::Components::ThirdPersonCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Third Person Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Components::FirstPersonCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::FirstPersonCharacter>(
			GameFramework::Components::FirstPersonCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("First Person Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Components::RollingBallCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::RollingBallCharacter>(
			GameFramework::Components::RollingBallCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Rolling Ball Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::Components::PlanarMovementCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Components::PlanarMovementCharacter>(
			GameFramework::Components::PlanarMovementCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Planar Movement Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid
			}}
		);
	};
}
