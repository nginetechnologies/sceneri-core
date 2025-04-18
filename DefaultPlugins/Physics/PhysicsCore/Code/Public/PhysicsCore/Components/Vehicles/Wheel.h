#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Torque.h>
#include <Common/Math/Length.h>
#include <Common/Math/Radius.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Function/Event.h>

namespace ngine::Physics
{
	struct Vehicle;
	struct Axle;

	struct Wheel final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "a693a6f2-46a7-4c47-a90a-957ce3fd686f"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		static const uint16 InvalidWheelIndex = Math::NumericLimits<uint16>::Max;

		Wheel(const Wheel& templateComponent, const Cloner& cloner);
		Wheel(const Deserializer& deserializer);
		Wheel(Initializer&& initializer);
		virtual ~Wheel() = default;

		void OnCreated();
		void OnDestroying();

		void SetRadius(Math::Lengthf radius)
		{
			m_radius = radius;
			OnDimensionsChanged();
		}
		[[nodiscard]] Math::Lengthf GetRadius() const
		{
			return m_radius;
		}

		void SetWidth(Math::Lengthf width)
		{
			m_width = width;
			OnDimensionsChanged();
		}
		[[nodiscard]] Math::Lengthf GetWidth() const
		{
			return m_width;
		}

		void SetSuspensionLength(Math::Lengthf minimum, Math::Lengthf maximum)
		{
			m_suspensionMinimumLength = minimum;
			m_suspensionMaximumLength = maximum;
		}
		[[nodiscard]] Math::Lengthf GetSuspensionMinimumLength() const
		{
			return m_suspensionMinimumLength;
		}
		[[nodiscard]] Math::Lengthf GetSuspensionMaximumLength() const
		{
			return m_suspensionMaximumLength;
		}

		void SetSuspensionProperties(float stiffness, float damping)
		{
			m_suspensionStiffness = stiffness;
			m_suspensionDamping = damping;
		}
		[[nodiscard]] float GetSuspensionStiffness() const
		{
			return m_suspensionStiffness;
		}
		[[nodiscard]] float GetSuspensionDamping() const
		{
			return m_suspensionDamping;
		}

		void SetMaximumSteeringAngle(Math::Anglef angle)
		{
			m_maximumSteeringAngle = angle;
		}
		[[nodiscard]] Math::Anglef GetMaximumSteeringAngle() const
		{
			return m_maximumSteeringAngle;
		}

		void SetMaximumHandbrakeTorque(Math::Torquef torque)
		{
			m_handbrakeMaximumTorque = torque;
		}
		[[nodiscard]] Math::Torquef GetMaximumHandbrakeTorque() const
		{
			return m_handbrakeMaximumTorque;
		}

		void SetForwardGrip(Math::Ratiof grip)
		{
			m_forwardGrip = grip;
		}
		[[nodiscard]] Math::Ratiof GetForwardGrip() const
		{
			return m_forwardGrip;
		}

		void SetSidewaysGrip(Math::Ratiof grip)
		{
			m_sidewaysGrip = grip;
		}
		[[nodiscard]] Math::Ratiof GetSidewaysGrip() const
		{
			return m_sidewaysGrip;
		}

		[[nodiscard]] uint16 GetIndex() const
		{
			return m_wheelIndex;
		}

		Event<void(void*), 24> OnDimensionsChanged;
	protected:
		Wheel(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);

		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		friend Vehicle;
		friend Axle;
		uint16 m_wheelIndex{InvalidWheelIndex};
		Optional<Vehicle*> m_pVehicle = nullptr;
	private:
		friend struct Reflection::ReflectedType<Physics::Wheel>;
		Math::Lengthf m_radius = 0.2_meters;
		Math::Lengthf m_width = 0.4_meters;
		Math::Lengthf m_suspensionMinimumLength = 0.1_meters;
		Math::Lengthf m_suspensionMaximumLength = 0.3_meters;
		float m_suspensionStiffness = 1.5f;
		float m_suspensionDamping = 0.5f;
		Math::Anglef m_maximumSteeringAngle = 0.3_degrees;
		Math::Torquef m_handbrakeMaximumTorque = 4000_newtonmeters;
		Math::Ratiof m_forwardGrip = 1.f;
		Math::Ratiof m_sidewaysGrip = 1.f;
		Math::Quaternionf m_defaultRotation{Math::Identity};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Wheel>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Wheel>(
			Physics::Wheel::TypeGuid,
			MAKE_UNICODE_LITERAL("Wheel"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Steering Angle"),
					"maxSteeringAngle",
					"{89450E3D-76E5-457C-8CDC-12D8D2AA8798}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_maximumSteeringAngle
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"{5AF8E2BA-EA70-4435-81E2-1F54F8474F73}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::SetRadius,
					&Physics::Wheel::GetRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Width"),
					"width",
					"{1EDF3337-AF8C-4F5C-8512-2F7BA60023F2}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::SetWidth,
					&Physics::Wheel::GetWidth
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Minimum Suspension Length"),
					"minSuspensionLength",
					"{5AF8E2BA-EA70-4435-81E2-1F54F8474F73}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_suspensionMinimumLength
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Suspension Length"),
					"maxSuspensionLength",
					"{27005CF3-7927-4E67-97A8-B739A6DA7248}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_suspensionMaximumLength
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Suspension Stiffness"),
					"suspensionStiffness",
					"{FB2503A1-947D-4D6C-8179-8CB4C0D4450F}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_suspensionStiffness
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Suspension Damping"),
					"suspensionDamping",
					"{50EE8EFA-D560-4BB9-B59B-BFF8230F02F4}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_suspensionDamping
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Handbrake Torque"),
					"maxHandbrakeTorque",
					"{6183096F-33EE-4BD2-B6E9-6043D144C910}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_handbrakeMaximumTorque
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Forward Grip"),
					"forwardGrip",
					"{6EDC9A6A-F1AB-48CD-AEBC-7FC376A2814A}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_forwardGrip
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Sideways Grip"),
					"sidewaysGrip",
					"{AD1EC3BF-A339-4DCC-A6B9-D728E32372DF}"_guid,
					MAKE_UNICODE_LITERAL("Wheel"),
					&Physics::Wheel::m_sidewaysGrip
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "01947303-1399-7996-09a7-2b7967c6f84a"_asset, "ed2cac99-a0bc-4793-bb03-f47dadecdcf9"_guid
				},
				Entity::IndicatorTypeExtension{
					"a5f453ba-0528-4f89-bf11-67fc0a060f97"_guid,
				}
			}
		);
	};
}
