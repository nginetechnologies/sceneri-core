#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceInterface.h>

#include <Common/Function/Event.h>

namespace ngine::GameFramework
{
	struct Ammunition final : public Entity::Data::Component
	{
		inline static constexpr Guid TypeGuid = "72576fe4-21fa-44b5-af80-635b2e9412f2"_guid;

		using BaseType = Entity::Data::Component;
		using BaseType::BaseType;

		Event<void(void*), 24> OnChanged;
		Event<void(void*), 24> OnMaxChanged;

		void operator+=(const int32 ammunition)
		{
			m_ammunition += ammunition;
			OnChanged();
		}

		void operator-=(const int32 ammunition)
		{
			m_ammunition -= ammunition;
			OnChanged();
		}

		void Add(const int32 ammunition)
		{
			m_ammunition += ammunition;
			OnChanged();
		}

		void Remove(const int32 ammunition)
		{
			m_ammunition -= ammunition;
			OnChanged();
		}

		[[nodiscard]] int32 GetAmmunition() const
		{
			return m_ammunition;
		}

		void SetMaximumAmmunition(const int32 maxAmmunition)
		{
			m_maxAmmunition = maxAmmunition;
			OnMaxChanged();
		}

		[[nodiscard]] int32 GetMaximumAmmunition() const
		{
			return m_maxAmmunition;
		}
	protected:
		int32 m_ammunition = 0;
		int32 m_maxAmmunition = 0;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Ammunition>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Ammunition>(
			GameFramework::Ammunition::TypeGuid, MAKE_UNICODE_LITERAL("Ammunition Component"), Reflection::TypeFlags::DisableWriteToDisk
		);
	};
}
