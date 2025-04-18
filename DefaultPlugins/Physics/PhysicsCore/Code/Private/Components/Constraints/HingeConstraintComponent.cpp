#include "PhysicsCore/Components/BodyComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Constraints/HingeConstraintComponent.h"
#include "PhysicsCore/Layer.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/HingeConstraint.h>

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
	HingeConstraint::HingeConstraint(const HingeConstraint& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_constrainedComponent{
				templateComponent.m_constrainedComponent,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			}
	{
	}

	HingeConstraint::HingeConstraint(Initializer&& initializer)
		: Entity::Component3D(Forward<BaseType::Initializer>(initializer))
	{
	}

	HingeConstraint::HingeConstraint(const Deserializer& deserializer)
		: Entity::Component3D(deserializer)
		, m_constrainedComponent{deserializer.m_reader.ReadWithDefaultValue<Entity::ComponentSoftReference>(
				"constrainedComponent", Entity::ComponentSoftReference{}, deserializer.GetParent()->GetSceneRegistry()
			)}
	{
	}

	void HingeConstraint::OnCreated()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void HingeConstraint::OnDestroy()
	{
		Assert(m_constraintIdentifier.IsInvalid());
	}

	void HingeConstraint::OnEnable()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void HingeConstraint::OnDisable()
	{
		if (m_constraintIdentifier.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveConstraint(m_constraintIdentifier);
			m_constraintIdentifier = Invalid;
		}
	}

	void HingeConstraint::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
		{
			CreateConstraint();
		}
	}

	void HingeConstraint::CreateConstraint()
	{
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

		JPH::HingeConstraintSettings* pSettings = new JPH::HingeConstraintSettings();
		pSettings->mSpace = JPH::EConstraintSpace::LocalToBody;
		pSettings->mPoint1 = constraintParentBodyWorldTransform.InverseTransformLocation(constraintWorldTransform.GetLocation());
		pSettings->mPoint2 = constrainedWorldTransform.InverseTransformLocation(constraintWorldTransform.GetLocation());

		pSettings->mHingeAxis1 = constraintParentBodyWorldTransform.InverseTransformDirection(constraintWorldTransform.GetForwardColumn());
		pSettings->mHingeAxis2 = constrainedWorldTransform.InverseTransformDirection(constraintWorldTransform.GetForwardColumn());

		pSettings->mNormalAxis1 = constraintParentBodyWorldTransform.InverseTransformDirection(constraintWorldTransform.GetUpColumn());
		pSettings->mNormalAxis2 = constrainedWorldTransform.InverseTransformDirection(constraintWorldTransform.GetUpColumn());

		Assert(m_constraintIdentifier.IsInvalid());
		m_constraintIdentifier = physicsScene.RegisterConstraint();
		physicsScene.GetCommandStage().AddConstraint(m_constraintIdentifier, bodyIdentifiers.GetView(), Move(pSettings));
	}

	void HingeConstraint::SetConstrainedComponent(Entity::Component3DPicker constrained)
	{
		m_constrainedComponent = constrained;
		CreateConstraint();
	}

	Entity::Component3DPicker HingeConstraint::GetConstrainedComponent() const
	{
		Entity::Component3DPicker picker{m_constrainedComponent, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Data::Body>()});
		return picker;
	}

	Optional<Entity::Component3D*> HingeConstraint::GetBodyComponent() const
	{
		const DataComponentResult<Data::Body> bodyResult = FindFirstDataComponentOfTypeInParents<Data::Body>(GetSceneRegistry());
		return bodyResult.m_pDataComponentOwner;
	}

	[[maybe_unused]] const bool wasHingeConstraintRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HingeConstraint>>::Make());
	[[maybe_unused]] const bool wasHingeConstraintTypeRegistered = Reflection::Registry::RegisterType<HingeConstraint>();
}
