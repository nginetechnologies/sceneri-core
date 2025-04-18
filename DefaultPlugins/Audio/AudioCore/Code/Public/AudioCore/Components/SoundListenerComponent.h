#pragma once

#include "../AudioListener.h"

#include <Common/Memory/UniquePtr.h>

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Data/Component3D.h>

namespace ngine::Audio
{
	struct SoundListenerComponent : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using Initializer = Entity::Data::Component3D::DynamicInitializer;

		SoundListenerComponent(Initializer&& initializer);
		SoundListenerComponent(const SoundListenerComponent&, const Cloner& cloner);
		SoundListenerComponent(const Deserializer& deserializer);
		virtual ~SoundListenerComponent();
	protected:
		void Initialize();
		void ApplyAllProperties();
		void OnOwnerTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags);
	protected:
		friend struct Reflection::ReflectedType<SoundListenerComponent>;
	protected:
		Entity::Component3D& m_owner;
		UniquePtr<Listener> m_listener = nullptr;

		Volume m_volume{100_percent};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Audio::SoundListenerComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Audio::SoundListenerComponent>(
			"2298dbf2-750e-4534-8e9a-891e56cdad00"_guid,
			MAKE_UNICODE_LITERAL("Sound Listener"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Volume"),
					"volume",
					"{30C154AD-E12A-4423-8260-57D9E52D43B1}"_guid,
					MAKE_UNICODE_LITERAL("Sound"),
					&Audio::SoundListenerComponent::m_volume
				),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				"eebed34e-7cff-7a0b-742a-f92bef66a445"_asset,
			}}
		);
	};
}
