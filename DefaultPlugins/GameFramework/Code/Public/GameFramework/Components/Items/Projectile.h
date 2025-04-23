#pragma once

#include "FirearmProperties.h"

#include <Engine/Entity/Data/Component3D.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>

#include <Common/Asset/Guid.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Math/ForwardDeclarations/Angle3.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/Vector.h>

namespace ngine::Physics
{
	struct Contact;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct Projectile : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "b84cb147-bcf5-46b9-a43a-a29a3e303c91"_guid;

		using BaseType = Entity::Data::Component3D;

		struct Initializer : public Entity::Data::Component3D::DynamicInitializer
		{
			using BaseType = Entity::Data::Component3D::DynamicInitializer;

			Initializer(BaseType&& baseType, Entity::Component3D& shooter, Data::ProjectileProperties&& properties = Data::ProjectileProperties{})
				: BaseType(Forward<BaseType>(baseType))
				, m_shooter(shooter)
				, m_properties(Forward<Data::ProjectileProperties>(properties))
			{
			}

			Entity::Component3D& m_shooter;
			Data::ProjectileProperties m_properties;
		};

		Projectile(const Deserializer& deserializer);
		Projectile(const Projectile& templateComponent, const Cloner& cloner);
		Projectile(Initializer&& initializer);

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying(Entity::Component3D& owner);

		void Fire(const Math::Vector3f velocity, const Math::Angle3f angularVelocity, const ArrayView<const JPH::BodyID> ignoredBodies);
	protected:
		void OnBeginContactInternal(const Physics::Contact& contact);
	private:
		friend struct Reflection::ReflectedType<GameFramework::Projectile>;

		Entity::Component3D& m_owner;
		Entity::Component3D& m_shooter;

		Data::ProjectileProperties m_properties;

		Vector<JPH::BodyID> m_ignoredBodies;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Projectile>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Projectile>(
			GameFramework::Projectile::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile"),
			Reflection::TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "eebed34e-7cff-7a0b-742a-f92bef66a445"_asset, "ef85043b-303d-43ac-84da-8b920b61fb2b"_guid
			}}
		);
	};
}
