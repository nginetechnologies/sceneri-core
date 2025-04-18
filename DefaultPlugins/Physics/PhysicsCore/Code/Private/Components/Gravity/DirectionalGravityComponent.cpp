#include "PhysicsCore/Components/Gravity/DirectionalGravityComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Plugin.h"
#include <3rdparty/jolt/Physics/Body/BodyLockMulti.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	DirectionalGravityComponent::DirectionalGravityComponent(const DirectionalGravityComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_acceleration(templateComponent.m_acceleration)
	{
	}

	DirectionalGravityComponent::DirectionalGravityComponent(const Deserializer& deserializer)
		: DirectionalGravityComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<DirectionalGravityComponent>().ToString().GetView())
			)
	{
	}

	DirectionalGravityComponent::DirectionalGravityComponent(
		const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer
	)
		: BaseType(deserializer)
		, m_acceleration(
				typeSerializer.IsValid() ? typeSerializer->ReadWithDefaultValue<Math::Accelerationf>("acceleration", 9.81_m_per_second_squared)
																 : 9.81_m_per_second_squared
			)
	{
	}

	DirectionalGravityComponent::DirectionalGravityComponent(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
		, m_acceleration(initializer.m_acceleration)
	{
	}

	DirectionalGravityComponent::~DirectionalGravityComponent()
	{
	}

	static Threading::Mutex globalGravityMutex;

	void DirectionalGravityComponent::OnCreated()
	{
		if (IsEnabled())
		{
			const Optional<Physics::Data::Scene*> pPhysicsScene = Physics::Data::Scene::Get(GetRootScene());
			if (Ensure(pPhysicsScene.IsValid()))
			{
				pPhysicsScene->m_physicsSystem.AddStepListener(this);

				{
					const Math::Vector3f gravity =
						GetWorldRotation().TransformDirection(Math::Vector3f{0, m_acceleration.GetMetersPerSecondSquared(), 0});
					m_previousGravity = gravity;

					Threading::UniqueLock globalGravityLock(globalGravityMutex);
					pPhysicsScene->m_physicsSystem.SetGlobalGravity(pPhysicsScene->m_physicsSystem.GetGlobalGravity() + gravity);
				}
			}
		}
	}

	void DirectionalGravityComponent::OnEnable()
	{
		const Optional<Physics::Data::Scene*> pPhysicsScene = Physics::Data::Scene::Get(GetRootScene());
		if (Ensure(pPhysicsScene.IsValid()))
		{
			pPhysicsScene->m_physicsSystem.AddStepListener(this);

			{
				const Math::Vector3f gravity =
					GetWorldRotation().TransformDirection(Math::Vector3f{0, m_acceleration.GetMetersPerSecondSquared(), 0});
				m_previousGravity = gravity;

				Threading::UniqueLock globalGravityLock(globalGravityMutex);
				pPhysicsScene->m_physicsSystem.SetGlobalGravity(pPhysicsScene->m_physicsSystem.GetGlobalGravity() + gravity);
			}
		}
	}

	void DirectionalGravityComponent::OnDisable()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		physicsScene.m_physicsSystem.RemoveStepListener(this);

		{
			const Math::Vector3f gravity = m_previousGravity;

			Threading::UniqueLock globalGravityLock(globalGravityMutex);
			physicsScene.m_physicsSystem.SetGlobalGravity(physicsScene.m_physicsSystem.GetGlobalGravity() - gravity);
		}
	}

	void DirectionalGravityComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags>)
	{
		const Math::Vector3f newGravity = GetWorldRotation().TransformDirection(Math::Vector3f{0, m_acceleration.GetMetersPerSecondSquared(), 0}
		);

		const Math::Vector3f previousGravity = m_previousGravity;

		{
			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

			Threading::UniqueLock globalGravityLock(globalGravityMutex);
			physicsScene.m_physicsSystem.SetGlobalGravity(physicsScene.m_physicsSystem.GetGlobalGravity() - previousGravity + newGravity);
		}

		m_previousGravity = newGravity;
	}

	void DirectionalGravityComponent::OnStep(float inDeltaTime, JPH::PhysicsSystem& physicsSystem)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::BodyIDVector bodyIDs;
		physicsSystem.GetActiveBodies(bodyIDs);

		const Math::Vector3f gravity = GetWorldRotation().TransformDirection(Math::Vector3f{0, m_acceleration.GetMetersPerSecondSquared(), 0});

		// TODO: Break into several jobs once performance starts to matter

		JPH::BodyLockMultiWrite multiLock(physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock(), bodyIDs.data(), (uint32)bodyIDs.size());
		for (uint32 i = 0, n = (uint32)bodyIDs.size(); i < n; ++i)
		{
			JPH::Body* pBody = multiLock.GetBody(i);
			if (pBody != nullptr)
			{
				if (pBody->GetMotionType() == JPH::EMotionType::Dynamic)
					pBody->GetMotionProperties()->ApplyForceTorqueAndDragInternal(pBody->GetRotation(), gravity, inDeltaTime);
			}
		}
	}

	[[maybe_unused]] const bool wasDirectionalGravityRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<DirectionalGravityComponent>>::Make());
	[[maybe_unused]] const bool wasDirectionalGravityTypeRegistered = Reflection::Registry::RegisterType<DirectionalGravityComponent>();
}
