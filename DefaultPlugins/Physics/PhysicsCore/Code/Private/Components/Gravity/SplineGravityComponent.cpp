#include "PhysicsCore/Components/Gravity/SplineGravityComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/SphereColliderComponent.h"
#include "PhysicsCore/Plugin.h"
#include "PhysicsCore/Layer.h"
#include "PhysicsCore/BroadPhaseLayer.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Splines/SplineComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include "3rdparty/jolt/Geometry/OrientedBox.h"

namespace ngine::Physics
{
	SplineGravityComponent::SplineGravityComponent(const SplineGravityComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_acceleration(templateComponent.m_acceleration)
	{
	}

	SplineGravityComponent::SplineGravityComponent(const Deserializer& deserializer)
		: SplineGravityComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SplineGravityComponent>().ToString().GetView())
			)
	{
	}

	SplineGravityComponent::SplineGravityComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer)
		: BaseType(deserializer)
		, m_acceleration(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Accelerationf>("acceleration", 9.81_m_per_second_squared)
																 : 9.81_m_per_second_squared
			)
	{
	}

	SplineGravityComponent::SplineGravityComponent(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
		, m_acceleration(initializer.m_acceleration)
	{
	}

	SplineGravityComponent::~SplineGravityComponent()
	{
	}

	void SplineGravityComponent::OnCreated()
	{
		if (IsEnabled())
		{
			Physics::Data::Scene& physicsScene = Physics::Data::Scene::Get(GetRootScene());
			physicsScene.m_physicsSystem.AddStepListener(this);
		}
	}

	void SplineGravityComponent::OnEnable()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		physicsScene.m_physicsSystem.AddStepListener(this);
	}

	void SplineGravityComponent::OnDisable()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		physicsScene.m_physicsSystem.RemoveStepListener(this);
	}

	Optional<Entity::SplineComponent*> SplineGravityComponent::GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const
	{
		return FindFirstParentOfType<Entity::SplineComponent>(sceneRegistry);
	}

	void SplineGravityComponent::OnStep(float deltaTime, JPH::PhysicsSystem& physicsSystem)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<const Entity::SplineComponent*> pSpline = GetSplineComponent(sceneRegistry);
		if (!pSpline.IsValid())
		{
			return;
		}

		const Math::Radiusf radius = m_radius;

		Math::Vector3f segmentStartLocation = Math::Zero;
		Math::Lengthf currentSegmentLength = 0_meters;

		using SplineType = Math::Splinef;

		JPH::BodyMask bodyMask;

		class Collector : public JPH::CollideShapeBodyCollector
		{
		public:
			Collector(JPH::BodyMask& bodyMask)
				: m_bodyMask(bodyMask)
			{
			}

			virtual void AddHit(const JPH::BodyID& inBodyID) override
			{
				m_bodyMask.Set(inBodyID);
			}
		private:
			JPH::BodyMask& m_bodyMask;
		};

		const Math::WorldTransform worldTransform = pSpline->GetWorldTransform();

		pSpline->VisitSpline(
			[this, deltaTime, &currentSegmentLength, &segmentStartLocation, radius, &physicsSystem, worldTransform, &bodyMask](
				const Math::Splinef& spline
			)
			{
				spline.IterateAdjustedSplinePoints(
					[&currentSegmentLength, &segmentStartLocation, radius, &physicsSystem, worldTransform, &bodyMask](
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

						while (currentSegmentLength >= radius)
						{
							const Math::Lengthf remainingLength = currentSegmentLength - radius;
							Assert(remainingLength.GetMeters() < currentBezierSegmentLength.GetMeters());
							const float remainingRatio = (remainingLength / currentBezierSegmentLength).GetMeters();
							const float usedRatio = 1.f - remainingRatio;

							const Math::Vector3f currentSegmentEndLocation = currentBezierPoint + currentBezierDistance * usedRatio;
							const Math::Vector3f currentSegmentDistance = currentSegmentEndLocation - segmentStartLocation;
							const Math::Vector3f currentSegmentDirection = currentSegmentDistance.GetNormalized();

							const Math::WorldCoordinate segmentLocation =
								worldTransform.TransformLocation(segmentStartLocation + currentSegmentDirection * radius.GetMeters());

							Collector collector(bodyMask);

							// TODO: Introduce CollideCapsule?
							physicsSystem.GetBroadPhaseQuery().CollideSphere(
								segmentLocation,
								radius.GetMeters() * 2.f, // Multiplied radius to ensure we catch bodies inbetween segments
								collector,
								JPH::SpecifiedBroadPhaseLayerFilter(static_cast<JPH::BroadPhaseLayer>((uint8)BroadPhaseLayer::Dynamic)),
								JPH::SpecifiedObjectLayerFilter(static_cast<JPH::ObjectLayer>(Layer::Dynamic))
							);

							currentSegmentLength -= radius;
							segmentStartLocation = currentSegmentEndLocation;
						}
					}
				);

				for (const JPH::BodyID::IndexType bodyIndex : bodyMask.GetSetBitsIterator())
				{
					const JPH::BodyID bodyIdentifier = JPH::BodyID::MakeFromValidIndex(bodyIndex);

					JPH::BodyLockWrite lock(physicsSystem.GetBodyLockInterfaceNoLock(), bodyIdentifier);
					JPH::Body& body = lock.GetBody();
					if (body.IsActive())
					{
						const JPH::Vec3 joltBodyLocation = body.GetPosition();
						const Math::WorldCoordinate bodyLocation{joltBodyLocation.GetX(), joltBodyLocation.GetY(), joltBodyLocation.GetZ()};

						const SplineType::ConstView points = spline.GetPoints();
						const bool isSplineClosed = spline.IsClosed();

						uint32 pointIndex = 0;
						Math::Ratiof pointRatio{0_percent};
						const Math::Vector3f closestPointOnSpline =
							spline.CalculateClosestPoint(worldTransform.InverseTransformLocation(bodyLocation), pointIndex, pointRatio);

						const SplineType::Point& nextPoint = *SplineType::WrapIterator(points.begin() + pointIndex + 1, points, isSplineClosed);
						const Math::Vector3f direction = spline.GetBezierDirectionBetweenPoints(points[pointIndex], nextPoint, pointRatio);
						const Math::Vector3f accelerationDirection = worldTransform.TransformDirection(direction);
						const Math::Vector3f acceleration = accelerationDirection * m_acceleration.GetMetersPerSecondSquared();

						const Math::WorldCoordinate closestPoint = worldTransform.TransformLocation(closestPointOnSpline);

						Math::Vector3f distance = closestPoint - bodyLocation;
						const float distanceLength = distance.GetLength();
						const float distanceRatio = distanceLength / m_radius.GetMeters();
						if (distanceRatio > 1.f)
						{
							return;
						}

						const float ratio = 1.f - distanceRatio;

						const Math::Vector3f gravity = (acceleration * ratio) + (body.GetLinearVelocity() * m_damping * -1.f) + (distance * 1.f);
						body.GetMotionProperties()->ApplyForceTorqueAndDragInternal(body.GetRotation(), gravity, deltaTime);
					}
				}
			}
		);
	}

	[[maybe_unused]] const bool wasSplineGravityRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SplineGravityComponent>>::Make());
	[[maybe_unused]] const bool wasSplineGravityTypeRegistered = Reflection::Registry::RegisterType<SplineGravityComponent>();
}
