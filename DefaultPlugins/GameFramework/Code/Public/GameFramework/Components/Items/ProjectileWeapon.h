#pragma once

#include "FirearmProperties.h"

#include <Engine/Entity/Data/Component3D.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Math/ForwardDeclarations/Transform.h>
#include <Common/Math/Radius.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Storage/Identifier.h>
#include <Common/Asset/Picker.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct ProjectileWeapon : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "d6bc2c71-b67b-4122-a448-183d2eca4138"_guid;

		using BaseType = Entity::Data::Component3D;

		struct Initializer : public Entity::Data::Component3D::DynamicInitializer
		{
			using BaseType = Entity::Data::Component3D::DynamicInitializer;

			Initializer(
				BaseType&& baseType,
				Data::FirearmProperties&& firearmProperties = Data::FirearmProperties{},
				Data::ProjectileProperties&& projectileProperties = Data::ProjectileProperties{}
			)
				: BaseType(Forward<BaseType>(baseType))
				, m_firearmProperties(Forward<Data::FirearmProperties>(firearmProperties))
				, m_projectileProperties(Forward<Data::ProjectileProperties>(projectileProperties))
			{
			}

			Data::FirearmProperties m_firearmProperties;
			Data::ProjectileProperties m_projectileProperties;
		};

		ProjectileWeapon(const Deserializer& deserializer);
		ProjectileWeapon(const ProjectileWeapon& templateComponent, const Cloner& cloner);
		ProjectileWeapon(Initializer&& initializer);

		enum class FireResult
		{
			OutOfAmmoSingle,
			OutOfAmmoBurst,
			NotReadyBurst,
			NotReadySingle,
			Fired
		};

		FireResult Fire(
			Entity::Component3D& owner, Entity::Component3D& shooter, const Math::WorldLine line, const ArrayView<const JPH::BodyID> ignoredBodies
		);
		[[nodiscard]] bool CanFire() const;
		void StartReload();
		void FinishReload(Entity::Component3D& owner, Entity::Component3D& shooter);

		enum class Sound : uint8
		{
			Fire,
			EmptyFire,
			Casing,
			Reload,
			Count
		};

		enum class Flags : uint8
		{
			IsTriggerHeld = 1 << 0,
			IsFiring = 1 << 1,
			IsReloading = 1 << 2,
			ShouldAutoReload = 1 << 3
		};

		void SetSoundAsset(
			Entity::Component3D& owner,
			Entity::SceneRegistry& sceneRegistry,
			const Sound sound,
			const Asset::Guid assetGuid,
			const Math::Radiusf soundRadius
		);
		[[nodiscard]] Optional<Audio::SoundSpotComponent*> GetSoundSoundComponent(const Sound sound) const
		{
			return m_soundSpotComponents[sound];
		}

		void SetFirearmProperties(Data::FirearmProperties&& properties)
		{
			m_firearmProperties = Forward<Data::FirearmProperties>(properties);
		}
		[[nodiscard]] const Data::FirearmProperties& GetFirearmProperties() const
		{
			return m_firearmProperties;
		}
		void SetProjectileProperties(Data::ProjectileProperties&& properties)
		{
			m_projectileProperties = Forward<Data::ProjectileProperties>(properties);
		}
		[[nodiscard]] const Data::ProjectileProperties& GetProjectileProperties() const
		{
			return m_projectileProperties;
		}

		void HoldTrigger();
		void ReleaseTrigger();

		[[nodiscard]] bool IsTriggerHeld() const
		{
			return m_flags.IsSet(Flags::IsTriggerHeld);
		}
		[[nodiscard]] bool IsReloading() const
		{
			return m_flags.IsSet(Flags::IsReloading);
		}
		[[nodiscard]] bool ShouldAutoReload() const
		{
			return m_flags.IsSet(Flags::ShouldAutoReload);
		}
		[[nodiscard]] int32 GetAmmunitionCount() const
		{
			return m_ammunitionCount;
		}
	protected:
		[[nodiscard]] Optional<Entity::Component3D*>
		CreateProjectile(Entity::Component3D& owner, const Math::WorldTransform worldTransform) const;

		[[nodiscard]] bool CanFireInternal() const;
	private:
		friend struct Reflection::ReflectedType<GameFramework::ProjectileWeapon>;

		Entity::Component3D& m_owner;

		Array<Optional<Audio::SoundSpotComponent*>, (uint8)Sound::Count, Sound> m_soundSpotComponents;

		Data::FirearmProperties m_firearmProperties;
		Data::ProjectileProperties m_projectileProperties;
		int32 m_burstCount{0};
		Time::Timestamp m_lastShotTime;
		int32 m_ammunitionCount{0};

		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(ProjectileWeapon::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ProjectileWeapon>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ProjectileWeapon>(
			GameFramework::ProjectileWeapon::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile Target Component"),
			Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
