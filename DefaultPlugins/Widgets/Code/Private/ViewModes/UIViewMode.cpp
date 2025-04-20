#include "ViewModes/UIViewMode.h"

#include "RootWidget.h"
#include "WidgetScene.h"
#include "ToolWindow.h"

#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Project/Project.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Engine.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Renderer/Scene/SceneView2D.h>

#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Data/DataSource.h>

#include <AssetCompilerCore/Plugin.h>

#include <Common/Asset/AssetOwners.h>
#include <Common/Asset/FolderAssetType.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobManager.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Widgets
{
	struct ActionMonitor final : public Input::ActionMonitor
	{
		ActionMonitor(UIViewMode& viewMode)
			: m_viewMode(viewMode)
		{
		}

		// Input::ActionMonitor
		virtual void OnLoseDeviceFocus(const Input::DeviceIdentifier, const Input::DeviceTypeIdentifier deviceTypeIdentifier) override final
		{
			if (deviceTypeIdentifier == System::Get<Input::Manager>().GetMouseDeviceTypeIdentifier())
			{
				m_viewMode.OnMonitorLostMouseDeviceFocus();
			}
		}
		// ~Input::ActionMonitor

		UIViewMode& m_viewMode;
	};

	UIViewMode::UIViewMode(Rendering::SceneView2D& sceneView)
		: CommonInputActions(sceneView)
		, m_pActionMonitor(Memory::ConstructInPlace, *this)
	{
		CommonInputActions& commonInputActions = *this;
		m_tapAction.OnStartTap.Bind(commonInputActions, &CommonInputActions::OnStartTap);
		m_tapAction.OnTap.Bind(commonInputActions, &CommonInputActions::OnEndTap);
		m_tapAction.OnCancelTap.Bind(commonInputActions, &CommonInputActions::OnCancelTap);
		m_doubleTapAction.OnDoubleTap.Bind(commonInputActions, &CommonInputActions::OnDoubleTap);
		m_longPressAction.OnStartLongPress.Bind(commonInputActions, &CommonInputActions::OnStartLongPress);
		m_longPressAction.OnMoveLongPress.Bind(commonInputActions, &CommonInputActions::OnMoveLongPress);
		m_longPressAction.OnEndLongPress.Bind(commonInputActions, &CommonInputActions::OnEndLongPress);
		m_longPressAction.OnCancelLongPress.Bind(commonInputActions, &CommonInputActions::OnCancelLongPress);
		m_panAction.OnStartPan.Bind(commonInputActions, &CommonInputActions::OnStartPan);
		m_panAction.OnMovePan.Bind(commonInputActions, &CommonInputActions::OnMovePan);
		m_panAction.OnEndPan.Bind(commonInputActions, &CommonInputActions::OnEndPan);
		m_panAction.OnCancelPan.Bind(commonInputActions, &CommonInputActions::OnCancelPan);
		m_scrollAction.OnStartScroll.Bind(commonInputActions, &CommonInputActions::OnStartScroll);
		m_scrollAction.OnScroll.Bind(commonInputActions, &CommonInputActions::OnScroll);
		m_scrollAction.OnEndScroll.Bind(commonInputActions, &CommonInputActions::OnEndScroll);
		m_scrollAction.OnCancelScroll.Bind(commonInputActions, &CommonInputActions::OnCancelScroll);
		m_textInputAction.OnInput.Bind(commonInputActions, &CommonInputActions::OnTextInput);
		m_textInputAction.OnMoveTextCursor.Bind(commonInputActions, &CommonInputActions::OnMoveTextCursor);
		m_textInputAction.OnApply.Bind(commonInputActions, &CommonInputActions::OnApplyTextInput);
		m_textInputAction.OnAbort.Bind(commonInputActions, &CommonInputActions::OnAbortTextInput);
		m_textInputAction.OnDelete.Bind(commonInputActions, &CommonInputActions::OnDeleteTextInput);
		m_copyPasteAction.OnCopy.Bind(commonInputActions, &CommonInputActions::OnCopy);
		m_copyPasteAction.OnPaste.Bind(commonInputActions, &CommonInputActions::OnPaste);
		m_tabAction.OnTabBack.Bind(commonInputActions, &CommonInputActions::OnTabBack);
		m_tabAction.OnTabForward.Bind(commonInputActions, &CommonInputActions::OnTabForward);
		m_hoverAction.OnHover.Bind(commonInputActions, &CommonInputActions::OnHover);

		m_pauseAction.OnStart.Bind(*this, &UIViewMode::OnPause);
	}

	CommonInputActions::CommonInputActions(Rendering::SceneView2D& sceneView)
		: m_sceneView(sceneView)
	{
	}

	UIViewMode::~UIViewMode()
	{
	}

	Optional<Input::Monitor*> UIViewMode::GetInputMonitor() const
	{
		return m_pActionMonitor.Get();
	}

	void UIViewMode::OnActivated(Rendering::SceneViewBase&)
	{
		// Bind inputs
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::MouseDeviceType& mouseDeviceType =
				inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
			Input::KeyboardDeviceType& keyboardDeviceType =
				inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
			Input::TouchscreenDeviceType& touchscreenDeviceType =
				inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
			// TODO: Support navigating with keyboard and gamepad

			Input::ActionMonitor& actionMonitor = *m_pActionMonitor;

			m_tapAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_tapAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_tapAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetTapInputIdentifier());

			m_doubleTapAction
				.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_doubleTapAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetDoubleTapInputIdentifier());

			m_longPressAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Right));
			m_longPressAction
				.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Right));
			m_longPressAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetLongPressInputIdentifier());

			m_panAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_panAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_panAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Middle));
			m_panAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Middle));
			m_panAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetDraggingMotionInputIdentifier());
			m_panAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetPanInputIdentifier());

			m_scrollAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetScrollInputIdentifier());
			m_scrollAction.BindInput(actionMonitor, touchscreenDeviceType, touchscreenDeviceType.GetPanInputIdentifier());
			m_textInputAction.BindInputs(actionMonitor, keyboardDeviceType);
			m_copyPasteAction.BindInputs(actionMonitor, keyboardDeviceType);

			m_tabAction.BindInputs(actionMonitor, keyboardDeviceType);

			m_hoverAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetHoveringMotionInputIdentifier());

			m_pauseAction.BindInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Escape));
		}
	}

	void UIViewMode::OnDeactivated(const Optional<SceneBase*>, Rendering::SceneViewBase&)
	{
		// Unbind inputs
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::MouseDeviceType& mouseDeviceType =
				inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
			Input::KeyboardDeviceType& keyboardDeviceType =
				inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
			Input::TouchscreenDeviceType& touchscreenDeviceType =
				inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

			Input::ActionMonitor& actionMonitor = *m_pActionMonitor;

			m_tapAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_tapAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_tapAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetTapInputIdentifier());

			m_doubleTapAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_doubleTapAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetDoubleTapInputIdentifier());

			m_longPressAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Right));
			m_longPressAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Right));
			m_longPressAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetLongPressInputIdentifier());

			m_panAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_panAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));
			m_panAction.UnbindInput(actionMonitor, mouseDeviceType.GetDraggingMotionInputIdentifier());
			m_panAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetPanInputIdentifier());

			m_scrollAction.UnbindInput(actionMonitor, mouseDeviceType.GetScrollInputIdentifier());
			m_scrollAction.UnbindInput(actionMonitor, touchscreenDeviceType.GetPanInputIdentifier());
			m_textInputAction.UnbindInputs(actionMonitor, keyboardDeviceType);

			m_tabAction.UnbindInputs(actionMonitor, keyboardDeviceType);

			m_hoverAction.UnbindInput(actionMonitor, mouseDeviceType.GetHoveringMotionInputIdentifier());

			m_pauseAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Escape));
		}
	}

	void UIViewMode::OnMonitorLostMouseDeviceFocus()
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		window.OnLostMouseFocus();
	}

	void CommonInputActions::OnStartTap(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			CursorResult cursorResult = pWidgetAtCoordinates->HandleStartTap(
				deviceIdentifier,
				fingerCount,
				pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate)
			);
			if (cursorResult.pHandledWidget == nullptr)
			{
				cursorResult.pHandledWidget = pWidgetAtCoordinates;
			}

			window.SetActiveFocusWidget(cursorResult.pHandledWidget);
		}
		else
		{
			window.SetActiveFocusWidget(nullptr);
		}
	}

	void CommonInputActions::OnEndTap(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			CursorResult cursorResult = pWidgetAtCoordinates->HandleEndTap(
				deviceIdentifier,
				fingerCount,
				pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate)
			);
			if (cursorResult.pHandledWidget == nullptr)
			{
				cursorResult.pHandledWidget = pWidgetAtCoordinates;
			}

			window.SetInputFocusWidget(cursorResult.pHandledWidget);
		}
		else
		{
			window.SetInputFocusWidget(nullptr);
		}

		window.SetActiveFocusWidget(nullptr);
	}

	void CommonInputActions::OnCancelTap(const Input::DeviceIdentifier deviceIdentifier)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			[[maybe_unused]] const CursorResult cursorResult = pInputFocusWidget->HandleCancelTap(deviceIdentifier);
		}

		window.SetActiveFocusWidget(nullptr);
	}

	void CommonInputActions::OnDoubleTap(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			const CursorResult cursorResult =
				pWidgetAtCoordinates->HandleDoubleTap(deviceIdentifier, pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate));
			window.SetInputFocusWidget(cursorResult.pHandledWidget);
		}
		else
		{
			window.SetInputFocusWidget(nullptr);
		}
	}

	struct DraggedWidget final : public Widgets::Widget
	{
		using BaseType = Widgets::Widget;

		DraggedWidget(Widget& draggedWidget, const Math::Vector2i draggedCoordinate)
			: Widget(draggedWidget.GetRootWidget(), draggedWidget.GetContentArea(), Widget::Flags::IsInputDisabled)
			, m_draggedWidget(draggedWidget, draggedWidget.GetSceneRegistry())
			, m_draggedCoordinate(draggedCoordinate)
		{
			// TODO: Width and height should be fit-content but it doesn't work atm
			Widgets::Style::Entry contentStyle;
			Widgets::Style::Entry::ModifierValues& modifier = contentStyle.EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
			modifier.ParseFromCSS(String().Format("display: flex; flex-direction: column; position: dynamic; width: 512px; height: 512px;"));
			System::Get<Input::Manager>().GetFeedback().TriggerImpact(1.0f);

			Array<Reflection::TypeDefinition, 3> typeDefinitions = {
				Reflection::TypeDefinition::Get<Asset::Reference>(),
				Reflection::TypeDefinition::Get<Asset::LibraryReference>(),
				Reflection::TypeDefinition::Get<Entity::ComponentSoftReference>()
			};
			[[maybe_unused]] const Memory::CallbackResult callbackResult = draggedWidget
			                                                                 .IterateAttachedItems(
																																				 m_draggedCoordinate,
																																				 typeDefinitions.GetView(),
																																				 [this, &modifier](ConstAnyView view) -> Memory::CallbackResult
																																				 {
																																					 if (const Optional<const Asset::Reference*> pAssetReference = view.Get<Asset::Reference>())
																																					 {
																																						 modifier.Emplace(Style::Value{
																																							 Style::ValueType::AttachedAsset,
																																							 pAssetReference->GetAssetGuid()
																																						 });
																																						 m_draggedApplicableData = *pAssetReference;
																																						 return Memory::CallbackResult::Break;
																																					 }
																																					 else if (const Optional<const Asset::LibraryReference*> pLibraryAssetReference = view.Get<Asset::LibraryReference>())
																																					 {
																																						 m_draggedApplicableData = *pLibraryAssetReference;
																																						 modifier.Emplace(Style::Value{
																																							 Style::ValueType::AttachedAsset,
																																							 pLibraryAssetReference->GetAssetGuid()
																																						 });
																																						 return Memory::CallbackResult::Break;
																																					 }
																																					 else if (const Optional<const Entity::ComponentSoftReference*> componentSoftReference = view.Get<Entity::ComponentSoftReference>())
																																					 {
																																						 m_draggedApplicableData = *componentSoftReference;
																																						 modifier.Emplace(Style::Value{
																																							 Style::ValueType::AttachedComponent,
																																							 *componentSoftReference
																																						 });
																																						 return Memory::CallbackResult::Break;
																																					 }
																																					 else
																																					 {
																																						 Assert(false);
																																						 return Memory::CallbackResult::Continue;
																																					 }
																																				 }
																																			 );

			contentStyle.OnValueTypesAdded(modifier.GetValueTypeMask());
			contentStyle.OnValueTypesAdded(modifier.GetDynamicValueTypeMask());
			ChangeInlineStyle(Move(contentStyle));
		}

		void OnCreated()
		{
			// Disable asset preview for augmented reality & web
			if constexpr (!PLATFORM_APPLE_VISIONOS && !PLATFORM_WEB)
			{
				if (m_draggedApplicableData.HasValue())
				{
					static constexpr const Guid DraggableWidgetAssetGuid = "4127E4DE-C33D-4CD0-AEB2-4C671B771368"_asset;
					Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
					Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
						DraggableWidgetAssetGuid,
						sceneRegistry,
						this,
						[&sceneRegistry, &foundAttachedItem = m_draggedApplicableData]([[maybe_unused]] const Optional<Widgets::Widget*> pWidget)
						{
							if (pWidget.IsValid())
							{
								pWidget->EnableWithChildren(sceneRegistry);

								pWidget->DisableInput();
								for (Widget& child : pWidget->GetChildren())
								{
									child.DisableInput();
								}
							}

							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							if (foundAttachedItem.Is<Asset::Reference>() || foundAttachedItem.Is<Asset::LibraryReference>())
							{
								Asset::Identifier assetIdentifier;
								if (const Optional<const Asset::Reference*> pAssetReference = foundAttachedItem.Get<Asset::Reference>())
								{
									assetIdentifier = assetManager.GetAssetIdentifier(pAssetReference->GetAssetGuid());
								}
								else
								{
									Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
									Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

									const Optional<const Asset::LibraryReference*> pLibraryAssetReference = foundAttachedItem.Get<Asset::LibraryReference>();
									Assert(pLibraryAssetReference.IsValid());

									const Asset::Identifier libraryAssetIdentifier = assetLibrary.GetAssetIdentifier(pLibraryAssetReference->GetAssetGuid());
									if (assetLibrary.IsTagSet(tagRegistry.FindOrRegister(Asset::Library::LocalAssetDatabaseTagGuid), libraryAssetIdentifier))
									{
										// Assets from the local asset database should be copied instead of imported
										AssetCompiler::Plugin& assetCompiler = *System::FindPlugin<AssetCompiler::Plugin>();
										Project& currentProject = System::Get<Project>();
										Engine& engine = System::Get<Engine>();
										const IO::Path existingAssetPath = assetLibrary.GetAssetPath(pLibraryAssetReference->GetAssetGuid());
										Asset::Owners sourceAssetOwners(existingAssetPath, Asset::Context{EngineInfo(engine.GetInfo())});

										IO::Path::StringType assetName = assetLibrary.GetAssetName(pLibraryAssetReference->GetAssetGuid());
										IO::Path targetAssetPath = IO::Path::Combine(
											currentProject.GetInfo()->GetDirectory(),
											currentProject.GetInfo()->GetRelativeAssetDirectory(),
											IO::Path::Merge(assetName.GetView(), existingAssetPath.GetAllExtensions())
										);
										[[maybe_unused]] const bool wasCopied = assetCompiler.CopyAsset(
											existingAssetPath,
											sourceAssetOwners.m_context,
											IO::Path::Combine(
												currentProject.GetInfo()->GetDirectory(),
												currentProject.GetInfo()->GetRelativeAssetDirectory(),
												IO::Path::Merge(assetName.GetView(), existingAssetPath.GetAllExtensions())
											),
											Asset::Context{ProjectInfo(*currentProject.GetInfo()), EngineInfo(engine.GetInfo())},
											Serialization::SavingFlags{}
										);
										Assert(wasCopied);
										if (pLibraryAssetReference->GetTypeGuid() == Asset::FolderAssetType::AssetFormat.assetTypeGuid)
										{
											IO::Path mainAssetPath =
												IO::Path::Combine(targetAssetPath, IO::Path::Merge(MAKE_PATH("Main"), targetAssetPath.GetAllExtensions()));
											if (!assetManager.HasAsset(mainAssetPath))
											{
												mainAssetPath = IO::Path::Combine(targetAssetPath, targetAssetPath.GetFileName());
											}

											assetIdentifier = assetManager.GetAssetIdentifier(assetManager.GetAssetGuid(mainAssetPath));
										}
										else
										{
											assetIdentifier = assetManager.GetAssetIdentifier(pLibraryAssetReference->GetAssetGuid());
										}
									}
									else
									{
										assetIdentifier = assetManager.Import(*pLibraryAssetReference);
									}
								}
								Assert(assetIdentifier.IsValid());
								if (LIKELY(assetIdentifier.IsValid() && pWidget.IsValid()))
								{
									if (Optional<Widgets::Data::DataSource*> pDataSource = pWidget->FindDataComponentOfType<Widgets::Data::DataSource>(sceneRegistry))
									{
										Asset::Mask mask;
										mask.Set(assetIdentifier);
										pDataSource->SetAllowedItems(*pWidget, mask);
									}
								}
							}
							else if (foundAttachedItem.Is<Entity::ComponentSoftReference>())
							{
								if (Optional<Widgets::Data::DataSource*> pDataSource = pWidget->FindDataComponentOfType<Widgets::Data::DataSource>(sceneRegistry))
								{
									Asset::Mask mask;

									const Entity::ComponentSoftReference softReference = foundAttachedItem.GetExpected<Entity::ComponentSoftReference>();
									Entity::Manager& entityManager = System::Get<Entity::Manager>();
									const Optional<const Entity::ComponentTypeInterface*> pComponentTypeInterface =
										entityManager.GetRegistry().Get(softReference.GetTypeIdentifier());
									Assert(pComponentTypeInterface.IsValid());
									if (LIKELY(pComponentTypeInterface.IsValid()))
									{
										const Asset::Identifier componentTypeAssetIdentifier =
											assetManager.GetAssetIdentifier(pComponentTypeInterface->GetTypeInterface().GetGuid());
										mask.Set(componentTypeAssetIdentifier);
									}

									pDataSource->SetAllowedItems(*pWidget, mask);
								}
							}
							else
							{
								Assert(false);
							}
						},
						{}
					);
					if (jobBatch.IsValid())
					{
						System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::UserInterfaceAction);
					}
				}
			}
		}

		void OnDestroying()
		{
			CancelInternal();
		}

		virtual ~DraggedWidget()
		{
		}

		// TODO: Show a number in the upper right corner for the number of dragged items (if more than 1)

		virtual Memory::CallbackResult IterateAttachedItems(
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			const ArrayView<const Reflection::TypeDefinition> allowedTypes,
			CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
		) const override
		{
			if (Widget::IterateAttachedItems(coordinate, allowedTypes, CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>{callback}) == Memory::CallbackResult::Break)
			{
				return Memory::CallbackResult::Break;
			}

			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			if (const Optional<Widgets::Widget*> pDraggedWidget = m_draggedWidget.Find<Widget>(sceneRegistry))
			{
				return pDraggedWidget->IterateAttachedItems(
					pDraggedWidget->ConvertWindowToLocalCoordinates(ConvertLocalToWindowCoordinates(coordinate)),
					allowedTypes,
					Forward<decltype(callback)>(callback)
				);
			}

			return Memory::CallbackResult::Continue;
		}

		virtual Memory::CallbackResult IterateAttachedItemsAsync(
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			const ArrayView<const Reflection::TypeDefinition> allowedTypes,
			CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
		) const override
		{
			if (Widget::IterateAttachedItemsAsync(coordinate, allowedTypes, CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>{callback}) == Memory::CallbackResult::Break)
			{
				return Memory::CallbackResult::Break;
			}

			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			if (const Optional<Widgets::Widget*> pDraggedWidget = m_draggedWidget.Find<Widget>(sceneRegistry))
			{
				return pDraggedWidget->IterateAttachedItemsAsync(
					pDraggedWidget->ConvertWindowToLocalCoordinates(ConvertLocalToWindowCoordinates(coordinate)),
					allowedTypes,
					Forward<decltype(callback)>(callback)
				);
			}

			return Memory::CallbackResult::Break;
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

		[[nodiscard]] virtual CursorResult OnMoveLongPress(
			const Input::DeviceIdentifier,
			const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			const Optional<uint16> touchRadius
		) override
		{
			SetCursorCallback pointer;
			return OnDrag(coordinate, touchRadius, pointer);
		}

		[[nodiscard]] virtual CursorResult OnEndLongPress(const Input::DeviceIdentifier, const LocalWidgetCoordinate coordinate) override
		{
			return OnStopDrag(coordinate);
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

		[[nodiscard]] virtual CursorResult OnMovePan(
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

		[[nodiscard]] virtual CursorResult
		OnEndPan(const Input::DeviceIdentifier, const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity) override
		{
			return OnStopDrag(coordinate);
		}

		virtual CursorResult OnCancelPan(const Input::DeviceIdentifier) override
		{
			return OnCancel();
		}

		virtual CursorResult OnCancelLongPress(const Input::DeviceIdentifier) override
		{
			return OnCancel();
		}
	protected:
		[[nodiscard]] Optional<Widgets::Widget*>
		GetTargetWidget(const Optional<Widgets::Widget*> pOriginalWidget, const WindowCoordinate windowCoordinate)
		{
			Optional<Widgets::Widget*> pWidget = pOriginalWidget;
			while (pWidget.IsValid())
			{
				if (pWidget->CanApplyAtPoint(
							m_draggedApplicableData,
							Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
							Entity::ApplyAssetFlags{}
						))
				{
					return pWidget;
				}
				pWidget = pWidget->GetParentSafe();
			}
			return pOriginalWidget;
		}

		[[nodiscard]] PanResult
		OnStartDrag(const LocalWidgetCoordinate coordinate, const Optional<uint16> touchRadius, SetCursorCallback& pointer)
		{
			pointer.SetOverridableCursor(Rendering::CursorType::NotPermitted);

			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			// Make sure the dragged widget is ignored from the query
			Ignore(GetSceneRegistry());
			if (Optional<Widgets::Widget*> pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
			{
				pWidgetAtCoordinates = GetTargetWidget(pWidgetAtCoordinates, windowCoordinate);
				if (pWidgetAtCoordinates->CanApplyAtPoint(
							m_draggedApplicableData,
							Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
							Entity::ApplyAssetFlags{}
						))
				{
					m_dragFocusWidget = Entity::ComponentSoftReference{pWidgetAtCoordinates, GetSceneRegistry()};
					m_isApplyingToPoint = true;
				}
				else
				{
					const Widgets::DragAndDropResult dragAndDropResult = pWidgetAtCoordinates->HandleStartDragWidgetOverThis(
						pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
						*this,
						pointer
					);
					m_dragFocusWidget = Entity::ComponentSoftReference{dragAndDropResult.pHandledWidget, GetSceneRegistry()};
					m_isApplyingToPoint = false;
				}
			}
			Unignore(GetSceneRegistry());

			return {this};
		}

		[[nodiscard]] CursorResult
		OnDrag(const LocalWidgetCoordinate coordinate, const Optional<uint16> touchRadius, SetCursorCallback& pointer)
		{
			pointer.SetOverridableCursor(Rendering::CursorType::NotPermitted);

			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			// Make sure the dragged widget is ignored from the query
			Ignore(GetSceneRegistry());
			if (Widgets::Widget* pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, touchRadius))
			{
				pWidgetAtCoordinates = GetTargetWidget(pWidgetAtCoordinates, windowCoordinate);

				Optional<Widgets::Widget*> pPreviousDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry);
				Optional<Widgets::Widget*> pDragFocusWidget = pWidgetAtCoordinates;
				if (pPreviousDragFocusWidget != nullptr && pWidgetAtCoordinates->IsChildOfRecursive(*pPreviousDragFocusWidget))
				{
					pDragFocusWidget = pPreviousDragFocusWidget;
				}

				if (pDragFocusWidget == pPreviousDragFocusWidget)
				{
					if (!pDragFocusWidget->CanApplyAtPoint(
								m_draggedApplicableData,
								Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
								Entity::ApplyAssetFlags{}
							))
					{
						Assert(!m_isApplyingToPoint);
						// Drag focus is still valid, notify move
						pDragFocusWidget = pPreviousDragFocusWidget;
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
					const bool wasApplyingToPoint = m_isApplyingToPoint;

					if (pWidgetAtCoordinates->CanApplyAtPoint(
								m_draggedApplicableData,
								Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
								Entity::ApplyAssetFlags{}
							))
					{
						m_dragFocusWidget = Entity::ComponentSoftReference{*pWidgetAtCoordinates, GetSceneRegistry()};
						m_isApplyingToPoint = true;
					}
					else
					{
						const Widgets::DragAndDropResult dragAndDropResult = pWidgetAtCoordinates->HandleStartDragWidgetOverThis(
							pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
							*this,
							pointer
						);
						pDragFocusWidget = dragAndDropResult.pHandledWidget;

						if (pPreviousDragFocusWidget != nullptr && !wasApplyingToPoint)
						{
							pPreviousDragFocusWidget->HandleCancelDragWidgetOverThis();
						}

						m_dragFocusWidget = {pDragFocusWidget, sceneRegistry};
						m_isApplyingToPoint = false;
					}
				}

				GetOwningWindow()->SetHoverFocusWidget(pDragFocusWidget);
			}
			else
			{
				GetOwningWindow()->SetHoverFocusWidget(nullptr);

				if (m_dragFocusWidget.IsPotentiallyValid())
				{
					if (Optional<Widgets::Widget*> pDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry))
					{
						pDragFocusWidget->HandleCancelDragWidgetOverThis();
					}
					m_dragFocusWidget = {};
				}
			}
			Unignore(GetSceneRegistry());

			if (!GetPosition().IsEquivalentTo(Math::Vector2i(windowCoordinate)))
			{
				Reposition(Math::Vector2i(windowCoordinate.x, windowCoordinate.y), sceneRegistry);
			}

			return CursorResult{this};
		}

		CursorResult OnStopDrag(const LocalWidgetCoordinate coordinate)
		{
			DisableWithChildren();
			const WindowCoordinate windowCoordinate = ConvertLocalToWindowCoordinates(coordinate);
			if (Widgets::Widget* pWidgetAtCoordinates = GetOwningWindow()->GetRootWidget().GetWidgetAtCoordinate(windowCoordinate, Invalid))
			{
				pWidgetAtCoordinates = GetTargetWidget(pWidgetAtCoordinates, windowCoordinate);

				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				Optional<Widgets::Widget*> pPreviousDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry);
				Optional<Widgets::Widget*> pDragFocusWidget = pPreviousDragFocusWidget;
				if (pPreviousDragFocusWidget != nullptr && pWidgetAtCoordinates->IsChildOfRecursive(*pPreviousDragFocusWidget))
				{
					pDragFocusWidget = pPreviousDragFocusWidget;
				}

				if (pWidgetAtCoordinates->CanApplyAtPoint(
							m_draggedApplicableData,
							Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
							Entity::ApplyAssetFlags{}
						))
				{
					[[maybe_unused]] const bool wasApplied = pWidgetAtCoordinates->ApplyAtPoint(
						m_draggedApplicableData,
						Math::WorldCoordinate2D{(Math::Vector2f)windowCoordinate},
						Entity::ApplyAssetFlags{}
					);

					if (!m_isApplyingToPoint)
					{
						if (pPreviousDragFocusWidget != nullptr)
						{
							pPreviousDragFocusWidget->HandleCancelDragWidgetOverThis();
						}
					}
				}
				else if (pDragFocusWidget == pWidgetAtCoordinates)
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

					if (pPreviousDragFocusWidget != nullptr && !m_isApplyingToPoint)
					{
						pPreviousDragFocusWidget->HandleCancelDragWidgetOverThis();
					}

					m_dragFocusWidget = {pDragFocusWidget, GetSceneRegistry()};
				}

				GetOwningWindow()->SetHoverFocusWidget(pDragFocusWidget);
			}

			Destroy(GetSceneRegistry());
			return {};
		}

		CursorResult OnCancel()
		{
			DisableWithChildren();
			if (m_dragFocusWidget.IsPotentiallyValid())
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				if (Optional<Widgets::Widget*> pDragFocusWidget = m_dragFocusWidget.Find<Widget>(sceneRegistry))
				{
					pDragFocusWidget->HandleCancelDragWidgetOverThis();
				}
				m_dragFocusWidget = {};
			}

			Destroy(GetSceneRegistry());
			return {};
		}

		void CancelInternal()
		{
			Hide();
			// m_draggedWidget.Enable();
			// GetOwningWindow()->InvalidateWidget(m_draggedWidget);
		}
	protected:
		Entity::ComponentSoftReference m_draggedWidget;
		Math::Vector2i m_draggedCoordinate;
		Entity::ApplicableData m_draggedApplicableData;
		Entity::ComponentSoftReference m_dragFocusWidget;
		bool m_isApplyingToPoint{false};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::DraggedWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::DraggedWidget>(
			"f8e6dbcd-57d8-4fd0-b7ae-9df3e7a45e6f"_guid,
			MAKE_UNICODE_LITERAL("Dragged Widget"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization
		);
	};
}

namespace ngine::Widgets
{
	[[maybe_unused]] const bool wasDraggedWidgetTypeRegistered = Reflection::Registry::RegisterType<DraggedWidget>();
	[[maybe_unused]] const bool wasDraggedWidgetComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<DraggedWidget>>::Make());

	void CommonInputActions::OnStartLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			const Widgets::LocalWidgetCoordinate localCoordinate = pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate);
			const PanResult cursorResult =
				pWidgetAtCoordinates->HandleStartLongPress(deviceIdentifier, localCoordinate, fingerCount, touchRadius);

			pWidgetAtCoordinates = cursorResult.pHandledWidget;

			switch (cursorResult.panType)
			{
				case Widgets::Widget::PanType::DragWidget:
				{
					Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
					Entity::ComponentTypeSceneData<DraggedWidget>& draggedSceneData = *sceneRegistry.GetOrCreateComponentTypeData<DraggedWidget>();
					DraggedWidget& draggedWidget = *draggedSceneData.CreateInstance(*pWidgetAtCoordinates, localCoordinate);

					pWidgetAtCoordinates = &draggedWidget;

					[[maybe_unused]] const Widgets::PanResult panResult =
						pWidgetAtCoordinates->OnStartLongPress(deviceIdentifier, localCoordinate, fingerCount, touchRadius);
				}
				break;
				default:
					break;
			}

			window.SetInputFocusWidget(pWidgetAtCoordinates);
		}
		else
		{
			window.SetInputFocusWidget(nullptr);
		}
	}

	void CommonInputActions::OnMoveLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const Widgets::CursorResult cursorResult = pInputFocusWidget->HandleMoveLongPress(
				deviceIdentifier,
				pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate),
				fingerCount,
				touchRadius
			);
		}
	}

	void CommonInputActions::OnEndLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		[[maybe_unused]] const uint8 fingerCount,
		[[maybe_unused]] const Optional<uint16> touchRadius
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const CursorResult cursorResult =
				pInputFocusWidget->HandleEndLongPress(deviceIdentifier, pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate));
		}
	}

	void CommonInputActions::OnCancelLongPress(const Input::DeviceIdentifier deviceIdentifier)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			[[maybe_unused]] const CursorResult cursorResult = pInputFocusWidget->HandleCancelLongPress(deviceIdentifier);
		}
	}

	// TODO: Rework screen input to support multiple inputs better
	// Should abstractify a cursor so we can async query the cursor and return the last response to WinAPI / the platform
	// Later this will also help with unifying cursor look, by having one in-engine

	void CommonInputActions::OnHover(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Math::Vector2i deltaCoordinate
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		Rendering::PointerDevice& pointerDevice = window.GetPointerDevice();
		pointerDevice.m_coordinate = windowCoordinate;

		Widget::SetCursorCallback pointer(pointerDevice.m_cursor);

		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, Invalid))
		{
			[[maybe_unused]] Widgets::CursorResult cursorResult = pWidgetAtCoordinates->HandleHover(
				deviceIdentifier,
				pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
				deltaCoordinate,
				pointer
			);
			pointerDevice.SetHoverFocusWidget(pWidgetAtCoordinates, window.GetEntitySceneRegistry());
		}
		else if (pointerDevice.m_hoverFocusWidget.IsPotentiallyValid())
		{
			pointerDevice.SetHoverFocusWidget(nullptr, window.GetEntitySceneRegistry());
		}

		window.SetCursor(pointerDevice.m_cursor);
	}

	void CommonInputActions::OnStartPan(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const uint8 fingerCount,
		const Math::Vector2f velocity,
		const Optional<uint16> touchRadius
	)
	{
		if (fingerCount > 1)
		{
			return;
		}

		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		Rendering::PointerDevice& pointerDevice = window.GetPointerDevice();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, touchRadius))
		{
			Widget::SetCursorCallback pointer(pointerDevice.m_cursor);

			const Widgets::LocalWidgetCoordinate localCoordinate = pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate);
			Widgets::PanResult cursorResult =
				pWidgetAtCoordinates->HandleStartPan(deviceIdentifier, localCoordinate, velocity, touchRadius, pointer);
			pWidgetAtCoordinates = cursorResult.pHandledWidget;

			switch (cursorResult.panType)
			{
				case Widgets::Widget::PanType::DragWidget:
				{
					Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
					Entity::ComponentTypeSceneData<DraggedWidget>& draggedSceneData = *sceneRegistry.GetOrCreateComponentTypeData<DraggedWidget>();
					DraggedWidget& draggedWidget = *draggedSceneData.CreateInstance(*pWidgetAtCoordinates, localCoordinate);

					pWidgetAtCoordinates = &draggedWidget;

					[[maybe_unused]] const Widgets::PanResult panResult = pWidgetAtCoordinates->HandleStartPan(
						deviceIdentifier,
						pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
						Math::Zero,
						touchRadius,
						pointer
					);
				}
				break;
				default:
					break;
			}

			window.SetInputFocusWidget(pWidgetAtCoordinates);
			window.SetCursor(pointerDevice.m_cursor);
		}
		else
		{
			window.SetInputFocusWidget(nullptr);

			window.SetCursor(Rendering::CursorType::Arrow);
		}
	}

	void CommonInputActions::OnMovePan(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const Math::Vector2i deltaCoordinate,
		const uint8 fingerCount,
		const Math::Vector2f velocity,
		const Optional<uint16> touchRadius
	)
	{
		if (fingerCount > 1)
		{
			return;
		}

		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		Rendering::PointerDevice& pointerDevice = window.GetPointerDevice();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			Widget::SetCursorCallback pointer(pointerDevice.m_cursor);
			[[maybe_unused]] const Widgets::CursorResult cursorResult = pInputFocusWidget->HandleMovePan(
				deviceIdentifier,
				pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate),
				deltaCoordinate,
				velocity,
				touchRadius,
				pointer
			);
			window.SetCursor(pointerDevice.m_cursor);
		}
		else
		{
			window.SetCursor(Rendering::CursorType::Arrow);
		}
	}

	void CommonInputActions::OnEndPan(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Math::Vector2f velocity
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const CursorResult cursorResult =
				pInputFocusWidget->HandleEndPan(deviceIdentifier, pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate), velocity);
		}
	}

	void CommonInputActions::OnCancelPan(const Input::DeviceIdentifier deviceIdentifier)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			[[maybe_unused]] const CursorResult cursorResult = pInputFocusWidget->HandleCancelPan(deviceIdentifier);
		}
	}

	void CommonInputActions::OnStartScroll(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Math::Vector2i delta
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (Widgets::Widget* pWidgetAtCoordinates = rootWidget.GetWidgetAtCoordinate(windowCoordinate, Invalid))
		{
			[[maybe_unused]] const CursorResult cursorResult = pWidgetAtCoordinates->HandleStartScroll(
				deviceIdentifier,
				pWidgetAtCoordinates->ConvertWindowToLocalCoordinates(windowCoordinate),
				delta
			);
			if (cursorResult.pHandledWidget.IsValid())
			{
				window.SetInputFocusWidget(cursorResult.pHandledWidget);
			}
		}
	}

	void CommonInputActions::OnScroll(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Math::Vector2i delta
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const CursorResult cursorResult =
				pInputFocusWidget->HandleScroll(deviceIdentifier, pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate), delta);
		}
	}

	void CommonInputActions::OnEndScroll(
		const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate, const Math::Vector2f velocity
	)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const CursorResult cursorResult = pInputFocusWidget->HandleEndScroll(
				deviceIdentifier,
				pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate),
				velocity
			);
		}
	}

	void CommonInputActions::OnCancelScroll(const Input::DeviceIdentifier deviceIdentifier, const ScreenCoordinate screenCoordinate)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
			[[maybe_unused]] const CursorResult cursorResult =
				pInputFocusWidget->HandleCancelScroll(deviceIdentifier, pInputFocusWidget->ConvertWindowToLocalCoordinates(windowCoordinate));
		}
	}

	void CommonInputActions::OnTabForward(const Input::TabAction::Mode mode)
	{
		// Pass input focus to the next available widget that can handle inputs
		Optional<Widget*> pNextCandidate;
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		switch (mode)
		{
			case Input::TabAction::Mode::Widget:
			{
				if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
				{
					pNextCandidate = pInputFocusWidget->GetSubsequentSiblingInHierarchy();
				}
				else
				{
					pNextCandidate = scene.GetRootWidget().GetFirstChild();
				}

				while (pNextCandidate != nullptr)
				{
					if (pNextCandidate->CanReceiveInputFocus(sceneRegistry))
					{
						window.SetInputFocusWidget(pNextCandidate);
						return;
					}

					pNextCandidate = pNextCandidate->GetSubsequentWidgetInHierarchy();
				}
			}
			break;
			case Input::TabAction::Mode::Tabs:
			{
				if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
				{
					pInputFocusWidget->HandleTabForward();
				}
			}
			break;
		}
	}

	void CommonInputActions::OnTabBack(const Input::TabAction::Mode mode)
	{
		// Pass input focus to the next (in the reverse direction) available widget that can handle inputs
		Optional<Widget*> pNextCandidate;
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		switch (mode)
		{
			case Input::TabAction::Mode::Widget:
			{
				if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
				{
					pNextCandidate = pInputFocusWidget->GetPreviousSiblingInHierarchy();
				}
				else
				{
					// Start with the very last widget
					pNextCandidate = scene.GetRootWidget();
					Optional<Widget*> pPossibleCandidate;
					do
					{
						pPossibleCandidate = pNextCandidate->GetSubsequentWidgetInHierarchy();
						if (pPossibleCandidate.IsValid())
						{
							pNextCandidate = pPossibleCandidate;
						}
					} while (pPossibleCandidate.IsValid());
				}

				while (pNextCandidate != nullptr)
				{
					if (pNextCandidate->CanReceiveInputFocus(sceneRegistry))
					{
						window.SetInputFocusWidget(pNextCandidate);
						return;
					}

					pNextCandidate = pNextCandidate->GetPreviousWidgetInHierarchy();
				}
			}
			break;
			case Input::TabAction::Mode::Tabs:
			{
				if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
				{
					pInputFocusWidget->HandleTabBack();
				}
			}
			break;
		}
	}

	void CommonInputActions::OnTextInput(const ConstUnicodeStringView text)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleTextInput(text);
		}
	}

	void CommonInputActions::OnCopy()
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleCopyToPasteboard();
		}
	}

	void CommonInputActions::OnPaste()
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandlePasteFromPasteboard();
		}
	}

	void CommonInputActions::OnMoveTextCursor(const EnumFlags<Input::MoveTextCursorFlags> flags)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleMoveTextCursor(flags);
		}
	}

	void CommonInputActions::OnApplyTextInput()
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleApplyTextInput();
		}
	}

	void CommonInputActions::OnAbortTextInput()
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleAbortTextInput();
		}
	}

	void CommonInputActions::OnDeleteTextInput(const Input::DeleteTextType deleteTextType)
	{
		Widgets::Scene& scene = static_cast<Widgets::Scene&>(*m_sceneView.GetScene());
		RootWidget& rootWidget = scene.GetRootWidget();
		Rendering::ToolWindow& window = *rootWidget.GetOwningWindow();
		if (const Optional<Widget*> pInputFocusWidget = window.GetInputFocusWidget())
		{
			pInputFocusWidget->HandleDeleteTextInput(deleteTextType);
		}
	}

	void UIViewMode::OnPause(const Input::DeviceIdentifier)
	{
		if (m_sceneView.GetScene()->IsEditing())
		{
			if (const Optional<SceneViewModeBase*> pEditViewMode = m_sceneView.FindMode("614E9F7B-FD1E-471B-9845-0E6B7BF9F678"_guid))
			{
				if (m_sceneView.GetCurrentMode() != pEditViewMode)
				{
					m_sceneView.ChangeMode(pEditViewMode);
				}
			}
		}
	}
}
