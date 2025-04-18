#include "Components/SoundListenerComponent.h"
#include "OpenAL/OpenALListener.h"

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

namespace ngine::Audio
{
	SoundListenerComponent::SoundListenerComponent(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		Initialize();
	}

	SoundListenerComponent::SoundListenerComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
	{
		Initialize();
	}

	SoundListenerComponent::SoundListenerComponent(const SoundListenerComponent&, const Cloner& cloner)
		: m_owner(cloner.GetParent())
	{
		Initialize();
	}

	SoundListenerComponent::~SoundListenerComponent()
	{
	}

	void SoundListenerComponent::Initialize()
	{
		Assert(m_listener == nullptr);
		m_listener = UniquePtr<OpenALListener>::Make();

		m_owner.OnWorldTransformChangedEvent.Add(
			*this,
			[](SoundListenerComponent& listener, const EnumFlags<Entity::TransformChangeFlags> flags)
			{
				listener.OnOwnerTransformChanged(flags);
			}
		);

		ApplyAllProperties();
	}

	void SoundListenerComponent::ApplyAllProperties()
	{
		Assert(m_listener != nullptr);

		const Math::WorldTransform transform = m_owner.GetWorldTransform();
		m_listener->SetPosition(transform.GetLocation());
		m_listener->SetOrientation(transform.GetUpColumn(), transform.GetForwardColumn());
		m_listener->SetVolume(m_volume);
	}

	void SoundListenerComponent::OnOwnerTransformChanged([[maybe_unused]] const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		const Math::WorldTransform transform = m_owner.GetWorldTransform();
		m_listener->SetPosition(transform.GetLocation());
		m_listener->SetOrientation(transform.GetUpColumn(), transform.GetForwardColumn());
	}

	[[maybe_unused]] const bool wasSoundListenerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SoundListenerComponent>>::Make());
	[[maybe_unused]] const bool wasSoundListenerTypeRegistered = Reflection::Registry::RegisterType<SoundListenerComponent>();
}
