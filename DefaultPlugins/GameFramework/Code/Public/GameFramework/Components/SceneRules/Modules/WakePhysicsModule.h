#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

namespace ngine::GameFramework
{
	struct WakePhysicsModule final : public SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		WakePhysicsModule(const WakePhysicsModule& templateComponent, const Cloner& cloner);
		WakePhysicsModule(const Deserializer& deserializer);
		WakePhysicsModule(const DynamicInitializer& initializer);
		virtual ~WakePhysicsModule();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayPaused(SceneRules&) override;
		virtual void OnGameplayResumed(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::WakePhysicsModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::WakePhysicsModule>(
			"1e7d22e0-52c4-4c64-8c37-20887eaee664"_guid, MAKE_UNICODE_LITERAL("Wake Physics Game Rules Module")
		);
	};
}
