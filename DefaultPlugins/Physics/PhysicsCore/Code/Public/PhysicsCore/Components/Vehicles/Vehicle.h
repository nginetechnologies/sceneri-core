#pragma once

#include <PhysicsCore/Components/BodyComponent.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Vehicle/WheeledVehicleController.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>
#include <PhysicsCore/ConstraintIdentifier.h>

#include <Common/ForwardDeclarations/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine::Physics
{
	struct Wheel;
	struct Axle;
	struct Engine;

	struct Vehicle final : public BodyComponent
	{
		static constexpr Guid TypeGuid = "2fe2589c-79f4-4456-84e2-7b57af139e49"_guid;
		using BaseType = BodyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 10>;

		using AxleIndex = uint16;
		using WheelIndex = uint16;

		enum class Flags : uint8
		{
			UpdateEngineProperties = 1 << 0,
		};

		using BodyComponent::BodyComponent;
		Vehicle(Initializer&& initializer);
		Vehicle(const Vehicle& templateComponent, const Cloner& cloner);
		Vehicle(const Deserializer& deserializer);
		virtual ~Vehicle();

		void FixedPhysicsUpdate();
		void AfterPhysicsUpdate();

		void OnCreated();
		void OnEnable();
		void OnDisable();
		void OnSimulationResumed();
		void OnSimulationPaused();

		void SetAccelerationPedalPosition(const Math::Ratiof position)
		{
			m_accelerationPedalPosition = position;
		}

		[[nodiscard]] Math::Ratiof GetAccelerationPedalPosition() const
		{
			return m_accelerationPedalPosition;
		}

		void SetBrakePedalPosition(const Math::Ratiof position)
		{
			m_brakePedalPosition = position;
		}

		[[nodiscard]] Math::Ratiof GetBrakePedalPosition() const
		{
			return m_brakePedalPosition;
		}

		void SetSteeringWheelPosition(const Math::Ratiof leftRightPosition)
		{
			m_steeringWheelPosition = leftRightPosition;
		}

		[[nodiscard]] Math::Ratiof GetSteeringWheelPosition() const
		{
			return m_steeringWheelPosition;
		}

		void SetHandbrakePosition(const Math::Ratiof position)
		{
			m_handbrakePosition = position;
		}

		[[nodiscard]] Math::Ratiof GetHandbrakePosition() const
		{
			return m_handbrakePosition;
		}

		//! Ensures that vehicle won't "flip" more than a given angle
		void SetMaximumPitchRollAngle(const Math::Anglef value);

		[[nodiscard]] const Math::Vector3f GetVelocity() const;
		[[nodiscard]] Math::WorldTransform GetWheelTransform(const WheelIndex wheelIndex) const;
		[[nodiscard]] Math::WorldTransform GetDefaultWheelTransform(const WheelIndex wheelIndex) const;

		void SetEngine(Engine& engine);
		void RemoveEngine(Engine& engine);

		void AddAxle(Axle& axle);
		void RemoveAxle(const AxleIndex axleIndex);
		void AddWheel(Wheel& wheel, const Optional<Axle*> pAxle = Invalid);
		void RemoveWheel(const WheelIndex wheelIndex, const AxleIndex axleIndex = Math::NumericLimits<AxleIndex>::Max);

		void OnWheelTransformChanged(const Wheel& wheel);

		virtual void DebugDraw(JPH::DebugRendering::DebugRendererImp* pDebugRenderer) override;
	protected:
		void SetEngineInternal(JPH::VehicleConstraint& vehicleConstraint, Engine& engine);
		void AddAxleInternal(JPH::VehicleConstraint& vehicleConstraint, Axle& axle);
		void RemoveAxleInternal(JPH::VehicleConstraint& vehicleConstraint, const AxleIndex axleIndex);
		void AddWheelInternal(JPH::VehicleConstraint& vehicleConstraint, Wheel& wheel, const Optional<Axle*> pAxle);
		void RemoveWheelInternal(JPH::VehicleConstraint& vehicleConstraint, const WheelIndex wheelIndex, const AxleIndex axleIndex);
	private:
		friend struct Reflection::ReflectedType<Physics::Vehicle>;

		JPH::Ref<JPH::VehicleCollisionTesterCastSphere> m_collisionTester;
		ConstraintIdentifier m_vehicleConstraintIdentifier;

		struct QueuedWheel
		{
			Wheel& wheel;
			Optional<Axle*> pAxle;
		};

		InlineVector<ReferenceWrapper<Axle>, 2, AxleIndex> m_queuedAxleAdditions;
		InlineVector<QueuedWheel, 4, WheelIndex> m_queuedWheelAdditions;

		struct QueuedWheelRemoval
		{
			WheelIndex wheelIndex;
			AxleIndex axleIndex = Math::NumericLimits<AxleIndex>::Max;
		};

		InlineVector<AxleIndex, 2, AxleIndex> m_queuedAxleRemovals;
		InlineVector<QueuedWheelRemoval, 4, WheelIndex> m_queuedWheelRemovals;

		InlineVector<ReferenceWrapper<Axle>, 2, AxleIndex> m_axles;
		InlineVector<ReferenceWrapper<Wheel>, 4, WheelIndex> m_wheels;

		Optional<Engine*> m_pEngine;
		EnumFlags<Flags> m_flags = {};

		Math::Ratiof m_accelerationPedalPosition = 0.f;
		Math::Ratiof m_brakePedalPosition = 0.f;
		Math::Ratiof m_steeringWheelPosition = 0.f;
		Math::Ratiof m_handbrakePosition = 0.f;

		Math::Anglef m_maximumPitchRollAngle = Math::PI;
	};

	ENUM_FLAG_OPERATORS(Vehicle::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Vehicle>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Vehicle>(
			Physics::Vehicle::TypeGuid,
			MAKE_UNICODE_LITERAL("Vehicle"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "01947304-c7d6-7a8c-7cc4-5e62e714c3fb"_asset, "ed2cac99-a0bc-4793-bb03-f47dadecdcf9"_guid
			}}
		);
	};
}
