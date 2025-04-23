#include "Components/LifetimeComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <GameFramework/Reset/ResetComponent.h>

#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	LifetimeComponent::LifetimeComponent(const LifetimeComponent& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_duration(templateComponent.m_duration)
	{
	}

	LifetimeComponent::LifetimeComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_duration(initializer.m_duration.GetSeconds())
	{
	}

	LifetimeComponent::LifetimeComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_duration(*deserializer.m_reader.Read<float>("duration"))
	{
	}

	void LifetimeComponent::OnCreated(Entity::Component3D& owner)
	{
		if (owner.IsEnabled() && owner.IsSimulationActive())
		{
			RegisterForUpdate(owner);
		}
	}

	void LifetimeComponent::OnDestroying(Entity::Component3D& owner)
	{
		if (owner.IsEnabled() && owner.IsSimulationActive())
		{
			DeregisterUpdate(owner);
		}
	}

	void LifetimeComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<LifetimeComponent>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<LifetimeComponent>();
		sceneData.EnableUpdate(*this);
	}

	void LifetimeComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<LifetimeComponent>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<LifetimeComponent>();
		sceneData.DisableUpdate(*this);
	}

	void LifetimeComponent::OnEnable(Entity::Component3D& owner)
	{
		if (owner.IsSimulationActive())
		{
			RegisterForUpdate(owner);
		}
	}

	void LifetimeComponent::OnDisable(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void LifetimeComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		if (owner.ShouldSaveToDisk() && !owner.HasAnyDataComponentsImplementingType<Data::Reset>(sceneRegistry))
		{
			owner.CreateDataComponent<Data::Reset>(
				sceneRegistry,
				Data::Reset::Initializer{
					Entity::Data::Component3D::DynamicInitializer{owner, sceneRegistry},
					[this](Entity::Component3D& owner)
					{
						if (owner.IsDisabled())
						{
							owner.EnableWithChildren();
						}
						m_current = 0_seconds;
					},
					owner
				}
			);
		}

		if (owner.IsEnabled())
		{
			RegisterForUpdate(owner);
		}
	}

	void LifetimeComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void LifetimeComponent::Update()
	{
		const FrameTime deltaTime = m_owner.GetCurrentFrameTime();
		m_current += deltaTime;

		if (m_current.GetSeconds() >= m_duration)
		{
			if (m_owner.ShouldSaveToDisk())
			{
				m_owner.DisableWithChildren();
			}
			else
			{
				m_owner.Destroy(m_owner.GetSceneRegistry());
			}
		}
	}

	[[maybe_unused]] const bool wasLifetimeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LifetimeComponent>>::Make());
	[[maybe_unused]] const bool wasLifetimeTypeRegistered = Reflection::Registry::RegisterType<LifetimeComponent>();
}
