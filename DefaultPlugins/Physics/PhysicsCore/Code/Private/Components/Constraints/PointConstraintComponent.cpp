#include "PhysicsCore/Components/BodyComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Constraints/PointConstraintComponent.h"
#include "PhysicsCore/Layer.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/PointConstraint.h>

#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Scene/Scene.h>

namespace ngine::Physics::Components
{
	PointConstraint::PointConstraint(const PointConstraint& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_constrainedComponent{
				templateComponent.m_constrainedComponent,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			}
	{
	}

	PointConstraint::PointConstraint(Initializer&& initializer)
		: Entity::Component3D(Forward<BaseType::Initializer>(initializer))
	{
	}

	PointConstraint::PointConstraint(const Deserializer& deserializer)
		: Entity::Component3D(deserializer)
		, m_constrainedComponent{deserializer.m_reader.ReadWithDefaultValue<Entity::ComponentSoftReference>(
				"constrainedComponent", Entity::ComponentSoftReference{}, deserializer.GetParent()->GetSceneRegistry()
			)}
	{
	}

	void PointConstraint::OnCreated()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void PointConstraint::OnDestroy()
	{
		Assert(m_constraintIdentifier.IsInvalid());
	}

	void PointConstraint::OnEnable()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void PointConstraint::OnDisable()
	{
		if (m_constraintIdentifier.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveConstraint(m_constraintIdentifier);
			m_constraintIdentifier = Invalid;
		}
	}

	void PointConstraint::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
		{
			CreateConstraint();
		}
	}

	void PointConstraint::CreateConstraint()
	{
		Assert(!GetRootScene().IsTemplate());
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		if (m_constraintIdentifier.IsValid())
		{
			physicsScene.GetCommandStage().RemoveConstraint(m_constraintIdentifier);
			m_constraintIdentifier = Invalid;
		}

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		const Optional<Entity::Component3D*> pParentBodyComponent = GetBodyComponent();
		const Optional<Data::Body*> pParentBody = pParentBodyComponent.IsValid()
		                                            ? pParentBodyComponent->FindDataComponentOfType<Data::Body>(sceneRegistry)
		                                            : Optional<Data::Body*>{};

		const Optional<Entity::Component3D*> pConstrainedComponent = m_constrainedComponent.Find<Entity::Component3D>(sceneRegistry);
		if (pConstrainedComponent.IsInvalid())
		{
			return;
		}
		const Optional<Data::Body*> pConstrainedBody = pConstrainedComponent.IsValid()
		                                                 ? pConstrainedComponent->FindDataComponentOfType<Data::Body>(sceneRegistry)
		                                                 : Optional<Data::Body*>{};
		if (pConstrainedBody.IsInvalid())
		{
			return;
		}

		const Array<JPH::BodyID, 2> bodyIdentifiers{
			pParentBody.IsValid() ? pParentBody->GetIdentifier() : JPH::BodyID{},
			pConstrainedBody->GetIdentifier()
		};

		const Math::WorldTransform constraintWorldTransform = GetWorldTransform();
		const Math::WorldTransform constraintParentBodyWorldTransform = pParentBody.IsValid() ? pParentBodyComponent->GetWorldTransform()
		                                                                                      : Math::Identity;
		const Math::WorldTransform constrainedWorldTransform = pConstrainedComponent->GetWorldTransform();

		JPH::PointConstraintSettings* pSettings = new JPH::PointConstraintSettings();
		pSettings->mSpace = JPH::EConstraintSpace::LocalToBody;
		pSettings->mPoint1 = constraintParentBodyWorldTransform.InverseTransformLocation(constraintWorldTransform.GetLocation());
		pSettings->mPoint2 = constrainedWorldTransform.InverseTransformLocation(constraintWorldTransform.GetLocation());

		Assert(m_constraintIdentifier.IsInvalid());
		m_constraintIdentifier = physicsScene.RegisterConstraint();
		physicsScene.GetCommandStage().AddConstraint(m_constraintIdentifier, bodyIdentifiers.GetView(), Move(pSettings));
	}

	void PointConstraint::SetConstrainedComponent(Entity::Component3DPicker constrained)
	{
		m_constrainedComponent = constrained;
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	Entity::Component3DPicker PointConstraint::GetConstrainedComponent() const
	{
		Entity::Component3DPicker picker{m_constrainedComponent, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Data::Body>()});
		return picker;
	}

	Optional<Entity::Component3D*> PointConstraint::GetBodyComponent() const
	{
		const DataComponentResult<Data::Body> bodyResult = FindFirstDataComponentOfTypeInParents<Data::Body>(GetSceneRegistry());
		return bodyResult.m_pDataComponentOwner;
	}

	[[maybe_unused]] const bool wasPointConstraintRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<PointConstraint>>::Make());
	[[maybe_unused]] const bool wasPointConstraintTypeRegistered = Reflection::Registry::RegisterType<PointConstraint>();
}
