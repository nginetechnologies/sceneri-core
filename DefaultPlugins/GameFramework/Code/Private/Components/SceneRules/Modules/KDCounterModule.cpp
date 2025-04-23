#include "Components/SceneRules/Modules/KDCounterModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/KDCounter.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Engine/Threading/JobManager.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/Components/Player/KDCounter.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Widgets/Widget.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/String.h>

namespace ngine::GameFramework
{
	KDCounterDataSource::KDCounterDataSource(Widgets::Widget& widget, KDCounterModule& counter)
		: Interface(System::Get<DataSource::Cache>().GetPropertySourceCache().FindOrRegister(
				Context::Utils::GetGuid(KDCounterDataSource::DataSourceGuid, widget, widget.GetSceneRegistry())
			))
		, m_counter(counter)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_killsPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("kills_value");
		m_assistsPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("assists_value");
		m_deathsPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("deaths_value");
	}

	KDCounterDataSource::~KDCounterDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.Deregister(m_identifier, propertySourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_killsPropertyIdentifier, "kills_value");
		dataSourceCache.DeregisterProperty(m_assistsPropertyIdentifier, "assists_value");
		dataSourceCache.DeregisterProperty(m_deathsPropertyIdentifier, "deaths_value");
	}

	void KDCounterDataSource::OnChanged()
	{
		OnDataChanged();
	}

	KDCounterModule::KDCounterModule(const KDCounterModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	KDCounterModule::KDCounterModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	KDCounterModule::KDCounterModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	KDCounterModule::~KDCounterModule()
	{
	}

	void KDCounterModule::OnCreated()
	{
	}

	void KDCounterModule::OnKDCounterChanged(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_pKDCounterDataSource)
		{
			m_pKDCounterDataSource->OnChanged();
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->Notify(KDCounterChangedUIEvent, playerIdentifier);
		}
	}

	void KDCounterModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		[[maybe_unused]] Optional<Physics::Data::Scene*> pPhysicsScene =
			sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		Assert(pPhysicsScene.IsValid());

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &KDCounterModule::OnPlayerSpawned);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &KDCounterModule::OnPlayerHUDLoaded);
		}
	}

	void KDCounterModule::OnGameplayStopped(SceneRules& sceneRules)
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

	void
	KDCounterModule::OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, Entity::Component3D& playerComponent)
	{
		// TODO: Root here is super flawed
		if (Optional<KDCounter*> pKDCounter = playerComponent.GetRootSceneComponent().FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<KDCounter>(sceneRules.GetSceneRegistry()).m_pDataComponent)
		{
			m_pKDCounter = pKDCounter;
			EnableUI(sceneRules, playerIdentifier);

			if (!m_pKDCounter->OnChanged.Contains(this))
			{
				m_pKDCounter->OnChanged.Add(
					this,
					[this, &sceneRules, playerIdentifier](KDCounterModule&)
					{
						OnKDCounterChanged(sceneRules, playerIdentifier);
					}
				);
			}

			if (m_pKDCounterDataSource)
			{
				m_pKDCounterDataSource->OnChanged();
			}
		}
	}

	void KDCounterModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_pKDCounter)
		{
			EnableUI(sceneRules, playerIdentifier);

			if (m_pKDCounterDataSource)
			{
				m_pKDCounterDataSource->OnChanged();
			}
		}
	}

	void KDCounterModule::EnableUI(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			if (m_pKDCounter && m_pKDCounterDataSource == nullptr)
			{
				if (Optional<Widgets::Widget*> pWidget = pHUDModule->GetWidget(playerIdentifier))
				{
					m_pKDCounterDataSource = UniquePtr<KDCounterDataSource>::Make(*pWidget, *this);
				}
			}

			pHUDModule->Notify(ShowKDCounterUIEvent, playerIdentifier);
		}
	}

	PropertySource::PropertyValue KDCounterDataSource::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_killsPropertyIdentifier)
		{
			const int32 value = m_counter.m_pKDCounter->GetKills();
			return UnicodeString().Format("{}", value);
		}

		if (identifier == m_assistsPropertyIdentifier)
		{
			const int32 value = m_counter.m_pKDCounter->GetAssists();
			return UnicodeString().Format("{}", value);
		}

		if (identifier == m_deathsPropertyIdentifier)
		{
			const int32 value = m_counter.m_pKDCounter->GetDeaths();
			return UnicodeString().Format("{}", value);
		}

		return UnicodeString().Format("{}", int32(0));
	}

	[[maybe_unused]] const bool wasKDCounterModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<KDCounterModule>>::Make());
	[[maybe_unused]] const bool wasKDCounterModuleTypeRegistered = Reflection::Registry::RegisterType<KDCounterModule>();
}
