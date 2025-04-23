#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Asset/Picker.h>
#include <Common/Function/Event.h>

namespace ngine::GameFramework::Signal
{
	struct Transmitter final : public Entity::Data::Component3D
	{
		static constexpr Guid TypeGuid = "ef986f60-a987-4434-9e88-254094e36e9c"_guid;

		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using Initializer = BaseType::DynamicInitializer;

		Transmitter(const Transmitter& templateComponent, const Cloner& cloner);
		Transmitter(const Deserializer& deserializer);
		Transmitter(Initializer&& initializer);

		void Start(Entity::Component3D& owner);
		void Stop(Entity::Component3D& owner);
	protected:
		friend struct Reflection::ReflectedType<Transmitter>;

		// TODO: Revisit when we have multipicker
		void SetReceiver(const Entity::Component3DPicker receiver);
		Entity::Component3DPicker GetReceiver(Entity::Component3D& owner) const;

		// TODO: Make this type safe
		Entity::ComponentSoftReference m_receiver;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Transmitter>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Transmitter>(
			GameFramework::Signal::Transmitter::TypeGuid,
			MAKE_UNICODE_LITERAL("Transmitter"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Receiver"),
				"receiver",
				"{302CE803-A7FD-4E0A-9A38-1B04DC552E59}"_guid,
				MAKE_UNICODE_LITERAL("Transmitter"),
				Reflection::PropertyFlags::VisibleToParentScope,
				&GameFramework::Signal::Transmitter::SetReceiver,
				&GameFramework::Signal::Transmitter::GetReceiver
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{Entity::ComponentTypeFlags(), {}, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid},
			}
		);
	};
}
