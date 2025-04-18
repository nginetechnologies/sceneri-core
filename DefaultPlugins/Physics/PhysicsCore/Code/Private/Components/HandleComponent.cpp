#include "PhysicsCore/Components/HandleComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "Components/Data/BodyComponent.h"

#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Constraints/PointConstraint.h>
#include <3rdparty/jolt/Physics/Body/BodyLockMulti.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	HandleComponent::HandleComponent(const HandleComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
		[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
			cloner.GetSceneRegistry(),
			Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
				BodySettings(
					BodyType::Kinematic,
					Layer::Dynamic,
					BodySettings().m_maximumAngularVelocity,
					BodySettings().m_overriddenMass,
					BodySettings().m_gravityScale,
					BodySettings().m_flags
				)
			}
		);
		Assert(pBody.IsValid());
		m_bodyID = pBody->GetIdentifier();
	}

	HandleComponent::HandleComponent(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
			deserializer.GetSceneRegistry(),
			Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
				BodySettings(
					BodyType::Kinematic,
					Layer::Dynamic,
					BodySettings().m_maximumAngularVelocity,
					BodySettings().m_overriddenMass,
					BodySettings().m_gravityScale,
					BodySettings().m_flags
				)
			}
		);
		Assert(pBody.IsValid());
		m_bodyID = pBody->GetIdentifier();
	}

	HandleComponent::HandleComponent(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
			initializer.GetSceneRegistry(),
			Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
				BodySettings(
					BodyType::Kinematic,
					Layer::Dynamic,
					BodySettings().m_maximumAngularVelocity,
					BodySettings().m_overriddenMass,
					BodySettings().m_gravityScale,
					BodySettings().m_flags
				)
			}
		);
		Assert(pBody.IsValid());
		m_bodyID = pBody->GetIdentifier();
	}

	void HandleComponent::OnChildAttached(
		HierarchyComponentBase& newChildComponent,
		[[maybe_unused]] const ChildIndex index,
		[[maybe_unused]] const Optional<ChildIndex> preferredChildIndex
	)
	{
		if (const Optional<Data::Body*> pBody = newChildComponent.FindDataComponentOfType<Data::Body>(GetSceneRegistry()))
		{
			GrabBody(*pBody, newChildComponent.AsExpected<Component3D>());
		}
	}

	void HandleComponent::OnChildDetached(HierarchyComponentBase& childComponent)
	{
		if (const Optional<Data::Body*> pBody = childComponent.FindDataComponentOfType<Data::Body>(GetSceneRegistry()))
		{
			ReleaseBody(*pBody);
		}
	}

	void HandleComponent::GrabBody(Data::Body& otherBody, [[maybe_unused]] const Entity::Component3D& otherBodyComponent)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		JPH::PointConstraintSettings* pSettings = new JPH::PointConstraintSettings();
		Physics::ConstraintIdentifier constraintIdentifier = physicsScene.RegisterConstraint();
		pSettings->mPoint1 = Math::Vector3f(Math::Zero);
		pSettings->mPoint2 = otherBodyComponent.GetWorldTransform().InverseTransformLocation(GetWorldLocation());
		pSettings->mSpace = JPH::EConstraintSpace::LocalToBody;
		physicsScene.GetCommandStage()
			.AddConstraint(constraintIdentifier, Array<JPH::BodyID, 2>{m_bodyID, otherBody.GetBodyIdentifier()}, Move(pSettings));
		m_constraints.EmplaceBack(Constraint{constraintIdentifier, otherBody.GetBodyIdentifier()});
	}

	void HandleComponent::ReleaseBody(const Data::Body& body)
	{
		OptionalIterator<Constraint> constraint = m_constraints.FindIf(
			[grabbedBodyID = body.GetBodyIdentifier()](const Constraint& constraint) -> bool
			{
				return constraint.m_grabbedBodyIdentifier == grabbedBodyID;
			}
		);
		if (constraint.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveConstraint(constraint->m_identifier);
			m_constraints.Remove(constraint);
		}
	}

	[[maybe_unused]] const bool wasHandleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HandleComponent>>::Make());
	[[maybe_unused]] const bool wasHandleTypeRegistered = Reflection::Registry::RegisterType<HandleComponent>();
}
