#include <GameFramework/PlayerManager.h>

#include <Renderer/Scene/SceneView.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Widgets/Documents/SceneDocument3D.h>
#include <Widgets/Widget.inl>
#include <Widgets/Style/Entry.h>

namespace ngine::GameFramework
{
	void PlayerManager::AddPlayer(const ClientIdentifier playerIdentifier, const Optional<Rendering::SceneView*> pSceneView)
	{
		Assert(playerIdentifier.IsValid());
		Assert(!m_activePlayersBitset.IsSet(playerIdentifier));
		if (LIKELY(m_activePlayersBitset.Set(playerIdentifier)))
		{
			PlayerInfo& playerInfo = m_playerInfos[playerIdentifier];
			playerInfo.m_clientIdentifier = playerIdentifier;
			playerInfo.m_pSceneView = pSceneView;
			if (pSceneView.IsValid())
			{
				playerInfo.m_pActionMonitor.CreateInPlace();
			}

			if (pSceneView.IsValid())
			{
				if (Optional<Widgets::Document::Scene3D*> pSceneWidget = pSceneView->GetOwningWidget())
				{
					UniquePtr<Widgets::Style::Entry> pStyle = []()
					{
						UniquePtr<Widgets::Style::Entry> pStyle{Memory::ConstructInPlace};
						Widgets::Style::Entry::ModifierValues& modifier = pStyle->EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
						modifier.ParseFromCSS("position: absolute; width: 100%; height: 100%;");
						pStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
						pStyle->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());
						return pStyle;
					}();
					playerInfo.m_pHUD = pSceneWidget->EmplaceChildWidget<Widgets::Widget>(
						Widgets::Widget::Initializer{*pSceneWidget, Widgets::Widget::Flags::IsInputDisabled, Move(pStyle)}
					);
				}
				m_virtualControllerManager.CreateController(playerInfo);
			}
		}
	}

	void PlayerManager::RemovePlayer(const ClientIdentifier playerIdentifier)
	{
		Assert(playerIdentifier.IsValid());
		if (m_activePlayersBitset.Clear(playerIdentifier))
		{
			PlayerInfo& playerInfo = m_playerInfos[playerIdentifier];
			m_virtualControllerManager.DestroyController(playerInfo);
			if (playerInfo.m_pHUD.IsValid())
			{
				playerInfo.m_pHUD->Destroy(playerInfo.m_pHUD->GetSceneRegistry());
				playerInfo.m_pHUD = {};
			}

			playerInfo.m_pSceneView = nullptr;
			playerInfo.m_pActionMonitor.DestroyElement();
		}
	}
}
