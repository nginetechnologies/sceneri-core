#pragma once

#include "AppWindow.h"

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Scene/Scene2D.h>

#include <Widgets/WidgetScene.h>

namespace ngine
{
	namespace Network::Session
	{
		struct Client;
	}
}

namespace ngine::App::UI::Document
{
	struct MainWindow final : public Window
	{
		using Initializer = ToolWindow::Initializer;

		MainWindow(Initializer&& initializer);
		virtual ~MainWindow();

		template<typename... Args>
		static MainWindow& Create(Args&&... args)
		{
			return *new MainWindow(Forward<Args&&>(args)...);
		}

		[[nodiscard]] virtual const Entity::SceneRegistry& GetEntitySceneRegistry() const override final
		{
			return m_entitySceneRegistry;
		}
		[[nodiscard]] virtual Entity::SceneRegistry& GetEntitySceneRegistry() override final
		{
			return m_entitySceneRegistry;
		}
		[[nodiscard]] virtual const Widgets::Scene& GetScene() const override final
		{
			return m_scene;
		}
		[[nodiscard]] virtual Widgets::Scene& GetScene() override final
		{
			return m_scene;
		}
		[[nodiscard]] virtual const Entity::RootComponent& GetRootComponent() const override final
		{
			return m_rootComponent;
		}
		[[nodiscard]] virtual Entity::RootComponent& GetRootComponent() override final
		{
			return m_rootComponent;
		}

		virtual void Close() override;
	private:
		void Initialize(Threading::JobBatch& jobBatch);
	private:
		Entity::SceneRegistry m_entitySceneRegistry;
		Entity::RootComponent& m_rootComponent;
		Network::Session::Client& m_clientComponent;
		Widgets::Scene m_scene;
	};
}
