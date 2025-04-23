#pragma once

#include <Engine/Entity/Component3D.h>

namespace ngine
{
	namespace Physics
	{
		struct Vehicle;
	}

	namespace Font
	{
		struct TextComponent;
	}
}

namespace ngine::GameFramework::TimeAttack
{
	struct HUD final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "12ca6356-60c0-4b32-a075-1797d89cc32b"_guid;
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using Component3D::Component3D;
		using Component3D::Serialize;

		HUD(const HUD& templateComponent, const Cloner& cloner);
		virtual ~HUD();

		void OnDeserialized(const Serialization::Reader reader, const Threading::JobBatch&);
		void SetTarget(Physics::Vehicle& vehicle)
		{
			m_vehicle = vehicle;
		}

		void AfterPhysicsUpdate();
	protected:
		void CreateTextComponent();
	protected:
		friend struct Reflection::ReflectedType<GameFramework::TimeAttack::HUD>;

		Optional<ReferenceWrapper<Physics::Vehicle>> m_vehicle = Invalid;
		Optional<ReferenceWrapper<Font::TextComponent>> m_velocityText = Invalid;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::TimeAttack::HUD>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::TimeAttack::HUD>(
			GameFramework::TimeAttack::HUD::TypeGuid,
			MAKE_UNICODE_LITERAL("HUD"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
