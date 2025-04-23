#include "TempVisualScripting/Transmitters/SensorTransmitter.h"
#include "Tags.h"

#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Scene/SceneComponent.h>

#include <Engine/Entity/Serialization/ComponentReference.h>

#include <Engine/Tag/TagRegistry.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Components/Player/Player.h>
#include <Components/Signals/Transmitter.h>

namespace ngine::GameFramework::Signal::Transmitters
{
	SensorTransmitter::SensorTransmitter(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
		CreateDataComponent<Transmitter>(Transmitter::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()}});
	}

	SensorTransmitter::SensorTransmitter(const SensorTransmitter& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
	{
	}

	SensorTransmitter::SensorTransmitter(const Deserializer& deserializer)
		: SensorComponent(deserializer)
	{
	}

	void SensorTransmitter::OnCreated()
	{
		SensorComponent::OnCreated();

		SensorComponent::OnComponentDetected.Add(*this, &SensorTransmitter::OnComponentDetected);
		SensorComponent::OnComponentLost.Add(*this, &SensorTransmitter::OnComponentLost);
	}

	void SensorTransmitter::OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*>)
	{
		const Optional<Transmitter*> pTransmitter = FindDataComponentOfType<Transmitter>();
		Assert(pTransmitter.IsValid());
		if (LIKELY(pTransmitter.IsValid()))
		{
			pTransmitter->Start(*this);
		}
	}

	void SensorTransmitter::OnComponentLost(SensorComponent&, Optional<Entity::Component3D*>)
	{
		const Optional<Transmitter*> pTransmitter = FindDataComponentOfType<Transmitter>();
		Assert(pTransmitter.IsValid());
		if (LIKELY(pTransmitter.IsValid()))
		{
			pTransmitter->Stop(*this);
		}
	}

	[[maybe_unused]] const bool wasSensorTransmitterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SensorTransmitter>>::Make());
	[[maybe_unused]] const bool wasSensorTransmitterTypeRegistered = Reflection::Registry::RegisterType<SensorTransmitter>();
}
