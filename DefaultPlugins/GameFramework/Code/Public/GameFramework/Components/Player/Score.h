#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceInterface.h>

#include <Common/Function/Event.h>

namespace ngine::GameFramework
{
	struct ScoreModule;

	struct Score final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using BaseType::BaseType;

		inline static constexpr int32 MaximumScore = Math::NumericLimits<int32>::Max;
	protected:
		friend ScoreModule;
		int32 m_score = 0;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Score>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Score>(
			"87a8c14a-291b-4d8f-9bf7-69d2d290c2aa"_guid, MAKE_UNICODE_LITERAL("Score Component"), Reflection::TypeFlags::DisableWriteToDisk
		);
	};
}
