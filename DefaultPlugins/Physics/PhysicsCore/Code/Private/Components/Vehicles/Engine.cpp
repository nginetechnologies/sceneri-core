#include "PhysicsCore/Components/Vehicles/Engine.h"
#include <PhysicsCore/Components/Vehicles/Vehicle.h>
#include <PhysicsCore/Components/Vehicles/Wheel.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>

namespace ngine::Physics
{
	Engine::Engine(const Engine& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_minimumRPM(templateComponent.m_minimumRPM)
		, m_maximumRPM(templateComponent.m_maximumRPM)
		, m_maximumTorque(templateComponent.m_maximumTorque)
		, m_gearRatios(templateComponent.m_gearRatios)
		, m_reverseGearRatio(templateComponent.m_reverseGearRatio)
		, m_gearSwitchTime(templateComponent.m_gearSwitchTime)
		, m_clutchReleaseTime(templateComponent.m_clutchReleaseTime)
		, m_shiftUpRPM(templateComponent.m_shiftUpRPM)
		, m_shiftDownRPM(templateComponent.m_shiftDownRPM)
	{
	}

	Engine::Engine(const Deserializer& deserializer)
		: Engine(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<Engine>().ToString().GetView()))
	{
	}

	Engine::Engine(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: BaseType(deserializer)
		, m_minimumRPM(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::RotationalSpeedf>("minRPM", 400_rpm) : 400_rpm)
		, m_maximumRPM(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::RotationalSpeedf>("maxRPM", 600_rpm) : 600_rpm)
		, m_maximumTorque(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Torquef>("maxTorque", 2000_newtonmeters)
																 : Math::Torquef{2000_newtonmeters}
			)
		, m_reverseGearRatio(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<float>("reverseGearRatio", -2.9f) : -2.9f)
		, m_gearSwitchTime(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Time::Durationf>("gearSwitchTime", 0.05_seconds)
																 : Time::Durationf{0.05_seconds}
			)
		, m_clutchReleaseTime(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Time::Durationf>("clutchReleaseTime", 0.05_seconds)
																 : Time::Durationf{0.05_seconds}
			)
		, m_shiftUpRPM(typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::RotationalSpeedf>("shiftUpRPM", 550_rpm) : 550_rpm)
		, m_shiftDownRPM(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::RotationalSpeedf>("shiftDownRPM", 500_rpm) : 500_rpm
			)
	{
	}

	Engine::Engine(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
	}

	void Engine::OnCreated()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->SetEngine(*this);
		}
	}

	void Engine::OnDestroying()
	{
		if (const Optional<Vehicle*> pVehicle = FindFirstParentOfType<Vehicle>())
		{
			pVehicle->RemoveEngine(*this);
		}
	}

	[[maybe_unused]] const bool wasEngineRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Engine>>::Make());
	[[maybe_unused]] const bool wasEngineTypeRegistered = Reflection::Registry::RegisterType<Engine>();
}
