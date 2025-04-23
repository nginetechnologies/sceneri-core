#pragma once

#include <Engine/Entity/Data/Component3D.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Storage/Identifier.h>
#include <Common/Math/Frequency.h>
#include <Common/Math/Speed.h>
#include <Common/Math/Radius.h>
#include <Common/Time/Duration.h>
#include <Common/Asset/Picker.h>

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct Turret : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "5088a9a4-5adc-469c-91d8-3f57f36cceea"_guid;

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		Turret(const Deserializer& deserializer);
		Turret(const Turret& templateComponent, const Cloner& cloner);
		Turret(Initializer&& initializer);

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying(Entity::Component3D& owner);
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void Update();

		[[nodiscard]] int32 GetBurstSize(Entity::Component3D& owner) const;
		void SetBurstSize(Entity::Component3D& owner, const int32 value);
		[[nodiscard]] Math::Frequencyf GetFireRate(Entity::Component3D& owner) const;
		void SetFireRate(Entity::Component3D& owner, const Math::Frequencyf value);
		[[nodiscard]] Asset::Picker GetFireSoundAsset(Entity::Component3D& owner) const;
		void SetFireSoundAsset(Entity::Component3D& owner, const Asset::Picker value);
		[[nodiscard]] int32 GetMagazineCapacity(Entity::Component3D& owner) const;
		void SetMagazineCapacity(Entity::Component3D& owner, const int32 value);
		[[nodiscard]] Time::Durationf GetReloadTime(Entity::Component3D& owner) const;
		void SetReloadTime(Entity::Component3D& owner, const Time::Durationf value);
		[[nodiscard]] Math::Speedf GetProjectileSpeed(Entity::Component3D& owner) const;
		void SetProjectileSpeed(Entity::Component3D& owner, const Math::Speedf value);
		[[nodiscard]] float GetProjectileDamage(Entity::Component3D& owner) const;
		void SetProjectileDamage(Entity::Component3D& owner, const float value);
		[[nodiscard]] Math::Radiusf GetProjectileRadius(Entity::Component3D& owner) const;
		void SetProjectileRadius(Entity::Component3D& owner, const Math::Radiusf value);
		[[nodiscard]] Asset::Picker GetProjectileMeshAsset(Entity::Component3D& owner) const;
		void SetProjectileMeshAsset(Entity::Component3D& owner, const Asset::Picker value);
		[[nodiscard]] Asset::Picker GetProjectileMaterial(Entity::Component3D& owner) const;
		void SetProjectileMaterial(Entity::Component3D& owner, const Asset::Picker value);
		[[nodiscard]] Asset::Picker GetProjectileImpactSoundAsset(Entity::Component3D& owner) const;
		void SetProjectileImpactSoundAsset(Entity::Component3D& owner, const Asset::Picker value);
		[[nodiscard]] Math::Radiusf GetProjectileImpactSoundRadius(Entity::Component3D& owner) const;
		void SetProjectileImpactSoundRadius(Entity::Component3D& owner, const Math::Radiusf value);
		[[nodiscard]] Asset::Picker GetProjectileImpactDecalAsset(Entity::Component3D& owner) const;
		void SetProjectileImpactDecalAsset(Entity::Component3D& owner, const Asset::Picker value);
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);
	private:
		friend struct Reflection::ReflectedType<GameFramework::Turret>;

		Entity::Component3D& m_owner;

		Time::Durationf m_reloadTime{0_seconds};
		Time::Durationf m_remainingReloadTime{0_seconds};

		Asset::Guid m_fireSoundAssetGuid;
		Math::Radiusf m_fireSoundRadius{10_meters};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Turret>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Turret>(
			GameFramework::Turret::TypeGuid,
			MAKE_UNICODE_LITERAL("Turret"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Burst Size"),
					"burstSize",
					"3afdc098-df77-4f87-956d-116c58363979"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetBurstSize,
					&GameFramework::Turret::GetBurstSize
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Fire Rate"),
					"fireRate",
					"18a2d64f-4fb3-4fca-8896-fdca6ea506d8"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetFireRate,
					&GameFramework::Turret::GetFireRate
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Magazine Capacity"),
					"magazineCapacity",
					"b707db24-483e-4c28-b721-e93366d8aa14"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetMagazineCapacity,
					&GameFramework::Turret::GetMagazineCapacity
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Reload Time"),
					"reloadTime",
					"8b699a08-27da-4a7c-9908-68575cba602b"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetReloadTime,
					&GameFramework::Turret::GetReloadTime
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Fire Sound"),
					"fireSound",
					"106b8db2-1d43-4406-a61a-70dc4fa47358"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetFireSoundAsset,
					&GameFramework::Turret::GetFireSoundAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Speed"),
					"projectileSpeed",
					"efd3ca0f-539e-43e2-a665-639c8f623208"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileSpeed,
					&GameFramework::Turret::GetProjectileSpeed
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Damage"),
					"projectileDamage",
					"3bc2911e-5173-4da5-b554-9e48c6464f8b"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileDamage,
					&GameFramework::Turret::GetProjectileDamage
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Radius"),
					"projectileRadius",
					"37874690-beb4-405a-a9a0-134e837a4c98"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileRadius,
					&GameFramework::Turret::GetProjectileRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Mesh"),
					"projectileMesh",
					"98097a93-c641-43c1-853d-a181598150ad"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileMeshAsset,
					&GameFramework::Turret::GetProjectileMeshAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Material"),
					"projectileMaterial",
					"39dd75d8-523d-47cb-9b89-c9cdc3e0f04a"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileMaterial,
					&GameFramework::Turret::GetProjectileMaterial
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Impact Sound"),
					"projectileSoundAsset",
					"91dae194-0cf5-45f4-98e8-5608e7a545e0"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileImpactSoundAsset,
					&GameFramework::Turret::GetProjectileImpactSoundAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Impact Radius"),
					"projectileSoundRadius",
					"184baa20-39c6-49ae-899d-3ca7cfc7b9e7"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileImpactSoundRadius,
					&GameFramework::Turret::GetProjectileImpactSoundRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Projectile Impact Decal"),
					"projectileImpactDecal",
					"d878ed28-62f7-4b79-ae47-1d119ffbfba4"_guid,
					MAKE_UNICODE_LITERAL("Turret"),
					PropertyFlags{},
					&GameFramework::Turret::SetProjectileImpactDecalAsset,
					&GameFramework::Turret::GetProjectileImpactDecalAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "eebed34e-7cff-7a0b-742a-f92bef66a445"_asset, "ef85043b-303d-43ac-84da-8b920b61fb2b"_guid
			}}
		);
	};
}
