#include "PhysicsCore/Components/BodyComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Constraints/SplineConstraintComponent.h"
#include "PhysicsCore/Layer.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/PathConstraint.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Mod.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Scene/Scene.h>

namespace ngine::Physics::Components
{
	SplineConstraint::SplineConstraint(const SplineConstraint& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_constrainedComponent{

				templateComponent.m_constrainedComponent,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			}
	{
	}

	SplineConstraint::SplineConstraint(Initializer&& initializer)
		: Entity::Component3D(Forward<BaseType::Initializer>(initializer))
	{
	}

	SplineConstraint::SplineConstraint(const Deserializer& deserializer)
		: Entity::Component3D(deserializer)
		, m_constrainedComponent{deserializer.m_reader.ReadWithDefaultValue<Entity::ComponentSoftReference>(
				"constrainedComponent", Entity::ComponentSoftReference{}, deserializer.GetParent()->GetSceneRegistry()
			)}
	{
	}

	void SplineConstraint::OnCreated()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void SplineConstraint::OnDestroy()
	{
		Assert(m_constraintIdentifier.IsInvalid());
	}

	void SplineConstraint::OnEnable()
	{
		if (!GetRootScene().IsTemplate())
		{
			CreateConstraint();
		}
	}

	void SplineConstraint::OnDisable()
	{
		if (m_constraintIdentifier.IsValid())
		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().RemoveConstraint(m_constraintIdentifier);
			m_constraintIdentifier = Invalid;
		}
	}

	void SplineConstraint::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
		{
			CreateConstraint();
		}
	}

	struct PathConstraintPath final : public JPH::PathConstraintPath
	{
		using SplineType = Math::Splinef;

		PathConstraintPath(SplineType&& spline)
			: m_spline(Forward<SplineType>(spline))
		{
			SetIsLooping(m_spline.IsClosed());
		}

		virtual float GetPathMaxFraction() const override
		{
			return float(m_spline.IsClosed() ? m_spline.GetPointCount() : (m_spline.GetPointCount() - 1));
		}

		virtual float GetClosestPoint(JPH::Vec3Arg inPosition) const override
		{
			const Math::Vector3f position{inPosition};
			SplineType::SizeType segmentIndex = 0;
			Math::Ratiof segmentRatio = 0_percent;
			[[maybe_unused]] const SplineType::CoordinateType point = m_spline.CalculateClosestPoint(position, segmentIndex, segmentRatio);
			return (float)segmentIndex + segmentRatio;
		}

		virtual void GetPointOnPath(
			float inFraction, JPH::Vec3& outPathPosition, JPH::Vec3& outPathTangent, JPH::Vec3& outPathNormal, JPH::Vec3& outPathBinormal
		) const override
		{
			const SplineType::SizeType segmentIndex = (SplineType::SizeType)Math::Floor(inFraction);
			const Math::Ratiof segmentRatio = Math::Mod(inFraction, 1.f);

			const SplineType::Point& startPoint = m_spline.GetPoint(segmentIndex);
			const SplineType::Point& nextPoint = m_spline.GetPoint(segmentIndex + 1);

			const SplineType::TransformType transform = m_spline.GetBezierTransformBetweenPoints(startPoint, nextPoint, segmentRatio);

			outPathPosition = transform.GetLocation();
			outPathTangent = transform.GetForwardColumn();
			outPathNormal = transform.GetUpColumn();
			outPathBinormal = transform.GetRightColumn();
		}

		SplineType m_spline;
	};

	void SplineConstraint::CreateConstraint()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);

		if (m_constraintIdentifier.IsValid())
		{
			physicsScene.GetCommandStage().RemoveConstraint(m_constraintIdentifier);
			m_constraintIdentifier = Invalid;
		}

		const Optional<const Entity::SplineComponent*> pSplineComponent = GetSplineComponent(sceneRegistry);
		if (pSplineComponent.IsInvalid())
		{
			return;
		}

		const Optional<Entity::Component3D*> pSplineBodyComponent = GetBodyComponent(sceneRegistry);

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

		pSplineComponent->VisitSpline(
			[this, &sceneRegistry, &physicsScene, pSplineBodyComponent, pSplineComponent, pConstrainedComponent, pConstrainedBody](
				const Math::Splinef& spline
			)
			{
				const Optional<Data::Body*> pSplineBody = pSplineBodyComponent.IsValid()
			                                              ? pSplineBodyComponent->FindDataComponentOfType<Data::Body>(sceneRegistry)
			                                              : Optional<Data::Body*>{};

				const Array<JPH::BodyID, (uint8)2> bodyIdentifiers{
					pSplineBody.IsValid() ? pSplineBody->GetIdentifier() : JPH::BodyID{},
					pConstrainedBody->GetIdentifier()
				};

				const Math::WorldTransform splineTransform = pSplineBodyComponent.IsValid() ? *pSplineBody->GetWorldTransform(physicsScene)
			                                                                              : pSplineComponent->GetWorldTransform(sceneRegistry);
				const Math::WorldTransform constrainedBodyTransform = pConstrainedComponent->GetWorldTransform(sceneRegistry);

				JPH::PathConstraintSettings* pSettings = new JPH::PathConstraintSettings();
				pSettings->mPath = new PathConstraintPath(Math::Splinef(spline));
				pSettings->mPathPosition = splineTransform.GetLocation();
				pSettings->mPathRotation = splineTransform.GetRotationQuaternion();

				const Math::Vector3f constrainedBodyLocationOnPath = splineTransform.InverseTransformLocation(constrainedBodyTransform.GetLocation()
			  );
				pSettings->mPathFraction = pSettings->mPath->GetClosestPoint(constrainedBodyLocationOnPath);

				pSettings->mRotationConstraintType = [](const RotationConstraintType rotationConstraintType)
				{
					switch (rotationConstraintType)
					{
						case RotationConstraintType::Free:
							return JPH::EPathRotationConstraintType::Free;
						case RotationConstraintType::ConstrainAroundTangent:
							return JPH::EPathRotationConstraintType::ConstrainAroundTangent;
						case RotationConstraintType::ConstrainAroundNormal:
							return JPH::EPathRotationConstraintType::ConstrainAroundNormal;
						case RotationConstraintType::ConstrainAroundBinormal:
							return JPH::EPathRotationConstraintType::ConstrainAroundBinormal;
						case RotationConstraintType::ConstainToPath:
							return JPH::EPathRotationConstraintType::ConstaintToPath;
						case RotationConstraintType::FullyConstrained:
							return JPH::EPathRotationConstraintType::FullyConstrained;
					}
					ExpectUnreachable();
				}(m_rotationConstraintType);

				Assert(m_constraintIdentifier.IsInvalid());
				m_constraintIdentifier = physicsScene.RegisterConstraint();
				physicsScene.GetCommandStage().AddConstraint(m_constraintIdentifier, bodyIdentifiers.GetView(), Move(pSettings));
			}
		);
	}

	void SplineConstraint::SetConstrainedComponent(Entity::Component3DPicker constrained)
	{
		m_constrainedComponent = constrained;
		CreateConstraint();
	}

	Entity::Component3DPicker SplineConstraint::GetConstrainedComponent() const
	{
		Entity::Component3DPicker picker{m_constrainedComponent, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Data::Body>()});
		return picker;
	}

	void SplineConstraint::OnRotationConstraintTypeChanged()
	{
		CreateConstraint();
	}

	Optional<Entity::SplineComponent*> SplineConstraint::GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const
	{
		return FindFirstParentOfType<Entity::SplineComponent>(sceneRegistry);
	}

	Optional<Entity::Component3D*> SplineConstraint::GetBodyComponent(Entity::SceneRegistry& sceneRegistry) const
	{
		const DataComponentResult<Data::Body> bodyResult = FindFirstDataComponentOfTypeInParents<Data::Body>(sceneRegistry);
		return bodyResult.m_pDataComponentOwner;
	}

	[[maybe_unused]] const bool wasSplineConstraintRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SplineConstraint>>::Make());
	[[maybe_unused]] const bool wasSplineConstraintTypeRegistered = Reflection::Registry::RegisterType<SplineConstraint>();
	[[maybe_unused]] const bool wasSplineConstraintRotationModeTypeRegistered =
		Reflection::Registry::RegisterType<SplineConstraint::RotationConstraintType>();
}
