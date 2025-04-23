#include "Components/SceneRules/Modules/CountdownModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"
#include "Components/SceneRules/Modules/FinishModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>

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
#include <GameFramework/PlayerManager.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Components/Player/Countdown.h>

#include <Widgets/Widget.h>

namespace ngine::GameFramework
{
	CountdownDataSource::CountdownDataSource(Widgets::Widget& widget, CountdownModule& countdownModule)
		: Interface(System::Get<DataSource::Cache>().GetPropertySourceCache().FindOrRegister(
				Context::Utils::GetGuid(CountdownDataSource::DataSourceGuid, widget, widget.GetSceneRegistry())
			))
		, m_countdown(countdownModule)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_countdownPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("countdown_timer");
	}

	CountdownDataSource::~CountdownDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.Deregister(m_identifier, propertySourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_countdownPropertyIdentifier, "countdown_timer");
	}

	void CountdownDataSource::OnChanged()
	{
		OnDataChanged();
	}

	CountdownModule::CountdownModule(const CountdownModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	CountdownModule::CountdownModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	CountdownModule::CountdownModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	CountdownModule::~CountdownModule()
	{
	}

	void CountdownModule::OnCreated()
	{
	}

	void CountdownModule::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<CountdownModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<CountdownModule>();
		sceneData.EnableUpdate(*this);
	}

	void CountdownModule::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<CountdownModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<CountdownModule>();
		sceneData.DisableUpdate(*this);
	}

	void CountdownModule::Update()
	{
		if (m_pCountdownDataSource)
		{
			m_pCountdownDataSource->OnChanged();
		}

		if (m_duration > 0_seconds && GetRemainingTime() <= 0_seconds)
		{
			if (const Optional<FinishModule*> pFinishModule = m_pSceneRules->FindDataComponentOfType<FinishModule>())
			{
				pFinishModule->FinishAllRemainingPlayers(*m_pSceneRules, GameRulesFinishResult::Failure);
			}
			DeregisterUpdate(*m_pSceneRules);
		}
	}

	void CountdownModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		m_pSceneRules = &sceneRules;

		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Add(*this, &CountdownModule::OnPlayerSpawned);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &CountdownModule::OnPlayerHUDLoaded);
		}

		m_stopwatch.Start();
		RegisterForUpdate(sceneRules);
	}

	void CountdownModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
		{
			pSpawningModule->OnPlayerSpawned.Remove(this);
		}

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Remove(this);
		}

		m_stopwatch.Stop();
		DeregisterUpdate(sceneRules);
	}

	void CountdownModule::OnGameplayPaused(SceneRules& sceneRules)
	{
		m_stopwatch.Pause();
		DeregisterUpdate(sceneRules);
	}

	void CountdownModule::OnGameplayResumed(SceneRules& sceneRules)
	{
		m_stopwatch.Resume();
		RegisterForUpdate(sceneRules);
	}

	void
	CountdownModule::OnPlayerSpawned(SceneRules& sceneRules, const ClientIdentifier clientIdentifier, Entity::Component3D& playerComponent)
	{
		// TODO: Root here is super flawed
		if (Optional<Countdown*> pCountdown = playerComponent.GetRootSceneComponent().FindFirstDataComponentOfTypeInChildrenRecursive<Countdown>().m_pDataComponent)
		{
			// TODO: Getting duration per player is flawed
			m_duration = pCountdown->Get();

			EnableUI(sceneRules, clientIdentifier);
		}
	}

	void CountdownModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (m_duration > 0_seconds)
		{
			EnableUI(sceneRules, playerIdentifier);
		}
	}

	PropertySource::PropertyValue CountdownDataSource::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_countdownPropertyIdentifier)
		{
			const Time::Durationf timeLeft = m_countdown.GetRemainingTime();
			if (timeLeft > 0_seconds)
			{
				return UnicodeString(timeLeft.ToString().GetView());
			}
			else
			{
				return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Over!"));
			}
		}

		return {};
	}

	Time::Durationf CountdownModule::GetRemainingTime() const
	{
		return m_duration - m_stopwatch.GetElapsedTime();
	}

	void CountdownModule::EnableUI(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			if (m_pCountdownDataSource == nullptr)
			{
				if (Optional<Widgets::Widget*> pWidget = pHUDModule->GetWidget(playerIdentifier))
				{
					m_pCountdownDataSource = UniquePtr<CountdownDataSource>::Make(*pWidget, *this);
				}
			}

			pHUDModule->Notify(ShowCountdownUIEvent, playerIdentifier);
		}
	}

	[[maybe_unused]] const bool wasCountdownModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CountdownModule>>::Make());
	[[maybe_unused]] const bool wasCountdownModuleTypeRegistered = Reflection::Registry::RegisterType<CountdownModule>();
}
