#include "Components/Signals/Transmitter.h"
#include "Components/Signals/Receiver.h"

#include <GameFramework/Components/Signals/Receiver.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/Asset/TagAssetType.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework::Signal
{
	Transmitter::Transmitter(const Transmitter& templateComponent, const Cloner& cloner)
		: m_receiver(
				templateComponent.m_receiver,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateParent().GetSceneRegistry(), cloner.GetSceneRegistry()}
			)
	{
	}

	Transmitter::Transmitter(const Deserializer& deserializer)
		: m_receiver(deserializer.m_reader.ReadWithDefaultValue<Entity::ComponentSoftReference>("receiver", {}, deserializer.GetSceneRegistry())
	    )
	{
	}
	Transmitter::Transmitter(Initializer&&)
	{
	}

	void Transmitter::Start(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Entity::Component3D*> pReceivingComponent = m_receiver.Find<Entity::Component3D>(sceneRegistry))
		{
			pReceivingComponent->IterateDataComponentsImplementingType<Receiver>(
				sceneRegistry,
				[this,
			   pReceivingComponent](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					Receiver& receiver = static_cast<Receiver&>(dataComponent);
					receiver.OnSignalReceived(*this, *pReceivingComponent);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void Transmitter::Stop(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Entity::Component3D*> pReceivingComponent = m_receiver.Find<Entity::Component3D>(sceneRegistry))
		{
			pReceivingComponent->IterateDataComponentsImplementingType<Receiver>(
				sceneRegistry,
				[this,
			   pReceivingComponent](Entity::Data::Component& dataComponent, const Optional<const Entity::ComponentTypeInterface*>, Entity::ComponentTypeSceneDataInterface&)
				{
					Receiver& receiver = static_cast<Receiver&>(dataComponent);
					receiver.OnSignalLost(*this, *pReceivingComponent);
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void Transmitter::SetReceiver(const Entity::Component3DPicker receiver)
	{
		m_receiver = receiver;
	}

	template<typename T>
	static Vector<Guid> FindTypeGuids()
	{
		const Guid componentTypeGuid = Reflection::GetTypeGuid<T>();

		Vector<Guid> typeGuids;
		typeGuids.EmplaceBack(componentTypeGuid);

		// Search for types that inherit from the given type
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		reflectionRegistry.IterateTypeInterfaces(
			[componentTypeGuid, &typeGuids](const Reflection::TypeInterface& typeInterface) -> Memory::CallbackResult
			{
				const Reflection::TypeInterface* pParentTypeInterface = typeInterface.GetParent();
				while (pParentTypeInterface)
				{
					if (pParentTypeInterface->GetGuid() == componentTypeGuid)
					{
						// Add all elements up to this point
						const Reflection::TypeInterface* pChildTypeInterface = &typeInterface;
						do
						{
							typeGuids.EmplaceBackUnique(pChildTypeInterface->GetGuid());
							pChildTypeInterface = pChildTypeInterface->GetParent();
						} while (pChildTypeInterface != pParentTypeInterface);
						break;
					}
					pParentTypeInterface = pParentTypeInterface->GetParent();
				}
				return Memory::CallbackResult::Continue;
			}
		);
		return typeGuids;
	}

	Entity::Component3DPicker Transmitter::GetReceiver(Entity::Component3D& owner) const
	{
		static Vector<Guid> TypeGuids = FindTypeGuids<ngine::GameFramework::Signal::Receiver>();

		Entity::Component3DPicker picker{m_receiver, owner.GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(TypeGuids);

		return picker;
	}

	[[maybe_unused]] const bool wasTransmitterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Transmitter>>::Make());
	[[maybe_unused]] const bool wasTransmitterTypeRegistered = Reflection::Registry::RegisterType<Transmitter>();
}
