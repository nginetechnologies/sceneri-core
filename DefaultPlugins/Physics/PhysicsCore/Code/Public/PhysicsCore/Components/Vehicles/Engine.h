#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Time/Duration.h>
#include <Common/Math/Torque.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine::Physics
{
	struct Vehicle;
	struct Wheel;

	struct Engine final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "d91b1554-ed01-40a7-90eb-638fdb6e8ce0"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		Engine(const Engine& templateComponent, const Cloner& cloner);
		Engine(const Deserializer& deserializer);
		Engine(Initializer&& initializer);
		virtual ~Engine() = default;

		void OnCreated();
		void OnDestroying();

		[[nodiscard]] Math::RotationalSpeedf GetMinimumRPM() const
		{
			return m_minimumRPM;
		}

		[[nodiscard]] Math::RotationalSpeedf GetMaximumRPM() const
		{
			return m_maximumRPM;
		}

		[[nodiscard]] Math::Torquef GetMaximumTorque() const
		{
			return m_maximumTorque;
		}

		[[nodiscard]] ArrayView<const float> GetGearRatios() const
		{
			return m_gearRatios.GetView();
		}

		[[nodiscard]] float GetReverseGearRatio() const
		{
			return m_reverseGearRatio;
		}

		[[nodiscard]] Time::Durationf GetGearSwitchTime() const
		{
			return m_gearSwitchTime;
		}

		[[nodiscard]] Time::Durationf GetClutchReleaseTime() const
		{
			return m_clutchReleaseTime;
		}

		[[nodiscard]] Math::RotationalSpeedf GetShiftUpRPM() const
		{
			return m_shiftUpRPM;
		}

		[[nodiscard]] Math::RotationalSpeedf GetShiftDownRPM() const
		{
			return m_shiftDownRPM;
		}
	private:
		Engine(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);
	private:
		friend struct Reflection::ReflectedType<Physics::Engine>;
		Math::RotationalSpeedf m_minimumRPM = 400_rpm;
		Math::RotationalSpeedf m_maximumRPM = 600_rpm;
		Math::Torquef m_maximumTorque = 2000_newtonmeters;

		// Gearbox
		InlineVector<float, 5> m_gearRatios = {2.66f, 1.78f, 1.3f, 1.0f, 0.74f};
		float m_reverseGearRatio = -2.9f;
		Time::Durationf m_gearSwitchTime = 0.05_seconds;
		Time::Durationf m_clutchReleaseTime = 0.05_seconds;
		Math::RotationalSpeedf m_shiftUpRPM = 550_rpm;
		Math::RotationalSpeedf m_shiftDownRPM = 500_rpm;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Engine>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Engine>(
			Physics::Engine::TypeGuid,
			MAKE_UNICODE_LITERAL("Engine"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Minimum RPM"),
					"minRPM",
					"{7E19E24D-F1FE-4310-8BA4-9180A181569D}"_guid,
					MAKE_UNICODE_LITERAL("Engine"),
					&Physics::Engine::m_minimumRPM
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum RPM"),
					"maxRPM",
					"{939190B7-F0AB-4859-A8AE-EE3139991E3C}"_guid,
					MAKE_UNICODE_LITERAL("Engine"),
					&Physics::Engine::m_maximumRPM
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Torque"),
					"maxTorque",
					"{F23FD76C-0109-4DEB-B6A1-D88A45C29FF3}"_guid,
					MAKE_UNICODE_LITERAL("Engine"),
					&Physics::Engine::m_maximumTorque
				),
				// TODO: Vector as a property doesn't compile currently
		    // Reflection::MakeProperty(
		    // 	MAKE_UNICODE_LITERAL("Gear Ratios"),
		    // 	MAKE_UNICODE_LITERAL("gearRatios"),
		    // 	MAKE_UNICODE_LITERAL("Transmission"),
		    // 	&Physics::Engine::m_gearRatios
		    // ),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Reverse Gear Ratio"),
					"reverseGearRatio",
					"{6A898460-514A-4D47-A45B-99574A7B0A9F}"_guid,
					MAKE_UNICODE_LITERAL("Transmission"),
					&Physics::Engine::m_reverseGearRatio
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Gear Switch Time"),
					"gearSwitchTime",
					"{03CA116E-EC8D-4CD5-A945-3FA4382C1909}"_guid,
					MAKE_UNICODE_LITERAL("Transmission"),
					&Physics::Engine::m_gearSwitchTime
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Clutch Release Time"),
					"clutchReleaseTime",
					"{8E1D5F2A-1B74-472F-960E-71A5DB74C8BB}"_guid,
					MAKE_UNICODE_LITERAL("Transmission"),
					&Physics::Engine::m_clutchReleaseTime
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Shift Up RPM"),
					"shiftUpRPM",
					"{23B2FA6B-BC3D-43EE-BAA4-6282C569E373}"_guid,
					MAKE_UNICODE_LITERAL("Transmission"),
					&Physics::Engine::m_shiftUpRPM
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Shift Down RPM"),
					"shiftDownRPM",
					"{5561702C-291B-458B-8263-AFCCCBBCE023}"_guid,
					MAKE_UNICODE_LITERAL("Transmission"),
					&Physics::Engine::m_shiftDownRPM
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "01947303-c1b3-7781-b57d-e8add026a3f2"_asset, "ed2cac99-a0bc-4793-bb03-f47dadecdcf9"_guid
			}}
		);
	};
}
