#include "Components/SceneRules/Modules/ResetSceneModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	ResetSceneModule::ResetSceneModule(const ResetSceneModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
		, m_resetSystem(cloner.GetParent().GetRootScene())
	{
	}

	ResetSceneModule::ResetSceneModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
		, m_resetSystem(deserializer.GetParent().GetRootScene())
	{
	}

	ResetSceneModule::ResetSceneModule(const DynamicInitializer& initializer)
		: SceneRulesModule(initializer)
		, m_resetSystem(initializer.GetParent().GetRootScene())
	{
	}

	ResetSceneModule::~ResetSceneModule() = default;

	void ResetSceneModule::OnGameplayStarted(SceneRules&)
	{
		m_resetSystem.Capture();
		m_resetSystem.ResumeSimulation();
	}

	void ResetSceneModule::OnGameplayPaused(SceneRules&)
	{
		m_resetSystem.PauseSimulation();
	}

	void ResetSceneModule::OnGameplayResumed(SceneRules&)
	{
		m_resetSystem.ResumeSimulation();
	}

	void ResetSceneModule::OnGameplayStopped(SceneRules&)
	{
		m_resetSystem.PauseSimulation();
		m_resetSystem.Reset();
	}

	[[maybe_unused]] const bool wasResetSceneRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ResetSceneModule>>::Make());
	[[maybe_unused]] const bool wasResetSceneTypeRegistered = Reflection::Registry::RegisterType<ResetSceneModule>();
}
