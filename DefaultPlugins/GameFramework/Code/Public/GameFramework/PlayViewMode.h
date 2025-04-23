#pragma once

#include <Renderer/Scene/SceneViewMode.h>

#include <Engine/Input/Actions/EventAction.h>
#include <Engine/DataSource/PropertySourceInterface.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Guid.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Function/Event.h>

namespace ngine::Entity
{
	struct Component3D;
	struct CameraComponent;
}

namespace ngine::Input
{
	struct ActionMonitor;
}

namespace ngine::Rendering
{
	struct Window;
	struct SceneView;
}

namespace ngine::Widgets
{
	struct Widget;

	namespace Document
	{
		struct Scene3D;
	}
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct PlayViewModeBase : public SceneViewMode, public PropertySource::Interface
	{
		inline static constexpr Guid DataSourceGuid = "4de6af23-70e6-47b4-93b5-ba7ebe826695"_guid;

		PlayViewModeBase(
			Widgets::Document::Scene3D& sceneWidget,
			const ClientIdentifier clientIdentifier,
			const PropertySource::Identifier propertySourceIdentifier
		);
		virtual ~PlayViewModeBase();

		// SceneViewMode
		virtual void OnActivated(Rendering::SceneViewBase&) override;
		virtual void OnDeactivated(Optional<SceneBase*>, Rendering::SceneViewBase&) override;
		[[nodiscard]] virtual void OnSceneAssigned(SceneBase&, Rendering::SceneViewBase&) override;
		virtual void OnSceneUnloading(SceneBase&, Rendering::SceneViewBase&) override;
		virtual void OnSceneLoaded(SceneBase&, Rendering::SceneViewBase&) override;

		virtual Optional<Input::Monitor*> GetInputMonitor() const override;
		// ~SceneViewMode

		// PropertySource::Interface
		virtual PropertyValue GetDataProperty(const DataSource::PropertyIdentifier identifier) const override;
		// ~PropertySource::Interface

		void OnRestartGameplayAction();
		void OnResumeGameplayAction();
		void OnPauseGameplayAction();

		void ShowCursor();
		void HideCursor();
	protected:
		void LoadWidget(const Asset::Guid widgetAssetGuid);
		[[nodiscard]] Optional<Input::Monitor*> GetInputMonitorInternal() const;

		void OnShowCursor(const Input::DeviceIdentifier);
		void OnHideCursor(const Input::DeviceIdentifier);

		virtual void OnPause(const Input::DeviceIdentifier) = 0;
		void OnShareAction(Widgets::Widget& requestingWidget, const ArrayView<const UnicodeString> sources);
		void OnInviteAction(Widgets::Widget& requestingWidget, const ArrayView<const UnicodeString> sources);

		void SubscribeToUIEvents();
		void UnsubscribeFromUIEvents();

		virtual void OnWidgetLoaded();

		[[nodiscard]] bool IsActive() const
		{
			return m_pSceneView.IsValid();
		}
	protected:
		Optional<Widgets::Document::Scene3D*> m_sceneWidget;
		Optional<Widgets::Widget*> m_pPlayModeWidget;
		Optional<Rendering::SceneView*> m_pSceneView;
		Optional<Entity::CameraComponent*> m_pSpectatorCamera{nullptr};

		Input::Actions::Event m_showCursorAction;
		Input::Actions::Event m_clickViewAction;
		Input::Actions::Event m_pauseAction;

		ClientIdentifier m_clientIdentifier;

		DataSource::PropertyIdentifier m_exitGameplayTextPropertyIdentifier;
		DataSource::PropertyIdentifier m_remixTextPropertyIdentifier;
		DataSource::PropertyIdentifier m_remixIconPropertyIdentifier;
		DataSource::PropertyIdentifier m_isRemixingPropertyIdentifier;
		DataSource::PropertyIdentifier m_isNotRemixingPropertyIdentifier;
	};

	//! Play mode in a pure playing context
	struct PlayViewMode final : public PlayViewModeBase
	{
		using BaseType = PlayViewModeBase;

		static constexpr Guid TypeGuid = "f646736a-e8fc-917c-10b5-9c3cbf7a392f"_guid;
		PlayViewMode(Widgets::Document::Scene3D& sceneWidget, const ClientIdentifier clientIdentifier);
		virtual ~PlayViewMode();

		virtual void OnPause(const Input::DeviceIdentifier) override;
		virtual void OnActivated(Rendering::SceneViewBase&) override;
		virtual void OnDeactivated(const Optional<SceneBase*> pScene, Rendering::SceneViewBase& sceneView) override;

		virtual Guid GetTypeGuid() const override
		{
			return TypeGuid;
		}
	protected:
		void ReturnToExplore();
	};
}
