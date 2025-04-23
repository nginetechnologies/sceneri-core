#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Common/Math/Transform.h>

namespace ngine::GameFramework::Data
{
	struct CachedWorldTransform final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Entity::Data::Component3D::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Math::WorldTransform transform)
				: BaseType(Forward<BaseType>(initializer))
				, m_transform(transform)
			{
			}

			Math::WorldTransform m_transform;
		};

		CachedWorldTransform(CachedWorldTransform::Initializer&& initializer)
			: m_transform(Move(initializer.m_transform))
		{
		}

		[[nodiscard]] Math::WorldTransform GetTransform() const
		{
			return m_transform;
		}
	protected:
		friend struct Reflection::ReflectedType<CachedWorldTransform>;

		Math::WorldTransform m_transform;
	};

	//! Temp data component to keep track of the owning component while  we're migrating away from object based components
	struct CachedOwner final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Entity::Data::Component3D::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, Entity::Component3D& owner)
				: BaseType(Forward<BaseType>(initializer))
				, m_owner(owner)
			{
			}

			Entity::Component3D& m_owner;
		};

		CachedOwner(CachedOwner::Initializer&& initializer)
			: m_owner(initializer.m_owner)
		{
		}

		[[nodiscard]] Entity::Component3D& Get() const
		{
			return m_owner;
		}
	protected:
		friend struct Reflection::ReflectedType<CachedOwner>;

		Entity::Component3D& m_owner;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::GameFramework::Data::CachedWorldTransform>
	{
		inline static constexpr auto Type = Reflection::Reflect<ngine::GameFramework::Data::CachedWorldTransform>(
			"1db45a15-4154-4fd1-aa6c-bb6992ebbeea"_guid,
			MAKE_UNICODE_LITERAL("Cached World Transform Component"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicCloning |
				TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};

	template<>
	struct ReflectedType<ngine::GameFramework::Data::CachedOwner>
	{
		inline static constexpr auto Type = Reflection::Reflect<ngine::GameFramework::Data::CachedOwner>(
			"3C2D3382-EDFE-4A67-A1E3-0078E956A427"_guid,
			MAKE_UNICODE_LITERAL("Cached Owner Component"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicCloning |
				TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
