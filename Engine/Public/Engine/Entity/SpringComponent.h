#pragma once

#include "Component3D.h"

namespace ngine::Entity
{
	struct SpringComponent : public Component3D
	{
	public:
		static constexpr Guid TypeGuid = "7c8b5c35-8225-4077-85c6-a4857a48b353"_guid;

		using BaseType = Component3D;

		SpringComponent(const SpringComponent& templateComponent, const Cloner& cloner);
		SpringComponent(const Deserializer& deserializer);
		SpringComponent(Initializer&& initializer);
		virtual ~SpringComponent();

		void Update();
	protected:
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;
	private:
		bool m_ignoreOwnerRotation = true;
		Math::Quaternionf m_relativeRotation = Math::Identity;
		Math::Vector3f m_relativeOffset;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::SpringComponent>
	{
		inline static constexpr auto Type =
			Reflection::Reflect<Entity::SpringComponent>(Entity::SpringComponent::TypeGuid, MAKE_UNICODE_LITERAL("Spring"));
	};
}
