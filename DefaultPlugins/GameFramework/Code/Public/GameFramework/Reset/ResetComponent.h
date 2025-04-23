#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Common/Function/Function.h>

namespace ngine::GameFramework::Data
{
	struct Reset final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 16>;
		using ResetCallback = Function<void(Entity::Component3D&), 24>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Entity::Data::Component3D::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, ResetCallback&& callback, Entity::Component3D& parent)
				: BaseType(Forward<BaseType>(initializer))
				, m_resetCallback(Forward<ResetCallback>(callback))
				, m_parent(parent)
			{
			}

			ResetCallback m_resetCallback;
			Entity::Component3D& m_parent;
		};

		Reset(Initializer&& initializer)
			: m_resetCallback(Move(initializer.m_resetCallback))
			, m_owner(initializer.m_parent)
		{
		}

		void TriggerReset()
		{
			m_resetCallback(m_owner);
		}
	protected:
		friend struct Reflection::ReflectedType<Reset>;

		ResetCallback m_resetCallback;
		Entity::Component3D& m_owner;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::GameFramework::Data::Reset>
	{
		inline static constexpr auto Type = Reflection::Reflect<ngine::GameFramework::Data::Reset>(
			"7db45a15-4154-4fd1-aa6c-bb6992ebbeea"_guid,
			MAKE_UNICODE_LITERAL("Reset Component"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicCloning |
				TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
