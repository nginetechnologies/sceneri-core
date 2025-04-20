#include "Widgets/Documents/WidgetDocument.h"
#include "Widgets/ViewModes/UIViewMode.h"

#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/ExternalScene.h>
#include <Engine/Input/InputManager.h>

#include <Renderer/Renderer.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Framegraph/FramegraphBuilder.h>
#include <Renderer/Window/DocumentData.h>

#include <Widgets/ToolWindow.h>
#include <Widgets/RootWidget.h>
#include <Widgets/WidgetScene.h>
#include <Widgets/Manager.h>
#include <Widgets/Stages/WidgetDrawingStage.h>
#include <Widgets/Stages/WidgetCopyToScreenStage.h>
#include <Widgets/WidgetAssetType.h>
#include <Widgets/Data/Tabs.h>

#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Asset/AssetDatabaseEntry.h>

namespace ngine::Widgets::Document
{
	// TODO: Load the viewport widget from disk to make it fully data driven (-> preview popups become viable)
	// TODO: Test preview popup widget

	WidgetDocument::WidgetDocument(Initializer&& initializer)
		: BaseType(initializer | Widget::Flags::HasCustomFramegraph)
		, m_pQuadtreeTraversalStage(Memory::ConstructInPlace, initializer.GetParent()->GetOwningWindow()->GetLogicalDevice(), GetSceneView())
		, m_pWidgetDrawingStage(
				Memory::ConstructInPlace,
				initializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				GetSceneView(),
				*initializer.GetParent()->GetOwningWindow()
			)
		, m_pWidgetsCopyToScreenStage(
				Memory::ConstructInPlace,
				initializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				"f0bbc81f-08ff-48f3-993d-c0682a7fa3e4"_guid
			)
	{
		GetSceneView().OnEnabled.Add(
			this,
			[](WidgetDocument& document, ngine::SceneBase& scene)
			{
				document.EnableFramegraph(static_cast<Widgets::Scene&>(scene), document.GetOwningWindow()->GetFramegraph());
			}
		);
		GetSceneView().OnDisabled.Add(
			this,
			[](WidgetDocument& document, ngine::SceneBase& scene)
			{
				document.DisableFramegraph(static_cast<Widgets::Scene&>(scene), document.GetOwningWindow()->GetFramegraph());
			}
		);
	}

	WidgetDocument::WidgetDocument(const Deserializer& deserializer)
		: BaseType(deserializer | Widget::Flags::HasCustomFramegraph)
		, m_pQuadtreeTraversalStage(Memory::ConstructInPlace, deserializer.GetParent()->GetOwningWindow()->GetLogicalDevice(), GetSceneView())
		, m_pWidgetDrawingStage(
				Memory::ConstructInPlace,
				deserializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				GetSceneView(),
				*deserializer.GetParent()->GetOwningWindow()
			)
		, m_pWidgetsCopyToScreenStage(
				Memory::ConstructInPlace,
				deserializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				"f0bbc81f-08ff-48f3-993d-c0682a7fa3e4"_guid
			)
	{
		GetSceneView().OnEnabled.Add(
			this,
			[](WidgetDocument& document, ngine::SceneBase& scene)
			{
				document.EnableFramegraph(static_cast<Widgets::Scene&>(scene), document.GetOwningWindow()->GetFramegraph());
			}
		);
		GetSceneView().OnDisabled.Add(
			this,
			[](WidgetDocument& document, ngine::SceneBase& scene)
			{
				document.DisableFramegraph(static_cast<Widgets::Scene&>(scene), document.GetOwningWindow()->GetFramegraph());
			}
		);
	}

	WidgetDocument::~WidgetDocument() = default;

	void WidgetDocument::OnCreated()
	{
		BaseType::OnCreated();

		if (IsVisible())
		{
			GetOwningWindow()->InvalidateFramegraph();
		}
	}

	void WidgetDocument::OnFramegraphInvalidated()
	{
		GetSceneView().InvalidateFramegraph();
	}

	void WidgetDocument::BuildFramegraph(Rendering::FramegraphBuilder& framegraphBuilder)
	{
		Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		const Math::Rectangleui outputArea = (Math::Rectangleui)GetContentArea();

		if (IsSceneEditable())
		{
			framegraphBuilder.EmplaceStage(Rendering::Framegraph::RenderPassStageDescription{
				"Draw widgets",
				Rendering::Framegraph::InvalidPassIndex,
				Invalid,
				Invalid,
				outputArea,
				framegraphBuilder.EmplaceColorAttachments(Array{
					Rendering::Framegraph::ColorAttachmentDescription{
						textureCache.FindOrRegisterRenderTargetTemplate(Rendering::Framegraph::RenderOutputRenderTargetGuid),
						outputArea.GetSize(),
						Rendering::MipRange(0, 1),
						Rendering::ArrayRange(0, 1),
						Rendering::Framegraph::AttachmentFlags::Clear | Rendering::Framegraph::AttachmentFlags::CanStore,
						"#575757"_colorf
					},
				})
			});
		}

		SceneViewDrawer& sceneViewDrawer = GetSceneView().GetDrawer();
		const Rendering::Framegraph::StageIndex quadtreeTraversalStageIndex = framegraphBuilder.GetNextAvailableStageIndex();
		framegraphBuilder.EmplaceStage(Rendering::Framegraph::GenericStageDescription{
			"Quadtree traversal",
			Rendering::Framegraph::InvalidStageIndex,
			*m_pQuadtreeTraversalStage
		});

		const Rendering::RenderTargetTemplateIdentifier widgetRenderTargetTemplateIdentifier =
			textureCache.FindOrRegisterRenderTargetTemplate(Widgets::WidgetDrawingStage::WidgetRenderTargetGuid);

		framegraphBuilder.EmplaceStage(Rendering::Framegraph::RenderPassStageDescription{
			"Draw widgets",
			quadtreeTraversalStageIndex,
			m_pWidgetDrawingStage.Get(),
			sceneViewDrawer,
			outputArea,
			framegraphBuilder.EmplaceColorAttachments(Array{
				Rendering::Framegraph::ColorAttachmentDescription{
					widgetRenderTargetTemplateIdentifier,
					outputArea.GetSize(),
					Rendering::MipRange(0, 1),
					Rendering::ArrayRange(0, 1),
					Rendering::Framegraph::AttachmentFlags::Clear | Rendering::Framegraph::AttachmentFlags::CanStore,
					Math::Color{0.f, 0.f, 0.f, 0.f}
				},
			}),
			Rendering::Framegraph::DepthAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(Widgets::WidgetDrawingStage::WidgetDepthRenderTargetGuid),
				outputArea.GetSize(),
				Rendering::MipRange(0, 1),
				Rendering::ArrayRange(0, 1),
				Rendering::Framegraph::AttachmentFlags::Clear | Rendering::Framegraph::AttachmentFlags::CanStore,
				Rendering::DepthValue{0.f}
			}
		});

		framegraphBuilder.EmplaceStage(Rendering::Framegraph::RenderPassStageDescription{
			"Copy widgets to screen",
			Rendering::Framegraph::InvalidStageIndex,
			m_pWidgetsCopyToScreenStage.Get(),
			sceneViewDrawer,
			outputArea,
			framegraphBuilder.EmplaceColorAttachments(Array{Rendering::Framegraph::ColorAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(Rendering::Framegraph::RenderOutputRenderTargetGuid),
				outputArea.GetSize(),
				Rendering::MipRange(0, 1),
				Rendering::ArrayRange(0, 1),
				Rendering::Framegraph::AttachmentFlags::CanRead | Rendering::Framegraph::AttachmentFlags::CanStore,
				Optional<Math::Color>{}
			}}),
			Optional<Rendering::Framegraph::DepthAttachmentDescription>{},
			Optional<Rendering::Framegraph::StencilAttachmentDescription>{},
			framegraphBuilder.EmplaceInputAttachments(Array{Rendering::Framegraph::InputAttachmentDescription{
				textureCache.FindOrRegisterRenderTargetTemplate(Widgets::WidgetDrawingStage::WidgetRenderTargetGuid),
				outputArea.GetSize(),
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange(0, 1), Rendering::ArrayRange(0, 1)}
			}})
		});
	}

	void WidgetDocument::OnFramegraphBuilt()
	{
		GetSceneView().OnFramegraphBuilt();
	}

	void WidgetDocument::EnableFramegraph(Widgets::Scene& scene, Rendering::Framegraph& framegraph)
	{
		Threading::Job& recalculateWidgetsHierarchyStage = scene.GetRootWidget().GetRecalculateWidgetsHierarchyStage();

		const Optional<Rendering::Stage*> pQuadtreeTraversalPass = framegraph.GetStagePass(*m_pQuadtreeTraversalStage);
		const Optional<Rendering::Stage*> pWidgetDrawingStage = framegraph.GetStagePass(*m_pWidgetDrawingStage);

		Assert(pQuadtreeTraversalPass.IsValid());
		Assert(pWidgetDrawingStage.IsValid());
		if (LIKELY(pQuadtreeTraversalPass.IsValid() && pWidgetDrawingStage.IsValid()))
		{
			recalculateWidgetsHierarchyStage.AddSubsequentStage(*pQuadtreeTraversalPass);

			scene.GetEntitySceneRegistry().GetDynamicRenderUpdatesFinishedStage().AddSubsequentStage(*pQuadtreeTraversalPass);
			// TODO: Introduce late stage visibility check stage like we do for 3D?
			scene.GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*pQuadtreeTraversalPass);

			pQuadtreeTraversalPass->AddSubsequentCpuStage(scene.GetRootComponent().GetQuadtreeCleanupJob());

			pQuadtreeTraversalPass->AddSubsequentCpuStage(scene.GetEndFrameStage());
			pQuadtreeTraversalPass->AddSubsequentCpuStage(scene.GetDestroyComponentsStage());

			scene.GetDestroyComponentsStage().AddSubsequentStage(*pWidgetDrawingStage);
			pWidgetDrawingStage->AddSubsequentCpuStage(scene.GetRootComponent().GetQuadtreeCleanupJob());

			// Ensure that components aren't destroyed while processing input
			Input::Manager& inputManager = System::Get<Input::Manager>();
			inputManager.GetPolledForInputStage().AddSubsequentStage(scene.GetDestroyComponentsStage());
		}
	}

	void WidgetDocument::DisableFramegraph(Widgets::Scene& scene, Rendering::Framegraph& framegraph)
	{
		Threading::Job& recalculateWidgetsHierarchyStage = scene.GetRootWidget().GetRecalculateWidgetsHierarchyStage();

		const Optional<Rendering::Stage*> pQuadtreeTraversalPass = framegraph.GetStagePass(*m_pQuadtreeTraversalStage);
		const Optional<Rendering::Stage*> pWidgetDrawingStage = framegraph.GetStagePass(*m_pWidgetDrawingStage);

		Assert(pQuadtreeTraversalPass.IsValid());
		Assert(pWidgetDrawingStage.IsValid());
		if (LIKELY(pQuadtreeTraversalPass.IsValid() && pWidgetDrawingStage.IsValid()))
		{
			recalculateWidgetsHierarchyStage.RemoveSubsequentStage(*pQuadtreeTraversalPass, Invalid, Threading::Job::RemovalFlags{});

			scene.GetEntitySceneRegistry()
				.GetDynamicRenderUpdatesFinishedStage()
				.RemoveSubsequentStage(*pQuadtreeTraversalPass, Invalid, Threading::Job::RemovalFlags{});
			// TODO: Introduce late stage visibility check stage like we do for 3D?
			scene.GetEntitySceneRegistry()
				.GetDynamicLateUpdatesFinishedStage()
				.RemoveSubsequentStage(*pQuadtreeTraversalPass, Invalid, Threading::Job::RemovalFlags{});

			pQuadtreeTraversalPass
				->RemoveSubsequentCpuStage(scene.GetRootComponent().GetQuadtreeCleanupJob(), Invalid, Threading::Job::RemovalFlags{});

			pQuadtreeTraversalPass->RemoveSubsequentCpuStage(scene.GetEndFrameStage(), Invalid, Threading::Job::RemovalFlags{});
			pQuadtreeTraversalPass->RemoveSubsequentCpuStage(scene.GetDestroyComponentsStage(), Invalid, Threading::Job::RemovalFlags{});

			scene.GetDestroyComponentsStage().RemoveSubsequentStage(*pWidgetDrawingStage, Invalid, Threading::Job::RemovalFlags{});
			pWidgetDrawingStage
				->RemoveSubsequentCpuStage(scene.GetRootComponent().GetQuadtreeCleanupJob(), Invalid, Threading::Job::RemovalFlags{});

			// Ensure that components aren't destroyed while processing input
			Input::Manager& inputManager = System::Get<Input::Manager>();
			inputManager.GetPolledForInputStage()
				.RemoveSubsequentStage(scene.GetDestroyComponentsStage(), Invalid, Threading::Job::RemovalFlags{});
		}
	}

	Rendering::Stage& WidgetDocument::GetQuadtreeTraversalStage()
	{
		return *m_pQuadtreeTraversalStage;
	}

	Rendering::Stage& WidgetDocument::GetWidgetDrawToRenderTargetStage()
	{
		return *m_pWidgetDrawingStage;
	}

	Rendering::Stage& WidgetDocument::GetWidgetCopyToScreenStage()
	{
		return *m_pWidgetsCopyToScreenStage;
	}

	void WidgetDocument::OnSceneChanged()
	{
		Optional<SceneViewModeBase*> pPlayViewMode = m_sceneView.FindMode(Widgets::UIViewMode::TypeGuid);
		if (pPlayViewMode.IsInvalid())
		{
			pPlayViewMode = &m_sceneView.RegisterMode<Widgets::UIViewMode>(m_sceneView);
		}

		if (pPlayViewMode)
		{
			if (m_sceneView.GetCurrentMode() != pPlayViewMode)
			{
				m_sceneView.ChangeMode(*pPlayViewMode);
			}
		}
	}

	Asset::Reference WidgetDocument::GetDocumentAssetReference() const
	{
		if (const Optional<Widgets::Widget*> pEditedWidget = GetEditedWidget())
		{
			Entity::SceneRegistry& sceneRegistry = pEditedWidget->GetSceneRegistry();
			Entity::Data::ExternalScene& externalScene = *pEditedWidget->FindDataComponentOfType<Entity::Data::ExternalScene>(sceneRegistry);

			return Asset::Reference{externalScene.GetAssetGuid(), WidgetAssetType::AssetFormat.assetTypeGuid};
		}
		else
		{
			return Asset::Reference{{}, WidgetAssetType::AssetFormat.assetTypeGuid};
		}
	}

	ArrayView<const Guid, uint16> WidgetDocument::GetSupportedDocumentAssetTypeGuids() const
	{
		constexpr Array supportedDocumentAssetTypeGuids{WidgetAssetType::AssetFormat.assetTypeGuid};
		return supportedDocumentAssetTypeGuids.GetView();
	}

	Threading::JobBatch WidgetDocument::OpenDocumentAssetInternal(
		const DocumentData&,
		const Serialization::Data& data,
		const Asset::DatabaseEntry& assetEntry,
		const EnumFlags<OpenDocumentFlags> openDocumentFlags
	)
	{
		if (assetEntry.m_assetTypeGuid == WidgetAssetType::AssetFormat.assetTypeGuid)
		{
			if (openDocumentFlags.IsSet(OpenDocumentFlags::EnableEditing))
			{
				AllowSceneEditing();
			}
			else
			{
				DisallowSceneEditing();
			}

			if (const Widgets::Widget::DataComponentResult<Widgets::Data::Tabs> queryTabsResult = GetRootWidget().FindFirstDataComponentOfTypeInChildrenRecursive<Widgets::Data::Tabs>(GetSceneRegistry()))
			{
				Widgets::Widget& tabsOwner = static_cast<Widgets::Widget&>(*queryTabsResult.m_pDataComponentOwner);
				return Widgets::Widget::Deserialize(
					Serialization::Reader(data),
					tabsOwner,
					GetSceneRegistry(),
					[this](const Optional<Widgets::Widget*> pWidget)
					{
						SetEditedWidget(pWidget);
					}
				);
			}
			else
			{
				return Widgets::Widget::Deserialize(
					Serialization::Reader(data),
					GetRootWidget(),
					GetSceneRegistry(),
					[this](const Optional<Widgets::Widget*> pWidget)
					{
						SetEditedWidget(pWidget);
					},
					0
				);
			}
		}
		else
		{
			return {};
		}
	}

	void WidgetDocument::CloseDocument()
	{
		Assert(false, "TODO");
		GetOwningWindow()->InvalidateFramegraph();
	}

	[[maybe_unused]] const bool wasWidgetDocumentTypeRegistered = Reflection::Registry::RegisterType<WidgetDocument>();
	[[maybe_unused]] const bool wasWidgetDocumentComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<WidgetDocument>>::Make());
}
