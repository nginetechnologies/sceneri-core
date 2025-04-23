#include "Components/SceneRules/Modules/WakePhysicsModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	WakePhysicsModule::WakePhysicsModule(const WakePhysicsModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	WakePhysicsModule::WakePhysicsModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	WakePhysicsModule::WakePhysicsModule(const DynamicInitializer& initializer)
		: SceneRulesModule(initializer)
	{
	}

	WakePhysicsModule::~WakePhysicsModule() = default;

	void WakePhysicsModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		if (Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			pPhysicsScene->DisableBodiesSleepByDefault();
			pPhysicsScene->WakeBodiesFromSleep();
		}
	}

	void WakePhysicsModule::OnGameplayPaused(SceneRules& sceneRules)
	{
		if (Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			pPhysicsScene->EnableBodiesSleepByDefault();
			pPhysicsScene->WakeBodiesFromSleep();
		}
	}

	void WakePhysicsModule::OnGameplayResumed(SceneRules& sceneRules)
	{
		if (Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			pPhysicsScene->DisableBodiesSleepByDefault();
			pPhysicsScene->WakeBodiesFromSleep();
		}
	}

	void WakePhysicsModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		if (Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			pPhysicsScene->EnableBodiesSleepByDefault();
			// Put all physics bodies to sleep again
			pPhysicsScene->PutAllBodiesToSleep();
		}
	}

	[[maybe_unused]] const bool wasWakePhysicsRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<WakePhysicsModule>>::Make());
	[[maybe_unused]] const bool wasWakePhysicsTypeRegistered = Reflection::Registry::RegisterType<WakePhysicsModule>();
}
