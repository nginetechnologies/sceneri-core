#include "Widgets/Documents/SceneDocument2D.h"

#include <Engine/Engine.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Scene/Scene2DAssetType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Input/InputManager.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Scene/SceneViewModeBase.h>

#include <Widgets/ToolWindow.h>
#include <Widgets/WidgetScene.h>
#include <Widgets/RootWidget.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Document
{
	Scene2D::Scene2D(Initializer&& initializer)
		: Widget(Forward<Initializer>(initializer))
		, m_sceneView(
				initializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				*this,
				initializer.GetParent()->GetOwningWindow()->GetOutput(),
				Rendering::SceneView2D::Flags::IsEnabled * IsVisible()
			)
		, m_flags(initializer.m_sceneWidgetFlags)
	{
		m_sceneView.OnCameraPropertiesChanged(
			// TODO: Camera component setup, get view area & rotation
			Math::Rectanglef{Math::Zero, (Math::Vector2f)GetSize()},
			0_degrees
		);
	}

	Scene2D::Scene2D(const Deserializer& deserializer)
		: Widget(deserializer)
		, m_sceneView(
				deserializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				*this,
				deserializer.GetParent()->GetOwningWindow()->GetOutput(),
				Rendering::SceneView2D::Flags::IsEnabled * IsVisible()
			)
	{
		m_sceneView.OnCameraPropertiesChanged(
			// TODO: Camera component setup, get view area & rotation
			Math::Rectanglef{Math::Zero, (Math::Vector2f)GetSize()},
			0_degrees
		);
	}

	Scene2D::~Scene2D()
	{
	}

	void Scene2D::OnCreated()
	{
		BaseType::OnCreated();

		// TODO: Enable again when quadtree sits inside the scene view
		// GetOwningWindow()->InvalidateFramegraph();
	}

	void Scene2D::OnBecomeVisible()
	{
		m_sceneView.Enable();
		GetOwningWindow()->InvalidateFramegraph();
	}

	void Scene2D::OnBecomeHidden()
	{
		m_sceneView.Disable();
	}

	void Scene2D::DoRenderPass(
		const Rendering::CommandEncoderView commandEncoder,
		const Rendering::RenderPassView renderPass,
		const Rendering::FramebufferView framebuffer,
		const Math::Rectangleui extent,
		const ArrayView<const Rendering::ClearValue, uint8> clearValues,
		RenderPassCallback&& callback,
		const uint32 maximumPushConstantInstanceCount
	)
	{
		Rendering::RenderCommandEncoder renderCommandEncoder =
			commandEncoder
				.BeginRenderPass(m_sceneView.GetLogicalDevice(), renderPass, framebuffer, extent, clearValues, maximumPushConstantInstanceCount);
		callback(renderCommandEncoder, m_sceneView.GetMatrices(), extent);
	}

	void Scene2D::DoComputePass(ComputePassCallback&& callback)
	{
		callback(m_sceneView.GetMatrices());
	}

	Math::Matrix4x4f Scene2D::CreateProjectionMatrix(
		[[maybe_unused]] const Math::Anglef fieldOfView,
		[[maybe_unused]] const Math::Vector2f renderResolution,
		[[maybe_unused]] const Math::Lengthf nearPlane,
		[[maybe_unused]] const Math::Lengthf farPlane
	) const
	{
		// TODO: Currently unused for 2D
		return Math::Identity;
	}

	Rendering::Framegraph& Scene2D::GetFramegraph()
	{
		return GetOwningWindow()->GetFramegraph();
	}

	void Scene2D::OnEnableFramegraph()
	{
		if (ngine::Scene2D* pScene = m_sceneView.GetScene())
		{
			pScene->ModifyFrameGraph(
				[this, &scene = *pScene]()
				{
					scene.Enable();

					Rendering::ToolWindow& toolWindow = *GetOwningWindow();
					Rendering::Framegraph& framegraph = toolWindow.GetFramegraph();

					Threading::Job& startFrameStage = scene.GetStartFrameStage();
					Threading::StageBase& endFrameStage = scene.GetEndFrameStage();
					Threading::Job& recalculateWidgetsHierarchyStage =
						static_cast<Widgets::Scene&>(scene).GetRootWidget().GetRecalculateWidgetsHierarchyStage();

					Input::Manager& inputManager = System::Get<Input::Manager>();
					inputManager.GetPolledForInputStage().AddSubsequentStage(recalculateWidgetsHierarchyStage);

					startFrameStage.AddSubsequentStage(framegraph.GetStartStage());
					startFrameStage.AddSubsequentStage(recalculateWidgetsHierarchyStage);
					framegraph.GetFinalStage().AddSubsequentStage(endFrameStage);
					framegraph.GetFinishFrameGpuExecutionStage().AddSubsequentStage(endFrameStage);
					recalculateWidgetsHierarchyStage.AddSubsequentStage(endFrameStage);
				}
			);
		}
	}

	void Scene2D::OnDisableFramegraph()
	{
		if (ngine::Scene2D* pScene = m_sceneView.GetScene())
		{
			pScene->ModifyFrameGraph(
				[this, &scene = *pScene]()
				{
					scene.Disable();

					Rendering::ToolWindow& toolWindow = *GetOwningWindow();
					Rendering::Framegraph& framegraph = toolWindow.GetFramegraph();

					Threading::Job& startFrameStage = scene.GetStartFrameStage();
					Threading::StageBase& endFrameStage = scene.GetEndFrameStage();
					Threading::Job& recalculateWidgetsHierarchyStage =
						static_cast<Widgets::Scene&>(scene).GetRootWidget().GetRecalculateWidgetsHierarchyStage();

					if (LIKELY(startFrameStage.IsDirectlyFollowedBy(framegraph.GetStartStage())))
					{

						Input::Manager& inputManager = System::Get<Input::Manager>();
						inputManager.GetPolledForInputStage()
							.RemoveSubsequentStage(recalculateWidgetsHierarchyStage, Invalid, Threading::Job::RemovalFlags{});

						startFrameStage.RemoveSubsequentStage(framegraph.GetStartStage(), Invalid, Threading::Job::RemovalFlags{});
						startFrameStage.RemoveSubsequentStage(recalculateWidgetsHierarchyStage, Invalid, Threading::Job::RemovalFlags{});
						framegraph.GetFinalStage().RemoveSubsequentStage(endFrameStage, Invalid, Threading::Job::RemovalFlags{});
						framegraph.GetFinishFrameGpuExecutionStage().RemoveSubsequentStage(endFrameStage, Invalid, Threading::Job::RemovalFlags{});
						recalculateWidgetsHierarchyStage.RemoveSubsequentStage(endFrameStage, Invalid, Threading::Job::RemovalFlags{});
					}
				}
			);
		}
	}

	void Scene2D::OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags)
	{
		if (changeFlags.AreAnySet(ContentAreaChangeFlags::SizeChanged))
		{
			m_sceneView.OnCameraPropertiesChanged(
				// TODO: Camera component setup, get view area & rotation
				Math::Rectanglef{Math::Zero, (Math::Vector2f)GetSize()},
				0_degrees
			);
		}
	}

	void Scene2D::AssignScene(ngine::Scene2D& scene)
	{
		if (m_sceneView.GetScene() != nullptr)
		{
			UnloadScene();
		}

		m_sceneView.AssignScene(scene);

		ngine::PropertySource::Interface::OnDataChanged();

		OnSceneChanged();
	}

	void Scene2D::AssignScene(UniquePtr<ngine::Scene2D>&& pScene)
	{
		Assert(pScene.IsValid());
		AssignScene(*pScene.StealOwnership());
		m_flags |= Flags::IsSceneOwned;
	}

	void Scene2D::UnloadScene()
	{
		const Optional<ngine::Scene2D*> pScene = m_sceneView.GetScene();
		const bool isSceneOwned = m_flags.IsSet(Flags::IsSceneOwned);
		if (pScene != nullptr && isSceneOwned)
		{
			pScene->OnBeforeUnload();
		}

		OnSceneUnloadedInternal();

		if (isSceneOwned)
		{
			Engine& engine = System::Get<Engine>();
			engine.OnBeforeStartFrame.Add(
				*this,
				[pScene = UniquePtr<ngine::Scene2D>::FromRaw(pScene)](Scene2D&)
				{
				}
			);
		}
	}

	void Scene2D::OnSceneUnloadedInternal()
	{
		const Optional<ngine::Scene2D*> pScene = m_sceneView.GetScene();
		Assert(pScene.IsValid());

		m_sceneView.DetachCurrentScene();
		m_flags.Clear(Flags::HasSceneFinishedLoading);

		ngine::PropertySource::Interface::OnDataChanged();

		OnSceneUnloaded(*pScene);
	}

	Optional<Input::Monitor*> Scene2D::GetFocusedInputMonitorAtCoordinates(const WindowCoordinate)
	{
		if (const Optional<SceneViewModeBase*> pMode = m_sceneView.GetCurrentMode())
		{
			return pMode->GetInputMonitor();
		}
		return nullptr;
	}

	Optional<Input::Monitor*> Scene2D::GetFocusedInputMonitor()
	{
		if (const Optional<SceneViewModeBase*> pMode = m_sceneView.GetCurrentMode())
		{
			return pMode->GetInputMonitor();
		}
		return nullptr;
	}

	void Scene2D::OnSceneFinishedLoading(ngine::Scene2D& scene)
	{
		m_flags |= Flags::HasSceneFinishedLoading;

		Rendering::SceneView2D& sceneView = GetSceneView();
		sceneView.OnSceneFinishedLoading(scene);

		ngine::PropertySource::Interface::OnDataChanged();
	}

	ArrayView<const Guid, uint16> Scene2D::GetSupportedDocumentAssetTypeGuids() const
	{
		constexpr Array supportedDocumentAssetTypeGuids{Scene2DAssetType::AssetFormat.assetTypeGuid};
		return supportedDocumentAssetTypeGuids.GetView();
	}

	Threading::JobBatch Scene2D::
		OpenDocumentAssetInternal(const DocumentData&, const Serialization::Data&, const Asset::DatabaseEntry&, const EnumFlags<OpenDocumentFlags>)
	{
		Assert(false, "TODO");
		return {};
	}

	void Scene2D::CloseDocument()
	{
		UnloadScene();
		GetOwningWindow()->InvalidateFramegraph();
	}

	[[maybe_unused]] const bool wasSceneDocument2DTypeRegistered = Reflection::Registry::RegisterType<Scene2D>();
	[[maybe_unused]] const bool wasSceneDocument2DComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Scene2D>>::Make());
}
