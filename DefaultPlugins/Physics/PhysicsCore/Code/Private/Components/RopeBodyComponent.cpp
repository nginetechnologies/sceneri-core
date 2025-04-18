#include "Components/RopeBodyComponent.h"
#include "Components/BodyComponent.h"
#include "Components/Data/SceneComponent.h"
#include "Components/Data/BodyComponent.h"
#include "Plugin.h"

#include <3rdparty/jolt/Physics/Constraints/SwingTwistConstraint.h>
#include <3rdparty/jolt/Physics/Collision/GroupFilterTable.h>

#include "Components/CapsuleColliderComponent.h"
#include "Components/SphereColliderComponent.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/Splines/RopeComponent.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <PhysicsCore/Material.h>
#include <PhysicsCore/MaterialAssetType.h>
#include <PhysicsCore/Components/Data/PhysicsCommandStage.h>
#include <PhysicsCore/DefaultMaterials.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	RopeBodyComponent::RopeBodyComponent(const RopeBodyComponent& templateComponent, const Cloner& cloner)
		: KinematicBodyComponent(templateComponent, cloner)
		, m_materialIdentifier(templateComponent.m_materialIdentifier)
	{
	}

	RopeBodyComponent::RopeBodyComponent(const Deserializer& deserializer)
		: RopeBodyComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<RopeBodyComponent>().ToString().GetView())
			)
	{
	}

	[[nodiscard]] inline MaterialIdentifier FindOrCreatePhysicsMaterialIdentifier(Asset::Guid guid)
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		return materialCache.FindOrRegisterAsset(guid);
	}

	RopeBodyComponent::RopeBodyComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: KinematicBodyComponent(deserializer)
		, m_materialIdentifier(FindOrCreatePhysicsMaterialIdentifier(
				typeSerializer.IsValid()
					? typeSerializer->ReadWithDefaultValue<ngine::Guid>("physical_material", ngine::Guid(Materials::DefaultAssetGuid))
					: ngine::Guid(Materials::DefaultAssetGuid)
			))
	{
	}

	RopeBodyComponent::RopeBodyComponent(Initializer&& initializer)
		: KinematicBodyComponent(Forward<Initializer>(initializer))
		, m_materialIdentifier(initializer.m_materialIdentifier)
	{
	}

	RopeBodyComponent::~RopeBodyComponent()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		for (const Physics::ConstraintIdentifier constraintIdentifier : m_constraints)
		{
			physicsScene.GetCommandStage().RemoveConstraint(constraintIdentifier);
		}
	}

	Optional<Entity::SplineComponent*> RopeBodyComponent::GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const
	{
		return FindFirstParentOfType<Entity::SplineComponent>(sceneRegistry);
	}

	Optional<Entity::RopeComponent*> RopeBodyComponent::GetRope(Entity::SceneRegistry& sceneRegistry) const
	{
		return FindFirstChildOfType<Entity::RopeComponent>(sceneRegistry);
	}

	void RopeBodyComponent::Update()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Optional<Entity::RopeComponent*> pRope = GetRope(sceneRegistry);
		if (!pRope.IsValid())
		{
			return;
		}

		Math::Splinef simulatedSpline;

		const uint16 physicalSegmentCount = GetSegments().GetSize();
		const uint32 numSubdivisions = 32;

		const float ropeLength = pRope->GetSpline()->CalculateSplineLength(numSubdivisions);
		const float physicalSegmentSplice = ropeLength / (float)physicalSegmentCount;

		const Math::WorldTransform ropeTransform = pRope->GetWorldTransform();
		for (uint16 physicalSegmentIndex = 0; physicalSegmentIndex < physicalSegmentCount; ++physicalSegmentIndex)
		{
			const Physics::BodyComponent& segmentBody = m_segments[physicalSegmentIndex];
			Math::WorldTransform segmentTransform = segmentBody.GetWorldTransform();
			segmentTransform.AddLocation(segmentTransform.TransformDirectionWithoutScale({0, 0, -physicalSegmentSplice * 0.5f}));
			Math::LocalTransform physicalSegmentLocalTransform = ropeTransform.GetTransformRelativeToAsLocal(segmentTransform);

			simulatedSpline.EmplacePoint(physicalSegmentLocalTransform.GetLocation(), physicalSegmentLocalTransform.GetForwardColumn());
		}

		if (physicalSegmentCount > 0)
		{
			const Physics::BodyComponent& lastSegmentBody = m_segments.GetLastElement();

			Math::WorldTransform segmentTransform = lastSegmentBody.GetWorldTransform();
			segmentTransform.AddLocation(segmentTransform.TransformDirectionWithoutScale({0, 0, physicalSegmentSplice * 0.5f}));
			Math::LocalTransform physicalSegmentLocalTransform = ropeTransform.GetTransformRelativeToAsLocal(segmentTransform);

			simulatedSpline.EmplacePoint(physicalSegmentLocalTransform.GetLocation(), physicalSegmentLocalTransform.GetUpColumn());
		}

		pRope->GetSimulatedSpline() = simulatedSpline;

		// TODO: Detect that the rope state changed, and do a partial update if possible
		pRope->RecreateMesh();
	}

	Optional<BodyComponent*> RopeBodyComponent::GetClosestSegmentBody(const Math::WorldCoordinate coordinate) const
	{
		Optional<Physics::BodyComponent*> pClosestRopeSegmentBody;
		Math::WorldCoordinateUnitType closestPointDistanceSquared = Math::NumericLimits<Math::WorldCoordinateUnitType>::Max;
		for (Physics::BodyComponent& ropeSegmentBody : m_segments)
		{
			const Math::WorldCoordinate segmentStartLocation =
				ropeSegmentBody.GetWorldTransform().TransformLocationWithoutScale({-m_segmentHalfHeight.GetMeters(), 0, 0});
			const Math::WorldCoordinate segmentEndLocation =
				ropeSegmentBody.GetWorldTransform().TransformLocationWithoutScale({m_segmentHalfHeight.GetMeters(), 0, 0});

			const Math::WorldLine segmentLine(segmentStartLocation, segmentEndLocation);
			const Math::WorldCoordinate coordinateOnLine = segmentLine.GetPointAtRatio(segmentLine.GetClosestPointRatio(coordinate));
			const Math::WorldCoordinateUnitType distanceSquared = (coordinateOnLine - coordinate).GetLengthSquared();
			if (distanceSquared < closestPointDistanceSquared)
			{
				closestPointDistanceSquared = distanceSquared;
				pClosestRopeSegmentBody = &ropeSegmentBody;
			}
		}

		return pClosestRopeSegmentBody;
	}

	void RopeBodyComponent::SetMaterialAsset(const PhysicalMaterialPicker asset)
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		m_materialIdentifier = materialCache.FindOrRegisterAsset(asset.GetAssetGuid());
	}

	RopeBodyComponent::PhysicalMaterialPicker RopeBodyComponent::GetMaterialAsset() const
	{
		MaterialCache& materialCache = System::FindPlugin<Plugin>()->GetMaterialCache();
		const MaterialIdentifier materialIdentifier = m_materialIdentifier;
		return {
			materialIdentifier.IsValid() ? materialCache.GetAssetGuid(materialIdentifier) : Asset::Guid{},
			Physics::MaterialAssetType::AssetFormat.assetTypeGuid
		};
	}

	void RopeBodyComponent::CreateCapsules(Entity::SceneRegistry& sceneRegistry)
	{
		const Optional<const Entity::SplineComponent*> pSpline = GetSplineComponent(sceneRegistry);
		if (!pSpline.IsValid())
		{
			return;
		}

		pSpline->VisitSpline(
			[this, &sceneRegistry](const Math::Splinef& spline)
			{
				Math::Radiusf radius = m_radius;
				if (Optional<Entity::RopeComponent*> pRope = GetRope(sceneRegistry))
				{
					radius = pRope->GetRadius();
				}

				const Math::Lengthf ropeLength = Math::Lengthf::FromMeters(spline.CalculateSplineLength());
				const Math::Lengthf halfHeight = radius * 2.f;
				const uint16 segmentCount = (uint16)Math::Floor((ropeLength / (halfHeight * 2.f)).GetMeters());
				const Math::Lengthf segmentLength = ropeLength / (float)segmentCount;
				m_segmentHalfHeight = segmentLength * 0.5f;

				Entity::ComponentTypeSceneData<Physics::BodyComponent>& bodySceneData =
					*sceneRegistry.GetOrCreateComponentTypeData<Physics::BodyComponent>();
				Entity::ComponentTypeSceneData<Physics::CapsuleColliderComponent>& colliderSceneData =
					*sceneRegistry.GetOrCreateComponentTypeData<Physics::CapsuleColliderComponent>();

				Physics::Plugin* pPhysicsPlugin = System::FindPlugin<Plugin>();
				MaterialCache& materialCache = pPhysicsPlugin->GetMaterialCache();
				MaterialIdentifier materialIdentifier = materialCache.FindOrRegisterAsset(GetMaterialAsset().GetAssetGuid());
				Optional<const Physics::Material*> pColliderMaterial = materialCache.FindMaterial(materialIdentifier);
				if (pColliderMaterial.IsInvalid())
				{
					pColliderMaterial = materialCache.GetDefaultMaterial();
				}

				using SplineType = typename Entity::SplineComponent::SplineType;

				m_segments.Reserve(segmentCount);

				JPH::Ref<JPH::GroupFilterTable> groupFilter = new JPH::GroupFilterTable(segmentCount);
				for (uint16 i = 0; i < segmentCount - 1; ++i)
				{
					groupFilter->DisableCollision(i, i + 1);
				}

				Math::Vector3f segmentStartLocation = Math::Zero;
				Math::Lengthf currentSegmentLength = 0_meters;

				Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
				const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();

				spline.IterateAdjustedSplinePoints(
					[&currentSegmentLength,
			     &segmentStartLocation,
			     segmentLength,
			     radius,
			     halfHeight = m_segmentHalfHeight,
			     &bodySceneData,
			     &colliderSceneData,
			     pColliderMaterial,
			     this,
			     groupFilter,
			     &bodyLockInterface,
			     &sceneRegistry](
						const SplineType::Spline::Point&,
						const SplineType::Spline::Point&,
						const Math::Vector3f currentBezierPoint,
						const Math::Vector3f nextBezierPoint,
						[[maybe_unused]] const Math::Vector3f direction,
						[[maybe_unused]] const Math::Vector3f normal
					)
					{
						const Math::Vector3f currentBezierDistance = nextBezierPoint - currentBezierPoint;
						const Math::Lengthf currentBezierSegmentLength = Math::Lengthf::FromMeters(currentBezierDistance.GetLength());
						currentSegmentLength += currentBezierSegmentLength;

						while (currentSegmentLength >= segmentLength)
						{
							const Math::Lengthf remainingLength = currentSegmentLength - segmentLength;
							Assert(remainingLength.GetMeters() < currentBezierSegmentLength.GetMeters());
							const float remainingRatio = (remainingLength / currentBezierSegmentLength).GetMeters();
							const float usedRatio = 1.f - remainingRatio;

							const Math::Vector3f currentSegmentEndLocation = currentBezierPoint + currentBezierDistance * usedRatio;
							const Math::Vector3f currentSegmentDistance = currentSegmentEndLocation - segmentStartLocation;
							const Math::Vector3f currentSegmentDirection = currentSegmentDistance.GetNormalized();

							const Math::Vector3f currentSegmentNormal = currentSegmentDirection.Cross(Math::Up);

							// Create the capsule here
							Math::LocalTransform::StoredRotationType capsuleRotation(
								Math::Matrix3x3f(currentSegmentNormal, currentSegmentNormal.Cross(currentSegmentDirection), currentSegmentDirection)
							);
							capsuleRotation.SetScale(Math::Vector3f{1.f});
							const Math::LocalTransform segmentTransform =
								Math::LocalTransform(capsuleRotation, segmentStartLocation + currentSegmentDirection * halfHeight.GetMeters());

							Optional<Physics::BodyComponent*> segmentBody = bodySceneData.CreateInstance(BodyComponent::Initializer{
								Component3D::Initializer{*this, segmentTransform},
								Physics::BodyComponent::Settings{Physics::Data::Body::Type::Dynamic, Physics::Layer::Dynamic}
							});
							Assert(segmentBody.IsValid());
							if (LIKELY(segmentBody.IsValid()))
							{
								{
									JPH::BodyLockWrite lock(bodyLockInterface, segmentBody->GetBodyID());
									if (lock.Succeeded())
									{
										lock.GetBody().SetCollisionGroup(
											JPH::CollisionGroup(groupFilter, 0, JPH::CollisionGroup::SubGroupID(m_segments.GetNextAvailableIndex()))
										);
									}
									// segmentBody->m_pBody->SetAllowSleeping(false);
								}

								m_segments.EmplaceBack(*segmentBody);

								[[maybe_unused]] Optional<CapsuleColliderComponent*> capsule =
									colliderSceneData.CreateInstance(CapsuleColliderComponent::Initializer{
										ColliderComponent::Initializer{
											Component3D::Initializer{*segmentBody, Math::LocalTransform(Math::Identity)},
											pColliderMaterial,
											segmentBody->FindDataComponentOfType<Data::Body>(sceneRegistry),
											segmentBody
										},
										radius,
										halfHeight - radius
									});
								Assert(capsule.IsValid());

								currentSegmentLength -= segmentLength;
								segmentStartLocation = currentSegmentEndLocation;
							}
						}
					}
				);

				if (m_segments.GetSize() == segmentCount - 1)
				{
					const SplineType::Point& secondToLastPoint = spline.GetPoints()[spline.GetPointCount() - 1];
					const SplineType::Point& lastPoint = spline.GetPoints().GetLastElement();

					const Math::Vector3f lastPosition = spline.GetBezierPositionBetweenPoints(secondToLastPoint, lastPoint, 1.f);

					const Math::Vector3f currentSegmentDistance = lastPosition - segmentStartLocation;
					const Math::Vector3f currentSegmentDirection = currentSegmentDistance.GetNormalized();

					const Math::Vector3f currentSegmentNormal = currentSegmentDirection.Cross(Math::Up);

					// Create the last capsule
					Math::LocalTransform::StoredRotationType capsuleRotation(
						Math::Matrix3x3f(currentSegmentNormal, currentSegmentNormal.Cross(currentSegmentDirection), currentSegmentDirection)
					);
					capsuleRotation.SetScale(Math::Vector3f{1.f});
					const Math::LocalTransform segmentTransform =
						Math::LocalTransform(capsuleRotation, segmentStartLocation + currentSegmentDirection * m_segmentHalfHeight.GetMeters());

					Optional<Physics::BodyComponent*> segmentBody =
						bodySceneData.CreateInstance(BodyComponent::Initializer{Component3D::Initializer{*this, segmentTransform}});
					Assert(segmentBody.IsValid());
					if (LIKELY(segmentBody.IsValid()))
					{
						m_segments.EmplaceBack(*segmentBody);

						[[maybe_unused]] Optional<Physics::CapsuleColliderComponent*> capsule =
							colliderSceneData.CreateInstance(CapsuleColliderComponent::Initializer{
								ColliderComponent::Initializer{
									Component3D::Initializer{*segmentBody, Math::LocalTransform(Math::Identity)},
									pColliderMaterial,
									segmentBody->FindDataComponentOfType<Data::Body>(),
									segmentBody
								},
								radius,
								m_segmentHalfHeight - radius
							});
						Assert(capsule.IsValid());
					}
				}

				Assert(m_segments.GetSize() == segmentCount);

				CreateConstraints(segmentCount, m_segmentHalfHeight);

				Entity::ComponentTypeSceneData<RopeBodyComponent>& sceneData =
					static_cast<Entity::ComponentTypeSceneData<RopeBodyComponent>&>(*GetTypeSceneData());
				sceneData.EnableUpdate(*this);
			}
		);
	}

	void RopeBodyComponent::CreateConstraints(const uint16 segmentCount, const Math::Lengthf segmentHalfLength)
	{
		const bool shouldAttachStart = segmentCount > 0;
		const bool shouldAttachEnd = segmentCount > 1;

		const uint16 numConstraints = (uint16)segmentCount - 1u + static_cast<uint16>(shouldAttachStart) + static_cast<uint16>(shouldAttachEnd);
		m_constraints.Reserve(numConstraints);

		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		if (m_constraints.HasElements())
		{
			for (const Physics::ConstraintIdentifier constraintIdentifier : m_constraints)
			{
				physicsScene.GetCommandStage().RemoveConstraint(constraintIdentifier);
			}
			m_constraints.Clear();
		}

		const auto addConstraint = [this, &physicsScene](
																 const BodyComponent& firstBody,
																 const BodyComponent* const pSecondBody,
																 const Math::WorldTransform& constraintTransform
															 )
		{
			const Math::WorldCoordinate constraintLocation = constraintTransform.GetLocation();

			const Math::Vector3f twistAxis = constraintTransform.GetForwardColumn();
			const Math::Vector3f planeAxis = constraintTransform.GetRightColumn();

			constexpr Math::Anglef normalHalfConeAngle = 65_degrees;
			constexpr Math::Anglef planeHalfConeAngle = 65_degrees;
			constexpr Math::Anglef twistMinimumAngle = -45_degrees;
			constexpr Math::Anglef twistMaximumAngle = 45_degrees;

			JPH::SwingTwistConstraintSettings* pSettings = new JPH::SwingTwistConstraintSettings();
			pSettings->mPosition1 = pSettings->mPosition2 = {constraintLocation.x, constraintLocation.y, constraintLocation.z};
			pSettings->mTwistAxis1 = pSettings->mTwistAxis2 = {twistAxis.x, twistAxis.y, twistAxis.z};
			pSettings->mPlaneAxis1 = pSettings->mPlaneAxis2 = {planeAxis.x, planeAxis.y, planeAxis.z};
			pSettings->mNormalHalfConeAngle = normalHalfConeAngle.GetRadians();
			pSettings->mPlaneHalfConeAngle = planeHalfConeAngle.GetRadians();
			pSettings->mTwistMinAngle = twistMinimumAngle.GetRadians();
			pSettings->mTwistMaxAngle = twistMaximumAngle.GetRadians();

			Physics::ConstraintIdentifier constraintIdentifier = physicsScene.RegisterConstraint();
			m_constraints.EmplaceBack(constraintIdentifier);

			physicsScene.GetCommandStage().AddConstraint(
				constraintIdentifier,
				Array<JPH::BodyID, 2>{firstBody.GetBodyID(), pSecondBody != nullptr ? pSecondBody->GetBodyID() : GetBodyID()},
				Move(pSettings)
			);
		};

		if (shouldAttachStart)
		{
			BodyComponent& firstSegment = m_segments[0];

			Math::WorldTransform firstSegmentTransform = firstSegment.GetWorldTransform();
			firstSegmentTransform.AddLocation(firstSegmentTransform.TransformDirectionWithoutScale({0, 0, -segmentHalfLength.GetMeters()}));

			BodyComponent* pStartAttachmentActor = nullptr;
			addConstraint(firstSegment, pStartAttachmentActor, firstSegmentTransform);
		}

		for (uint16 capsuleIndex = 1u; capsuleIndex < segmentCount; ++capsuleIndex)
		{
			BodyComponent& previousSegment = m_segments[capsuleIndex - 1u];
			BodyComponent& currentSegment = m_segments[capsuleIndex];

			Math::WorldTransform currentSegmentTransform = currentSegment.GetWorldTransform();
			currentSegmentTransform.AddLocation(currentSegmentTransform.TransformDirectionWithoutScale({0, 0, -segmentHalfLength.GetMeters()}));

			addConstraint(previousSegment, &currentSegment, currentSegmentTransform);
		}

		if (shouldAttachEnd)
		{
			BodyComponent& lastSegment = m_segments.GetLastElement();

			Math::WorldTransform lastSegmentTransform = lastSegment.GetWorldTransform();
			lastSegmentTransform.AddLocation(lastSegmentTransform.TransformDirectionWithoutScale({0, 0, segmentHalfLength.GetMeters()}));

			BodyComponent* pEndAttachmentActor = nullptr;
			addConstraint(lastSegment, pEndAttachmentActor, lastSegmentTransform);
		}
	}

	// TODO: Attach the start and end joints to the actor if global
	// That way moving the component moves the rope

	[[maybe_unused]] const bool wasRopeBodyRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RopeBodyComponent>>::Make());
	[[maybe_unused]] const bool wasRopeBodyTypeRegistered = Reflection::Registry::RegisterType<RopeBodyComponent>();
}
