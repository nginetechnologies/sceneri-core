#include "PhysicsCore/Components/Gravity/SphericalGravityComponent.h"
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

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	SphericalGravityComponent::SphericalGravityComponent(const SphericalGravityComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_acceleration(templateComponent.m_acceleration)
		, m_radius(templateComponent.m_radius)
	{
		Assert(m_radius.GetMeters() > 0.f);
	}

	SphericalGravityComponent::SphericalGravityComponent(const Deserializer& deserializer)
		: SphericalGravityComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SphericalGravityComponent>().ToString().GetView())
			)
	{
	}

	SphericalGravityComponent::SphericalGravityComponent(
		const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer
	)
		: BaseType(deserializer)
		, m_acceleration(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Accelerationf>("acceleration", 9.81_m_per_second_squared)
																 : 9.81_m_per_second_squared
			)
		, m_radius(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Radiusf>("radius", 20_meters) : Math::Radiusf{20_meters}
			)
	{
		Assert(m_radius.GetMeters() > 0.f);
	}

	SphericalGravityComponent::SphericalGravityComponent(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
		, m_acceleration(initializer.m_acceleration)
		, m_radius(initializer.m_radius)
	{
		Assert(m_radius.GetMeters() > 0.f);
	}

	SphericalGravityComponent::~SphericalGravityComponent()
	{
	}

	void SphericalGravityComponent::OnCreated()
	{
		if (IsEnabled())
		{
			Physics::Data::Scene& physicsScene = Physics::Data::Scene::Get(GetRootScene());
			physicsScene.m_physicsSystem.AddStepListener(this);
		}
	}

	void SphericalGravityComponent::OnEnable()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		physicsScene.m_physicsSystem.AddStepListener(this);
	}

	void SphericalGravityComponent::OnDisable()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		physicsScene.m_physicsSystem.RemoveStepListener(this);
	}

	void SphericalGravityComponent::OnStep(float deltaTime, JPH::PhysicsSystem& physicsSystem)
	{
		class Collector : public JPH::CollideShapeBodyCollector
		{
		public:
			Collector(
				JPH::PhysicsSystem& physicsSystem,
				const Math::WorldCoordinate centerLocation,
				const Math::Radiusf radius,
				const Math::Accelerationf acceleration,
				const float deltaTime
			)
				: m_physicsSystem(physicsSystem)
				, m_centerLocation(centerLocation)
				, m_radius(radius)
				, m_acceleration(acceleration)
				, m_deltaTime(deltaTime)
			{
			}

			virtual void AddHit(const JPH::BodyID& inBodyID) override
			{
				JPH::BodyLockWrite lock(m_physicsSystem.GetBodyLockInterfaceNoLock(), inBodyID);
				JPH::Body& body = lock.GetBody();
				if (body.IsActive())
				{
					const JPH::Vec3 joltBodyLocation = body.GetPosition();
					const Math::WorldCoordinate bodyLocation{joltBodyLocation.GetX(), joltBodyLocation.GetY(), joltBodyLocation.GetZ()};

					const Math::Vector3f distance = m_centerLocation - bodyLocation;
					const float ratio = 1.f - (distance.GetLength() / m_radius.GetMeters());

					const Math::Vector3f gravity = distance.GetNormalized() * m_acceleration.GetMetersPerSecondSquared() * ratio;
					body.GetMotionProperties()->ApplyForceTorqueAndDragInternal(body.GetRotation(), gravity, m_deltaTime);
				}
			}
		private:
			JPH::PhysicsSystem& m_physicsSystem;
			const Math::WorldCoordinate m_centerLocation;
			const Math::Radiusf m_radius;
			const Math::Accelerationf m_acceleration;
			const float m_deltaTime;
		};
		const Math::WorldCoordinate location = GetWorldLocation();
		const Math::Radiusf radius = m_radius * Math::Radiusf::FromMeters(GetWorldScale().GetLength());
		Collector collector(physicsSystem, location, radius, m_acceleration, deltaTime);

		physicsSystem.GetBroadPhaseQuery().CollideSphere(
			{location.x, location.y, location.z},
			radius.GetMeters(),
			collector,
			JPH::SpecifiedBroadPhaseLayerFilter(static_cast<JPH::BroadPhaseLayer>((uint8)BroadPhaseLayer::Dynamic)),
			JPH::SpecifiedObjectLayerFilter(static_cast<JPH::ObjectLayer>(Layer::Dynamic))
		);
	}

	[[maybe_unused]] const bool wasSphericalGravityRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SphericalGravityComponent>>::Make());
	[[maybe_unused]] const bool wasSphericalGravityTypeRegistered = Reflection::Registry::RegisterType<SphericalGravityComponent>();
}
