#include "PhysicsCore/Components/CharacterBase.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"
#include "PhysicsCore/Layer.h"
#include "PhysicsCore/Material.h"
#include "PhysicsCore/MaterialAsset.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	CharacterBase::CharacterBase(const CharacterBase& templateComponent, const Cloner& cloner)
		: BodyComponent(templateComponent, cloner)
	{
	}

	CharacterBase::CharacterBase(const Deserializer& deserializer, Settings&& defaultSettings)
		: BodyComponent(deserializer, Forward<Settings>(defaultSettings))
	{
	}

	CharacterBase::CharacterBase(Initializer&& initializer)
		: BodyComponent(Forward<Initializer>(initializer))
	{
	}

	CharacterBase::~CharacterBase()
	{
	}

	Math::WorldCoordinate CharacterBase::GetFootLocation() const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const JPH::MutableCompoundShape& shape = static_cast<const JPH::MutableCompoundShape&>(*body.GetShape());
			return GetWorldLocation() - Math::WorldCoordinate{0, 0, (Math::WorldCoordinateUnitType)shape.GetLocalBounds().GetExtent().GetZ()};
		}
		return Math::Zero;
	}

	Optional<Entity::Component3D*> CharacterBase::GetGroundComponent() const
	{
		return nullptr;
	}

	Math::Vector3f CharacterBase::GetLinearVelocity()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();
			return Math::Vector3f(body.GetLinearVelocity());
		}

		return Math::Vector3f(Math::Zero);
	}

	void CharacterBase::Jump(const Math::Vector3f acceleration)
	{
		AddImpulse(acceleration);
	}

	[[maybe_unused]] const bool wasCharacterBaseRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CharacterBase>>::Make());
	[[maybe_unused]] const bool wasCharacterBaseTypeRegistered = Reflection::Registry::RegisterType<CharacterBase>();
}
