#include "Components/SceneRules/Modules/TimerModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>

#include <Common/Reflection/Registry.inl>

#include <Common/Memory/Containers/Format/String.h>

#include <Engine/Context/Utils.h>

#include <Widgets/Widget.h>

namespace ngine::GameFramework
{

	TimerDataSource::TimerDataSource(Widgets::Widget& widget, TimerModule& timer)
		: Interface(System::Get<DataSource::Cache>().GetPropertySourceCache().FindOrRegister(
				Context::Utils::GetGuid(TimerDataSource::DataSourceGuid, widget, widget.GetSceneRegistry())
			))
		, m_timer(timer)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_timePropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("gamerules_timer");
	}

	TimerDataSource::~TimerDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.Deregister(m_identifier, propertySourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_timePropertyIdentifier, "gamerules_timer");
	}

	void TimerDataSource::OnChanged()
	{
		OnDataChanged();
	}

	TimerModule::TimerModule(const TimerModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
	{
	}

	TimerModule::TimerModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
	{
	}

	TimerModule::TimerModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
	{
	}

	TimerModule::~TimerModule()
	{
	}

	void TimerModule::OnCreated()
	{
	}

	void TimerModule::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<TimerModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<TimerModule>();
		sceneData.EnableUpdate(*this);
	}

	void TimerModule::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<TimerModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<TimerModule>();
		sceneData.DisableUpdate(*this);
	}

	void TimerModule::Update()
	{
		if (m_pTimerDataSource)
		{
			m_pTimerDataSource->OnChanged();
		}
	}

	void TimerModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		RegisterForUpdate(sceneRules);
		m_timer.Start();

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Add(*this, &TimerModule::OnPlayerHUDLoaded);
		}
	}

	void TimerModule::OnGameplayPaused(SceneRules& sceneRules)
	{
		DeregisterUpdate(sceneRules);
		m_timer.Pause();
	}

	void TimerModule::OnGameplayResumed(SceneRules& sceneRules)
	{
		RegisterForUpdate(sceneRules);
		m_timer.Resume();
	}

	void TimerModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		DeregisterUpdate(sceneRules);

		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			pHUDModule->OnPlayerHUDLoaded.Remove(this);
		}
	}

	void TimerModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		if (Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>())
		{
			m_pTimerDataSource = UniquePtr<TimerDataSource>::Make(*pHUDModule->GetWidget(playerIdentifier), *this);
		}
	}

	PropertySource::PropertyValue TimerDataSource::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_timePropertyIdentifier)
		{
			const Time::Durationf elapsedTime = m_timer.GetElapsedTime();
			return UnicodeString().Format("{:02}:{:02}:{:03}", elapsedTime.GetMinutes(), elapsedTime.GetSeconds(), elapsedTime.GetMilliseconds());
		}

		return {};
	}

	[[maybe_unused]] const bool wasTimerRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<TimerModule>>::Make()
	);
	[[maybe_unused]] const bool wasTimerTypeRegistered = Reflection::Registry::RegisterType<TimerModule>();
}
