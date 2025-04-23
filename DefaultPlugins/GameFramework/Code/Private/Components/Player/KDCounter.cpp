#include <GameFramework/Components/Player/KDCounter.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/SceneRules/Modules/ScoreModule.h>

namespace ngine::GameFramework
{
	KDCounter::KDCounter(const Deserializer& deserializer)
		: m_owner{deserializer.GetParent()}
	{
	}

	KDCounter::KDCounter(const KDCounter& templateComponent, const Cloner& cloner)
		: m_owner{cloner.GetParent()}
		, m_kills{templateComponent.m_kills}
		, m_assists{templateComponent.m_assists}
		, m_deaths{templateComponent.m_deaths}
		, m_killScore{templateComponent.m_killScore}
		, m_assistScore{templateComponent.m_assistScore}
		, m_deathScore{templateComponent.m_deathScore}
	{
	}

	KDCounter::KDCounter(Initializer&& initializer)
		: m_owner{initializer.GetParent()}
	{
	}

	KDCounter::KDCounter(DynamicInitializer& dynamicInitializer)
		: m_owner{dynamicInitializer.GetParent()}
	{
	}

	void KDCounter::AddKill()
	{
		m_kills += 1;

		if (const Entity::DataComponentResult<ScoreModule> pScoreModule = SceneRules::FindModule<ScoreModule>(m_owner.GetRootSceneComponent()))
		{
			if (const Optional<Player*> pPlayerDataComponent = m_owner.FindFirstDataComponentOfTypeInChildrenRecursive<Player>())
			{
				pScoreModule->AddScore(*pScoreModule.m_pDataComponentOwner, pPlayerDataComponent->GetClientIdentifier(), m_killScore);
			}
		}

		OnChanged();
	}

	void KDCounter::AddAssist()
	{
		m_assists += 1;

		if (const Entity::DataComponentResult<ScoreModule> pScoreModule = SceneRules::FindModule<ScoreModule>(m_owner.GetRootSceneComponent()))
		{
			if (const Optional<Player*> pPlayerDataComponent = m_owner.FindFirstDataComponentOfTypeInChildrenRecursive<Player>())
			{
				pScoreModule->AddScore(*pScoreModule.m_pDataComponentOwner, pPlayerDataComponent->GetClientIdentifier(), m_assistScore);
			}
		}

		OnChanged();
	}

	void KDCounter::AddDeath()
	{
		m_deaths += 1;

		if (const Entity::DataComponentResult<ScoreModule> pScoreModule = SceneRules::FindModule<ScoreModule>(m_owner.GetRootSceneComponent()))
		{
			if (const Optional<Player*> pPlayerDataComponent = m_owner.FindFirstDataComponentOfTypeInChildrenRecursive<Player>())
			{
				pScoreModule->AddScore(*pScoreModule.m_pDataComponentOwner, pPlayerDataComponent->GetClientIdentifier(), m_deathScore);
			}
		}

		OnChanged();
	}

	[[maybe_unused]] const bool wasKDCounterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<KDCounter>>::Make());
	[[maybe_unused]] const bool wasKDCounterTypeRegistered = Reflection::Registry::RegisterType<KDCounter>();
}
