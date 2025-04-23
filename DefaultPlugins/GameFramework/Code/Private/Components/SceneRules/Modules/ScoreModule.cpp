#include "Components/SceneRules/Modules/ScoreModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/Score.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Data/Tags.h>

#include <Engine/Threading/JobManager.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/StringView.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <NetworkingCore/Components/BoundComponent.h>

namespace ngine::GameFramework
{
	ScoreModule::ScoreModule(const ScoreModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
		m_pDataSource = UniquePtr<DataSource>::Make(cloner.GetParent(), *this);
	}

	ScoreModule::ScoreModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	ScoreModule::ScoreModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
		m_pDataSource = UniquePtr<DataSource>::Make(initializer.GetParent(), *this);
	}

	// TODO: Fix walking character fps reliance so it synchronizes properly
	// TODO: Test with two people
	// TODO: Test with someone remote
	// Once this works well, switch to editing together

	ScoreModule::~ScoreModule()
	{
	}

	void ScoreModule::AddScore(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const int32 value)
	{
		Assert(sceneRules.IsHost());
		if (LIKELY(sceneRules.IsHost()))
		{
			const Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>();
			Assert(pSpawningModule.IsValid());
			if (LIKELY(pSpawningModule.IsValid()))
			{
				const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(playerIdentifier);
				Assert(pPlayerComponent.IsValid());
				if (LIKELY(pPlayerComponent.IsValid()))
				{
					const Optional<Score*> pScore =
						pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Score>(sceneRules.GetSceneRegistry()).m_pDataComponent;
					Assert(pScore.IsValid());
					if (LIKELY(pScore.IsValid()))
					{
						const int32 previousScore = pScore->m_score;
						const int32 newScore = Math::Clamp(previousScore + value, 0, Score::MaximumScore);
						if (newScore != previousScore)
						{
							SetScoreInternal(sceneRules, *pScore, playerIdentifier, newScore);

							if (const Optional<Network::Session::BoundComponent*> pBoundComponent = sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>())
							{
								for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
								{
									const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
									pBoundComponent->SendMessageToClient<&ScoreModule::ClientOnPlayerScoreChanged>(
										sceneRules,
										sceneRules.GetSceneRegistry(),
										clientIdentifier,
										Network::Channel{0},
										ScoreChangedData{playerIdentifier, newScore}
									);
								}
							}
						}
					}
				}
			}
		}
	}

	void
	ScoreModule::SetScoreInternal(SceneRules& sceneRules, Score& playerScore, const ClientIdentifier clientIdentifier, const int32 newScore)
	{
		playerScore.m_score = newScore;
		if (m_pDataSource)
		{
			m_pDataSource->OnDataChanged();
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->Notify(ScoreChangedUIEvent, clientIdentifier);
		}
	}

	void ScoreModule::ClientOnPlayerScoreChanged(
		Entity::HierarchyComponentBase& parent, Network::Session::BoundComponent&, Network::LocalClient&, const ScoreChangedData data
	)
	{
		const Optional<SpawningModule*> pSpawningModule = parent.FindDataComponentOfType<SpawningModule>(parent.GetSceneRegistry());
		Assert(pSpawningModule.IsValid());
		if (LIKELY(pSpawningModule.IsValid()))
		{
			const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(data.playerIdentifier);
			Assert(pPlayerComponent.IsValid());
			if (LIKELY(pPlayerComponent.IsValid()))
			{
				const Optional<Score*> pScore =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Score>(parent.GetSceneRegistry()).m_pDataComponent;
				Assert(pScore.IsValid());
				if (LIKELY(pScore.IsValid()))
				{
					SetScoreInternal(parent.AsExpected<SceneRules>(), *pScore, data.playerIdentifier, data.score);
				}
			}
		}
	}

	int32 ScoreModule::GetScore(SceneRules& sceneRules, const ClientIdentifier clientIdentifier)
	{
		const Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>();
		Assert(pSpawningModule.IsValid());
		if (LIKELY(pSpawningModule.IsValid()))
		{
			if (Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
			{
				const Optional<Score*> pScore =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Score>(sceneRules.GetSceneRegistry()).m_pDataComponent;
				Assert(pScore.IsValid());
				if (LIKELY(pScore.IsValid()))
				{
					return pScore->m_score;
				}
			}
		}

		return 0;
	}

	void ScoreModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		[[maybe_unused]] Optional<Physics::Data::Scene*> pPhysicsScene =
			sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		Assert(pPhysicsScene.IsValid());

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &ScoreModule::OnPlayerSpawned);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &ScoreModule::OnPlayerHUDLoaded);
		}
	}

	void ScoreModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Remove(this);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Remove(this);
		}
	}

	void ScoreModule::OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, Entity::Component3D& playerComponent)
	{
		Optional<Score*> pScore =
			playerComponent.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Score>(sceneRules.GetSceneRegistry()).m_pDataComponent;
		if (pScore.IsInvalid())
		{
			pScore = playerComponent.CreateDataComponent<Score>(Score::Initializer{playerComponent, playerComponent.GetSceneRegistry()});
		}
		if (m_pDataSource)
		{
			m_pDataSource->OnDataChanged();
		}

		Assert(pScore.IsValid());
		if (LIKELY(pScore.IsValid()))
		{
			EnableUI(sceneRules, playerIdentifier);
		}
	}

	void ScoreModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		EnableUI(sceneRules, playerIdentifier);
	}

	void ScoreModule::EnableUI(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->Notify(ShowScoreUIEvent, playerIdentifier);
		}
	}

	ScoreModule::DataSource::DataSource(SceneRules& sceneRules, ScoreModule& scoreModule)
		: Interface(System::Get<ngine::DataSource::Cache>().FindOrRegister(
				Context::Utils::GetGuid(DataSourceGuid, sceneRules, sceneRules.GetSceneRegistry())
			))
		, m_sceneRules(sceneRules)
		, m_scoreModule(scoreModule)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_scorePropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("score_value");
	}

	ScoreModule::DataSource::~DataSource()
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_scorePropertyIdentifier, "score_value");
	}

	DataSource::GenericDataIndex ScoreModule::DataSource::GetDataCount() const
	{
		return m_sceneRules.GetPlayerCount();
	}

	// TODO: Make this into a generic ClientDataSource so we can implement other modules easily.
	// Maybe merge into one scene rules data source? Easier to manage.
	void ScoreModule::DataSource::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
	{
		IdentifierMask<ClientIdentifier>& selectedClients = reinterpret_cast<IdentifierMask<ClientIdentifier>&>(cachedQueryOut);
		selectedClients.ClearAll();

		const Optional<SpawningModule*> pSpawningModule = m_sceneRules.FindDataComponentOfType<SpawningModule>();
		Assert(pSpawningModule.IsValid());
		if (UNLIKELY(!pSpawningModule.IsValid()))
		{
			return;
		}

		if (query.m_allowedItems.IsValid())
		{
			selectedClients = IdentifierMask<ClientIdentifier>(*query.m_allowedItems);
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			IdentifierMask<ClientIdentifier> allowedFilterMask;
			for (const ClientIdentifier::IndexType clientIndex : m_sceneRules.GetPlayerIterator())
			{
				const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
				if (const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
				{
					if (const Optional<Entity::Data::Tags*> pPlayerTagsComponent = pPlayerComponent->FindDataComponentOfType<Entity::Data::Tags>())
					{
						if (query.m_allowedFilterMask.AreAnySet(pPlayerTagsComponent->GetMask()))
						{
							allowedFilterMask.Set(clientIdentifier);
						}
					}
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				selectedClients |= allowedFilterMask;
			}
			else
			{
				selectedClients &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			selectedClients = (const IdentifierMask<ClientIdentifier>&)m_sceneRules.GetPlayersMask();
		}

		const bool hasDisallowed = query.m_disallowedFilterMask.AreAnySet();
		const bool hasRequired = query.m_requiredFilterMask.AreAnySet();
		if (hasRequired || hasDisallowed)
		{
			for (const ClientIdentifier::IndexType clientIndex : selectedClients.GetSetBitsIterator())
			{
				const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
				if (const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
				{
					if (const Optional<Entity::Data::Tags*> pPlayerTagsComponent = pPlayerComponent->FindDataComponentOfType<Entity::Data::Tags>())
					{
						const Tag::Mask componentTagMask = pPlayerTagsComponent->GetMask();
						if (hasDisallowed && componentTagMask.AreAnySet(query.m_disallowedFilterMask))
						{
							selectedClients.Clear(clientIdentifier);
						}
						if (hasRequired && !componentTagMask.AreAllSet(query.m_requiredFilterMask))
						{
							selectedClients.Clear(clientIdentifier);
						}
					}
				}
			}
		}
	}

	void ScoreModule::DataSource::IterateData(
		const CachedQuery& cachedQuery, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset
	) const
	{
		const IdentifierMask<ClientIdentifier>& selectedClients = reinterpret_cast<const IdentifierMask<ClientIdentifier>&>(cachedQuery);
		if (selectedClients.AreAnySet())
		{
			const IdentifierMask<ClientIdentifier>::SetBitsIterator iterator = selectedClients.GetSetBitsIterator();

			for (auto it = iterator.begin() + offset.GetMinimum(), endIt = Math::Min(iterator.begin() + offset.GetEnd(), iterator.end());
			     it < endIt;
			     ++it)
			{
				ClientIdentifier::IndexType clientIndex = *it;
				callback(clientIndex);
			}
		}
	}

	void ScoreModule::DataSource::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset
	) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			ClientIdentifier::IndexType clientIndex = (ClientIdentifier::IndexType)identifierIndex;
			callback(clientIndex);
		}
	}

	DataSource::PropertyValue ScoreModule::DataSource::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_scorePropertyIdentifier)
		{
			const ClientIdentifier::IndexType clientIndex = data.GetExpected<ClientIdentifier::IndexType>();
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			const int32 playerScore = m_scoreModule.GetScore(m_sceneRules, clientIdentifier);
			return UnicodeString().Format("{}", playerScore);
		}
		return {};
	}

	[[maybe_unused]] const bool wasScoreModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ScoreModule>>::Make());
	[[maybe_unused]] const bool wasScoreModuleTypeRegistered = Reflection::Registry::RegisterType<ScoreModule>();
}
