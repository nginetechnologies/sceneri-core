#include "PhysicsCore/Components/RotatingCharacter.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"
#include "PhysicsCore/Layer.h"
#include "PhysicsCore/Material.h"
#include "PhysicsCore/MaterialAsset.h"
#include "PhysicsCore/Contact.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/CollideShape.h>
#include <3rdparty/jolt/Physics/Collision/ShapeCast.h>
#include <3rdparty/jolt/Physics/Collision/Shape/CylinderShape.h>

#include <3rdparty/jolt/Physics/Collision/RayCast.h>
#include <3rdparty/jolt/Physics/Collision/CastResult.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	RotatingCharacter::RotatingCharacter(const RotatingCharacter& templateComponent, const Cloner& cloner)
		: CharacterBase(templateComponent, cloner)
	{
	}

	RotatingCharacter::RotatingCharacter(const Deserializer& deserializer)
		: CharacterBase(deserializer)
	{
	}

	RotatingCharacter::RotatingCharacter(Initializer&& initializer)
		: CharacterBase(Forward<Initializer>(initializer))
	{
	}

	RotatingCharacter::~RotatingCharacter()
	{
	}

	void RotatingCharacter::OnCreated()
	{
		Entity::ComponentTypeSceneData<RotatingCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RotatingCharacter>&>(*GetTypeSceneData());
		if (IsEnabled())
		{
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void RotatingCharacter::OnEnable()
	{
		Entity::ComponentTypeSceneData<RotatingCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RotatingCharacter>&>(*GetTypeSceneData());
		sceneData.EnableFixedPhysicsUpdate(*this);
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void RotatingCharacter::OnDisable()
	{
		Entity::ComponentTypeSceneData<RotatingCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RotatingCharacter>&>(*GetTypeSceneData());
		sceneData.DisableFixedPhysicsUpdate(*this);
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	RotatingCharacter::GroundState RotatingCharacter::GetGroundState() const
	{
		return m_groundState;
	}

	Math::Vector3f RotatingCharacter::GetGroundNormal() const
	{
		Assert(IsOnGround());
		return m_groundNormal;
	}

	void RotatingCharacter::FixedPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const FrameTime frameTime{physicsScene.GetDeltaTime()};

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const Math::Vector3f currentVelocity = body.GetLinearVelocity();

			Math::Vector3f newVelocity = currentVelocity;
			if (m_groundState == GroundState::InAir)
			{
				// Add some in air control
				newVelocity += m_movementRequest.velocity * m_inAirControl * frameTime;
			}

			newVelocity = newVelocity.GetNormalizedSafe(Math::Forward) *
			              Math::Min(newVelocity.GetLength(), Math::Max(currentVelocity.GetLength(), m_movementRequest.velocity.GetLength()));

			newVelocity += m_movementRequest.impulse;
			body.SetLinearVelocity(newVelocity);

			// Add angular velocity
			const Math::Vector3f movementImpulse = Math::Vector3f(Math::Up).Cross(m_movementRequest.velocity.GetNormalizedSafe(Math::Forward));
			body.AddAngularImpulse(movementImpulse * m_movementRequest.velocity.GetLength() * frameTime);

			m_movementRequest = MovementRequest{};

			if (!body.IsActive())
			{
				lock.ReleaseLock();
				physicsScene.m_physicsSystem.GetBodyInterfaceNoLock().ActivateBody(GetBodyID());
			}
		}
	}

	void RotatingCharacter::AfterPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			const JPH::Body& body = lock.GetBody();

			// Ignore ourselves
			const JPH::IgnoreSingleBodyFilter bodyFilter(body.GetID());
			const JPH::Vec3 gravity = body.GetGravity(
				physicsScene.m_physicsSystem,
				bodyLockInterface,
				JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Gravity)),
				static_cast<JPH::ObjectLayer>(Layer::Gravity)
			);

			// Release body lock
			lock.ReleaseLock();

			JPH::MultiBroadPhaseLayerFilter broadphaseLayerFilter;
			broadphaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Static)));
			broadphaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Dynamic)));

			JPH::MultiObjectLayerFilter objectLayerFilter;
			objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Static));
			objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Dynamic));

			// Create ray
			JPH::RayCast ray{GetFootLocation(), gravity.Normalized() * m_minimumGroundDistance.GetMeters()};

			// Cast ray
			JPH::RayCastResult rayCastResult;
			const bool hitGround = physicsScene.m_physicsSystem.GetNarrowPhaseQuery()
			                         .CastRay(ray, rayCastResult, broadphaseLayerFilter, objectLayerFilter, bodyFilter);

			if (hitGround)
			{
				m_groundState = GroundState::OnGround;

				JPH::BodyLockRead contactBodyLock(bodyLockInterface, rayCastResult.mBodyID);
				if (LIKELY(contactBodyLock.Succeeded()))
				{
					const JPH::Body& contactBody = contactBodyLock.GetBody();

					JPH::Vec3 contactPosition = ray.mOrigin + rayCastResult.mFraction * ray.mDirection;
					m_groundNormal = contactBody.GetWorldSpaceSurfaceNormal(rayCastResult.mSubShapeID2, contactPosition);
				}
			}
			else
			{
				m_groundState = GroundState::InAir;
				m_groundNormal = Math::Zero;
			}
		}
	}

	[[maybe_unused]] const bool wasRotatingCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RotatingCharacter>>::Make());
	[[maybe_unused]] const bool wasRotatingCharacterTypeRegistered = Reflection::Registry::RegisterType<RotatingCharacter>();
}
