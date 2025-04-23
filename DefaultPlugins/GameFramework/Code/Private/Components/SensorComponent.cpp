#include "Components/SensorComponent.h"
#include "Tags.h"

#include "PhysicsCore/Layer.h"
#include <PhysicsCore/Contact.h>
#include <PhysicsCore/Components/ColliderComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	SensorComponent::SensorComponent(const Deserializer& deserializer)
		: Component3D(deserializer)
	{
		if (!HasDataComponentOfType<Physics::Data::Body>(GetSceneRegistry()))
		{
			[[maybe_unused]] Optional<Physics::Data::Body*> pPhysicsBody =
				CreateDataComponent<Physics::Data::Body>(Physics::Data::Body::Initializer{
					Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
					Physics::Data::Body::Settings(
						Physics::Data::Body::Type::Static,
						Physics::Layer::Triggers,
						Physics::Data::Body::Settings().m_maximumAngularVelocity,
						Physics::Data::Body::Settings().m_overriddenMass,
						Physics::Data::Body::Settings().m_gravityScale,
						Physics::Data::Body::Flags::IsSensorOnly
					)
				});
			Assert(pPhysicsBody.IsValid());
		}
	}

	SensorComponent::SensorComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_tagMask(initializer.m_tagMask)
	{
		// Default to player only
		m_tagMask.Set(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid));

		[[maybe_unused]] Optional<Physics::Data::Body*> pPhysicsBody =
			CreateDataComponent<Physics::Data::Body>(Physics::Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
				Physics::Data::Body::Settings(
					Physics::Data::Body::Type::Static,
					Physics::Layer::Triggers,
					Physics::Data::Body::Settings().m_maximumAngularVelocity,
					Physics::Data::Body::Settings().m_overriddenMass,
					Physics::Data::Body::Settings().m_gravityScale,
					Physics::Data::Body::Flags::IsSensorOnly
				)
			});
		Assert(pPhysicsBody.IsValid());
	}

	SensorComponent::SensorComponent(const SensorComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_tagMask(templateComponent.m_tagMask)
	{
	}

	void SensorComponent::OnCreated()
	{
		if (const Optional<Physics::Data::Body*> pBody = FindDataComponentOfType<Physics::Data::Body>())
		{
			pBody->OnContactFound.Add<&SensorComponent::OnBeginContactInternal>(*this);
			pBody->OnContactLost.Add<&SensorComponent::OnLoseContactInternal>(*this);
		}
	}

	[[nodiscard]] Tag::Mask GetContactTags(const Physics::Contact& contact)
	{
		Tag::Mask tags;

		// Try to get tags of the collider
		if (contact.otherCollider.IsValid())
		{
			Optional<Entity::Data::Tags*> pTagComponent = contact.otherCollider->FindDataComponentOfType<Entity::Data::Tags>();
			if (pTagComponent.IsValid())
			{
				tags |= pTagComponent->GetMask();
			}
		}

		if (contact.otherComponent.IsValid())
		{
			// Now try to get tags of a body
			Optional<Entity::Data::Tags*> pTagComponent = contact.otherComponent->FindDataComponentOfType<Entity::Data::Tags>();
			if (pTagComponent.IsValid())
			{
				tags |= pTagComponent->GetMask();
			}
		}

		return tags;
	}

	void SensorComponent::OnBeginContactInternal(const Physics::Contact& contact)
	{
		if (m_tagMask.AreAnySet())
		{
			if ((m_tagMask & GetContactTags(contact)).AreNoneSet())
			{
				return;
			}
		}

		OnComponentDetected(*this, contact.otherComponent);

		if (contact.otherComponent)
		{
			const Entity::ComponentIdentifier identifier = contact.otherComponent->GetIdentifier();
			if (!m_colliding.IsSet(identifier))
			{
				m_colliding.Set(contact.otherComponent->GetIdentifier());
				OnComponentEntered(*this, contact.otherComponent);
			}
		}
	}

	void SensorComponent::OnLoseContactInternal(const Physics::Contact& contact)
	{
		if (m_tagMask.AreAnySet())
		{
			if ((m_tagMask & GetContactTags(contact)).AreNoneSet())
			{
				return;
			}
		}

		if (contact.otherComponent)
		{
			const Entity::ComponentIdentifier identifier = contact.otherComponent->GetIdentifier();
			if (m_colliding.IsSet(identifier))
			{
				m_colliding.Clear(identifier);
				OnComponentLost(*this, contact.otherComponent);
			}
		}
	}

	void SensorComponent::SetTags(const Tag::QueryableMaskProperty tags)
	{
		m_tagMask = tags.m_mask;
	}

	Tag::QueryableMaskProperty SensorComponent::GetTags() const
	{
		return Tag::QueryableMaskProperty{m_tagMask, System::Get<Tag::Registry>()};
	}

	[[maybe_unused]] const bool wasSensorComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SensorComponent>>::Make());
	[[maybe_unused]] const bool wasSensorComponentTypeRegistered = Reflection::Registry::RegisterType<SensorComponent>();
}
