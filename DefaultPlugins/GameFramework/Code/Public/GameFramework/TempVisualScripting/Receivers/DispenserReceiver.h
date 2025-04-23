#pragma once

#include <Components/Signals/Receiver.h>

#include <Engine/Tag/TagMask.h>
#include <Engine/Tag/TagMaskProperty.h>
#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Engine/Entity/ComponentSoftReference.h>

#include <Common/Math/Vector3.h>
#include <Common/Time/Duration.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework::Signal::Receivers
{
	//! Receiver that spawns a component on signal
	struct Dispenser final : public Receiver
	{
		static constexpr Guid TypeGuid = "{8C064F53-1E0E-4C37-8D94-19465F6DA3EB}"_guid;

		using BaseType = Receiver;

		Dispenser(const Dispenser& templateComponent, const Cloner& cloner);
		Dispenser(const Deserializer& deserializer);
		Dispenser(Initializer&& initializer);

		void OnCreated(Entity::Component3D& owner);
	protected:
		friend struct Reflection::ReflectedType<Dispenser>;

		virtual void Activate(Entity::Component3D& owner) override final;
		virtual void Deactivate(Entity::Component3D& owner) override final;

		void SpawnComponent(Entity::Component3D& owner);
		void DestroySpawnedComponents(Entity::Component3D& owner);

		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);
	protected:
		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SetSpawnedComponentTags(const Tag::ModifiableMaskProperty tags);
		[[nodiscard]] Tag::ModifiableMaskProperty GetSpawnedComponentTags() const;
	protected:
		Entity::ComponentTemplateIdentifier m_spawnedComponentTemplateIdentifier;
		Time::Durationf m_spawnedComponentLifetime{0_seconds};
		Math::Vector3f m_spawnedComponentVelocity{Math::Zero};
		Tag::Mask m_spawnedComponentTags;

		InlineVector<Entity::ComponentSoftReference, 1> m_spawnedComponents;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Signal::Receivers::Dispenser>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Signal::Receivers::Dispenser>(
			GameFramework::Signal::Receivers::Dispenser::TypeGuid,
			MAKE_UNICODE_LITERAL("Dispenser Receiver"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Spawned Asset"),
					"spawnedObject",
					"{7E54DD2A-0FBF-4D5F-BB85-DBF54DA4A1FA}"_guid,
					MAKE_UNICODE_LITERAL("Dispenser Receiver"),
					&GameFramework::Signal::Receivers::Dispenser::SetSceneAsset,
					&GameFramework::Signal::Receivers::Dispenser::GetSceneAsset
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Lifetime"),
					"lifetime",
					"{912CC51B-43C9-43D3-906E-8E65A2ED00BB}"_guid,
					MAKE_UNICODE_LITERAL("Dispenser Receiver"),
					&GameFramework::Signal::Receivers::Dispenser::m_spawnedComponentLifetime
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Velocity"),
					"velocity",
					"{11D8F3CA-D4D7-479D-A6C9-BC74F7B8A2A4}"_guid,
					MAKE_UNICODE_LITERAL("Dispenser Receiver"),
					&GameFramework::Signal::Receivers::Dispenser::m_spawnedComponentVelocity
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Spawned Tags"),
					"spawnedTags",
					"{BE118146-9A07-49A7-9499-76231F3317C8}"_guid,
					MAKE_UNICODE_LITERAL("Dispenser Receiver"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::Signal::Receivers::Dispenser::SetSpawnedComponentTags,
					&GameFramework::Signal::Receivers::Dispenser::GetSpawnedComponentTags
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{Entity::ComponentTypeFlags(), {}, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid},
			}
		);
	};
}
