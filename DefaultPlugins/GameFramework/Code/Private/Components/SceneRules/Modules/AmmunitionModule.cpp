#include "Components/SceneRules/Modules/AmmunitionModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/Ammunition.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Engine/Threading/JobManager.h>

#include <Common/Reflection/Registry.inl>

#include <GameFramework/Plugin.h>
#include <GameFramework/Components/Player/Ammunition.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Widgets/Widget.h>

#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::GameFramework
{
	AmmunitionDataSource::AmmunitionDataSource(Widgets::Widget& widget, AmmunitionModule& ammunition)
		: Interface(System::Get<DataSource::Cache>().GetPropertySourceCache().FindOrRegister(
				Context::Utils::GetGuid(AmmunitionDataSource::DataSourceGuid, widget, widget.GetSceneRegistry())
			))
		, m_ammunition(ammunition)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_ammunitionPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("ammunition_value");
		m_maxAmmunitionPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("ammunition_max_value");
	}

	AmmunitionDataSource::~AmmunitionDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.Deregister(m_identifier, propertySourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_ammunitionPropertyIdentifier, "ammunition_value");
		dataSourceCache.DeregisterProperty(m_maxAmmunitionPropertyIdentifier, "ammunition_max_value");
	}

	void AmmunitionDataSource::OnChanged()
	{
		OnDataChanged();
	}

	AmmunitionModule::AmmunitionModule(const AmmunitionModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	AmmunitionModule::AmmunitionModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	AmmunitionModule::AmmunitionModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	AmmunitionModule::~AmmunitionModule()
	{
	}

	void AmmunitionModule::OnCreated()
	{
	}

	void AmmunitionModule::OnAmmunitionChanged(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_pAmmunitionDataSource)
		{
			m_pAmmunitionDataSource->OnChanged();
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->Notify(AmmunitionChangedUIEvent, playerIdentifier);
		}
	}

	void AmmunitionModule::OnMaxAmmunitionChanged(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_pAmmunitionDataSource)
		{
			m_pAmmunitionDataSource->OnChanged();
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->Notify(AmmunitionChangedUIEvent, playerIdentifier);
		}
	}

	void AmmunitionModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		[[maybe_unused]] Optional<Physics::Data::Scene*> pPhysicsScene =
			sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		Assert(pPhysicsScene.IsValid());

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &AmmunitionModule::OnPlayerSpawned);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &AmmunitionModule::OnPlayerHUDLoaded);
		}
	}

	void AmmunitionModule::OnGameplayStopped(SceneRules& sceneRules)
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
	AmmunitionModule::OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier playerIdentifier, Entity::Component3D& playerComponent)
	{
		// TODO: Root here is super flawed
		if (Optional<Ammunition*> pAmmunition = playerComponent.GetRootSceneComponent().FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Ammunition>(sceneRules.GetSceneRegistry()).m_pDataComponent)
		{
			m_pAmmunition = pAmmunition;
			EnableUI(sceneRules, playerIdentifier);

			if (!m_pAmmunition->OnChanged.Contains(this))
			{
				m_pAmmunition->OnChanged.Add(
					this,
					[this, &sceneRules, playerIdentifier](AmmunitionModule&)
					{
						OnAmmunitionChanged(sceneRules, playerIdentifier);
					}
				);
			}

			if (!m_pAmmunition->OnMaxChanged.Contains(this))
			{
				m_pAmmunition->OnMaxChanged.Add(
					this,
					[this, &sceneRules, playerIdentifier](AmmunitionModule&)
					{
						OnMaxAmmunitionChanged(sceneRules, playerIdentifier);
					}
				);
			}

			if (m_pAmmunitionDataSource)
			{
				m_pAmmunitionDataSource->OnChanged();
			}
		}
	}

	void AmmunitionModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_pAmmunition)
		{
			EnableUI(sceneRules, playerIdentifier);

			if (m_pAmmunitionDataSource)
			{
				m_pAmmunitionDataSource->OnChanged();
			}
		}
	}

	void AmmunitionModule::EnableUI(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			if (m_pAmmunition && m_pAmmunitionDataSource == nullptr)
			{
				if (Optional<Widgets::Widget*> pWidget = pHUDModule->GetWidget(playerIdentifier))
				{
					m_pAmmunitionDataSource = UniquePtr<AmmunitionDataSource>::Make(*pWidget, *this);
				}
			}

			pHUDModule->Notify(ShowAmmunitionUIEvent, playerIdentifier);
		}
	}

	PropertySource::PropertyValue AmmunitionDataSource::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_ammunitionPropertyIdentifier)
		{
			const int32 value = m_ammunition.m_pAmmunition->GetAmmunition();
			return UnicodeString().Format("{}", value);
		}
		else if (identifier == m_maxAmmunitionPropertyIdentifier)
		{
			const int32 value = m_ammunition.m_pAmmunition->GetMaximumAmmunition();
			return UnicodeString().Format("{}", value);
		}

		return UnicodeString().Format("{}", int32(0));
	}

	[[maybe_unused]] const bool wasAmmunitionModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<AmmunitionModule>>::Make());
	[[maybe_unused]] const bool wasAmmunitionModuleTypeRegistered = Reflection::Registry::RegisterType<AmmunitionModule>();
}
