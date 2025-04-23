#pragma once

#include <Engine/Entity/Data/Component3D.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Math/Primitives/WorldLine.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Storage/Identifier.h>
#include <Common/Function/Event.h>

namespace ngine::Physics::Data
{
	struct Body;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct ProjectileTarget : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "223c3cd4-ce54-4627-87f9-b6f27edb5311"_guid;

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		struct HitSettings
		{
			ClientIdentifier shooterClientIdentifier;
			Math::WorldCoordinate contactPosition;
			float damage;
			Optional<Entity::Component3D*> pComponent;
			Optional<Physics::Data::Body*> pBody;
		};

		Event<void(void*, const HitSettings& hitSettings), 24> OnHit;

		ProjectileTarget(const Deserializer& deserializer);
		ProjectileTarget(const ProjectileTarget& templateComponent, const Cloner& cloner);
		ProjectileTarget(Initializer&& initializer);

		void HitTarget(const HitSettings& hitSettings);

		void Enable()
		{
			m_isEnabled = true;
		}
		void Disable()
		{
			m_isEnabled = false;
		}
	private:
		friend struct Reflection::ReflectedType<GameFramework::ProjectileTarget>;

		Entity::Component3D& m_owner;
		bool m_isEnabled{true};
	};

	struct ProjectileTargetScoreAttachment : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "c7ac2327-3344-429c-b4e7-6858ce697719"_guid;

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		ProjectileTargetScoreAttachment(const Deserializer& deserializer);
		ProjectileTargetScoreAttachment(const ProjectileTargetScoreAttachment& templateComponent, const Cloner& cloner);
		ProjectileTargetScoreAttachment(Initializer&& initializer);

		void OnParentCreated(Entity::Component3D& parent);
	private:
		friend struct Reflection::ReflectedType<GameFramework::ProjectileTargetScoreAttachment>;
		int32 m_score{1};
	};

	struct ProjectileTargetDamageAttachment : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "77433f9b-3f2d-49be-847d-5c574a4ca51e"_guid;

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		ProjectileTargetDamageAttachment(const Deserializer& deserializer);
		ProjectileTargetDamageAttachment(const ProjectileTargetDamageAttachment& templateComponent, const Cloner& cloner);
		ProjectileTargetDamageAttachment(Initializer&& initializer);

		void OnParentCreated(Entity::Component3D& parent);
	private:
		friend struct Reflection::ReflectedType<GameFramework::ProjectileTargetDamageAttachment>;

		float m_currentDamage{0};
		float m_minimumDamage{0};
		float m_maximumDamage{100};
		float m_damageMultiplier{1};
		bool m_shouldRemoveTarget{false};
		bool m_shouldCountAsKill{false};
	};

	struct ProjectileTargetHealthAttachment : public Entity::Data::Component3D
	{
		inline static constexpr Guid TypeGuid = "6b94a270-d264-4a7a-b019-58c83b995f2c"_guid;

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		ProjectileTargetHealthAttachment(const Deserializer& deserializer);
		ProjectileTargetHealthAttachment(const ProjectileTargetHealthAttachment& templateComponent, const Cloner& cloner);
		ProjectileTargetHealthAttachment(Initializer&& initializer);

		void OnParentCreated(Entity::Component3D& parent);
	private:
		friend struct Reflection::ReflectedType<GameFramework::ProjectileTargetHealthAttachment>;

		float m_healthReductionMultiplier{1.f};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ProjectileTarget>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ProjectileTarget>(
			GameFramework::ProjectileTarget::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile Target Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};

	template<>
	struct ReflectedType<GameFramework::ProjectileTargetScoreAttachment>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ProjectileTargetScoreAttachment>(
			GameFramework::ProjectileTargetScoreAttachment::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile Target Score Attachment Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Score"),
				"scoreMultiplier",
				"{B37AC82E-E262-4A9D-AC31-2D9C76628152}"_guid,
				MAKE_UNICODE_LITERAL("Scoring"),
				Reflection::PropertyFlags{},
				&GameFramework::ProjectileTargetScoreAttachment::m_score
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::ProjectileTargetDamageAttachment>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ProjectileTargetDamageAttachment>(
			GameFramework::ProjectileTargetDamageAttachment::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile Target Damage Attachment Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Minimum Damage"),
					"minimumDamage",
					"{52293960-790A-4E1A-9D1A-2D101AE64A45}"_guid,
					MAKE_UNICODE_LITERAL("Damage"),
					Reflection::PropertyFlags{},
					&GameFramework::ProjectileTargetDamageAttachment::m_minimumDamage
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Maximum Damage"),
					"maximumDamage",
					"{2067CA0B-C5CF-4F0D-ACF2-7EEDD342354B}"_guid,
					MAKE_UNICODE_LITERAL("Damage"),
					Reflection::PropertyFlags{},
					&GameFramework::ProjectileTargetDamageAttachment::m_maximumDamage
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Damage Multiplier"),
					"damageMultiplier",
					"{D5D3049C-72E8-42C2-94E7-B14EA63A4D1E}"_guid,
					MAKE_UNICODE_LITERAL("Damage"),
					Reflection::PropertyFlags{},
					&GameFramework::ProjectileTargetDamageAttachment::m_damageMultiplier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Should Remove Target"),
					"shouldRemoveTarget",
					"{6A02CD2A-D6E7-4B76-97F0-EED48A0DA24E}"_guid,
					MAKE_UNICODE_LITERAL("Damage"),
					Reflection::PropertyFlags{},
					&GameFramework::ProjectileTargetDamageAttachment::m_shouldRemoveTarget
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Should Count As Kill"),
					"shouldCountAsKill",
					"{6B8E8215-18A0-411C-ABA3-F72096AC8987}"_guid,
					MAKE_UNICODE_LITERAL("Damage"),
					Reflection::PropertyFlags{},
					&GameFramework::ProjectileTargetDamageAttachment::m_shouldCountAsKill
				}
			}

		);
	};

	template<>
	struct ReflectedType<GameFramework::ProjectileTargetHealthAttachment>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ProjectileTargetHealthAttachment>(
			GameFramework::ProjectileTargetHealthAttachment::TypeGuid,
			MAKE_UNICODE_LITERAL("Projectile Target Health Attachment Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Health Reduction Multiplier"),
				"healthReductionMultiplier",
				"{213C6DA7-CD5B-4703-B6EB-18D29EF412B4}"_guid,
				MAKE_UNICODE_LITERAL("Health"),
				Reflection::PropertyFlags{},
				&GameFramework::ProjectileTargetHealthAttachment::m_healthReductionMultiplier
			}}
		);
	};
}
