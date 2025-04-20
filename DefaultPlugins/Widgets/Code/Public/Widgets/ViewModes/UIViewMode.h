#pragma once

#include <Renderer/Scene/SceneViewModeBase.h>
#include <Renderer/Window/CursorType.h>

#include <Engine/Input/Actions/Surface/TapAction.h>
#include <Engine/Input/Actions/Surface/HoverAction.h>
#include <Engine/Input/Actions/Surface/DoubleTapAction.h>
#include <Engine/Input/Actions/Surface/LongPressAction.h>
#include <Engine/Input/Actions/Surface/PanAction.h>
#include <Engine/Input/Actions/Surface/ScrollAction.h>
#include <Engine/Input/Actions/TextInputAction.h>
#include <Engine/Input/Actions/CopyPasteAction.h>
#include <Engine/Input/Actions/TabAction.h>
#include <Engine/Input/Actions/EventAction.h>
#include <Engine/Input/WindowCoordinate.h>

#include <Common/Memory/UniqueRef.h>

namespace ngine::Rendering
{
	struct SceneView2D;
}

namespace ngine::Input
{
	struct ActionMonitor;
}

namespace ngine::Widgets
{
	struct Widget;

	struct UIViewMode;

	struct ActionMonitor;

	struct CommonInputActions
	{
		CommonInputActions(Rendering::SceneView2D& sceneView);

		[[nodiscard]] Rendering::SceneView2D& GetSceneView() const
		{
			return m_sceneView;
		}

		void OnStartTap(const Input::DeviceIdentifier, const ScreenCoordinate, const uint8 fingerCount, const Optional<uint16> touchRadius);
		void OnEndTap(const Input::DeviceIdentifier, const ScreenCoordinate, const uint8 fingerCount, const Optional<uint16> touchRadius);
		void OnCancelTap(const Input::DeviceIdentifier);
		void OnDoubleTap(const Input::DeviceIdentifier, const ScreenCoordinate, const Optional<uint16> touchRadius);
		void
		OnStartLongPress(const Input::DeviceIdentifier, const ScreenCoordinate, const uint8 fingerCount, const Optional<uint16> touchRadius);
		void
		OnMoveLongPress(const Input::DeviceIdentifier, const ScreenCoordinate, const uint8 fingerCount, const Optional<uint16> touchRadius);
		void OnEndLongPress(const Input::DeviceIdentifier, const ScreenCoordinate, const uint8 fingerCount, const Optional<uint16> touchRadius);
		void OnCancelLongPress(const Input::DeviceIdentifier);
		void OnHover(const Input::DeviceIdentifier, const ScreenCoordinate, Math::Vector2i deltaCoordinate);
		void OnStartPan(
			const Input::DeviceIdentifier,
			const ScreenCoordinate,
			const uint8 fingerCount,
			const Math::Vector2f velocity,
			const Optional<uint16> touchRadius
		);
		void OnMovePan(
			const Input::DeviceIdentifier,
			const ScreenCoordinate,
			const Math::Vector2i deltaCoordinate,
			const uint8 fingerCount,
			const Math::Vector2f velocity,
			const Optional<uint16> touchRadius
		);
		void OnEndPan(const Input::DeviceIdentifier, const ScreenCoordinate, const Math::Vector2f velocity);
		void OnCancelPan(const Input::DeviceIdentifier);
		void OnStartScroll(const Input::DeviceIdentifier, const ScreenCoordinate, const Math::Vector2i delta);
		void OnScroll(const Input::DeviceIdentifier, const ScreenCoordinate, const Math::Vector2i delta);
		void OnEndScroll(const Input::DeviceIdentifier, const ScreenCoordinate, const Math::Vector2f velocity);
		void OnCancelScroll(const Input::DeviceIdentifier, const ScreenCoordinate);
		void OnTextInput(const ConstUnicodeStringView text);
		void OnCopy();
		void OnPaste();
		void OnMoveTextCursor(const EnumFlags<Input::MoveTextCursorFlags>);
		void OnApplyTextInput();
		void OnAbortTextInput();
		void OnDeleteTextInput(Input::DeleteTextType);
		void OnTabBack(const Input::TabAction::Mode mode);
		void OnTabForward(const Input::TabAction::Mode mode);
	protected:
		Rendering::SceneView2D& m_sceneView;
	};

	//! Scene view mode for viewing / using the UI
	struct UIViewMode final : public SceneViewModeBase, public CommonInputActions
	{
		using BaseType = SceneViewModeBase;

		static constexpr Guid TypeGuid = "C561ECD2-7C51-4C62-BF64-194BCADF0CF2"_guid;
		UIViewMode(Rendering::SceneView2D& sceneView);
		UIViewMode(const UIViewMode&) = delete;
		UIViewMode(UIViewMode&&) = delete;
		UIViewMode& operator=(const UIViewMode&) = delete;
		UIViewMode& operator=(UIViewMode&&) = delete;
		virtual ~UIViewMode();

		// SceneViewModeBase
		virtual Guid GetTypeGuid() const override
		{
			return TypeGuid;
		}

		virtual void OnActivated(Rendering::SceneViewBase&) override;
		virtual void OnDeactivated(const Optional<SceneBase*>, Rendering::SceneViewBase&) override;

		[[nodiscard]] virtual Optional<Input::Monitor*> GetInputMonitor() const override;
		// ~SceneViewModeBase
	protected:
		friend ActionMonitor;
		void OnMonitorLostMouseDeviceFocus();

		void OnPause(const Input::DeviceIdentifier);
	protected:
		UniqueRef<ActionMonitor> m_pActionMonitor;

		Input::TapAction m_tapAction;
		Input::DoubleTapAction m_doubleTapAction;
		Input::LongPressAction m_longPressAction;
		Input::PanAction m_panAction;
		Input::ScrollAction m_scrollAction;
		Input::TextInputAction m_textInputAction;
		Input::CopyPasteAction m_copyPasteAction;
		Input::TabAction m_tabAction;
		Input::HoverAction m_hoverAction;

		Input::Actions::Event m_pauseAction;
	};
}
