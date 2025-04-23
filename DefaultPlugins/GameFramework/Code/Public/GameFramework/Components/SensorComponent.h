#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentMask.h>
#include <Engine/Tag/TagMask.h>
#include <Engine/Tag/TagMaskProperty.h>

#include <Common/Function/Event.h>

namespace ngine::Physics
{
	struct Contact;
}

namespace ngine::GameFramework
{
	struct SensorComponent : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "183d677a-b427-4b1d-a276-97b3e6681304"_guid;

		using BaseType = Entity::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		struct Initializer : public Entity::Component3D::Initializer
		{
			using BaseType = Entity::Component3D::Initializer;
			Initializer(BaseType&& initializer, const Tag::Mask tagMask = Tag::Mask())
				: BaseType(Forward<BaseType>(initializer))
				, m_tagMask(tagMask)
			{
			}

			Tag::Mask m_tagMask;
		};

		SensorComponent(Initializer&& initializer);
		SensorComponent(const SensorComponent& templateComponent, const Cloner& cloner);
		SensorComponent(const Deserializer& deserializer);

		void OnCreated();

		Event<void(void*, SensorComponent&, Optional<Entity::Component3D*>), 24> OnComponentEntered;
		Event<void(void*, SensorComponent&, Optional<Entity::Component3D*>), 24> OnComponentDetected;
		Event<void(void*, SensorComponent&, Optional<Entity::Component3D*>), 24> OnComponentLost;
	private:
		void OnBeginContactInternal(const Physics::Contact& contact);
		void OnLoseContactInternal(const Physics::Contact& contact);
	protected:
		friend struct Reflection::ReflectedType<SensorComponent>;

		void SetTags(const Tag::QueryableMaskProperty tags);
		[[nodiscard]] Tag::QueryableMaskProperty GetTags() const;
	protected:
		Tag::Mask m_tagMask;
		Entity::ComponentMask m_colliding;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SensorComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SensorComponent>(
			GameFramework::SensorComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Sensor Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Detected Tags"),
				"mask",
				"{BD1435E7-D269-45B5-86B9-03AE4E7FE7F8}"_guid,
				MAKE_UNICODE_LITERAL("Detected Tags"),
				Reflection::PropertyFlags::VisibleToParentScope,
				&GameFramework::SensorComponent::SetTags,
				&GameFramework::SensorComponent::GetTags
			)}
		);
	};
}
