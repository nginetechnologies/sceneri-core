#include <Widgets/ToolWindow.h>
#include <Widgets/RootWidget.h>
#include <Widgets/Widget.inl>
#include <Widgets/WidgetScene.h>
#include <Widgets/Style/Entry.h>
#include <Widgets/SetCursorCallback.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Documents/WidgetDocument.h>
#include <Widgets/ViewModes/UIViewMode.h>
#include <Data/Layout.h>
#include <Engine/DataSource/PropertySourceInterface.h>
#include <Engine/DataSource/PropertySourceIdentifier.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Renderer.h>

#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Project/Project.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/Data/QuadtreeNode.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Data/WorldTransform2D.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Context/EventManager.inl>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/PresentStage.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Framegraph/FramegraphBuilder.h>
#include <Renderer/Scene/SceneViewMode.h>

#include <AssetCompilerCore/Plugin.h>
#include <DeferredShading/FSR/fsr_settings.h>

#include <Common/Math/Color.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Math/Floor.h>
#include <Common/Math/Vector2/Ceil.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Math/Primitives/Serialization/RectangleEdges.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Asset/Asset.h>
#include <Common/Asset/Picker.h>
#include <Common/Asset/AssetOwners.h>
#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/Asset/FolderAssetType.h>
#include <Common/System/Query.h>

namespace ngine::Rendering
{
	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> SampledImageDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(0, ShaderStage::Fragment, SampledImageType::Float, ImageMappingType::TwoDimensional),
		DescriptorSetLayout::Binding::MakeSampler(1, ShaderStage::Fragment, SamplerBindingType::Filtering),
	};

	ToolWindow::ToolWindow(Initializer&& initializer)
		: Window(Forward<Initializer>(initializer))
		, SwapchainOutput(
				GetClientAreaSize(),
				Array<Format, 2>{Format::R8G8B8A8_UNORM_PACK8, Format::B8G8R8A8_UNORM}.GetDynamicView(),
				GetLogicalDevice(),
				GetSurface(),
				UsageFlags::ColorAttachment | UsageFlags::Storage,
				this
			)
		, m_sampledImageDescriptorLayout(GetLogicalDevice(), SampledImageDescriptorBindings)
		, m_framegraph(GetLogicalDevice(), GetOutput())
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		m_sampledImageDescriptorLayout.SetDebugName(GetLogicalDevice(), "Sampled Image");
#endif
	}

	ToolWindow::~ToolWindow()
	{
		LogicalDevice& logicalDevice = GetLogicalDevice();
		SwapchainOutput::Destroy(logicalDevice);
	}

	Widgets::RootWidget& ToolWindow::GetRootWidget() const
	{
		return GetScene().GetRootWidget();
	}

	void ToolWindow::Initialize(Threading::JobBatch&)
	{
		Window::Initialize();

		// Workaround for latest (21.0 dev) clang broken warning
		PUSH_CLANG_WARNINGS
		DISABLE_CLANG_WARNING("-Wunused-result")
		SetTitle(System::Get<Engine>().GetInfo().NativeName);
		POP_CLANG_WARNINGS

		if (IsVisible())
		{
			OnBecomeVisible();
		}
	}

	struct QueuedFramegraphBuilderWidget
	{
		ReferenceWrapper<Widgets::Widget> widget;
		float depth;
	};
	using QueuedFramegraphBuilderWidgets = InlineVector<QueuedFramegraphBuilderWidget, 1>;
	void TraverseFramegraphBuilderWidgets(Widgets::Widget& widget, QueuedFramegraphBuilderWidgets& queuedFramegraphBuilderWidgets)
	{
		const EnumFlags<Widgets::WidgetFlags> widgetFlags = widget.GetFlags();
		if (widgetFlags.AreNoneSet(
					Widgets::WidgetFlags::IsHidden | Widgets::WidgetFlags::IsHiddenFromStyle | Widgets::WidgetFlags::IsIgnoredFromAnySource
				))
		{
			if (widgetFlags.IsSet(Widgets::WidgetFlags::HasCustomFramegraph))
			{
				const float depth = widget.GetDepthRatio();
				QueuedFramegraphBuilderWidget* pIt = std::lower_bound(
					queuedFramegraphBuilderWidgets.begin().Get(),
					queuedFramegraphBuilderWidgets.end().Get(),
					depth,
					[](const QueuedFramegraphBuilderWidget& existingElement, const float newDepth) -> bool
					{
						return newDepth > existingElement.depth;
					}
				);
				queuedFramegraphBuilderWidgets.Emplace(pIt, Memory::Uninitialized, QueuedFramegraphBuilderWidget{widget, depth});
			}

			for (Widgets::Widget& child : widget.GetChildren())
			{
				TraverseFramegraphBuilderWidgets(child, queuedFramegraphBuilderWidgets);
			}
		}
	}

	void ToolWindow::InvalidateFramegraph(const ArrayView<const QueuedFramegraphBuilderWidget> queuedFramegraphBuilderWidgets)
	{
		for (const QueuedFramegraphBuilderWidget& queuedFramegraphBuilderWidget : queuedFramegraphBuilderWidgets)
		{
			queuedFramegraphBuilderWidget.widget->OnFramegraphInvalidated();
		}
	}

	void ToolWindow::BuildFramegraph(const ArrayView<const QueuedFramegraphBuilderWidget> queuedFramegraphBuilderWidgets)
	{
		UniquePtr<FramegraphBuilder> pFramegraphBuilder{Memory::ConstructInPlace};
		FramegraphBuilder& framegraphBuilder = *pFramegraphBuilder;

		// Build the framegraph
		for (const QueuedFramegraphBuilderWidget& queuedFramegraphBuilderWidget : queuedFramegraphBuilderWidgets)
		{
			queuedFramegraphBuilderWidget.widget->BuildFramegraph(framegraphBuilder);
		}

		m_framegraph.Reset();

		Threading::JobBatch jobBatch;
		m_framegraph.Compile(framegraphBuilder.GetStages(), jobBatch);

		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(jobBatch);
		}

		for (const QueuedFramegraphBuilderWidget& queuedFramegraphBuilderWidget : queuedFramegraphBuilderWidgets)
		{
			queuedFramegraphBuilderWidget.widget->OnFramegraphBuilt();
		}
	}

	void ToolWindow::InvalidateFramegraph()
	{
		System::Get<Engine>().ModifyFrameGraph(
			[this]()
			{
				const bool wasFramegraphActive = m_framegraph.IsEnabled();
				if (wasFramegraphActive)
				{
					DisableFramegraph();
				}

				// Collect widgets that want to contribute to building this framegraph
				QueuedFramegraphBuilderWidgets queuedFramegraphBuilderWidgets;
				TraverseFramegraphBuilderWidgets(GetRootWidget(), queuedFramegraphBuilderWidgets);

				if (queuedFramegraphBuilderWidgets.HasElements())
				{
					InvalidateFramegraph(queuedFramegraphBuilderWidgets);

					BuildFramegraph(queuedFramegraphBuilderWidgets);

					if (wasFramegraphActive)
					{
						EnableFramegraph();
					}
				}
				else
				{
					m_framegraph.Reset();
				}
			}
		);
	}

	void ToolWindow::EnableFramegraph()
	{
		System::Get<Engine>().ModifyFrameGraph(
			[this]()
			{
				Rendering::Framegraph& framegraph = m_framegraph;
				if (framegraph.IsEnabled())
				{
					return;
				}

				framegraph.Enable();

				QueuedFramegraphBuilderWidgets queuedFramegraphBuilderWidgets;
				for (Widgets::Widget& child : GetRootWidget().GetChildren())
				{
					TraverseFramegraphBuilderWidgets(child, queuedFramegraphBuilderWidgets);
				}
				for (const QueuedFramegraphBuilderWidget& framegraphBuilder : queuedFramegraphBuilderWidgets)
				{
					framegraphBuilder.widget->OnEnableFramegraph();
				}
			}
		);
	}

	void ToolWindow::DisableFramegraph()
	{
		System::Get<Engine>().ModifyFrameGraph(
			[this]()
			{
				Rendering::Framegraph& framegraph = m_framegraph;
				if (!m_framegraph.IsEnabled())
				{
					return;
				}

				framegraph.Disable();

				QueuedFramegraphBuilderWidgets queuedFramegraphBuilderWidgets;
				for (Widgets::Widget& child : GetRootWidget().GetChildren())
				{
					TraverseFramegraphBuilderWidgets(child, queuedFramegraphBuilderWidgets);
				}
				for (const QueuedFramegraphBuilderWidget& framegraphBuilder : queuedFramegraphBuilderWidgets)
				{
					framegraphBuilder.widget->OnDisableFramegraph();
				}
			}
		);
	}

	bool ToolWindow::DidWindowSizeChange() const
	{
		return (SwapchainOutput::GetOutputArea().GetSize() != m_clientAreaSize).AreAnySet();
	}

	bool ToolWindow::CanStartResizing() const
	{
		if (!Window::CanStartResizing())
		{
			return false;
		}

		if (m_framegraph.HasPendingCompilationTasks())
		{
			return false;
		}

		return true;
	}

	void ToolWindow::OnStartResizing()
	{
		if (m_framegraph.IsEnabled())
		{
			m_framegraph.WaitForProcessingFramesToFinish(Rendering::AllFramesMask);
			DisableFramegraph();
		}

		GetRootWidget().Hide();

		m_framegraph.OnBeforeRenderOutputResize();

#if PLATFORM_ANDROID
		SwapchainOutput::Destroy(GetLogicalDevice());
#endif

		// Notify all widgets that we are resizing
		using NotifyWidget = void (*)(Widget& widget);
		static NotifyWidget notifyWidget = [](Widget& widget)
		{
			widget.OnBeforeContentAreaChanged(Widget::ContentAreaChangeFlags::SizeChanged);
			for (Widget& child : widget.GetChildren())
			{
				notifyWidget(child);
			}
		};
		notifyWidget(GetRootWidget());
	}

	void ToolWindow::WaitForProcessingFramesToFinish()
	{
		if (m_framegraph.IsEnabled())
		{
			m_framegraph.WaitForProcessingFramesToFinish(Rendering::AllFramesMask);
		}
	}

	void ToolWindow::OnFinishedResizing()
	{
		[[maybe_unused]] const bool createdSwapchain = SwapchainOutput::RecreateSwapchain(
			GetLogicalDevice(),
			GetSurface(),
			GetClientAreaSize(),
			UsageFlags::ColorAttachment | UsageFlags::Storage
		);
		Assert(createdSwapchain);

		const Math::Rectanglei clientArea = GetLocalClientArea();
		if (clientArea != GetRootWidget().GetContentArea())
		{
			// Update root widget size
			Widget& rootWidget = GetRootWidget();
			Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
			Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
			Entity::Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(rootWidget.GetIdentifier(
			));
			worldTransform.SetLocation((Math::Vector2f)clientArea.GetPosition());
			worldTransform.SetScale((Math::Vector2f)clientArea.GetSize());

			// Make sure we can't remove widgets while recalculating hierarchy
			System::Get<Engine>().ModifyFrameGraph(
				[this]()
				{
					// Collect widgets that want to contribute to building this framegraph
					QueuedFramegraphBuilderWidgets queuedFramegraphBuilderWidgets;
					for (Widgets::Widget& child : GetRootWidget().GetChildren())
					{
						TraverseFramegraphBuilderWidgets(child, queuedFramegraphBuilderWidgets);
					}

					if (queuedFramegraphBuilderWidgets.HasElements())
					{
						InvalidateFramegraph(queuedFramegraphBuilderWidgets);
					}

					GetRootWidget().QueueRecalculateWidgetHierarchy(GetRootWidget());

					GetRootWidget().ProcessWidgetHierarchyRecalculation();

					GetRootWidget().MakeVisible();

					GetRootWidget().ProcessWidgetHierarchyRecalculation();

					if (queuedFramegraphBuilderWidgets.HasElements())
					{
						BuildFramegraph(queuedFramegraphBuilderWidgets);
					}

					if (IsInForeground())
					{
						EnableFramegraph();
					}
				}
			);
		}
		else
		{
			if (IsInForeground())
			{
				EnableFramegraph();
			}
			GetRootWidget().MakeVisible();
		}
	}

	void PointerDevice::SetHoverFocusWidget(const Optional<Widgets::Widget*> pWidget, Entity::SceneRegistry& sceneRegistry)
	{
		Widgets::Widget* pPreviousFocusWidget = m_hoverFocusWidget.Find<Widgets::Widget>(sceneRegistry);
		if (pPreviousFocusWidget == pWidget)
		{
			return;
		}

		m_hoverFocusWidget = Entity::ComponentSoftReference{pWidget, sceneRegistry};

		if (pPreviousFocusWidget != nullptr)
		{
			pPreviousFocusWidget->OnLostHoverFocusInternal(sceneRegistry);
		}

		if (pWidget != nullptr)
		{
			pWidget->OnReceivedHoverFocusInternal(sceneRegistry);
		}
	}

	void ToolWindow::SetHoverFocusWidget(const Optional<Widget*> pWidget)
	{
		m_pointerDevice.SetHoverFocusWidget(pWidget, GetEntitySceneRegistry());
	}

	Optional<Widgets::Widget*> ToolWindow::GetHoverFocusWidget() const
	{
		return m_pointerDevice.m_hoverFocusWidget.Find<Widgets::Widget>(GetEntitySceneRegistry());
	}

	void ToolWindow::SetInputFocusWidget(const Optional<Widget*> pWidget)
	{
		Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
		Widgets::Widget* pPreviousFocusWidget = m_inputFocusWidget.Find<Widget>(sceneRegistry);
		if (pPreviousFocusWidget == pWidget)
		{
			return;
		}

		m_inputFocusWidget = Entity::ComponentSoftReference{pWidget, sceneRegistry};

		// Start by setting input flags so callbacks can see all correctly set in one go
		if (pPreviousFocusWidget != nullptr)
		{
			pPreviousFocusWidget->ClearHasInputFocus();
		}
		if (pWidget != nullptr)
		{
			pWidget->SetHasInputFocus();
		}

		// Now notify
		if (pPreviousFocusWidget != nullptr)
		{
			pPreviousFocusWidget->OnLostInputFocusInternal(sceneRegistry);
		}
		if (pWidget != nullptr)
		{
			pWidget->OnReceivedInputFocusInternal(sceneRegistry);
		}
	}

	Optional<Widgets::Widget*> ToolWindow::GetInputFocusWidget() const
	{
		return m_inputFocusWidget.Find<Widgets::Widget>(GetEntitySceneRegistry());
	}

	void ToolWindow::SetActiveFocusWidget(const Optional<Widget*> pWidget)
	{
		Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
		Widgets::Widget* pPreviousFocusWidget = m_activeFocusWidget.Find<Widget>(sceneRegistry);
		if (pPreviousFocusWidget == pWidget)
		{
			return;
		}

		m_activeFocusWidget = Entity::ComponentSoftReference{pWidget, sceneRegistry};
		if (pPreviousFocusWidget != nullptr)
		{
			pPreviousFocusWidget->OnLostActiveFocusInternal(sceneRegistry);
		}

		if (pWidget != nullptr)
		{
			pWidget->OnReceivedActiveFocusInternal(sceneRegistry);
		}
	}

	Optional<Widgets::Widget*> ToolWindow::GetActiveFocusWidget() const
	{
		return m_activeFocusWidget.Find<Widgets::Widget>(GetEntitySceneRegistry());
	}

	Optional<Input::Monitor*> ToolWindow::GetMouseMonitorAtCoordinates(const WindowCoordinate coordinates, const Optional<uint16> touchRadius)
	{
		if (Widgets::Widget* pFocusedWidget = GetRootWidget().GetWidgetAtCoordinate(coordinates, touchRadius))
		{
			if (Input::Monitor* pMonitor = pFocusedWidget->GetFocusedInputMonitorAtCoordinates(coordinates))
			{
				return pMonitor;
			}
		}

		Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
		if (const Optional<Widgets::Document::WidgetDocument*> pRootWidgetDocument = GetRootWidget().FindFirstChildImplementingType<Widgets::Document::WidgetDocument>(sceneRegistry))
		{
			if (const Optional<SceneViewModeBase*> pSceneViewMode = pRootWidgetDocument->GetSceneView().GetCurrentMode())
			{
				return pSceneViewMode->GetInputMonitor();
			}
		}
		return {};
	}

	Optional<Input::Monitor*> ToolWindow::GetFocusedInputMonitor()
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			if (Input::Monitor* pMonitor = pInputFocusWidget->GetFocusedInputMonitor())
			{
				return pMonitor;
			}
		}

		Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
		if (const Optional<Widgets::Document::WidgetDocument*> pRootWidgetDocument = GetRootWidget().FindFirstChildImplementingType<Widgets::Document::WidgetDocument>(sceneRegistry))
		{
			if (const Optional<SceneViewModeBase*> pSceneViewMode = pRootWidgetDocument->GetSceneView().GetCurrentMode())
			{
				return pSceneViewMode->GetInputMonitor();
			}
		}
		return {};
	}

	void ToolWindow::OnReceivedMouseFocus()
	{
		// Wait for mouse event to give focus to the widget in focus
	}

	void ToolWindow::OnLostMouseFocus()
	{
		m_pointerDevice.SetHoverFocusWidget(nullptr, GetEntitySceneRegistry());
	}

	void ToolWindow::OnReceivedKeyboardFocusInternal()
	{
		Window::OnReceivedKeyboardFocusInternal();

		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			pInputFocusWidget->SetHasInputFocus();
			pInputFocusWidget->OnReceivedInputFocusInternal(GetEntitySceneRegistry());
		}
	}

	void ToolWindow::OnLostKeyboardFocusInternal()
	{
		Window::OnLostKeyboardFocusInternal();

		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			pInputFocusWidget->ClearHasInputFocus();
			pInputFocusWidget->OnLostInputFocusInternal(GetEntitySceneRegistry());
		}
	}

	bool ToolWindow::HasTextInInputFocus() const
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			return pInputFocusWidget->HasEditableText();
		}
		return false;
	}

	void ToolWindow::InsertTextIntoInputFocus(const ConstUnicodeStringView text)
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			pInputFocusWidget->HandleTextInput(text);
		}
	}

	void ToolWindow::DeleteTextInInputFocusBackwards()
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			pInputFocusWidget->HandleDeleteTextInput(Input::DeleteTextType::LeftCharacter);
		}
	}

	struct DraggedItemsWidget final : public Widgets::Widget
	{
		static constexpr Guid TypeGuid = "9646ea22-814f-492c-92b0-d72830c345c6"_guid;
		using BaseType = Widgets::Widget;

		DraggedItemsWidget(
			Rendering::ToolWindow& owningWindow,
			const ArrayView<const Widgets::DragAndDropData> draggedItems,
			const WindowCoordinate windowCoordinate
		)
			: Widget(owningWindow.GetRootWidget(), (Coordinate)windowCoordinate, Widget::Flags::IsInputDisabled)
			, m_dragInfo(draggedItems)
		{
		}
		virtual ~DraggedItemsWidget() = default;

		virtual Memory::CallbackResult IterateAttachedItems(
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			const ArrayView<const Reflection::TypeDefinition> allowedTypes,
			CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
		) const override
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();

			for (Widgets::DragAndDropData& draggedItem : m_dragInfo)
			{
				const Reflection::TypeDefinition draggedItemType = draggedItem.GetActiveType();
				if (const Optional<const IO::Path*> path = draggedItem.Get<IO::Path>())
				{
					// Automatically replace references to existing assets with an asset reference
					if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
					{
						const Guid existingAssetGuid = assetManager.GetAssetGuid(*path);
						if (existingAssetGuid.IsValid())
						{
							const Asset::Reference assetReference{existingAssetGuid, assetManager.GetAssetTypeGuid(existingAssetGuid)};
							if (callback(assetReference) == Memory::CallbackResult::Break)
							{
								return Memory::CallbackResult::Break;
							}
							continue;
						}
					}
					else if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::LibraryReference>()))
					{
						Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
						const Guid existingAssetGuid = assetLibrary.GetAssetGuid(*path);
						if (existingAssetGuid.IsValid())
						{
							const Asset::LibraryReference assetReference{existingAssetGuid, assetLibrary.GetAssetTypeGuid(existingAssetGuid)};
							if (callback(assetReference) == Memory::CallbackResult::Break)
							{
								return Memory::CallbackResult::Break;
							}
							continue;
						}
					}
				}

				if (allowedTypes.Contains(draggedItemType))
				{
					if (callback(draggedItem.Get()) == Memory::CallbackResult::Break)
					{
						return Memory::CallbackResult::Break;
					}
				}
				else if (draggedItemType == Reflection::TypeDefinition::Get<IO::Path>())
				{
					if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
					{
						IO::Path draggedItemPath = *draggedItem.Get<IO::Path>();
						if (draggedItemPath.GetRightMostExtension() == Asset::Asset::FileExtension)
						{
							IO::Path assetFilePath = draggedItemPath;
							if (draggedItemPath.IsDirectory())
							{
								IO::Path filePath = IO::Path::Combine(assetFilePath, assetFilePath.GetFileName());
								if (filePath.Exists())
								{
									assetFilePath = Move(filePath);
								}
								else
								{
									assetFilePath = IO::Path::Combine(assetFilePath, IO::Path::Merge(MAKE_PATH("Main"), assetFilePath.GetAllExtensions()));
								}
							}

							// Check if the asset exists in the asset database from the specified path
							if (const Asset::Guid existingAssetGuid = assetManager.GetAssetGuid(assetFilePath); existingAssetGuid.IsValid())
							{
								Guid assetTypeGuid = assetManager.GetAssetTypeGuid(existingAssetGuid);
								if (assetTypeGuid.IsValid())
								{
									draggedItem = Widgets::DragAndDropData{Asset::Reference{existingAssetGuid, assetTypeGuid}};
									if (callback(draggedItem.Get()) == Memory::CallbackResult::Break)
									{
										return Memory::CallbackResult::Break;
									}
									continue;
								}
							}
							else
							{
								Serialization::Data data(assetFilePath);
								if (LIKELY(data.IsValid()))
								{
									Asset::Asset asset(data, IO::Path(assetFilePath));
									if (asset.IsValid())
									{
										// Check if the asset exists in the asset database at a different path
										if (assetManager.HasAsset(asset.GetGuid()))
										{
											draggedItem = Widgets::DragAndDropData{Asset::Reference{asset.GetGuid(), asset.GetTypeGuid()}};
											if (callback(draggedItem.Get()) == Memory::CallbackResult::Break)
											{
												return Memory::CallbackResult::Break;
											}
											continue;
										}

										Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
										Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

										// Check if the asset exists in the asset library
										Guid assetTypeGuid = assetLibrary.GetAssetTypeGuid(asset.GetGuid());
										if (assetTypeGuid.IsValid())
										{
											Asset::Identifier assetIdentifier;
											const Asset::Identifier libraryAssetIdentifier = assetLibrary.GetAssetIdentifier(asset.GetGuid());
											if (assetLibrary
											      .IsTagSet(tagRegistry.FindOrRegister(Asset::Library::LocalAssetDatabaseTagGuid), libraryAssetIdentifier))
											{
												// Assets from the local asset database should be copied instead of imported
												AssetCompiler::Plugin& assetCompiler = *System::FindPlugin<AssetCompiler::Plugin>();
												Project& currentProject = System::Get<Project>();
												Engine& engine = System::Get<Engine>();
												const IO::Path existingAssetPath = assetLibrary.GetAssetPath(asset.GetGuid());
												Asset::Owners sourceAssetOwners(existingAssetPath, Asset::Context{EngineInfo(engine.GetInfo())});
												[[maybe_unused]] const bool wasCopied = assetCompiler.CopyAsset(
													existingAssetPath,
													sourceAssetOwners.m_context,
													IO::Path::Combine(
														currentProject.GetInfo()->GetDirectory(),
														currentProject.GetInfo()->GetRelativeAssetDirectory(),
														existingAssetPath.GetFileName()
													),
													Asset::Context{ProjectInfo(*currentProject.GetInfo()), EngineInfo(engine.GetInfo())},
													Serialization::SavingFlags{}
												);
												Assert(wasCopied);
												assetIdentifier = assetManager.GetAssetIdentifier(asset.GetGuid());
											}
											else
											{
												// Attempt to import into the asset library
												assetIdentifier = assetManager.Import(Asset::LibraryReference{asset.GetGuid(), assetTypeGuid});
											}
											if (assetIdentifier.IsValid())
											{
												draggedItem = Widgets::DragAndDropData{Asset::Reference{asset.GetGuid(), assetTypeGuid}};
												if (callback(draggedItem.Get()) == Memory::CallbackResult::Break)
												{
													break;
												}
												continue;
											}
										}

										// Import the asset into the asset library
										const Asset::Identifier libraryAssetIdentifier = assetLibrary.ImportAsset(asset.GetGuid(), IO::Path(draggedItemPath));
										if (libraryAssetIdentifier.IsValid())
										{
											assetTypeGuid = assetLibrary.GetAssetTypeGuid(asset.GetGuid());
											if (assetTypeGuid.IsValid())
											{
												// Now import the asset
												Asset::Identifier assetIdentifier;
												if (assetLibrary
												      .IsTagSet(tagRegistry.FindOrRegister(Asset::Library::LocalAssetDatabaseTagGuid), libraryAssetIdentifier))
												{
													// Assets from the local asset database should be copied instead of imported
													AssetCompiler::Plugin& assetCompiler = *System::FindPlugin<AssetCompiler::Plugin>();
													Project& currentProject = System::Get<Project>();
													Engine& engine = System::Get<Engine>();
													const IO::Path existingAssetPath = assetLibrary.GetAssetPath(asset.GetGuid());
													Asset::Owners sourceAssetOwners(existingAssetPath, Asset::Context{EngineInfo(engine.GetInfo())});
													[[maybe_unused]] const bool wasCopied = assetCompiler.CopyAsset(
														existingAssetPath,
														sourceAssetOwners.m_context,
														IO::Path::Combine(
															currentProject.GetInfo()->GetDirectory(),
															currentProject.GetInfo()->GetRelativeAssetDirectory(),
															existingAssetPath.GetFileName()
														),
														Asset::Context{ProjectInfo(*currentProject.GetInfo()), EngineInfo(engine.GetInfo())},
														Serialization::SavingFlags{}
													);
													Assert(wasCopied);
													assetIdentifier = assetManager.GetAssetIdentifier(asset.GetGuid());
												}
												else
												{
													// Attempt to import into the asset library
													assetIdentifier = assetManager.Import(Asset::LibraryReference{asset.GetGuid(), assetTypeGuid});
												}
												if (assetIdentifier.IsValid())
												{
													draggedItem = Widgets::DragAndDropData{Asset::Reference{asset.GetGuid(), assetTypeGuid}};
													if (callback(draggedItem.Get()) == Memory::CallbackResult::Break)
													{
														return Memory::CallbackResult::Break;
													}
													continue;
												}
											}
										}
									}
								}
							}
						}
						else
						{
							// Compile the asset into the asset library
							// TODO: Validate asset browser drag and drop, in AB should be -> current folder
							// Into viewport should be into context (editing project -> project, engine -> engine etc)
							Engine& engine = System::Get<Engine>();
							Project& currentProject = System::Get<Project>();

							const IO::Path targetDirectory = Asset::LocalDatabase::GetAssetPath(Guid::Generate(), Asset::Asset::FileExtension);
							const Asset::Context context{
								currentProject.IsValid() ? ProjectInfo(*currentProject.GetInfo()) : ProjectInfo(),
								EngineInfo(engine.GetInfo())
							};

							AssetCompiler::Plugin& assetCompiler = *System::FindPlugin<AssetCompiler::Plugin>();

							auto compile =
								[draggedItem, callback, &assetCompiler, draggedItemPath, context, targetDirectory](Threading::JobRunnerThread& thread
							  ) mutable
							{
								Threading::Job* pJob = assetCompiler.CompileAnyAsset(
									AssetCompiler::CompileFlags::WasDirectlyRequested,
									[&draggedItem, callback](
										const EnumFlags<AssetCompiler::CompileFlags> compileFlags,
										const ArrayView<Asset::Asset> assets,
										[[maybe_unused]] const ArrayView<const Serialization::Data> assetsData
									)
									{
										if (compileFlags.IsSet(AssetCompiler::CompileFlags::WasDirectlyRequested) & compileFlags.AreAnySet(AssetCompiler::CompileFlags::Compiled | AssetCompiler::CompileFlags::UpToDate))
										{
											Asset::Manager& assetManager = System::Get<Asset::Manager>();
											Asset::Library& assetLibrary = assetManager.GetAssetLibrary();

											for (const Asset::Asset& asset : assets)
											{
												if (assetManager.HasAsset(asset.GetGuid()))
												{
													draggedItem = Widgets::DragAndDropData{Asset::Reference{asset.GetGuid(), asset.GetTypeGuid()}};
													callback(draggedItem.Get());
												}
												else if (assetLibrary.HasAsset(asset.GetGuid()))
												{
													draggedItem = Widgets::DragAndDropData{Asset::LibraryReference{asset.GetGuid(), asset.GetTypeGuid()}};
													callback(draggedItem.Get());
												}
											}
										}
									},
									thread,
									Platform::Current,
									IO::Path(draggedItemPath),
									context,
									context,
									targetDirectory
								);

								if (pJob != nullptr)
								{
									pJob->Queue(thread);
								}
							};

							if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
							{
								callback(*pThread);
							}
							else
							{
								System::Get<Threading::JobManager>().QueueCallback(
									[callback = Move(callback)](Threading::JobRunnerThread& thread)
									{
										callback(thread);
									},
									Threading::JobPriority::AssetCompilation
								);
							}
						}
					}
				}
			}

			return Memory::CallbackResult::Continue;
		}

		[[nodiscard]] virtual PanResult OnStartPan(
			const Input::DeviceIdentifier,
			const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2f velocity,
			const Optional<uint16> touchRadius,
			SetCursorCallback& pointer
		) override
		{
			return OnStartDrag(coordinate, touchRadius, pointer);
		}
		[[nodiscard]] virtual Widgets::CursorResult OnMovePan(
			const Input::DeviceIdentifier,
			const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2i delta,
			[[maybe_unused]] const Math::Vector2f velocity,
			const Optional<uint16> touchRadius,
			SetCursorCallback& pointer
		) override
		{
			return OnDrag(coordinate, touchRadius, pointer);
		}
		[[nodiscard]] virtual Widgets::CursorResult OnEndPan(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity
		) override
		{
			return OnEndDrag(coordinate);
		}
		[[nodiscard]] virtual Widgets::CursorResult OnCancelPan(const Input::DeviceIdentifier) override
		{
			return OnCancelDrag();
		}

		[[nodiscard]] virtual PanResult OnStartLongPress(
			const Input::DeviceIdentifier,
			const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			const Optional<uint16> touchRadius
		) override
		{
			SetCursorCallback pointer;
			return OnStartDrag(coordinate, touchRadius, pointer);
		}
		[[nodiscard]] virtual Widgets::CursorResult OnMoveLongPress(
			const Input::DeviceIdentifier,
			const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			const Optional<uint16> touchRadius
		) override
		{
			SetCursorCallback pointer;
			return OnDrag(coordinate, touchRadius, pointer);
		}
		[[nodiscard]] virtual Widgets::CursorResult
		OnEndLongPress(const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate) override
		{
			return OnEndDrag(coordinate);
		}
		[[nodiscard]] virtual Widgets::CursorResult OnCancelLongPress(const Input::DeviceIdentifier) override
		{
			return OnCancelDrag();
		}

		[[nodiscard]] bool CanAcceptItems() const
		{
			return m_dragFocusWidget.IsPotentiallyValid();
		}
	protected:
		[[nodiscard]] PanResult
		OnStartDrag(const LocalWidgetCoordinate coordinate, const Optional<uint16> touchRadius, SetCursorCallback& pointer)
		{
			pointer.SetOverridableCursor(CursorType::NotPermitted);

			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			if (Widgets::Widget* pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
			{
				const Widgets::DragAndDropResult dragAndDropResult = pWidgetAtCoordinates->HandleStartDragWidgetOverThis(
					pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
					*this,
					pointer
				);
				m_dragFocusWidget = {dragAndDropResult.pHandledWidget, GetSceneRegistry()};
			}

			return {this};
		}

		[[nodiscard]] Widgets::CursorResult
		OnDrag(const LocalWidgetCoordinate coordinate, const Optional<uint16> touchRadius, SetCursorCallback& pointer)
		{
			pointer.SetOverridableCursor(CursorType::NotPermitted);

			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			if (Widgets::Widget* pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
			{
				Optional<Widgets::Widget*> pPreviousDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry);
				Optional<Widgets::Widget*> pDragFocusWidget = pPreviousDragFocusWidget;
				if (pPreviousDragFocusWidget != nullptr && pWidgetAtCoordinates->IsChildOfRecursive(*pPreviousDragFocusWidget))
				{
					pDragFocusWidget = pPreviousDragFocusWidget;
				}

				if (pDragFocusWidget == pPreviousDragFocusWidget)
				{
					// Drag focus is still valid, notify move
					pDragFocusWidget = pPreviousDragFocusWidget;
					if (pDragFocusWidget != nullptr)
					{
						const Widgets::DragAndDropResult dragAndDropResult = pDragFocusWidget->HandleMoveDragWidgetOverThis(
							pDragFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate),
							*this,
							pointer
						);
						pDragFocusWidget = dragAndDropResult.pHandledWidget;
						if (pDragFocusWidget != pPreviousDragFocusWidget)
						{
							m_dragFocusWidget = {};
						}
					}
				}
				else
				{
					const Widgets::DragAndDropResult dragAndDropResult = pWidgetAtCoordinates->HandleStartDragWidgetOverThis(
						pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
						*this,
						pointer
					);
					pDragFocusWidget = dragAndDropResult.pHandledWidget;

					if (pPreviousDragFocusWidget != nullptr)
					{
						pPreviousDragFocusWidget->HandleCancelDragWidgetOverThis();
					}

					m_dragFocusWidget = {pDragFocusWidget, GetSceneRegistry()};
				}

				GetOwningWindow()->m_pointerDevice.SetHoverFocusWidget(pDragFocusWidget, GetSceneRegistry());
			}
			else
			{
				GetOwningWindow()->m_pointerDevice.SetHoverFocusWidget(nullptr, GetSceneRegistry());

				if (m_dragFocusWidget.IsPotentiallyValid())
				{
					if (Optional<Widgets::Widget*> pDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry))
					{
						pDragFocusWidget->HandleCancelDragWidgetOverThis();
					}
					m_dragFocusWidget = {};
				}
			}

			return {this};
		}

		[[nodiscard]] Widgets::CursorResult OnEndDrag(const LocalWidgetCoordinate coordinate)
		{
			DisableWithChildren();
			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			if (Widgets::Widget* pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, Invalid))
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				Optional<Widgets::Widget*> pPreviousDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry);
				Optional<Widgets::Widget*> pDragFocusWidget = pPreviousDragFocusWidget;
				for (; pDragFocusWidget != nullptr && pDragFocusWidget != pWidgetAtCoordinates;
				     pDragFocusWidget = pDragFocusWidget->GetParentSafe())
					;

				if (pDragFocusWidget == pWidgetAtCoordinates)
				{
					// Drag focus is still valid, notify end
					pDragFocusWidget = pPreviousDragFocusWidget;
					const Widgets::DragAndDropResult dragAndDropResult =
						pDragFocusWidget->HandleReleaseDragWidgetOverThis(pDragFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate), *this);
					pDragFocusWidget = dragAndDropResult.pHandledWidget;
					if (pDragFocusWidget != pPreviousDragFocusWidget)
					{
						m_dragFocusWidget = {};
					}
				}
				else
				{
					const Widgets::DragAndDropResult dragAndDropResult = pWidgetAtCoordinates->HandleReleaseDragWidgetOverThis(
						pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
						*this
					);
					pDragFocusWidget = dragAndDropResult.pHandledWidget;

					if (pPreviousDragFocusWidget != nullptr)
					{
						pPreviousDragFocusWidget->HandleCancelDragWidgetOverThis();
					}

					m_dragFocusWidget = {pDragFocusWidget, GetSceneRegistry()};
				}

				GetOwningWindow()->m_pointerDevice.SetHoverFocusWidget(pDragFocusWidget, GetSceneRegistry());
			}
			return {};
		}

		[[nodiscard]] Widgets::CursorResult OnCancelDrag()
		{
			DisableWithChildren();
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			if (m_dragFocusWidget.IsPotentiallyValid())
			{
				if (Optional<Widgets::Widget*> pDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry))
				{
					pDragFocusWidget->HandleCancelDragWidgetOverThis();
				}
			}
			m_dragFocusWidget = {};

			Destroy(sceneRegistry);
			return {};
		}
	protected:
		mutable Vector<Widgets::DragAndDropData> m_dragInfo;
		Entity::ComponentSoftReference m_dragFocusWidget;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Rendering::DraggedItemsWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Rendering::DraggedItemsWidget>(
			Rendering::DraggedItemsWidget::TypeGuid,
			MAKE_UNICODE_LITERAL("Dragged Items Widget"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization
		);
	};
}

namespace ngine::Rendering
{
	bool ToolWindow::OnStartDragItemsIntoWindow(
		const WindowCoordinate windowCoordinate, const ArrayView<const Widgets::DragAndDropData> draggedItems
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<DraggedItemsWidget>& draggedItemsSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<DraggedItemsWidget>();
		DraggedItemsWidget& draggedWidget = *draggedItemsSceneData.CreateInstance(*this, draggedItems, windowCoordinate);

		m_pointerDevice.SetHoverFocusWidget(&draggedWidget, GetEntitySceneRegistry());
		SetInputFocusWidget(&draggedWidget);

		Widget::SetCursorCallback pointer(m_pointerDevice.m_cursor);
		[[maybe_unused]] const Widgets::PanResult panResult = draggedWidget.HandleStartPan(
			Input::DeviceIdentifier{},
			draggedWidget.ConvertWindowToLocalCoordinates(windowCoordinate),
			Math::Zero,
			Invalid,
			pointer
		);
		Assert(panResult.pHandledWidget == &draggedWidget);
		return draggedWidget.CanAcceptItems();
	}

	void ToolWindow::OnCancelDragItemsIntoWindow()
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			Assert(pInputFocusWidget->Is<DraggedItemsWidget>());
			[[maybe_unused]] const Widgets::CursorResult result = pInputFocusWidget->HandleCancelPan(Input::DeviceIdentifier{});
		}
	}

	bool ToolWindow::OnDragItemsOverWindow(const WindowCoordinate windowCoordinate, const ArrayView<const Widgets::DragAndDropData>)
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			const Optional<DraggedItemsWidget*> pDraggedWidget = pInputFocusWidget->As<DraggedItemsWidget>();
			Assert(pDraggedWidget.IsValid());
			if (LIKELY(pDraggedWidget.IsValid()))
			{
				Assert(pInputFocusWidget->Is<DraggedItemsWidget>());
				const Widgets::LocalWidgetCoordinate localCoordinate = pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate);
				Widget::SetCursorCallback pointer(m_pointerDevice.m_cursor);
				[[maybe_unused]] const Widgets::CursorResult cursorResult = pInputFocusWidget->HandleMovePan(
					Input::DeviceIdentifier{},
					localCoordinate,
					localCoordinate - pInputFocusWidget->GetPosition(),
					Math::Zero,
					Invalid,
					pointer
				);
				return pDraggedWidget->CanAcceptItems();
			}
		}
		return false;
	}

	bool ToolWindow::OnDropItemsIntoWindow(const WindowCoordinate windowCoordinate, const ArrayView<const Widgets::DragAndDropData>)
	{
		if (const Optional<Widget*> pInputFocusWidget = GetInputFocusWidget())
		{
			const Optional<DraggedItemsWidget*> pDraggedWidget = pInputFocusWidget->As<DraggedItemsWidget>();
			Assert(pDraggedWidget.IsValid());
			if (LIKELY(pDraggedWidget.IsValid()))
			{
				[[maybe_unused]] const Widgets::CursorResult result = pInputFocusWidget->HandleEndPan(
					Input::DeviceIdentifier{},
					pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate),
					Math::Zero
				);
				const bool completedDrag = pDraggedWidget->CanAcceptItems();
				pDraggedWidget->Destroy(GetEntitySceneRegistry());
				return completedDrag;
			}
		}
		return false;
	}

	void ToolWindow::SetInputFocusAtCoordinate(WindowCoordinate windowCoordinate, const Optional<uint16> touchRadius)
	{
		if (Widgets::Widget* pWidgetAtCoordinates = GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			SetInputFocusWidget(pWidgetAtCoordinates);
		}
	}

	void ToolWindow::NotifySwitchToForegroundRecursive(Widgets::Widget& widget)
	{
		widget.OnSwitchToForeground();
		for (Widgets::Widget& childWidget : widget.GetChildren())
		{
			NotifySwitchToForegroundRecursive(childWidget);
		}
	}

	void ToolWindow::OnSwitchToForegroundInternal()
	{
		// Pause all queue submissions on mobile
		// Especially important for iOS where GPU work in the background is strictly forbidden and will emulate a GPU crash
		if constexpr (PLATFORM_MOBILE || PLATFORM_SPATIAL)
		{
			GetLogicalDevice().ResumeQueueSubmissions();
		}

		const bool wasCleared = m_stateFlags.TryClearFlags(StateFlags::SwitchingToForeground);
		Assert(wasCleared);
		if (wasCleared)
		{
			System::Get<Engine>().ModifyFrameGraph(
				[this]()
				{
					NotifySwitchToForegroundRecursive(GetRootWidget());
					EnableFramegraph();
				}
			);
		}
		else
		{
			NotifySwitchToForegroundRecursive(GetRootWidget());
		}
	}

	void ToolWindow::NotifySwitchToBackgroundRecursive(Widgets::Widget& widget)
	{
		widget.OnSwitchToBackground();
		for (Widgets::Widget& childWidget : widget.GetChildren())
		{
			NotifySwitchToBackgroundRecursive(childWidget);
		}
	}

	void ToolWindow::OnSwitchToBackgroundInternal()
	{
		const bool wasCleared = m_stateFlags.TryClearFlags(StateFlags::SwitchingToBackground);
		Assert(wasCleared);
		if (wasCleared)
		{
			m_stateFlags |= StateFlags::InBackground;

			// Disable rendering
			System::Get<Engine>().ModifyFrameGraph(
				[this]()
				{
					DisableFramegraph();
					NotifySwitchToBackgroundRecursive(GetRootWidget());
				}
			);
		}
		else
		{
			NotifySwitchToBackgroundRecursive(GetRootWidget());
			m_stateFlags |= StateFlags::InBackground;
		}

		// Pause all queue submissions on mobile
		// Especially important for iOS where GPU work in the background is strictly forbidden and will emulate a GPU crash
		if constexpr (PLATFORM_MOBILE || PLATFORM_SPATIAL)
		{
			GetLogicalDevice().PauseQueueSubmissions();
		}
	}

	Optional<FrameImageId> ToolWindow::AcquireNextImage(
		const LogicalDeviceView logicalDevice,
		const uint64 imageAvailabilityTimeoutNanoseconds,
		const SemaphoreView imageAvailableSemaphore,
		const FenceView fence
	)
	{
		if (UNLIKELY(m_stateFlags.AreAnySet(StateFlags::SwitchingToBackground | StateFlags::SwitchingToForeground | StateFlags::InBackground)))
		{
			return Invalid;
		}

		return SwapchainOutput::AcquireNextImage(logicalDevice, imageAvailabilityTimeoutNanoseconds, imageAvailableSemaphore, fence);
	}

	void ToolWindow::OnDisplayPropertiesChanged()
	{
		Widgets::RootWidget& rootWidget = GetRootWidget();
		rootWidget.OnDisplayPropertiesChanged(GetEntitySceneRegistry());
		rootWidget.QueueRecalculateWidgetHierarchy(rootWidget);
	}
}

namespace ngine::Rendering
{
	[[maybe_unused]] const bool wasDraggedItemsWidgetTypeRegistered = Reflection::Registry::RegisterType<DraggedItemsWidget>();
	[[maybe_unused]] const bool wasDraggedItemsWidgetComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<DraggedItemsWidget>>::Make());
}
