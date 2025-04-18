#pragma once

#include "BodyComponent.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>
#include <PhysicsCore/Components/CharacterBase.h>
#include <PhysicsCore/ConstraintIdentifier.h>

#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/Containers/CircularBuffer.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Time/Timestamp.h>
#include <Common/Threading/Mutexes/Mutex.h>

namespace JPH
{
	class PhysicsMaterial;
}

namespace ngine::Physics
{
	struct Material;
	struct ColliderComponent;

	namespace Data
	{
		struct Scene3D;
		struct Body;
	}

	struct RigidbodyCharacter final : public CharacterBase
	{
		static constexpr Guid TypeGuid = "1d21d79c-5708-46c9-8e57-ee9e86476b80"_guid;

		using BaseType = CharacterBase;
		using InstanceIdentifier = TIdentifier<uint32, 6>;

		RigidbodyCharacter(Initializer&& initializer);
		RigidbodyCharacter(const RigidbodyCharacter& templateComponent, const Cloner& cloner);
		RigidbodyCharacter(const Deserializer& deserializer);
		virtual ~RigidbodyCharacter();

		void BeforePhysicsUpdate();
		void FixedPhysicsUpdate();
		void OnCreated();
		void OnDestroying();
		void OnEnable();
		void OnDisable();

		virtual void Jump(const Math::Vector3f acceleration) override;

		[[nodiscard]] virtual GroundState GetGroundState() const override;
		[[nodiscard]] virtual Math::Vector3f GetGroundNormal() const override;
		[[nodiscard]] virtual Optional<Entity::Component3D*> GetGroundComponent() const override;
		[[nodiscard]] JPH::BodyID GetGroundBodyID() const
		{
			return m_latestState.groundBodyID;
		}
	protected:
		friend struct Reflection::ReflectedType<RigidbodyCharacter>;
		friend struct Data::Scene;

		struct RemoteState
		{
			Math::Vector3f m_position{Math::Zero};
			Math::Vector3f m_velocity{Math::Zero};
			Time::Timestamp m_timestamp;
		};
		struct State
		{
			Time::Timestamp currentTimestamp;
			Math::Vector3f gravity{Math::Zero};
			Math::Vector3f groundBodyVelocity = Math::Zero;
			Math::Vector3f groundPosition = Math::Zero;
			Math::Vector3f groundNormal{Math::Up};
			RemoteState remoteState;
			JPH::BodyID groundBodyID;
			GroundState groundState{GroundState::InAir};
			JPH::RefConst<JPH::PhysicsMaterial> groundMaterial;
			float groundDetectionBypassTime = 0.f;
			float targetHeight = 0.f;
		};

		void PreTick(State& state, const MovementRequest& movementRequest);
		void DetectGroundBody(State& state, const MovementRequest& movementRequest);
		void PostTick(const Time::Timestamp currentTime, const MovementRequest& movementRequest);

		[[nodiscard]] Math::Vector3f GetHostPosition() const;
		void SetHostPosition(const Math::Vector3f position);
		[[nodiscard]] Math::Vector3f GetHostVelocity() const;
		void SetHostVelocity(const Math::Vector3f velocity);
		[[nodiscard]] Time::Timestamp GetHostStateTimestamp() const;
		void SetHostStateTimestamp(const Time::Timestamp timestamp);

		struct HistoryEntry
		{
			MovementRequest movementRequest;
			State state;
		};
		[[nodiscard]] HistoryEntry GetHistoryEntry(const Time::Timestamp timestamp) const;
	protected:
		//! Maximum angle the character can walk on, past this they will slide
		Math::Anglef m_maximumWalkableAngle = 50_degrees;
		//! Maximum ground angle the character can be considered to stand on, past this we will consider them in air
		Math::Anglef m_maximumGroundAngle = 50_degrees;

		// How strong acceleration should be
		float m_accelerationStrength = 30.f;
		// Amount of in air controls
		float m_inAirControl = 0.04f;
		// Height of character steps
		float m_stepUpHeight = 0.5f;
		// Speed at which character will step up
		float m_stepUpSpeed = 2.f;
		// Speed at which character will descend
		float m_stepDownSpeed = 1.f;
		// Collider height above the ground
		float m_colliderHeightAboveGround = 0.1f;
	private:
		ConstraintIdentifier m_constraintIdentifier;

		State m_latestState;

		RemoteState m_latestLocalState;
		RemoteState m_latestRemoteState;

		Threading::Mutex m_remoteStateMutex;
		RemoteState m_nextRemoteState;
		Time::Timestamp m_lastResolvedHostStateTimestamp;
		Vector<RemoteState> m_queuedRemoteStates;

		// Store one second's worth of ticks in history, given 60 tick rate
		inline static constexpr uint8 HistorySize = 60;
		FixedCircularBuffer<HistoryEntry, HistorySize> m_history;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::RigidbodyCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::RigidbodyCharacter>(
			Physics::RigidbodyCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Rigidbody Character"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Walkable Angle"),
					"maxWalkableAngle",
					"{250F4E1F-7C6E-4159-A25C-1DF8A4EC727D}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_maximumWalkableAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Ground Angle"),
					"maxGroundAngle",
					"{EC982DA0-A73F-4AA8-87D4-D9FCCFF7F423}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_maximumGroundAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Acceleration Strength"),
					"accelStrength",
					"{EEC8B7C1-0D0A-450D-93F9-BE8F966627CB}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_accelerationStrength
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("In Air Control Amount"),
					"inAirControl",
					"{5C84B8E9-B5D4-462D-9A71-29E46132DCCB}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_inAirControl
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Step Up Height"),
					"stepUpHeight",
					"{EC531AAF-1F40-45F4-BEB7-2699E9D9AA62}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_stepUpHeight
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Step Up Speed"),
					"stepUpSpeed",
					"{E8E6BFEB-F5F7-4F3C-B63B-4EF0EDBE6311}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_stepUpSpeed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Step Down Speed"),
					"stepDownSpeed",
					"{F92BA85A-73D1-42D0-8580-EF8B8710B7A1}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_stepDownSpeed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Collider Height Above Ground"),
					"colliderHeightAboveGround",
					"{2C46EB55-882C-4D37-9E3A-D45EAE5DC5E7}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					&Physics::RigidbodyCharacter::m_colliderHeightAboveGround
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Host Position"),
					"hostPosition",
					"{90E648CB-5215-4A4B-B893-71CA49FFFA1D}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateHostToClient,
					&Physics::RigidbodyCharacter::SetHostPosition,
					&Physics::RigidbodyCharacter::GetHostPosition
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Host Velocity"),
					"hostVelocity",
					"{A0019834-1A1B-4320-9E4A-A6B1C69512AD}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateHostToClient,
					&Physics::RigidbodyCharacter::SetHostVelocity,
					&Physics::RigidbodyCharacter::GetHostVelocity
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Host State Timestamp"),
					"hostStateTimestamp",
					"{4A6B19D6-C914-4FE0-A127-36E5F60371D9}"_guid,
					MAKE_UNICODE_LITERAL("Rigidbody Character"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::HideFromUI | Reflection::PropertyFlags::PropagateHostToClient,
					&Physics::RigidbodyCharacter::SetHostStateTimestamp,
					&Physics::RigidbodyCharacter::GetHostStateTimestamp
				),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
			}}
		);
	};
}
