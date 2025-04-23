#include "Components/SceneRules/Modules/StartCountdownModule.h"
#include "Components/SceneRules/Modules/SpawningModule.h"
#include "Components/SceneRules/Modules/HUDModule.h"

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
#include <Common/Memory/Containers/Format/StringView.h>

#include <GameFramework/Plugin.h>
#include <GameFramework/PlayerManager.h>

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertySourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

namespace ngine::GameFramework
{
	StartCountdownModule::StartCountdownModule(const StartCountdownModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
		, Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
		, m_countdownTime(templateComponent.m_countdownTime)
		, m_options(templateComponent.m_options)
	{
	}

	StartCountdownModule::StartCountdownModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
		, Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
	}

	StartCountdownModule::StartCountdownModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
		, Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
		, m_options(initializer.m_options)
	{
	}

	StartCountdownModule::~StartCountdownModule()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.Deregister(m_identifier, propertySourceCache.FindGuid(m_identifier));
		dataSourceCache.DeregisterProperty(m_countdownPropertyIdentifier, "gamerules_countdown_timer");
	}

	void StartCountdownModule::OnCreated()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_countdownPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("gamerules_countdown_timer");
	}

	void StartCountdownModule::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<StartCountdownModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<StartCountdownModule>(
		);
		sceneData.EnableUpdate(*this);
	}

	void StartCountdownModule::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<StartCountdownModule>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<StartCountdownModule>(
		);
		sceneData.DisableUpdate(*this);
	}

	void StartCountdownModule::Update()
	{
		OnDataChanged();
	}

	void StartCountdownModule::OnGameplayStarted(SceneRules& sceneRules)
	{
		if (m_countdownTime <= 0_seconds)
		{
			return;
		}

		RegisterForUpdate(sceneRules);
		m_timer.Start();

		if (!m_options.IsEmpty())
		{
			Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			Assert(pPhysicsScene.IsValid());

			Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>();

			if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
			{
				for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetPlayerIterator())
				{
					const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
					if (Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
					{
						if (m_options.IsSet(Options::SleepPlayerPhysics))
						{
							if (Entity::Component3D::DataComponentResult<Physics::Data::Body> bodyComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
							{
								bodyComponent.m_pDataComponent->Sleep(*pPhysicsScene);
							}
						}

						if (pHUDModule)
						{
							pHUDModule->Notify(CountdownStartEvent, clientIdentifier);
						}

						// TODO: Block Player Input
						// if (m_options.IsSet(Options::BlockPlayerInput))
						// {
						// 	if (Optional<Entity::InputComponent*> pInputComponent =
						// pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
						// 	{
						// 		pPlayerComponent->AssignInput(*pInputComponent);
						// 	}
						// }
					}
				}

				pSpawningModule->OnPlayerSpawned.Add(*this, &StartCountdownModule::OnPlayerSpawned);
			}

			if (pHUDModule)
			{
				pHUDModule->OnPlayerHUDLoaded.Add(*this, &StartCountdownModule::OnPlayerHUDLoaded);
			}
		}

		OnCountdownStarted();

		System::Get<Threading::JobManager>().ScheduleAsync(
			m_countdownTime + 1_seconds, // 1 second buffer for showing 0 value as "GO" etc
			[this, &sceneRules](Threading::JobRunnerThread&)
			{
				EndCountdown(sceneRules);
			},
			Threading::JobPriority::InteractivityLogic
		);
	}

	void StartCountdownModule::OnGameplayStopped(SceneRules& sceneRules)
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

	void StartCountdownModule::EndCountdown(SceneRules& sceneRules)
	{
		if (m_timer.IsRunning() && GetTimeLeft() <= -1_seconds)
		{
			DeregisterUpdate(sceneRules);
			m_timer.Stop();

			if (!m_options.IsEmpty())
			{

				Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>();

				Optional<Physics::Data::Scene*> pPhysicsScene = sceneRules.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				Assert(pPhysicsScene.IsValid());

				if (Optional<SpawningModule*> pSpawningModule = sceneRules.FindDataComponentOfType<SpawningModule>())
				{
					for (const ClientIdentifier::IndexType clientIndex : sceneRules.GetPlayerIterator())
					{
						const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(clientIndex);
						if (Optional<Entity::Component3D*> pPlayerComponent = pSpawningModule->GetPlayerComponent(clientIdentifier))
						{
							if (m_options.IsSet(Options::SleepPlayerPhysics))
							{
								if (Entity::Component3D::DataComponentResult<Physics::Data::Body> bodyComponent = pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
								{
									bodyComponent.m_pDataComponent->Wake(*pPhysicsScene);
								}
							}

							if (pHUDModule)
							{
								pHUDModule->Notify(CountdownEndEvent, clientIdentifier);
							}

							// TODO: Block Player Input
							// if (m_options.IsSet(Options::BlockPlayerInput))
							// {
							// 	if (Optional<Entity::InputComponent*> pInputComponent =
							// pPlayerComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::InputComponent>().m_pDataComponent)
							// 	{
							// 		pPlayerComponent->AssignInput(*pInputComponent);
							// 	}
							// }
						}
					}
				}
			}

			OnCountdownEnded();
		}
	}

	void StartCountdownModule::OnPlayerSpawned(SceneRules&, const ClientIdentifier, Entity::Component3D& playerComponent)
	{
		if (m_timer.GetElapsedTime() < m_countdownTime)
		{
			if (m_options.IsSet(Options::SleepPlayerPhysics))
			{
				Optional<Physics::Data::Scene*> pPhysicsScene =
					playerComponent.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				Assert(pPhysicsScene.IsValid());
				if (Entity::Component3D::DataComponentResult<Physics::Data::Body> bodyComponent = playerComponent.FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
				{
					bodyComponent.m_pDataComponent->Sleep(*pPhysicsScene);
				}
			}
		}
	}

	void StartCountdownModule::OnPlayerHUDLoaded(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Optional<HUDModule*> pHUDModule = sceneRules.FindDataComponentOfType<HUDModule>();
		if (m_timer.GetElapsedTime() < m_countdownTime)
		{
			pHUDModule->Notify(CountdownStartEvent, playerIdentifier);
		}
		else
		{
			pHUDModule->Notify(CountdownEndEvent, playerIdentifier);
		}
	}

	PropertySource::PropertyValue StartCountdownModule::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_countdownPropertyIdentifier)
		{
			const Time::Durationf timeLeft = GetTimeLeft();
			if (timeLeft > 0_seconds)
			{
				return UnicodeString().Format("{:.0f}", Math::Ceil(timeLeft.GetSeconds()));
			}
			else
			{
				return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("GO!"));
			}
		}

		return {};
	}

	[[maybe_unused]] const bool wasStartCountdownModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<StartCountdownModule>>::Make());
	[[maybe_unused]] const bool wasStartCountdownModuleTypeRegistered = Reflection::Registry::RegisterType<StartCountdownModule>();
}
