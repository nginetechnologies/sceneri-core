#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/TransformChangeFlags.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <PhysicsCore/Layer.h>
#include <PhysicsCore/Components/Data/BodyFlags.h>
#include <PhysicsCore/Components/Data/BodyType.h>
#include <PhysicsCore/Components/Data/BodySettings.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>
#include <PhysicsCore/Components/ColliderIdentifier.h>

#include <Common/Reflection/CoreTypes.h>
#include <Common/Math/Mass.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Math/ForwardDeclarations/Transform.h>
#include <Common/Math/ForwardDeclarations/Angle3.h>
#include <Common/Math/ForwardDeclarations/Quaternion.h>
#include <Common/Math/Angle.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Reflection/EnumTypeExtension.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/NativeEvent.h>

namespace JPH
{
	class Body;
	class BodyLockRead;
	class BodyLockWrite;
	class BodyCreationSettings;
}

namespace ngine::Entity::Data
{
	struct WorldTransform;
	struct LocalTransform3D;
	struct Flags;
}

namespace ngine::Physics
{
	struct Contact;
	struct ColliderComponent;
}

namespace ngine::Physics::Data
{
	struct Scene;
	struct PhysicsCommandStage;

	using BodyLockRead = JPH::BodyLockRead;
	using BodyLockWrite = JPH::BodyLockWrite;

	struct Body final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		using Type = BodyType;
		using Flags = BodyFlags;
		using Settings = BodySettings;

		struct Initializer : public Component3D::DynamicInitializer
		{
			using BaseType = Component3D::DynamicInitializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, Settings&& settings = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_settings{Forward<Settings>(settings)}
			{
			}

			BodySettings m_settings;
		};

		Body(Initializer&& initializer);
		Body(const Deserializer& deserializer);
		Body(const Body& templateComponent, const Cloner&);
		virtual ~Body();

		[[nodiscard]] static JPH::BodyCreationSettings
		GetJoltBodyCreationSettings(const Math::WorldTransform transform, const BodySettings& bodySettings);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::Component3D& parent);

		[[nodiscard]] JPH::BodyID GetIdentifier() const
		{
			return m_bodyIdentifier;
		}

		[[nodiscard]] Entity::Component3D& GetOwner()
		{
			return m_owner;
		}

		void OnDestroying(Entity::Component3D& owner);
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);

		void Wake(Scene& physicsScene);
		void Sleep(Scene& physicsScene);
		[[nodiscard]] bool IsAwake(Scene& physicsScene) const;
		[[nodiscard]] bool IsSleeping(Scene& physicsScene) const
		{
			return !IsAwake(physicsScene);
		}

		//! Checks whether the physics body was created and remains valid
		[[nodiscard]] bool WasCreated(Scene& physicsScene) const;

		[[nodiscard]] Type GetActiveType(Scene& physicsScene) const;
		[[nodiscard]] Type GetType() const
		{
			return m_settings.m_type;
		}
		bool SetType(Scene& physicsScene, const Type type);

		void SetLayer(Scene& physicsScene, const Layer layer);
		[[nodiscard]] Layer GetActiveLayer(Scene& physicsScene) const;
		[[nodiscard]] Layer GetLayer() const
		{
			return m_settings.m_layer;
		}

		[[nodiscard]] Math::Massf GetOverriddenMass() const
		{
			return m_settings.m_overriddenMass;
		}

		void AddForce(Scene& physicsScene, const Math::Vector3f force);
		void AddForceAtLocation(Scene& physicsScene, const Math::Vector3f force, const Math::WorldCoordinate location);
		void AddImpulse(Scene& physicsScene, const Math::Vector3f impulse);
		void AddImpulseAtLocation(Scene& physicsScene, const Math::Vector3f impulse, const Math::WorldCoordinate location);
		void AddTorque(Scene& physicsScene, const Math::Vector3f torque);
		void AddAngularImpulse(Scene& physicsScene, const Math::Vector3f acceleration);
		void SetVelocity(Scene& physicsScene, const Math::Vector3f velocity);
		[[nodiscard]] Math::Vector3f GetVelocity(Scene& physicsScene) const;
		[[nodiscard]] Math::Vector3f GetVelocityAtPoint(Scene& physicsScene, const Math::WorldCoordinate coordinate) const;

		void SetAngularVelocity(Scene& physicsScene, const Math::Angle3f angularVelocity);
		[[nodiscard]] Math::Angle3f GetAngularVelocity(Scene& physicsScene) const;
		void SetMaximumAngularVelocity(Scene& physicsScene, const Math::Anglef angularVelocity);
		[[nodiscard]] Math::Anglef GetMaximumAngularVelocity() const
		{
			return m_settings.m_maximumAngularVelocity;
		}

		[[nodiscard]] Optional<Math::WorldCoordinate> GetWorldLocation(Scene& physicsScene) const;
		[[nodiscard]] Optional<Math::WorldRotation> GetWorldRotation(Scene& physicsScene) const;
		[[nodiscard]] Optional<Math::WorldTransform> GetWorldTransform(Scene& physicsScene) const;
		[[nodiscard]] Optional<Math::WorldTransform> GetCenterOfMassWorldTransform(Scene& physicsScene) const;
		void SetWorldLocation(Scene& physicsScene, const Math::WorldCoordinate location);
		void SetWorldRotation(Scene& physicsScene, const Math::WorldRotation rotation);
		void SetWorldTransform(Scene& physicsScene, const Math::WorldTransform transform);

		[[nodiscard]] EnumFlags<Flags> GetActiveFlags(const JPH::Body& body) const;
		[[nodiscard]] EnumFlags<Flags> GetActiveFlags(Scene& physicsScene) const;
		[[nodiscard]] EnumFlags<Flags> GetFlags() const
		{
			return m_settings.m_flags;
		}

		void SetMassOverride(Scene& physicsScene, const Math::Massf overriddenMass);
		[[nodiscard]] Optional<Math::Massf> GetMassOverride() const
		{
			if (m_settings.m_flags.IsSet(Flags::HasOverriddenMass))
			{
				return m_settings.m_overriddenMass;
			}
			return Invalid;
		}

		void SetGravityScale(Scene& physicsScene, const Math::Ratiof scale);
		[[nodiscard]] Math::Ratiof GetGravityScale() const
		{
			return m_settings.m_gravityScale;
		}

		[[nodiscard]] Optional<Math::Massf> GetMass(Scene& physicsScene) const;

		void DisableRotation()
		{
			m_settings.m_flags |= Flags::DisableRotation;
		}

		[[nodiscard]] Optional<ColliderComponent*> GetCollider(const ColliderIdentifier identifier) const
		{
			Threading::SharedLock lock(m_colliderMutex);
			if (identifier.GetFirstValidIndex() >= m_colliders.GetSize())
			{
				return Invalid;
			}
			return m_colliders[identifier.GetFirstValidIndex()];
		}
		template<typename Callback>
		void VisitColliders(Callback&& callback) const
		{
			Threading::SharedLock lock(m_colliderMutex);
			for (const Optional<ColliderComponent*> pCollider : m_colliders)
			{
				if (pCollider.IsValid())
				{
					callback(*pCollider);
				}
			}
		}
		[[nodiscard]] JPH::BodyID GetBodyIdentifier() const
		{
			return m_bodyIdentifier;
		}

		[[nodiscard]] BodyLockRead LockRead(Scene& physicsScene) const;
		[[nodiscard]] BodyLockWrite LockWrite(Scene& physicsScene);

		[[nodiscard]] BodyLockRead AutoLockRead(Scene& physicsScene) const;
		[[nodiscard]] BodyLockWrite AutoLockWrite(Scene& physicsScene);

		Scripting::VM::NativeEvent<void(const Contact& contact)> OnContactFound;
		Scripting::VM::NativeEvent<void(const Contact& contact)> OnContactLost;
	protected:
		void SetTypeInternal(Scene& physicsScene, const Type type);
		void OnOwnerWorldTransformChanged(Entity::Component3D& owner, const EnumFlags<Entity::TransformChangeFlags> flags);
		void SetWorldLocationAndRotationFromPhysics(
			Entity::Component3D& owner,
			const Math::WorldCoordinate location,
			const Math::Quaternionf rotation,
			Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData,
			Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData,
			Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData
		);
		void RegisterDynamicBody(Scene& physicsScene);
		void UnRegisterDynamicBody(Scene& physicsScene);

		void RecalculateShapeMass(Scene& physicsScene);
		void RecalculateShapeMass(JPH::Body& body);

		[[nodiscard]] bool AddCollider(ColliderComponent& collider);

		void ChangeTypeFromProperty(Entity::Component3D& owner, const Type type);
		void SetTypeFromProperty(Entity::Component3D& owner, const Type type);
		[[nodiscard]] Type GetTypeFromProperty(Entity::Component3D& owner);

		void SetLayerFromProperty(Entity::Component3D& owner, const Layer layer);
		[[nodiscard]] Layer GetLayerFromProperty(Entity::Component3D& owner);

		void SetFlagsFromProperty(Entity::Component3D& owner, const EnumFlags<Flags> flags);
		[[nodiscard]] EnumFlags<Flags> GetFlagsFromProperty(Entity::Component3D& owner);

		void SetMaximumAngularVelocityFromProperty(Entity::Component3D& owner, const Math::Anglef velocity);
		[[nodiscard]] Math::Anglef GetMaximumAngularVelocityFromProperty(Entity::Component3D& owner);

		void SetOverriddenMassFromProperty(Entity::Component3D& owner, const Math::Massf mass);
		[[nodiscard]] Math::Massf GetOverriddenMassFromProperty(Entity::Component3D& owner);

		void SetGravityScaleFromProperty(Entity::Component3D& owner, const Math::Ratiof scale);
		[[nodiscard]] Math::Ratiof GetGravityScaleFromProperty(Entity::Component3D& owner);
	protected:
		friend Scene;
		friend PhysicsCommandStage;
		friend ColliderComponent;
		friend struct Reflection::ReflectedType<Physics::Data::Body>;

		Entity::Component3D& m_owner; // TODO: Remove when we don't need it in the reset system anymore
		JPH::BodyID m_bodyIdentifier;

		BodySettings m_settings;

		TSaltedIdentifierStorage<ColliderIdentifier> m_colliderIdentifiers;
		mutable Threading::SharedMutex m_colliderMutex;
		InlineVector<Optional<ColliderComponent*>, 1> m_colliders;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Data::Body>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Data::Body>(
			"{653EE503-F473-47D2-BE45-08A577D17B86}"_guid,
			MAKE_UNICODE_LITERAL("Physics Body"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Type"),
					"typeUI",
					"{E6A91CCD-4526-462C-95CF-20009FE721B7}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::Transient,
					&Physics::Data::Body::ChangeTypeFromProperty,
					&Physics::Data::Body::GetTypeFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Type"),
					"type",
					"{E0EC9EB5-8763-4CF8-8647-2595E0AF8B2F}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetTypeFromProperty,
					&Physics::Data::Body::GetTypeFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Layer"),
					"layer",
					"{0CC1CE00-644C-4801-B4BB-E89C0A2A2C67}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetLayerFromProperty,
					&Physics::Data::Body::GetLayerFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Flags"),
					"body_flags",
					"{B8F601BD-7D32-4980-A1ED-8576A5FD2B9E}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetFlagsFromProperty,
					&Physics::Data::Body::GetFlagsFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Maximum Angular Velocity"),
					"maximumAngularVelocity",
					"{4DBA3978-3023-4093-B72D-1C7FC7BB21B2}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetMaximumAngularVelocityFromProperty,
					&Physics::Data::Body::GetMaximumAngularVelocityFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mass Override"),
					"overriddenMass",
					"{D9113A63-F548-4231-883D-C60E4A175B37}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetOverriddenMassFromProperty,
					&Physics::Data::Body::GetOverriddenMassFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Gravity Scale"),
					"gravityScale",
					"{D9113A63-F548-4231-883D-C60E4A175B37}"_guid,
					MAKE_UNICODE_LITERAL("Physics"),
					Reflection::PropertyFlags::HideFromUI,
					&Physics::Data::Body::SetGravityScaleFromProperty,
					&Physics::Data::Body::GetGravityScaleFromProperty
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "2ef618e0-db7a-9ab7-5e4f-c5c01df6efc9"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_asset
			}}
		);
	};
}
