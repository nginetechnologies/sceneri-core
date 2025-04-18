#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Torque.h>

namespace ngine::Physics
{
	struct Vehicle;
	struct Wheel;

	struct Axle final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "6ab4b4e1-d1d8-481a-973d-3f6f7cc47e13"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		static const uint16 InvalidAxleIndex = Math::NumericLimits<uint16>::Max;

		Axle(const Axle& templateComponent, const Cloner& cloner);
		Axle(const Deserializer& deserializer);
		Axle(Initializer&& initializer);
		virtual ~Axle() = default;

		void OnCreated();
		void OnDestroying();

		[[nodiscard]] Math::Ratiof GetEnginePowerRatio() const
		{
			return m_enginePowerRatio;
		}

		[[nodiscard]] Math::Ratiof GetLeftRightPowerRatio() const
		{
			return m_leftRightPowerRatio;
		}

		[[nodiscard]] Math::Ratiof GetDifferentialRatio() const
		{
			return m_differentialRatio;
		}

		[[nodiscard]] Math::Torquef GetAntiRollBarStiffness() const
		{
			return m_antiRollBarStiffness;
		}

		[[nodiscard]] Optional<Wheel*> GetLeftWheel() const
		{
			return m_leftWheel;
		}

		[[nodiscard]] Optional<Wheel*> GetRightWheel() const
		{
			return m_rightWheel;
		}
	protected:
		friend struct Vehicle;
		friend struct Wheel;

		Axle(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);
		void AddWheel(Wheel& wheel);
		void RemoveWheel(Wheel& wheel);

		uint16 m_vehicleAxleIndex{InvalidAxleIndex};
	private:
		friend struct Reflection::ReflectedType<Physics::Axle>;
		Math::Ratiof m_enginePowerRatio = 0.3f;
		Math::Ratiof m_leftRightPowerRatio = 0.5f;
		Math::Ratiof m_differentialRatio = 3.42f;
		Math::Torquef m_antiRollBarStiffness = 500_newtonmeters;

		Optional<Wheel*> m_leftWheel = Invalid;
		Optional<Wheel*> m_rightWheel = Invalid;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Axle>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Axle>(
			Physics::Axle::TypeGuid,
			MAKE_UNICODE_LITERAL("Axle"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Engine Power Ratio"),
					"powerRatio",
					"{F5D1F0BC-D480-4EAA-A87F-70062E25BE80}"_guid,
					MAKE_UNICODE_LITERAL("Axle"),
					&Physics::Axle::m_enginePowerRatio
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Left/Right Power Split"),
					"powerSplit",
					"{C0FD83B2-716A-49E2-88E4-07D8512884A8}"_guid,
					MAKE_UNICODE_LITERAL("Axle"),
					&Physics::Axle::m_leftRightPowerRatio
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Differential Ratio"),
					"differentialRatio",
					"{385D40D0-5945-4002-8B96-EBB002BF474C}"_guid,
					MAKE_UNICODE_LITERAL("Axle"),
					&Physics::Axle::m_differentialRatio
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Anti-Roll Bar Stiffness"),
					"antiRollBarStiffness",
					"{6C081AC7-A23C-4CCB-8071-263C11091EDF}"_guid,
					MAKE_UNICODE_LITERAL("Axle"),
					&Physics::Axle::m_antiRollBarStiffness
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "01947305-2c51-7183-8885-728ef2788530"_asset, "ed2cac99-a0bc-4793-bb03-f47dadecdcf9"_guid
				},
				Entity::IndicatorTypeExtension{
					"b463d9cc-120d-4046-a21c-ae12c425a090"_guid,
				}
			}
		);
	};
}
