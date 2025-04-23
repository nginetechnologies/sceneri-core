#include "Components/SceneRules/Modules/HUDModule.h"

#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/Player/Player.h>

#include <Engine/Entity/ComponentType.h>

#include <Tags.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Context/EventManager.inl>

#include <Common/Reflection/Registry.inl>

#include <GameFramework/Plugin.h>

#include <Widgets/Widget.h>

namespace ngine::GameFramework
{
	HUDModule::HUDModule(const HUDModule& templateComponent, const Cloner& cloner)
		: SceneRulesModule(templateComponent, cloner)
		, m_hudAsset(templateComponent.m_hudAsset)
	{
	}

	HUDModule::HUDModule(const Deserializer& deserializer)
		: SceneRulesModule(deserializer)
		, m_hudAsset(deserializer.m_reader.ReadWithDefaultValue<Guid>("asset", Guid()))
	{
	}

	HUDModule::HUDModule(Initializer&& initializer)
		: SceneRulesModule(Forward<Initializer>(initializer))
		, m_hudAsset(initializer.m_hudWidgetAsset)
	{
	}

	void HUDModule::ClientOnLocalPlayerJoined(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Assert(m_hudAsset.IsValid());
		if (LIKELY(m_hudAsset.IsValid()))
		{
			const GameFramework::PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
			if (Optional<const GameFramework::PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(playerIdentifier))
			{
				const Optional<Widgets::Widget*> pHUDWidget = pPlayerInfo->GetHUD();
				Assert(pHUDWidget.IsValid());
				if (LIKELY(pHUDWidget.IsValid()))
				{
					Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
						m_hudAsset,
						pHUDWidget->GetSceneRegistry(),
						pHUDWidget,
						[this, playerIdentifier, &sceneRules](const Optional<Widgets::Widget*> pWidget)
						{
							Assert(pWidget.IsValid());
							if (LIKELY(pWidget.IsValid()))
							{
								Assert(m_playerWidgets[playerIdentifier].IsInvalid());
								m_playerWidgets[playerIdentifier] = pWidget;
								pWidget->RecalculateHierarchy();

								OnPlayerHUDLoaded(sceneRules, playerIdentifier);
							}
						}
					);
					if (jobBatch.IsValid())
					{
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
				}
			}
		}
	}

	void HUDModule::ClientOnLocalPlayerLeft(SceneRules& sceneRules, const ClientIdentifier playerIdentifier)
	{
		Optional<Widgets::Widget*>& pPlayerWidget = m_playerWidgets[playerIdentifier];
		if (pPlayerWidget.IsValid())
		{
			Entity::SceneRegistry& sceneRegistry = sceneRules.GetSceneRegistry();
			pPlayerWidget->Destroy(sceneRegistry);
			pPlayerWidget = Invalid;
		}
	}

	void HUDModule::OnGameplayStopped(SceneRules& sceneRules)
	{
		Entity::SceneRegistry& sceneRegistry = sceneRules.GetSceneRegistry();
		for (Optional<Widgets::Widget*>& pWidget : m_playerWidgets.GetView())
		{
			if (pWidget.IsValid())
			{
				pWidget->Destroy(sceneRegistry);
				pWidget = Invalid;
			}
		}
	}

	void HUDModule::Notify(const Guid eventGuid, const ClientIdentifier playerIdentifier)
	{
		if (Optional<Widgets::Widget*> pWidget = m_playerWidgets[playerIdentifier])
		{
			Context::EventManager eventManager(*pWidget, pWidget->GetSceneRegistry());
			eventManager.Notify(eventGuid);
		}
	}

	Optional<Widgets::Widget*> HUDModule::GetWidget(const ClientIdentifier playerIdentifier)
	{
		return m_playerWidgets[playerIdentifier];
	}

	[[maybe_unused]] const bool wasHUDModuleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HUDModule>>::Make());
	[[maybe_unused]] const bool wasHUDModuleTypeRegistered = Reflection::Registry::RegisterType<HUDModule>();
}
