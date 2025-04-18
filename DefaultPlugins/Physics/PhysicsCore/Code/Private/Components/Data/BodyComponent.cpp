#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/Data/BodySettings.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/BodySettings.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/ColliderComponent.h"
#include "PhysicsCore/Components/MeshColliderComponent.h"
#include "PhysicsCore/Plugin.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Scene/Scene.h>
#include <Common/System/Query.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/ClampedValue.h>

#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ScaledShape.h>

namespace ngine::Physics::Data
{
	static Threading::Mutex bodyCreationMutex;

	[[nodiscard]] JPH::BodyCreationSettings
	Body::GetJoltBodyCreationSettings(const Math::WorldTransform transform, const BodySettings& bodySettings)
	{
		JPH::BodyCreationSettings bodyCreationSettings;

		const Math::WorldCoordinate worldLocation = transform.GetLocation();
		const Math::Quaternionf worldRotation = transform.GetRotationQuaternion();
		const JPH::Vec3 joltLocation = {worldLocation.x, worldLocation.y, worldLocation.z};
		const JPH::Quat joltRotation = {worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w};

		JPH::MutableCompoundShapeSettings compoundShapeSettings;
		JPH::ShapeSettings::ShapeResult compoundShapeResult = compoundShapeSettings.Create();
		JPH::ShapeRefC compoundShape = compoundShapeResult.Get();

		Data::Body::Type bodyType = bodySettings.m_type;

		switch (bodyType)
		{
			case Data::Body::Type::Static:
				bodyCreationSettings = JPH::BodyCreationSettings(
					compoundShape,
					joltLocation,
					joltRotation,
					JPH::EMotionType::Static,
					static_cast<JPH::ObjectLayer>(bodySettings.m_layer)
				);
				break;
			case Data::Body::Type::Dynamic:
				bodyCreationSettings = JPH::BodyCreationSettings(
					compoundShape,
					joltLocation,
					joltRotation,
					JPH::EMotionType::Dynamic,
					static_cast<JPH::ObjectLayer>(bodySettings.m_layer)
				);
				break;
			case Data::Body::Type::Kinematic:
				bodyCreationSettings = JPH::BodyCreationSettings(
					compoundShape,
					joltLocation,
					joltRotation,
					JPH::EMotionType::Kinematic,
					static_cast<JPH::ObjectLayer>(bodySettings.m_layer)
				);
				break;
		}

		if (bodySettings.m_flags.IsSet(Data::Body::Flags::HasOverriddenMass))
		{
			bodyCreationSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateInertia;
			bodyCreationSettings.mMassPropertiesOverride.mMass = bodySettings.m_overriddenMass.GetKilograms();
		}
		else
		{
			bodyCreationSettings.mOverrideMassProperties = JPH::EOverrideMassProperties::CalculateMassAndInertia;
		}

		bodyCreationSettings.mIsSensor = bodySettings.m_flags.IsSet(Data::Body::Flags::IsSensorOnly);
		bodyCreationSettings.mMotionQuality = bodySettings.m_flags.IsSet(Data::Body::Flags::LinearCast) ? JPH::EMotionQuality::LinearCast
		                                                                                                : JPH::EMotionQuality::Discrete;
		bodyCreationSettings.mMaxAngularVelocity = bodySettings.m_maximumAngularVelocity.GetRadians();
		bodyCreationSettings.mAllowSleeping = bodySettings.m_flags.IsNotSet(Data::Body::Flags::KeepAwake);
		bodyCreationSettings.mGravityFactor = bodySettings.m_gravityScale;

		// TODO: Remove this and add a conversion path for static to dynamic data components.
		// For now this makes it possible to turn every static body into a dynamic one at runtime.
		bodyCreationSettings.mAllowDynamicOrKinematic = true;

		return bodyCreationSettings;
	}

	Body::Body(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_settings(initializer.m_settings)
	{
		Assert(initializer.m_settings.m_overriddenMass == 0_kilograms || initializer.m_settings.m_flags.IsSet(Flags::HasOverriddenMass));

		Entity::Component3D& owner = initializer.GetParent();
		ngine::Scene3D& scene = owner.GetRootScene();
		Physics::Data::Scene& physicsScene = Physics::Data::Scene::Get(scene);
		const JPH::BodyID bodyIdentifier = physicsScene.RegisterBody();
		m_bodyIdentifier = bodyIdentifier;
		Assert(bodyIdentifier.IsValid());
		if (LIKELY(bodyIdentifier.IsValid()))
		{
			// TODO: Move this out of the data namespace and rename to remove the Physics prefix
			Physics::Data::PhysicsCommandStage& commandStage = physicsScene.GetCommandStage();

			commandStage.CreateBody(
				bodyIdentifier,
				GetJoltBodyCreationSettings(owner.GetWorldTransform(), initializer.m_settings),
				reinterpret_cast<uint64>(&owner)
			);

			if (owner.IsEnabled())
			{
				commandStage.AddBody(bodyIdentifier);

				RegisterDynamicBody(physicsScene);

				owner.OnWorldTransformChangedEvent.Add(
					*this,
					[&owner](Body& body, const EnumFlags<Entity::TransformChangeFlags> flags)
					{
						if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
						{
							body.OnOwnerWorldTransformChanged(owner, flags);
						}
					}
				);
			}
		}
	}

	Body::Body(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		Assert(m_settings.m_overriddenMass == 0_kilograms || m_settings.m_flags.IsSet(Flags::HasOverriddenMass));

		Entity::Component3D& owner = deserializer.GetParent();
		ngine::Scene3D& scene = owner.GetRootScene();
		Physics::Data::Scene& physicsScene = Physics::Data::Scene::Get(scene);
		const JPH::BodyID bodyIdentifier = physicsScene.RegisterBody();
		m_bodyIdentifier = bodyIdentifier;
		Assert(bodyIdentifier.IsValid());
		if (LIKELY(bodyIdentifier.IsValid()))
		{
			// TODO: Move this out of the data namespace and rename to remove the Physics prefix
			Physics::Data::PhysicsCommandStage& commandStage = physicsScene.GetCommandStage();

			m_settings.m_type = deserializer.m_reader.ReadWithDefaultValue<Type>("type", Type{m_settings.m_type});
			m_settings.m_layer = deserializer.m_reader.ReadWithDefaultValue<Layer>("layer", Layer{m_settings.m_layer});
			m_settings.m_maximumAngularVelocity = deserializer.m_reader.ReadWithDefaultValue<Math::Anglef>(
				"maximumAngularVelocity",
				Math::Anglef(m_settings.m_maximumAngularVelocity)
			);
			m_settings.m_overriddenMass =
				deserializer.m_reader.ReadWithDefaultValue<Math::Massf>("overriddenMass", Math::Massf(m_settings.m_overriddenMass));
			m_settings.m_gravityScale =
				deserializer.m_reader.ReadWithDefaultValue<Math::Ratiof>("gravityScale", Math::Ratiof(m_settings.m_gravityScale));
			EnumFlags<Flags> flags =
				deserializer.m_reader.ReadWithDefaultValue<EnumFlags<Flags>>("body_flags", EnumFlags<Flags>{m_settings.m_flags});
			flags |= Flags::HasOverriddenMass * (m_settings.m_overriddenMass > 0_kilograms);
			m_settings.m_flags = flags;

			commandStage
				.CreateBody(bodyIdentifier, GetJoltBodyCreationSettings(owner.GetWorldTransform(), m_settings), reinterpret_cast<uint64>(&owner));

			if (owner.IsEnabled())
			{
				commandStage.AddBody(bodyIdentifier);

				RegisterDynamicBody(physicsScene);

				owner.OnWorldTransformChangedEvent.Add(
					*this,
					[&owner](Body& body, const EnumFlags<Entity::TransformChangeFlags> flags)
					{
						if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
						{
							body.OnOwnerWorldTransformChanged(owner, flags);
						}
					}
				);
			}
		}
	}

	Body::Body(const Body& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_settings(templateComponent.m_settings)
	{
		Assert(m_settings.m_overriddenMass == 0_kilograms || m_settings.m_flags.IsSet(Flags::HasOverriddenMass));

		Entity::Component3D& owner = cloner.GetParent();
		ngine::Scene3D& scene = owner.GetRootScene();
		Physics::Data::Scene& physicsScene = Physics::Data::Scene::Get(scene);
		const JPH::BodyID bodyIdentifier = physicsScene.RegisterBody();
		m_bodyIdentifier = bodyIdentifier;
		Assert(bodyIdentifier.IsValid());
		if (LIKELY(bodyIdentifier.IsValid() & templateComponent.m_bodyIdentifier.IsValid()))
		{
			// TODO: Move this out of the data namespace and rename to remove the Physics prefix
			Physics::Data::PhysicsCommandStage& commandStage = physicsScene.GetCommandStage();

			Physics::Data::Scene& templatePhysicsScene =
				*cloner.GetTemplateParent().GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			commandStage.CloneBody(
				bodyIdentifier,
				templatePhysicsScene,
				templateComponent.m_bodyIdentifier,
				reinterpret_cast<uint64>(&owner),
				reinterpret_cast<uint64>(&cloner.GetTemplateParent())
			);

			if (cloner.GetParent().IsEnabled())
			{
				commandStage.AddBody(bodyIdentifier);

				RegisterDynamicBody(physicsScene);

				owner.OnWorldTransformChangedEvent.Add(
					*this,
					[&owner](Body& body, const EnumFlags<Entity::TransformChangeFlags> flags)
					{
						if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
						{
							body.OnOwnerWorldTransformChanged(owner, flags);
						}
					}
				);
			}
		}
	}

	Body::~Body()
	{
	}

	void Body::DeserializeCustomData(const Optional<Serialization::Reader> reader, Entity::Component3D& owner)
	{
		if (reader.IsValid())
		{
			Physics::Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

			BodySettings newSettings;
			newSettings.m_type = reader->ReadWithDefaultValue<Type>("type", Type{m_settings.m_type});
			newSettings.m_layer = reader->ReadWithDefaultValue<Layer>("layer", Layer{m_settings.m_layer});
			newSettings.m_maximumAngularVelocity =
				reader->ReadWithDefaultValue<Math::Anglef>("maximumAngularVelocity", Math::Anglef(m_settings.m_maximumAngularVelocity));
			newSettings.m_overriddenMass = reader->ReadWithDefaultValue<Math::Massf>("overriddenMass", Math::Massf(m_settings.m_overriddenMass));
			newSettings.m_gravityScale = reader->ReadWithDefaultValue<Math::Ratiof>("gravityScale", Math::Ratiof(m_settings.m_gravityScale));
			newSettings.m_flags = reader->ReadWithDefaultValue<EnumFlags<Flags>>("body_flags", EnumFlags<Flags>{m_settings.m_flags});

			if (newSettings.m_flags != m_settings.m_flags)
			{
				m_settings.m_flags = newSettings.m_flags;
			}

			if (newSettings.m_type != m_settings.m_type)
			{
				SetTypeInternal(physicsScene, newSettings.m_type);
			}
			if (newSettings.m_layer != m_settings.m_layer)
			{
				SetLayer(physicsScene, newSettings.m_layer);
			}
			if (newSettings.m_maximumAngularVelocity != m_settings.m_maximumAngularVelocity)
			{
				SetMaximumAngularVelocity(physicsScene, newSettings.m_maximumAngularVelocity);
			}
			if (newSettings.m_overriddenMass != m_settings.m_overriddenMass)
			{
				SetMassOverride(physicsScene, newSettings.m_overriddenMass);
			}
			if (newSettings.m_gravityScale != m_settings.m_gravityScale)
			{
				SetGravityScale(physicsScene, newSettings.m_gravityScale);
			}

			if (newSettings.m_type != m_settings.m_type || newSettings.m_layer != m_settings.m_layer)
			{
				Entity::SceneRegistry& sceneRegistry = physicsScene.m_engineScene.GetEntitySceneRegistry();
				Threading::SharedLock lock(m_colliderMutex);
				for (ColliderComponent* pCollider : m_colliders)
				{
					MeshColliderComponent* pMeshCollider = pCollider ? pCollider->AsExactType<MeshColliderComponent>(sceneRegistry) : nullptr;
					if (pMeshCollider)
					{
						pMeshCollider->CreateStaticMeshShape(this);
					}
				}
			}
		}
	}

	void Body::OnOwnerWorldTransformChanged(Entity::Component3D& owner, const EnumFlags<Entity::TransformChangeFlags> changeFlags)
	{
		const Math::WorldTransform worldTransform = owner.GetWorldTransform();
		const Math::Quaternionf worldRotation = worldTransform.GetRotationQuaternion();
		const JPH::Vec3 joltLocation = {worldTransform.GetLocation().x, worldTransform.GetLocation().y, worldTransform.GetLocation().z};
		JPH::Quat joltRotation = {worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w};
		joltRotation = joltRotation.Normalized();

		Physics::Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		if (changeFlags.IsSet(Entity::TransformChangeFlags::ChangedByTransformReset))
		{
			physicsScene.GetCommandStage().SetBodyTransform(m_bodyIdentifier, joltLocation, joltRotation);
		}
		else
		{
			switch (m_settings.m_type)
			{
				case Type::Kinematic:
					physicsScene.GetCommandStage().MoveKinematicBody(m_bodyIdentifier, joltLocation, joltRotation);
					break;
				default:
					physicsScene.GetCommandStage().SetBodyTransform(m_bodyIdentifier, joltLocation, joltRotation);
					break;
			}
		}
	}

	void Body::SetWorldLocationAndRotationFromPhysics(
		Entity::Component3D& owner,
		const Math::WorldCoordinate location,
		const Math::WorldQuaternion rotation,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData,
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData
	)
	{
		Assert(m_settings.m_type != Type::Kinematic);
		Assert(m_settings.m_type != Type::Static);

		const Entity::ComponentIdentifier identifier = owner.GetIdentifier();
		Entity::Data::WorldTransform& worldTransformComponent = *worldTransformSceneData.GetComponentImplementation(identifier);

		Math::WorldTransform worldTransform = worldTransformComponent;
		if (worldTransform.GetLocation().IsEquivalentTo(location) && worldTransform.GetRotationQuaternion().IsEquivalentTo(rotation))
		{
			return;
		}

		worldTransform.SetLocation(location);
		worldTransform.SetRotation(rotation);
		worldTransformComponent = worldTransform;

		const Math::WorldTransform parentWorldTransform = *worldTransformSceneData.GetComponentImplementation(owner.GetParent().GetIdentifier()
		);

		Entity::Data::LocalTransform3D& localTransformComponent = *localTransformSceneData.GetComponentImplementation(identifier);

		const Math::LocalTransform newLocalTransform = parentWorldTransform.GetTransformRelativeToAsLocal(worldTransform);
		localTransformComponent = newLocalTransform;

		owner.OnWorldTransformChanged(Entity::TransformChangeFlags::ChangedByPhysics);
		owner.OnWorldTransformChangedEvent(Entity::TransformChangeFlags::ChangedByPhysics);

		for (Entity::Component3D& child : owner.GetChildren())
		{
			child.OnParentWorldTransformChanged(
				worldTransform,
				worldTransformSceneData,
				localTransformSceneData,
				flagsSceneData,
				Entity::TransformChangeFlags::ChangedByPhysics
			);
		}

		Assert(owner.IsRegisteredInTree());
		if (LIKELY(owner.IsRegisteredInTree()))
		{
			owner.GetRootSceneComponent().OnComponentWorldLocationOrBoundsChanged(owner, worldTransformSceneData.GetSceneRegistry());
		}
	}

	void Body::RegisterDynamicBody(Scene& scene)
	{
		if (m_settings.m_type == Type::Dynamic || m_settings.m_type == Type::Kinematic)
		{
			scene.AddDynamicBody(m_bodyIdentifier);
		}
	}

	void Body::UnRegisterDynamicBody(Scene& scene)
	{
		scene.RemoveDynamicBody(m_bodyIdentifier);
	}

	bool Body::AddCollider(ColliderComponent& collider)
	{
		Assert(collider.IsEnabled());

		const ColliderIdentifier colliderIdentifier = m_colliderIdentifiers.AcquireIdentifier();
		Assert(colliderIdentifier.IsValid());
		if (LIKELY(colliderIdentifier.IsValid()))
		{
			{
				Threading::UniqueLock lock(m_colliderMutex);
				m_colliders.Resize(colliderIdentifier.GetFirstValidIndex() + 1);
				m_colliders[colliderIdentifier.GetFirstValidIndex()] = &collider;
			}
			collider.m_pShape->SetUserData(colliderIdentifier.GetValue());
			collider.m_colliderIdentifier = colliderIdentifier;

			Assert(collider.m_pShape->GetSubType() == JPH::EShapeSubType::RotatedTranslated);
			JPH::DecoratedShape& rotatedTranslatedShape = static_cast<JPH::DecoratedShape&>(*collider.m_pShape);
			Assert(rotatedTranslatedShape.GetInnerShape()->GetSubType() == JPH::EShapeSubType::Scaled);
			JPH::DecoratedShape& scaledShape = static_cast<JPH::DecoratedShape&>(const_cast<JPH::Shape&>(*rotatedTranslatedShape.GetInnerShape())
			);
			scaledShape.SetUserData(colliderIdentifier.GetValue());

			if (scaledShape.GetInnerShape()->GetSubType() == JPH::EShapeSubType::RotatedTranslated)
			{
				JPH::DecoratedShape& innerRotatedTranslatedShape =
					static_cast<JPH::DecoratedShape&>(const_cast<JPH::Shape&>(*scaledShape.GetInnerShape()));
				innerRotatedTranslatedShape.SetUserData(colliderIdentifier.GetValue());
				JPH::Shape& actualShape = const_cast<JPH::Shape&>(*innerRotatedTranslatedShape.GetInnerShape());
				actualShape.SetUserData(colliderIdentifier.GetValue());
			}
			else
			{
				JPH::Shape& actualShape = const_cast<JPH::Shape&>(*scaledShape.GetInnerShape());
				actualShape.SetUserData(colliderIdentifier.GetValue());
			}

			Physics::Data::Scene& physicsScene = *collider.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			physicsScene.GetCommandStage().AddCollider(m_bodyIdentifier, collider.m_pShape, colliderIdentifier);
			return true;
		}
		else
		{
			Assert(collider.m_colliderIdentifier.IsInvalid());
		}
		return false;
	}

	void Body::OnDestroying(Entity::Component3D& owner)
	{
		const Optional<Physics::Data::Scene*> pPhysicsScene = owner.GetRootSceneComponent().FindDataComponentOfType<Scene>();
		Assert(pPhysicsScene.IsValid());
		if (LIKELY(pPhysicsScene.IsValid()))
		{
			{
				const JPH::BodyLockInterface& bodyLockInterface = pPhysicsScene->m_physicsSystem.GetBodyLockInterface();
				JPH::BodyLockWrite lock(bodyLockInterface, m_bodyIdentifier);
				if (lock.Succeeded())
				{
					lock.GetBody().SetUserData(0);
				}
			}

			if (owner.IsEnabled())
			{
				pPhysicsScene->GetCommandStage().RemoveBody(m_bodyIdentifier);
				UnRegisterDynamicBody(*pPhysicsScene);

				owner.OnWorldTransformChangedEvent.Remove(this);
			}

			pPhysicsScene->GetCommandStage().DestroyBody(m_bodyIdentifier);
		}
	}

	void Body::OnEnable(Entity::Component3D& owner)
	{
		Physics::Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Scene>();
		physicsScene.GetCommandStage().AddBody(m_bodyIdentifier);

		owner.OnWorldTransformChangedEvent.Add(
			*this,
			[&owner](Body& body, const EnumFlags<Entity::TransformChangeFlags> flags)
			{
				if (flags.IsNotSet(Entity::TransformChangeFlags::ChangedByPhysics))
				{
					body.OnOwnerWorldTransformChanged(owner, flags);
				}
			}
		);
	}

	void Body::OnDisable(Entity::Component3D& owner)
	{
		Physics::Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Scene>();
		physicsScene.GetCommandStage().RemoveBody(m_bodyIdentifier);
		UnRegisterDynamicBody(physicsScene);

		owner.OnWorldTransformChangedEvent.Remove(this);
	}

	void Body::Wake(Scene& physicsScene)
	{
		physicsScene.GetCommandStage().WakeBodyFromSleep(m_bodyIdentifier);
	}

	void Body::Sleep(Scene& physicsScene)
	{
		physicsScene.GetCommandStage().PutBodyToSleep(m_bodyIdentifier);
	}

	bool Body::IsAwake(Scene& physicsScene) const
	{
		JPH::BodyLockRead lock = AutoLockRead(physicsScene);
		if (lock.Succeeded())
		{
			return lock.GetBody().IsActive();
		}
		Assert(false);
		return false;
	}

	bool Body::WasCreated(Scene& physicsScene) const
	{
		const JPH::BodyInterface& bodyInterfaceNoLock = physicsScene.m_physicsSystem.GetBodyInterfaceNoLock();
		return bodyInterfaceNoLock.IsBodyValid(m_bodyIdentifier);
	}

	void Body::SetTypeInternal(Scene& physicsScene, const Type type)
	{
		m_settings.m_type = type;
		switch (type)
		{
			case Type::Static:
				physicsScene.GetCommandStage().SetBodyMotionType(m_bodyIdentifier, JPH::EMotionType::Static, JPH::EActivation::DontActivate);
				UnRegisterDynamicBody(physicsScene);
				break;
			case Type::Kinematic:
				physicsScene.GetCommandStage().SetBodyMotionType(m_bodyIdentifier, JPH::EMotionType::Kinematic, JPH::EActivation::DontActivate);
				RegisterDynamicBody(physicsScene);
				break;
			case Type::Dynamic:
				physicsScene.GetCommandStage().SetBodyMotionType(m_bodyIdentifier, JPH::EMotionType::Dynamic, JPH::EActivation::DontActivate);
				RegisterDynamicBody(physicsScene);
				break;
			default:
				ExpectUnreachable();
		}
	}

	Body::Type Body::GetActiveType(Scene& physicsScene) const
	{
		JPH::BodyLockRead lock = AutoLockRead(physicsScene);
		if (lock.Succeeded())
		{
			switch (lock.GetBody().GetMotionType())
			{
				case JPH::EMotionType::Static:
					return Type::Static;
				case JPH::EMotionType::Kinematic:
					return Type::Kinematic;
				case JPH::EMotionType::Dynamic:
					return Type::Dynamic;
			}
		}
		return m_settings.m_type;
	}

	bool Body::SetType(Scene& physicsScene, const Type type)
	{
		if (type == m_settings.m_type)
		{
			return false;
		}

		SetTypeInternal(physicsScene, type);
		switch (type)
		{
			case Type::Static:
			{
				if (m_settings.m_layer != Layer::Static)
				{
					SetLayer(physicsScene, Physics::Layer::Static);
				}
			}
			break;
			case Type::Kinematic:
			case Type::Dynamic:
			{
				if (m_settings.m_layer != Layer::Dynamic)
				{
					SetLayer(physicsScene, Physics::Layer::Dynamic);
				}
			}
			break;
		}

		Entity::SceneRegistry& sceneRegistry = physicsScene.m_engineScene.GetEntitySceneRegistry();
		Threading::SharedLock lock(m_colliderMutex);
		for (ColliderComponent* pCollider : m_colliders)
		{
			MeshColliderComponent* pMeshCollider = pCollider ? pCollider->AsExactType<MeshColliderComponent>(sceneRegistry) : nullptr;
			if (pMeshCollider)
			{
				pMeshCollider->CreateStaticMeshShape(this);
			}
		}

		return true;
	}

	void Body::ChangeTypeFromProperty(Entity::Component3D& owner, const Data::Body::Type type)
	{
		Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>();
		SetType(physicsScene, type);
	}

	void Body::SetTypeFromProperty(Entity::Component3D& owner, const Type type)
	{
		SetTypeInternal(*owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(), type);
	}

	Body::Type Body::GetTypeFromProperty(Entity::Component3D&)
	{
		return GetType();
	}

	void Body::SetLayerFromProperty(Entity::Component3D& owner, const Layer layer)
	{
		SetLayer(*owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(), layer);
	}

	Layer Body::GetLayerFromProperty(Entity::Component3D&)
	{
		return GetLayer();
	}

	void Body::SetFlagsFromProperty(Entity::Component3D&, const EnumFlags<Flags> flags)
	{
		m_settings.m_flags = flags;
	}

	EnumFlags<Body::Flags> Body::GetFlagsFromProperty(Entity::Component3D&)
	{
		return m_settings.m_flags;
	}

	void Body::SetMaximumAngularVelocityFromProperty(Entity::Component3D& owner, const Math::Anglef velocity)
	{
		SetMaximumAngularVelocity(*owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(), velocity);
	}

	Math::Anglef Body::GetMaximumAngularVelocityFromProperty(Entity::Component3D&)
	{
		return m_settings.m_maximumAngularVelocity;
	}

	void Body::SetOverriddenMassFromProperty(Entity::Component3D& owner, const Math::Massf mass)
	{
		SetMassOverride(*owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(), mass);
	}

	Math::Massf Body::GetOverriddenMassFromProperty(Entity::Component3D&)
	{
		return m_settings.m_overriddenMass;
	}

	void Body::SetGravityScaleFromProperty(Entity::Component3D& owner, const Math::Ratiof scale)
	{
		SetGravityScale(*owner.GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(), scale);
	}

	Math::Ratiof Body::GetGravityScaleFromProperty(Entity::Component3D&)
	{
		return m_settings.m_gravityScale;
	}

	void Body::SetLayer(Scene& physicsScene, const Layer layer)
	{
		m_settings.m_layer = layer;
		physicsScene.GetCommandStage().SetBodyLayer(m_bodyIdentifier, static_cast<JPH::ObjectLayer>(layer));
	}

	Layer Body::GetActiveLayer(Scene& physicsScene) const
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			return static_cast<Physics::Layer>(lock.GetBody().GetObjectLayer());
		}
		return m_settings.m_layer;
	}

	void Body::AddForce(Scene& physicsScene, const Math::Vector3f force)
	{
		physicsScene.GetCommandStage().AddForce(m_bodyIdentifier, force);
	}

	void Body::AddForceAtLocation(Scene& physicsScene, const Math::Vector3f force, const Math::WorldCoordinate location)
	{
		physicsScene.GetCommandStage().AddForceAtLocation(m_bodyIdentifier, force, location);
	}

	void Body::AddImpulse(Scene& physicsScene, const Math::Vector3f impulse)
	{
		physicsScene.GetCommandStage().AddImpulse(m_bodyIdentifier, impulse);
	}

	void Body::AddImpulseAtLocation(Scene& physicsScene, const Math::Vector3f impulse, const Math::WorldCoordinate location)
	{
		physicsScene.GetCommandStage().AddImpulseAtLocation(m_bodyIdentifier, impulse, location);
	}

	void Body::AddTorque(Scene& physicsScene, const Math::Vector3f torque)
	{
		physicsScene.GetCommandStage().AddTorque(m_bodyIdentifier, torque);
	}

	void Body::AddAngularImpulse(Scene& physicsScene, const Math::Vector3f acceleration)
	{
		physicsScene.GetCommandStage().AddAngularImpulse(m_bodyIdentifier, acceleration);
	}

	void Body::SetVelocity(Scene& physicsScene, const Math::Vector3f velocity)
	{
		physicsScene.GetCommandStage().SetBodyVelocity(m_bodyIdentifier, velocity);
	}

	Math::Vector3f Body::GetVelocity(Scene& physicsScene) const
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			return lock.GetBody().GetLinearVelocity();
		}
		return Math::Zero;
	}

	Math::Vector3f Body::GetVelocityAtPoint(Scene& physicsScene, const Math::WorldCoordinate coordinate) const
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			return lock.GetBody().GetPointVelocity(coordinate);
		}
		return Math::Zero;
	}

	void Body::SetAngularVelocity(Scene& physicsScene, const Math::Angle3f angularVelocity)
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			lock.GetBody().SetAngularVelocity({angularVelocity.x.GetRadians(), angularVelocity.y.GetRadians(), angularVelocity.z.GetRadians()});
		}
	}

	Math::Angle3f Body::GetAngularVelocity(Scene& physicsScene) const
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::Vec3 angularVelocity = lock.GetBody().GetAngularVelocity();
			return {
				Math::Anglef::FromRadians(angularVelocity.GetX()),
				Math::Anglef::FromRadians(angularVelocity.GetY()),
				Math::Anglef::FromRadians(angularVelocity.GetZ())
			};
		}
		return Math::Zero;
	}

	void Body::SetMaximumAngularVelocity(Scene& physicsScene, const Math::Anglef angularVelocity)
	{
		m_settings.m_maximumAngularVelocity = angularVelocity;

		JPH::BodyLockWrite lock = AutoLockWrite(physicsScene);
		if (lock.Succeeded())
		{
			JPH::Body& body = lock.GetBody();
			JPH::MotionProperties* pMotionProperties = body.GetMotionPropertiesUnchecked();
			if (pMotionProperties != nullptr)
			{
				pMotionProperties->SetMaxAngularVelocity(angularVelocity.GetRadians());
			}
		}
	}

	Optional<Math::WorldCoordinate> Body::GetWorldLocation(Scene& physicsScene) const
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::Vec3 bodyLocation = lock.GetBody().GetPosition();
			return Math::WorldCoordinate{bodyLocation.GetX(), bodyLocation.GetY(), bodyLocation.GetZ()};
		}

		return Invalid;
	}

	Optional<Math::WorldRotation> Body::GetWorldRotation(Scene& physicsScene) const
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::Quat bodyRotation = lock.GetBody().GetRotation();
			return Math::WorldRotation{bodyRotation.GetX(), bodyRotation.GetY(), bodyRotation.GetZ(), bodyRotation.GetW()};
		}

		return Invalid;
	}

	Optional<Math::WorldTransform> Body::GetWorldTransform(Scene& physicsScene) const
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::Vec3 bodyLocation = lock.GetBody().GetPosition();
			const JPH::Quat bodyRotation = lock.GetBody().GetRotation();
			return Math::WorldTransform{
				Math::WorldRotation{bodyRotation.GetX(), bodyRotation.GetY(), bodyRotation.GetZ(), bodyRotation.GetW()},
				Math::WorldCoordinate{bodyLocation.GetX(), bodyLocation.GetY(), bodyLocation.GetZ()}
			};
		}

		return Invalid;
	}

	Optional<Math::WorldTransform> Body::GetCenterOfMassWorldTransform(Scene& physicsScene) const
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::Vec3 bodyCenterOfMassLocation = lock.GetBody().GetCenterOfMassPosition();
			const JPH::Quat bodyRotation = lock.GetBody().GetRotation();
			return Math::WorldTransform{
				Math::WorldRotation{bodyRotation.GetX(), bodyRotation.GetY(), bodyRotation.GetZ(), bodyRotation.GetW()},
				Math::WorldCoordinate{bodyCenterOfMassLocation.GetX(), bodyCenterOfMassLocation.GetY(), bodyCenterOfMassLocation.GetZ()}
			};
		}

		return Invalid;
	}

	void Body::SetWorldLocation(Scene& physicsScene, const Math::WorldCoordinate location)
	{
		const JPH::Vec3 joltLocation = {location.x, location.y, location.z};

		switch (m_settings.m_type)
		{
			case Type::Kinematic:
				physicsScene.GetCommandStage().MoveKinematicBody(m_bodyIdentifier, joltLocation);
				break;
			default:
				physicsScene.GetCommandStage().SetBodyLocation(m_bodyIdentifier, joltLocation);
				break;
		}
	}

	void Body::SetWorldRotation(Scene& physicsScene, const Math::WorldRotation worldRotation)
	{
		JPH::Quat joltRotation = {worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w};
		joltRotation = joltRotation.Normalized();

		switch (m_settings.m_type)
		{
			case Type::Kinematic:
				physicsScene.GetCommandStage().MoveKinematicBody(m_bodyIdentifier, joltRotation);
				break;
			default:
				physicsScene.GetCommandStage().SetBodyRotation(m_bodyIdentifier, joltRotation);
				break;
		}
	}

	void Body::SetWorldTransform(Scene& physicsScene, const Math::WorldTransform worldTransform)
	{
		const Math::Quaternionf worldRotation = worldTransform.GetRotationQuaternion();
		const JPH::Vec3 joltLocation = {worldTransform.GetLocation().x, worldTransform.GetLocation().y, worldTransform.GetLocation().z};
		JPH::Quat joltRotation = {worldRotation.x, worldRotation.y, worldRotation.z, worldRotation.w};
		joltRotation = joltRotation.Normalized();

		switch (m_settings.m_type)
		{
			case Type::Kinematic:
				physicsScene.GetCommandStage().MoveKinematicBody(m_bodyIdentifier, joltLocation, joltRotation);
				break;
			default:
				physicsScene.GetCommandStage().SetBodyTransform(m_bodyIdentifier, joltLocation, joltRotation);
				break;
		}
	}

	EnumFlags<Body::Flags> Body::GetActiveFlags(const JPH::Body& body) const
	{
		EnumFlags<Flags> flags;
		flags |= Flags::IsSensorOnly * body.IsSensor();
		flags |= Flags::KeepAwake * (body.CanBeKinematicOrDynamic() && !body.GetAllowSleeping());
		flags |= Flags::HasOverriddenMass * (m_settings.m_overriddenMass > 0_grams);
		flags |= Flags::DisableRotation * m_settings.m_flags.IsSet(Flags::DisableRotation);
		return flags;
	}

	EnumFlags<Body::Flags> Body::GetActiveFlags(Scene& physicsScene) const
	{
		EnumFlags<Flags> flags = m_settings.m_flags;

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			flags |= GetActiveFlags(lock.GetBody());
		}
		return flags;
	}

	void Body::SetMassOverride(Scene& physicsScene, const Math::Massf overriddenMass)
	{
		m_settings.m_overriddenMass = overriddenMass;
		if (overriddenMass > 0_kilograms)
		{
			m_settings.m_flags |= Flags::HasOverriddenMass;
			RecalculateShapeMass(physicsScene);
		}
	}

	void Body::RecalculateShapeMass(JPH::Body& body)
	{
		JPH::MassProperties massProperties = body.GetShape()->GetMassProperties();

		if (m_settings.m_flags.IsSet(Flags::HasOverriddenMass))
		{
			massProperties.ScaleToMass(m_settings.m_overriddenMass.GetKilograms());
			massProperties.mInertia(3, 3) = 1.0f;
		}
		else
		{
			Math::Massf mass{0_kilograms};
			JPH::Mat44 inertia = JPH::Mat44::sZero();
			Threading::SharedLock lock(m_colliderMutex);
			for (const Optional<ColliderComponent*> pCollider : m_colliders)
			{
				if (pCollider.IsValid())
				{
					const JPH::MassProperties colliderMassProperties = pCollider->GetMassProperties();

					// Accumulate mass and inertia
					mass += Math::Massf::FromKilograms(colliderMassProperties.mMass);
					inertia += colliderMassProperties.mInertia;
				}
			}

			// Ensure that inertia is a 3x3 matrix, adding inertias causes the bottom right element to change
			inertia.SetColumn4(3, JPH::Vec4(0, 0, 0, 1));

			massProperties.mMass = mass.GetKilograms();
			massProperties.mInertia = inertia;
		}

		body.GetMotionPropertiesUnchecked()->SetMassProperties(massProperties);
	}

	void Body::RecalculateShapeMass(Scene& physicsScene)
	{
		JPH::BodyLockWrite lock = AutoLockWrite(physicsScene);
		if (LIKELY(lock.Succeeded()) && lock.GetBody().GetMotionPropertiesUnchecked() != nullptr)
		{
			RecalculateShapeMass(lock.GetBody());
		}
	}

	Optional<Math::Massf> Body::GetMass(Scene& physicsScene) const
	{
		if (m_settings.m_flags.IsSet(Flags::HasOverriddenMass))
		{
			return GetMassOverride();
		}

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock(bodyLockInterface, m_bodyIdentifier);
		if (lock.Succeeded())
		{
			const JPH::MotionProperties* pMotionProperties = lock.GetBody().GetMotionPropertiesUnchecked();
			if (pMotionProperties != nullptr)
			{
				return Math::Massf::FromKilograms(1.f / pMotionProperties->GetInverseMass());
			}
		}

		return Invalid;
	}

	void Body::SetGravityScale(Scene& physicsScene, const Math::Ratiof scale)
	{
		m_settings.m_gravityScale = scale;

		JPH::BodyLockWrite lock = AutoLockWrite(physicsScene);
		if (lock.Succeeded())
		{
			JPH::MotionProperties* pMotionProperties = lock.GetBody().GetMotionPropertiesUnchecked();
			if (pMotionProperties != nullptr)
			{
				pMotionProperties->SetGravityFactor(scale);
			}
		}
	}

	BodyLockRead Body::LockRead(Scene& physicsScene) const
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		Assert(!bodyLockInterface.IsLocked(m_bodyIdentifier));
		return BodyLockRead(bodyLockInterface, m_bodyIdentifier);
	}

	BodyLockWrite Body::LockWrite(Scene& physicsScene)
	{
		const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		Assert(!bodyLockInterface.IsLocked(m_bodyIdentifier));
		return BodyLockWrite(bodyLockInterface, m_bodyIdentifier);
	}

	BodyLockRead Body::AutoLockRead(Scene& physicsScene) const
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterfaceNoLock = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		if (bodyLockInterfaceNoLock.IsLocked(m_bodyIdentifier))
		{
			return BodyLockRead(bodyLockInterfaceNoLock, m_bodyIdentifier);
		}
		else
		{
			const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
			return BodyLockRead(bodyLockInterface, m_bodyIdentifier);
		}
	}

	BodyLockWrite Body::AutoLockWrite(Scene& physicsScene)
	{
		const JPH::BodyLockInterfaceNoLock& bodyLockInterfaceNoLock = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		if (bodyLockInterfaceNoLock.IsLocked(m_bodyIdentifier))
		{
			return BodyLockWrite(bodyLockInterfaceNoLock, m_bodyIdentifier);
		}
		else
		{
			const JPH::BodyLockInterface& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
			return BodyLockWrite(bodyLockInterface, m_bodyIdentifier);
		}
	}

	[[maybe_unused]] const bool wasBodyDataComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Body>>::Make());
	[[maybe_unused]] const bool wasBodyDataComponentTypeRegistered = Reflection::Registry::RegisterType<Body>();
	[[maybe_unused]] const bool wasLayerTypeRegistered = Reflection::Registry::RegisterType<Layer>();
	[[maybe_unused]] const bool wasBodyTypeRegistered = Reflection::Registry::RegisterType<BodyType>();
}
