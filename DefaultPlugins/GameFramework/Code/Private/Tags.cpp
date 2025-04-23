#include "Tags.h"

#include <Common/Reflection/Type.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::GameplayTag>
	{
		static constexpr auto Type =
			Reflection::Reflect<GameFramework::GameplayTag>(GameFramework::GameplayTag::TypeGuid, MAKE_UNICODE_LITERAL("Gameplay"));
	};

	template<>
	struct ReflectedType<GameFramework::CharacterTag>
	{
		static constexpr auto Type =
			Reflection::Reflect<GameFramework::CharacterTag>(GameFramework::CharacterTag::TypeGuid, MAKE_UNICODE_LITERAL("Character"));
	};
}

namespace ngine::GameFramework
{
	[[maybe_unused]] const bool wasGameplayTagRegistered = Reflection::Registry::RegisterType<GameFramework::GameplayTag>();
	[[maybe_unused]] const bool wasCharacterTagRegistered = Reflection::Registry::RegisterType<GameFramework::CharacterTag>();
}
