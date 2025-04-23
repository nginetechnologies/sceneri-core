#pragma once

#include <Engine/Input/Actions/ActionMonitor.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Memory/Optional.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::Rendering
{
	struct SceneView;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct PlayerInfo
	{
		[[nodiscard]] ClientIdentifier GetClientIdentifier() const
		{
			return m_clientIdentifier;
		}

		[[nodiscard]] Optional<Widgets::Widget*> GetHUD() const
		{
			return m_pHUD;
		}

		void SetSceneView(Rendering::SceneView* pSceneView)
		{
			m_pSceneView = pSceneView;
		}
		[[nodiscard]] Optional<Rendering::SceneView*> GetSceneView() const
		{
			return m_pSceneView;
		}

		[[nodiscard]] Optional<Input::Monitor*> GetInputMonitor() const
		{
			return m_pActionMonitor.Get();
		}
	private:
		friend struct PlayerManager;

		ClientIdentifier m_clientIdentifier;
		Optional<Widgets::Widget*> m_pHUD;
		Optional<Rendering::SceneView*> m_pSceneView;
		UniquePtr<Input::ActionMonitor> m_pActionMonitor;
	};
}
