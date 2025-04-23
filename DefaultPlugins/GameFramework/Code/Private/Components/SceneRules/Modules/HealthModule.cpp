#include "Components/SceneRules/Modules/HealthModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"
#include "Components/SceneRules/Modules/FinishModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/Health.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Data/Tags.h>

#include <Engine/Threading/JobManager.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/String.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <NetworkingCore/Components/BoundComponent.h>

#include <Widgets/Widget.h>

namespace ngine::GameFramework
{
	HealthModule::HealthModule(const HealthModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
		m_pDataSource = UniquePtr<DataSource>::Make(cloner.GetParent(), *this);
	}

	HealthModule::HealthModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	HealthModule::HealthModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
		m_pDataSource = UniquePtr<DataSource>::Make(initializer.GetParent(), *this);
	}

	HealthModule::~HealthModule()
	{
	}

	void HealthModule::AddHealth(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, const float value)
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
					const Optional<Health*> pHealth = pPlayerComponent
					                                    ->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(sceneRules.GetSceneRegistry()
					                                    )
					                                    .m_pDataComponent;
					Assert(pHealth.IsValid());
					if (LIKELY(pHealth.IsValid()))
					{
						const float previousHealth = pHealth->m_health;
						const float newHealth = Math::Clamp(previousHealth + value, 0.f, Math::NumericLimits<Health::ValueType>::Max);
						if (newHealth != previousHealth && previousHealth > 0)
						{
							SetHealthInternal(sceneRules, *pHealth, playerIdentifier, newHealth);

							if (const Optional<Network::Session::BoundComponent*> pBoundComponent = sceneRules.FindDataComponentOfType<Network::Session::BoundComponent>())
							{
								for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetLoadedPlayerIterator())
								{
									const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
									pBoundComponent->SendMessageToClient<&HealthModule::ClientOnPlayerHealthChanged>(
										sceneRules,
										sceneRules.GetSceneRegistry(),
										clientIdentifier,
										Network::Channel{0},
										HealthChangedData{playerIdentifier, newHealth}
									);
								}
							}

							if (newHealth == 0)
							{
								if (const Optional<FinishModule*> pFinishModule = sceneRules.FindDataComponentOfType<FinishModule>();
								    pFinishModule.IsValid() && !pFinishModule->HasPlayerFinished(playerIdentifier))
								{
									pFinishModule->NotifyPlayerFinished(sceneRules, playerIdentifier, GameRulesFinishResult::Failure);
								}
							}
						}
					}
				}
			}
		}
	}

	void HealthModule::SetHealthInternal(SceneRules&, Health& playerHealth, const ClientIdentifier clientIdentifier, const float neHealth)
	{
		playerHealth.m_health = neHealth;
		if (m_pDataSource)
		{
			m_pDataSource->OnDataChanged();
		}
		if (const Optional<PerPlayerDataSource*> pPerPlayerDataSource = m_perPlayerDataSources[clientIdentifier])
		{
			pPerPlayerDataSource->OnDataChanged();
		}
	}

	void HealthModule::ClientOnPlayerHealthChanged(
		Entity::HierarchyComponentBase& parent, Network::Session::BoundComponent&, Network::LocalClient&, const HealthChangedData data
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
				const Optional<Health*> pHealth =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(parent.GetSceneRegistry()).m_pDataComponent;
				Assert(pHealth.IsValid());
				if (LIKELY(pHealth.IsValid()))
				{
					SetHealthInternal(parent.AsExpected<SceneRules>(), *pHealth, data.playerIdentifier, data.health);
				}
			}
		}
	}

	float HealthModule::GetHealth(SceneRules& sceneRules, const ClientIdentifier clientIdentifier)
	{
		const Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>();
		Assert(pSpawningModule.IsValid());
		if (LIKELY(pSpawningModule.IsValid()))
		{
			if (const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
			{
				const Optional<Health*> pHealth =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(sceneRules.GetSceneRegistry()).m_pDataComponent;
				Assert(pHealth.IsValid());
				if (LIKELY(pHealth.IsValid()))
				{
					return pHealth->m_health;
				}
			}
		}

		return 0;
	}

	float HealthModule::GetMaximumHealth(SceneRules& sceneRules, const ClientIdentifier clientIdentifier)
	{
		const Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>();
		Assert(pSpawningModule.IsValid());
		if (LIKELY(pSpawningModule.IsValid()))
		{
			if (const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
			{
				const Optional<Health*> pHealth =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(sceneRules.GetSceneRegistry()).m_pDataComponent;
				Assert(pHealth.IsValid());
				if (LIKELY(pHealth.IsValid()))
				{
					return pHealth->m_maximum;
				}
			}
		}

		return 0;
	}

	Math::Ratiof HealthModule::GetHealthRatio(SceneRules& sceneRules, const ClientIdentifier clientIdentifier)
	{
		const Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>();
		Assert(pSpawningModule.IsValid());
		if (LIKELY(pSpawningModule.IsValid()))
		{
			if (const Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
			{
				const Optional<Health*> pHealth =
					pPlayerComponent->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(sceneRules.GetSceneRegistry()).m_pDataComponent;
				Assert(pHealth.IsValid());
				if (LIKELY(pHealth.IsValid()))
				{
					return pHealth->GetRatio();
				}
			}
		}

		return 0_percent;
	}

	void HealthModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &HealthModule::OnPlayerSpawned);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &HealthModule::OnPlayerHUDLoaded);
		}
	}

	void HealthModule::OnGameplayStopped(SceneRules& sceneRules)
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

	void HealthModule::OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, Entity::Component3D& playerComponent)
	{
		Optional<Health*> pHealth =
			playerComponent.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Health>(sceneRules.GetSceneRegistry()).m_pDataComponent;
		if (pHealth.IsInvalid())
		{
			pHealth = playerComponent.CreateDataComponent<Health>(Health::Initializer{playerComponent, playerComponent.GetSceneRegistry()});
		}
		if (m_pDataSource)
		{
			m_pDataSource->OnDataChanged();
		}
		if (const Optional<PerPlayerDataSource*> pPerPlayerDataSource = m_perPlayerDataSources[playerIdentifier])
		{
			pPerPlayerDataSource->OnDataChanged();
		}

		Assert(pHealth.IsValid());
		if (LIKELY(pHealth.IsValid()))
		{
			EnableUI(sceneRules, playerIdentifier);
		}
	}

	void HealthModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		EnableUI(sceneRules, playerIdentifier);
	}

	void HealthModule::EnableUI(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			if (m_perPlayerDataSources[playerIdentifier].IsInvalid())
			{
				if (Optional<Widgets::Widget*> pWidget = pHUDModule->GetWidget(playerIdentifier))
				{
					m_perPlayerDataSources[playerIdentifier].CreateInPlace(*pWidget, sceneRules, *this, playerIdentifier);
				}
			}

			pHUDModule->Notify(ShowHealthUIEvent, playerIdentifier);
		}
	}

	HealthModule::PerPlayerDataSource::PerPlayerDataSource(
		Widgets::Widget& widget, SceneRules& sceneRules, HealthModule& healthModule, const ClientIdentifier clientIdentifier
	)
		: Interface(
				System::Get<ngine::DataSource::Cache>().FindOrRegister(Context::Utils::GetGuid(DataSourceGuid, widget, widget.GetSceneRegistry()))
			)
		, m_clientIdentifier(clientIdentifier)
		, m_sceneRules(sceneRules)
		, m_healthModule(healthModule)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_healthPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("health_icon_guid");
	}

	HealthModule::PerPlayerDataSource::~PerPlayerDataSource()
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_healthPropertyIdentifier, "health_icon_guid");
	}

	DataSource::GenericDataIndex HealthModule::PerPlayerDataSource::GetDataCount() const
	{
		return (DataSource::GenericDataIndex)m_healthModule.GetMaximumHealth(m_sceneRules, m_clientIdentifier);
	}

	void HealthModule::PerPlayerDataSource::IterateData(
		const CachedQuery& query, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset
	) const
	{
		UNUSED(offset);
		for (GenericDataIndex index : query.GetSetBitsIterator())
		{
			callback(index);
		}
	}

	void HealthModule::PerPlayerDataSource::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset
	) const
	{
		for (GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			callback(identifierIndex);
		}
	}

	DataSource::PropertyValue HealthModule::PerPlayerDataSource::GetDataProperty(const Data data, const PropertyIdentifier identifier) const
	{
		if (identifier == m_healthPropertyIdentifier)
		{
			const GenericDataIndex index = data.GetExpected<GenericDataIndex>();
			if (float(index + 1u) <= m_healthModule.GetHealth(m_sceneRules, m_clientIdentifier))
			{
				return FilledHealthIcon;
			}
			else
			{
				return EmptyHealthIcon;
			}
		}

		return {};
	}

	void HealthModule::PerPlayerDataSource::CacheQuery(const Query&, CachedQuery& cachedQueryOut) const
	{
		for (Health::ValueType i = 0, maximumHealth = m_healthModule.GetMaximumHealth(m_sceneRules, m_clientIdentifier); i < maximumHealth; ++i)
		{
			cachedQueryOut.Set(GenericDataIdentifier::MakeFromValidIndex((GenericDataIdentifier::IndexType)i));
		}
	}

	HealthModule::DataSource::DataSource(SceneRules& sceneRules, HealthModule& healthModule)
		: Interface(System::Get<ngine::DataSource::Cache>().FindOrRegister(
				Context::Utils::GetGuid(DataSourceGuid, sceneRules, sceneRules.GetSceneRegistry())
			))
		, m_sceneRules(sceneRules)
		, m_healthModule(healthModule)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_healthPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("health_value");
		m_healthRatioPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("health_ratio");
	}

	HealthModule::DataSource::~DataSource()
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_healthPropertyIdentifier, "health_value");
		dataSourceCache.DeregisterProperty(m_healthRatioPropertyIdentifier, "health_ratio");
	}

	DataSource::GenericDataIndex HealthModule::DataSource::GetDataCount() const
	{
		return m_sceneRules.GetPlayerCount();
	}

	// TODO: Make this into a generic ClientDataSource so we can implement other modules easily.
	// Maybe merge into one scene rules data source? Easier to manage.
	void HealthModule::DataSource::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
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

	void HealthModule::DataSource::IterateData(
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

	void HealthModule::DataSource::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset
	) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			ClientIdentifier::IndexType clientIndex = (ClientIdentifier::IndexType)identifierIndex;
			callback(clientIndex);
		}
	}

	PropertySource::PropertyValue
	HealthModule::DataSource::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_healthPropertyIdentifier)
		{
			const ClientIdentifier::IndexType clientIndex = data.GetExpected<ClientIdentifier::IndexType>();
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			const int32 playerHealth = (int32)m_healthModule.GetHealth(m_sceneRules, clientIdentifier);
			return UnicodeString().Format("{}", playerHealth);
		}
		else if (identifier == m_healthRatioPropertyIdentifier)
		{
			const ClientIdentifier::IndexType clientIndex = data.GetExpected<ClientIdentifier::IndexType>();
			const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);

			const Math::Ratiof playerRatio = m_healthModule.GetHealthRatio(m_sceneRules, clientIdentifier);
			return UnicodeString().Format("{}", (float)playerRatio);
		}

		return {};
	}

	[[maybe_unused]] const bool wasHealthModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HealthModule>>::Make());
	[[maybe_unused]] const bool wasHealthModuleTypeRegistered = Reflection::Registry::RegisterType<HealthModule>();
}
