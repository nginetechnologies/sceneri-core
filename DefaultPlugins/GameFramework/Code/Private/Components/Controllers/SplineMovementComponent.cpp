#include "Components/Controllers/SplineMovementComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include "Engine/Entity/ComponentType.h"
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Wrap.h>
#include <Common/Math/Mod.h>

namespace ngine::GameFramework
{
	SplineMovementComponent::SplineMovementComponent(const SplineMovementComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_owner(cloner.GetParent())
		, m_spline(
				templateComponent.m_spline,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateParent().GetSceneRegistry(), cloner.GetSceneRegistry()}
			)
		, m_velocity(templateComponent.m_velocity)
		, m_mode(templateComponent.m_mode)
		, m_initialLocationRelativeToSpline(templateComponent.m_initialLocationRelativeToSpline)
		, m_overrideRelativePosition(templateComponent.m_overrideRelativePosition)
		, m_applyRotation(templateComponent.m_applyRotation)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	SplineMovementComponent::SplineMovementComponent(Initializer&& initializer)
		: BaseType(initializer)
		, m_owner(initializer.GetParent())
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	SplineMovementComponent::SplineMovementComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: BaseType(deserializer)
		, m_owner(deserializer.GetParent())
		, m_spline(
				deserializer.m_reader.HasSerializer("spline")
					? *deserializer.m_reader.Read<Entity::ComponentSoftReference>("spline", deserializer.GetSceneRegistry())
					: Entity::ComponentSoftReference{}
			)
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	SplineMovementComponent::~SplineMovementComponent() = default;

	using SplineType = Math::Splinef;
	using SplinePointsViewType = SplineType::ConstView;

	void SplineMovementComponent::InitializeSpline(Entity::Component3D& owner, Entity::Component3D& splineComponent)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		const Math::WorldTransform splineWorldTransform = splineComponent.GetWorldTransform(sceneRegistry);
		Math::WorldCoordinate location = owner.GetWorldLocation(sceneRegistry);
		if (m_overrideRelativePosition)
		{
			location = splineWorldTransform.GetLocation();
		}

		Math::Vector3f initialLocationRelativeToSpline = location - splineWorldTransform.GetLocation();

		if (const Optional<const Entity::SplineComponent*> pSplineComponent = splineComponent.AsExactType<Entity::SplineComponent>(sceneRegistry))
		{
			pSplineComponent->VisitSpline(
				[this, splineWorldTransform, location, initialLocationRelativeToSpline](const Math::Splinef& spline) mutable
				{
					const SplinePointsViewType points = spline.GetPoints();
					const bool isSplineClosed = spline.IsClosed();

					SplineType::SizeType currentIndex = 0;
					Math::Ratiof currentRatio = 0_percent;
					[[maybe_unused]] const Math::Vector3f closestPointRelative = spline.CalculateClosestPoint(
						splineWorldTransform.InverseTransformLocation(location),
						currentIndex,
						currentRatio,
						BezierSubdivisions
					);

					const Math::WorldCoordinate closestPoint = splineWorldTransform.TransformLocation(spline.GetBezierPositionBetweenPoints(
						points[currentIndex],
						*SplineType::WrapIterator(points.begin() + currentIndex + 1, points, isSplineClosed),
						currentRatio
					));

					initialLocationRelativeToSpline = location - closestPoint;

					if (isSplineClosed)
					{
						// TODO: Allow configuring initial direction
						m_travelDirection = 1;
					}
					else
					{
						if (currentIndex == 0 && currentRatio == 0.f)
						{
							m_travelDirection = 1;
						}
						else if (currentIndex == points.GetSize() - 2 && currentRatio == 1.f)
						{
							m_travelDirection = -1;
						}
						else
						{
							// TODO: Allow configuring initial direction
							m_travelDirection = 1;
						}
					}

					m_currentIndex = currentIndex;
					m_currentSegmentRatio = currentRatio;
					return false;
				}
			);
		}

		if (m_overrideRelativePosition)
		{
			m_initialLocationRelativeToSpline = Math::Zero;
		}
		else
		{
			m_initialLocationRelativeToSpline = initialLocationRelativeToSpline;
		}
	}

	void SplineMovementComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		if (Optional<Entity::ComponentTypeSceneData<SplineMovementComponent>*> pSceneData = owner.GetSceneRegistry().FindComponentTypeData<SplineMovementComponent>())
		{
			if (const Optional<Entity::Component3D*> pSplineComponent3D = m_spline.Find<Entity::Component3D>(m_owner.GetSceneRegistry()))
			{
				InitializeSpline(owner, *pSplineComponent3D);
			}
			pSceneData->EnableAfterPhysicsUpdate(*this);
		}
	}

	void SplineMovementComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		if (Optional<Entity::ComponentTypeSceneData<SplineMovementComponent>*> pSceneData = owner.GetSceneRegistry().FindComponentTypeData<SplineMovementComponent>())
		{
			pSceneData->DisableAfterPhysicsUpdate(*this);
		}
	}

	void SplineMovementComponent::StopPhysicsBody(Entity::Component3D& owner)
	{
		if (Entity::Component3D::DataComponentResult<Physics::Data::Body> result = owner.FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
		{
			if (result.m_pDataComponent && result.m_pDataComponentOwner)
			{
				Physics::Data::Scene& physicsScene =
					*result.m_pDataComponentOwner->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				Physics::Data::Body& body = *result.m_pDataComponent;
				body.Sleep(physicsScene);
			}
		}
	}

	void SplineMovementComponent::OnCreated(Entity::Component3D&)
	{
	}

	void SplineMovementComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void SplineMovementComponent::OnEnable(Entity::Component3D&)
	{
	}

	void SplineMovementComponent::OnDisable(Entity::Component3D&)
	{
	}

	void SplineMovementComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		SetSplineFromReference(owner, m_spline);
		RegisterForUpdate(owner);
	}

	void SplineMovementComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
		StopPhysicsBody(owner);
	}

	void SplineMovementComponent::SetSplineFromReference(Entity::Component3D& owner, const Entity::ComponentSoftReference spline)
	{
		m_spline = spline;

		if (const Optional<Entity::Component3D*> pSplineComponent3D = spline.Find<Entity::Component3D>(owner.GetSceneRegistry()))
		{
			InitializeSpline(owner, *pSplineComponent3D);
		}
	}

	void SplineMovementComponent::SetSpline(Entity::Component3D& owner, Entity::Component3DPicker splinePicker)
	{
		SetSplineFromReference(owner, static_cast<Entity::ComponentSoftReference>(splinePicker));
	}

	Entity::Component3DPicker SplineMovementComponent::GetSpline(Entity::Component3D& owner) const
	{
		Entity::Component3DPicker picker{m_spline, owner.GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Entity::SplineComponent>()});
		return picker;
	}

	void SplineMovementComponent::AfterPhysicsUpdate()
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		const Optional<Entity::Component3D*> pSplineComponent3D = m_spline.Find<Entity::Component3D>(sceneRegistry);
		if (pSplineComponent3D.IsInvalid())
		{
			return;
		}

		const Optional<const Entity::SplineComponent*> pSplineComponent = pSplineComponent3D->AsExactType<Entity::SplineComponent>(sceneRegistry
		);
		Assert(pSplineComponent.IsValid());
		if (pSplineComponent.IsInvalid())
		{
			return;
		}

		const Math::WorldTransform splineWorldTransform = pSplineComponent->GetWorldTransform(sceneRegistry);

		pSplineComponent->VisitSpline(
			[this, splineWorldTransform](const Math::Splinef& spline)
			{
				const SplinePointsViewType points = spline.GetPoints();

				const int32 pointsCount = points.GetSize();
				Assert(pointsCount > 0);
				if (UNLIKELY(pointsCount == 0))
				{
					return false;
				}

				const Math::Vector3f initialLocationRelativeToSpline = m_initialLocationRelativeToSpline;

				int8 travelDirection = m_travelDirection;
				float distanceToTravel = m_velocity * m_owner.GetCurrentFrameTime();

				const bool isSplineClosed = spline.IsClosed();

				Math::Ratiof initialSegmentRatio = m_currentSegmentRatio;
				Math::Ratiof currentSegmentRatio = initialSegmentRatio;

				SplineType::Container::const_iterator currentPointIt = points.begin() + m_currentIndex;

				SplineType::Container::const_iterator nextPointIt = SplineType::WrapIterator(currentPointIt + 1, points, isSplineClosed);
				SplineType::Container::const_iterator previousPointIt;
				if (!isSplineClosed)
				{
					previousPointIt = Math::Min(currentPointIt, nextPointIt);
					nextPointIt = Math::Max(currentPointIt, nextPointIt);
				}
				else
				{
					previousPointIt = currentPointIt;
				}

				Assert(previousPointIt >= points.begin() && previousPointIt < points.end());
				Assert(nextPointIt >= points.begin() && nextPointIt < points.end());
				float segmentLength = spline.CalculateLengthBetweenPoints(*previousPointIt, *nextPointIt, BezierSubdivisions);

				currentSegmentRatio += Math::Ratiof((Math::Mod(distanceToTravel, segmentLength) / segmentLength) * travelDirection);

				while (currentSegmentRatio < 0 || currentSegmentRatio > 1)
				{
					// Subtract the completed traveled distanace on the segment we completed
					float travelDirectionRatio = (float)((travelDirection + 1) / 2);
					distanceToTravel -= segmentLength * Math::Abs(travelDirectionRatio - initialSegmentRatio);

					SplineType::Container::const_iterator newPointIt = currentPointIt + travelDirection;

					const bool reachedEnd = (newPointIt == points.begin() - 1) | (newPointIt == (points.end() - 1));
					if (reachedEnd)
					{
						if (isSplineClosed)
						{
							if (travelDirection == 1)
							{
								previousPointIt = SplineType::WrapIterator(newPointIt, points, isSplineClosed);
								nextPointIt = SplineType::WrapIterator(newPointIt + 1, points, isSplineClosed);
								currentPointIt = previousPointIt;
							}
							else
							{
								nextPointIt = currentPointIt;
								previousPointIt = SplineType::WrapIterator(newPointIt, points, isSplineClosed);
							}

							segmentLength = spline.CalculateLengthBetweenPoints(*previousPointIt, *nextPointIt, BezierSubdivisions);
						}
						else
						{
							switch (m_mode)
							{
								case Mode::Single:
									DeregisterUpdate(m_owner);
									StopPhysicsBody(m_owner);
									return false;
								case Mode::Loop:
									previousPointIt = nextPointIt;
									nextPointIt = SplineType::WrapIterator(newPointIt + 1, points, true);
									currentPointIt = nextPointIt;
									segmentLength = spline.CalculateLengthBetweenPoints(*previousPointIt, *nextPointIt, BezierSubdivisions);
									break;
								case Mode::PingPong:
									travelDirection = (int8)-travelDirection;
									travelDirectionRatio = (1.f - travelDirectionRatio);
									break;
							}
						}
					}
					else
					{
						currentPointIt = SplineType::WrapIterator(newPointIt, points, isSplineClosed);
						nextPointIt = SplineType::WrapIterator(currentPointIt + 1, points, isSplineClosed);

						previousPointIt = Math::Min(currentPointIt, nextPointIt);
						nextPointIt = Math::Max(currentPointIt, nextPointIt);

						segmentLength = spline.CalculateLengthBetweenPoints(*previousPointIt, *nextPointIt, BezierSubdivisions);
					}

					initialSegmentRatio = currentSegmentRatio;

					currentSegmentRatio =
						Math::Ratiof(Math::Abs((1.f - travelDirectionRatio) - Math::Mod(distanceToTravel, segmentLength) / segmentLength));
				}

				Assert(currentSegmentRatio >= 0 && currentSegmentRatio <= 1);

				const Math::Vector3f localForward =
					spline.GetBezierDirectionBetweenPoints(*previousPointIt, *nextPointIt, currentSegmentRatio).GetNormalized() * travelDirection;
				m_velocityDirection = localForward;
				const Math::Vector3f newLocation = splineWorldTransform.TransformLocation(
																						 spline.GetBezierPositionBetweenPoints(*previousPointIt, *nextPointIt, currentSegmentRatio)
																					 ) +
			                                     initialLocationRelativeToSpline;

				if (m_applyRotation)
				{
					Math::Vector3f localRight = localForward.Cross(Math::Up).GetNormalized();
					Math::Vector3f localUp = localRight.Cross(localForward).GetNormalized();

					const Math::Quaternionf targetRotation(localForward, localUp);

					const Math::Quaternionf oldRotation = m_owner.GetWorldRotation();

					const FrameTime deltaTime = m_owner.GetCurrentFrameTime();

					const Math::EulerAnglesf rotationDelta = Math::Quaternionf{oldRotation}.InverseTransformRotation(targetRotation).GetEulerAngles();

					const float speed = 20.f;
					const float alpha = Math::Min(speed * deltaTime, 1.f);
					const Math::Quaternionf newRotation = Math::Quaternionf{oldRotation}.TransformRotation(Math::Quaternionf{rotationDelta * alpha});

					m_owner.SetWorldLocationAndRotation(newLocation, newRotation);
				}
				else
				{
					m_owner.SetWorldLocation(newLocation);
				}

				m_travelDirection = travelDirection;
				m_currentIndex = points.GetIteratorIndex(currentPointIt);
				m_currentSegmentRatio = currentSegmentRatio;
				return false;
			}
		);
	}

	[[maybe_unused]] const bool wasSplineMovementModeTypeRegistered = Reflection::Registry::RegisterType<SplineMovementComponent::Mode>();
	[[maybe_unused]] const bool wasSplineMovementRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SplineMovementComponent>>::Make());
	[[maybe_unused]] const bool wasSplineMovementTypeRegistered = Reflection::Registry::RegisterType<SplineMovementComponent>();
}
