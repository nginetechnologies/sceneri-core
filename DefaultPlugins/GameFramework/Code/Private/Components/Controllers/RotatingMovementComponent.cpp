#include "Components/Controllers/RotatingMovementComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Math/Serialization/Range.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	RotatingMovementComponent::RotatingMovementComponent(const RotatingMovementComponent& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_velocity(templateComponent.m_velocity)
		, m_rotationAxis(templateComponent.m_rotationAxis)
		, m_rotationLimit(templateComponent.m_rotationLimit)
		, m_mode(templateComponent.m_mode)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	RotatingMovementComponent::RotatingMovementComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	RotatingMovementComponent::RotatingMovementComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	RotatingMovementComponent::~RotatingMovementComponent() = default;

	void RotatingMovementComponent::OnCreated(Entity::Component3D&)
	{
	}

	void RotatingMovementComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void RotatingMovementComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<RotatingMovementComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<RotatingMovementComponent>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void RotatingMovementComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<RotatingMovementComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<RotatingMovementComponent>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void RotatingMovementComponent::OnEnable(Entity::Component3D&)
	{
	}
	void RotatingMovementComponent::OnDisable(Entity::Component3D&)
	{
	}

	void RotatingMovementComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		RegisterForUpdate(owner);
	}
	void RotatingMovementComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void RotatingMovementComponent::AfterPhysicsUpdate()
	{
		if (m_velocity != 0.f)
		{
			const FrameTime deltaTime = m_owner.GetCurrentFrameTime();
			const bool isMovementLimited = m_rotationLimit.GetSize() != 1.f;
			if (!isMovementLimited)
			{
				const Math::Quaternionf oldRotation = m_owner.GetWorldRotation();
				const Math::YawPitchRollf deltaRotation =
					Math::YawPitchRollf::FromDegrees(m_rotationAxis.GetNormalizedSafe(Math::Up).zxy() * m_velocity * deltaTime);
				const Math::Quaternionf finalRotation = Math::Quaternionf(deltaRotation);

				Math::Quaternionf newRotation = oldRotation.TransformRotation(finalRotation);
				newRotation.m_vector.Normalize();

				m_owner.SetWorldRotation(newRotation);
			}
			else
			{
				Math::Anglef newRotation = m_currentRotation + Math::Anglef::FromDegrees(m_velocity * deltaTime);

				switch (m_mode)
				{
					case Mode::Single:
					{
						newRotation = Math::Anglef::FromDegrees(m_rotationLimit.GetClampedValue(newRotation.GetDegrees()));
					}
					break;

					case Mode::Loop:
					{
						const Math::Anglef maximumRotation = Math::Anglef::FromDegrees(m_rotationLimit.GetMaximum());
						const Math::Anglef minimumRotation = Math::Anglef::FromDegrees(m_rotationLimit.GetMinimum());
						const Math::Anglef rotationRange = Math::Anglef::FromDegrees(Math::Abs(m_rotationLimit.GetSize() - 1.f));

						while (Math::Abs(newRotation - m_currentRotation) >= rotationRange)
						{
							newRotation -= rotationRange * Math::SignNonZero(newRotation.GetDegrees());
						}

						if (newRotation > maximumRotation)
						{
							newRotation = minimumRotation + (newRotation - maximumRotation);
						}
						else if (newRotation < minimumRotation)
						{
							newRotation = maximumRotation - (minimumRotation - newRotation);
						}
					}
					break;

					case Mode::PingPong:
					{
						const Math::Anglef maximumRotation = Math::Anglef::FromDegrees(m_rotationLimit.GetMaximum());
						const Math::Anglef minimumRotation = Math::Anglef::FromDegrees(m_rotationLimit.GetMinimum());
						const Math::Anglef rotationRange = Math::Anglef::FromDegrees(Math::Abs(m_rotationLimit.GetSize() - 1.f));

						while (Math::Abs(newRotation - m_currentRotation) >= rotationRange)
						{
							newRotation -= rotationRange * Math::SignNonZero(newRotation.GetDegrees());
						}

						if (newRotation >= maximumRotation)
						{
							m_velocity = -m_velocity;
							newRotation = maximumRotation - (newRotation - maximumRotation);
						}
						else if (newRotation <= minimumRotation)
						{
							m_velocity = -m_velocity;
							newRotation = minimumRotation + (minimumRotation - newRotation);
						}
					}
					break;

					default:
						ExpectUnreachable();
				}

				const Math::YawPitchRollf deltaRotation = Math::YawPitchRollf::FromDegrees(
					m_rotationAxis.GetNormalizedSafe(Math::Up).zxy() * (newRotation - m_currentRotation).GetDegrees()
				);
				const Math::Quaternionf oldRotation = m_owner.GetWorldRotation();
				const Math::Quaternionf quaternionDeltaRotation = Math::Quaternionf(deltaRotation);

				Math::Quaternionf finalRotation = oldRotation.TransformRotation(quaternionDeltaRotation);
				finalRotation.m_vector.Normalize();

				m_owner.SetWorldRotation(finalRotation);
				m_currentRotation = newRotation;
			}
		}
	}

	[[maybe_unused]] const bool wasRotationMovementRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RotatingMovementComponent>>::Make());
	[[maybe_unused]] const bool wasRotationMovementTypeRegistered = Reflection::Registry::RegisterType<RotatingMovementComponent>();
	[[maybe_unused]] const bool wasRotationMovementModeTypeRegistered = Reflection::Registry::RegisterType<RotatingMovementComponent::Mode>();
}
