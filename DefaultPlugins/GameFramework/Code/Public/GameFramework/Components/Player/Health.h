#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Common/Math/Ratio.h>

namespace ngine::GameFramework
{
	struct HealthModule;

	struct Health final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using BaseType::BaseType;
		using ValueType = float;

		using Initializer = BaseType::DynamicInitializer;

		Health(const Deserializer& deserializer);
		Health(const Health& templateComponent, const Cloner& cloner);
		Health(Initializer&& initializer);

		void OnCreated();

		[[nodiscard]] ValueType Get() const
		{
			return m_health;
		}
		[[nodiscard]] Math::Ratiof GetRatio() const
		{
			Assert(m_maximum != 0);
			const ValueType minimum = 0;
			return float(m_health - minimum) / float(m_maximum - minimum);
		}
	protected:
		friend HealthModule;
		friend struct Reflection::ReflectedType<GameFramework::Health>;
		ValueType m_health = 0;
		ValueType m_maximum = 3;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Health>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Health>(
			"ee97e36e-3e03-45ba-9bd2-8734e32dcb8e"_guid,
			MAKE_UNICODE_LITERAL("Health Component"),
			Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Maximum"),
				"maximum",
				"{568AA14D-74E5-4502-A849-CACD391EEB24}"_guid,
				MAKE_UNICODE_LITERAL("Health"),
				&GameFramework::Health::m_maximum
			)}
		);
	};
}
