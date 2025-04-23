#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>
#include <GameFramework/Reset/ResetSystem.h>

namespace ngine::GameFramework
{
	struct ResetSceneModule final : public SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		ResetSceneModule(const ResetSceneModule& templateComponent, const Cloner& cloner);
		ResetSceneModule(const Deserializer& deserializer);
		ResetSceneModule(const DynamicInitializer& initializer);
		virtual ~ResetSceneModule();

		virtual void OnGameplayStarted(SceneRules&) override;
		virtual void OnGameplayPaused(SceneRules&) override;
		virtual void OnGameplayResumed(SceneRules&) override;
		virtual void OnGameplayStopped(SceneRules&) override;

		[[nodiscard]] ResetSystem& GetResetSystem()
		{
			return m_resetSystem;
		}
	protected:
		ResetSystem m_resetSystem;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ResetSceneModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ResetSceneModule>(
			"b17f5afc-5476-4345-82ae-56ea87607d17"_guid, MAKE_UNICODE_LITERAL("Reset Game Rules Module")
		);
	};
}
