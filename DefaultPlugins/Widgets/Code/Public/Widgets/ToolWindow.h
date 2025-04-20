#pragma once

#include <Renderer/Window/Window.h>
#include "Widget.h"
#include <Renderer/RenderOutput/SwapchainOutput.h>

#include <Widgets/Style/ReferenceValue.h>
#include <Widgets/Style/Point.h>

#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Framegraph/Framegraph.h>

#include <Engine/Entity/ComponentSoftReference.h>

namespace ngine::Entity
{
	struct Component2D;
	struct SceneRegistry;
	struct RootComponent;
}

namespace ngine::Widgets
{
	struct Scene;
	struct RootWidget;
	struct DraggedWidget;
	struct UIViewMode;
}

namespace ngine::Widgets::Events
{
	using namespace ngine::Events;
}

namespace ngine::Widgets::Document
{
	struct WidgetDocument;
}

namespace ngine::GameFramework
{
	// TODO: Remove when we fix virtual controller to be implemented as a widget
	struct VirtualController;
}

namespace ngine::Rendering
{
	struct SceneWindow;
	struct SceneDataBase;
	struct FrameImageId;
	struct Stage;
	struct StartFrameStage;
	struct PresentStage;
	struct DraggedItemsWidget;
	struct RenderTargetCache;
	struct QueuedFramegraphBuilderWidget;

	struct PointerDevice
	{
		const Input::DeviceIdentifier m_deviceIdentifier;
		Rendering::CursorType m_cursor = Rendering::CursorType::Arrow;
		Entity::ComponentSoftReference m_hoverFocusWidget;
		WindowCoordinate m_coordinate{Math::Zero};

		void SetHoverFocusWidget(const Optional<Widgets::Widget*> pWidget, Entity::SceneRegistry& sceneRegistry);
	};

	struct ToolWindow : public Window, private Rendering::SwapchainOutput
	{
		using Widget = Widgets::Widget;

		ToolWindow(Initializer&& initializer);
		ToolWindow(const ToolWindow&) = delete;
		ToolWindow& operator=(const ToolWindow&) = delete;
		ToolWindow(ToolWindow&& other) = delete;
		ToolWindow& operator=(ToolWindow&&) = delete;
		virtual ~ToolWindow();

		void Initialize(Threading::JobBatch& jobBatch);

		[[nodiscard]] Widgets::RootWidget& GetRootWidget() const;

		[[nodiscard]] Rendering::Framegraph& GetFramegraph()
		{
			return m_framegraph;
		}

		// SwapchainOutput
		[[nodiscard]] virtual Optional<FrameImageId> AcquireNextImage(
			const LogicalDeviceView logicalDevice,
			const uint64 imageAvailabilityTimeoutNanoseconds,
			const SemaphoreView imageAvailableSemaphore,
			const FenceView fence
		) override final;
		// ~SwapchainOutput

		[[nodiscard]] SwapchainOutput& GetOutput()
		{
			return *this;
		}

		// Window
		[[nodiscard]] virtual bool
		OnStartDragItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> items) override;
		virtual void OnCancelDragItemsIntoWindow() override;
		[[nodiscard]] virtual bool
		OnDragItemsOverWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> items) override;
		[[nodiscard]] virtual bool
		OnDropItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> items) override;
		// ~Window

		[[nodiscard]] virtual const Entity::SceneRegistry& GetEntitySceneRegistry() const = 0;
		[[nodiscard]] virtual Entity::SceneRegistry& GetEntitySceneRegistry() = 0;
		[[nodiscard]] virtual const Widgets::Scene& GetScene() const = 0;
		[[nodiscard]] virtual Widgets::Scene& GetScene() = 0;

		void InvalidateFramegraph();

		[[nodiscard]] DescriptorSetLayoutView GetSampledImageDescriptorLayout() const
		{
			return m_sampledImageDescriptorLayout;
		}

		void SetInputFocusWidget(const Optional<Widget*> pWidget);
		[[nodiscard]] Optional<Widget*> GetInputFocusWidget() const;
		void SetActiveFocusWidget(const Optional<Widget*> pWidget);
		[[nodiscard]] Optional<Widget*> GetActiveFocusWidget() const;
		void SetHoverFocusWidget(const Optional<Widget*> pWidget);
		[[nodiscard]] Optional<Widget*> GetHoverFocusWidget() const;

		[[nodiscard]] PointerDevice& GetPointerDevice()
		{
			return m_pointerDevice;
		}
	protected:
		friend struct SceneWindow;
		friend struct Window;
		friend Widgets::Widget;
		friend Widgets::DraggedWidget;
		friend Widgets::UIViewMode;
		friend GameFramework::VirtualController;

		// Window
		virtual void SetInputFocusAtCoordinate(WindowCoordinate, [[maybe_unused]] const Optional<uint16> touchRadius) override;

		virtual void OnSwitchToForegroundInternal() override final;
		virtual void OnSwitchToBackgroundInternal() override final;

		[[nodiscard]] virtual bool DidWindowSizeChange() const override final;
		virtual bool CanStartResizing() const override final;
		virtual void OnStartResizing() override final;
		virtual void OnFinishedResizing() override final;
		virtual void WaitForProcessingFramesToFinish() override final;

		virtual bool HasTextInInputFocus() const override;
		virtual void InsertTextIntoInputFocus(const ConstUnicodeStringView text) override final;
		virtual void DeleteTextInInputFocusBackwards() override final;

		virtual void OnDisplayPropertiesChanged() override final;
		// ~Window

		virtual void OnReceivedMouseFocus() override final;
		virtual void OnLostMouseFocus() override final;
		virtual void OnReceivedKeyboardFocusInternal() override final;
		virtual void OnLostKeyboardFocusInternal() override final;
		[[nodiscard]] virtual Optional<Input::Monitor*>
		GetMouseMonitorAtCoordinates(const WindowCoordinate coordinates, [[maybe_unused]] const Optional<uint16> touchRadius) override final;
		[[nodiscard]] virtual Optional<Input::Monitor*> GetFocusedInputMonitor() override final;
	private:
		void NotifySwitchToBackgroundRecursive(Widget& widget);
		void NotifySwitchToForegroundRecursive(Widget& widget);

		void EnableFramegraph();
		void DisableFramegraph();
		void InvalidateFramegraph(const ArrayView<const QueuedFramegraphBuilderWidget> queuedFramegraphBuilderWidgets);
		void BuildFramegraph(const ArrayView<const QueuedFramegraphBuilderWidget> queuedFramegraphBuilderWidgets);
	protected:
		friend Widgets::Widget;
		friend DraggedItemsWidget;

		DescriptorSetLayout m_sampledImageDescriptorLayout;

		// TODO: Extend to support multiple?
		PointerDevice m_pointerDevice;

		Entity::ComponentSoftReference m_activeFocusWidget;
		Entity::ComponentSoftReference m_inputFocusWidget;

		Rendering::Framegraph m_framegraph;
	};
}
