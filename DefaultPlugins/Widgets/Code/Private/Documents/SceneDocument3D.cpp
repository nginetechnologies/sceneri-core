#include "Widgets/Documents/SceneDocument3D.h"

#include <Engine/Engine.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Project/Project.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Scene/SceneViewMode.h>

#include <Widgets/ToolWindow.h>
#include <Widgets/Widget.inl>
#include <Widgets/Style/Entry.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Widgets::Document
{
	Scene3D::Scene3D(Initializer&& initializer)
		: Widget(initializer | Widget::Flags::HasCustomFramegraph)
		, m_sceneView(
				initializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				*this,
				*this,
				initializer.GetParent()->GetOwningWindow()->GetOutput(),
				Rendering::SceneView::Flags::IsEnabled * (IsVisible() && initializer.GetParent()->GetOwningWindow()->IsInForeground())
			)
		, m_flags(initializer.m_sceneWidgetFlags)
	{
	}

	Scene3D::Scene3D(const Deserializer& deserializer)
		: Widget(deserializer | Widget::Flags::HasCustomFramegraph)
		, m_sceneView(
				deserializer.GetParent()->GetOwningWindow()->GetLogicalDevice(),
				*this,
				*this,
				deserializer.GetParent()->GetOwningWindow()->GetOutput(),
				Rendering::SceneView::Flags::IsEnabled * (IsVisible() && deserializer.GetParent()->GetOwningWindow()->IsInForeground())
			)
	{
	}

	Scene3D::~Scene3D()
	{
		Assert(m_sceneView.GetSceneChecked() == nullptr);
	}

	void Scene3D::OnCreated()
	{
		BaseType::OnCreated();

		if (IsVisible())
		{
			GetOwningWindow()->InvalidateFramegraph();
		}
	}

	void Scene3D::OnBecomeVisible()
	{
		if (GetOwningWindow()->IsInForeground())
		{
			m_sceneView.Enable();
		}
		GetOwningWindow()->InvalidateFramegraph();
	}

	void Scene3D::OnSwitchToForeground()
	{
		if (IsVisible())
		{
			m_sceneView.Enable();
		}
	}

	void Scene3D::OnBecomeHidden()
	{
		m_sceneView.Disable();
	}

	void Scene3D::OnSwitchToBackground()
	{
		m_sceneView.Disable();
	}

	void Scene3D::DoRenderPass(
		const Rendering::CommandEncoderView commandEncoder,
		const Rendering::RenderPassView renderPass,
		const Rendering::FramebufferView framebuffer,
		const Math::Rectangleui extent,
		const ArrayView<const Rendering::ClearValue, uint8> clearValues,
		RenderPassCallback&& callback,
		const uint32 maximumPushConstantInstanceCount
	)
	{
#if SPLIT_SCREEN_TEST
		auto getEyeRenderAreaRatio = [](const uint8 eyeIndex) -> Math::Rectanglef
		{
			return {{0.5f * eyeIndex, 0}, {0.5f, 1.f}};
		};

		Entity::CameraComponent* pCamera = m_sceneView.GetActiveCameraComponentSafe();
		Math::WorldTransform camerTransform = pCamera != nullptr ? pCamera->GetWorldTransform() : Math::WorldTransform(Math::Identity);

		{
			const Math::Rectanglef ratio = getEyeRenderAreaRatio(0);
			const Math::Rectangleui renderArea{
				extent.GetPosition() + Math::Vector2ui((Math::Vector2f)extent.GetSize() * ratio.GetPosition()),
				Math::Vector2ui((Math::Vector2f)extent.GetSize() * ratio.GetSize())
			};

			Rendering::RenderCommandEncoder renderCommandEncoder =
				commandEncoder.BeginRenderPass(renderPass, framebuffer, renderArea, clearValues);

			const Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				camerTransform.GetLocation(),
				camerTransform.GetLocation() + camerTransform.GetForwardColumn(),
				camerTransform.GetUpColumn()
			);

			Rendering::ViewMatrices matrices = m_sceneView.GetMatrices();
			matrices = Rendering::ViewMatrices(
				viewMatrix,
				matrices.m_viewProjectionMatrices[Rendering::ViewMatrices::Type::Projection],
				camerTransform.GetLocation(),
				camerTransform.GetRotationMatrix()
			);

			renderCommandEncoder.SetViewport(renderArea);
			callback(renderCommandEncoder, matrices, renderArea);
		}
		{
			const Math::Rectanglef ratio = getEyeRenderAreaRatio(1);
			const Math::Rectangleui renderArea{
				extent.GetPosition() + Math::Vector2ui((Math::Vector2f)extent.GetSize() * ratio.GetPosition()),
				Math::Vector2ui((Math::Vector2f)extent.GetSize() * ratio.GetSize())
			};

			Rendering::RenderCommandEncoder renderCommandEncoder =
				commandEncoder.BeginRenderPass(renderPass, framebuffer, renderArea, clearValues);

			camerTransform.SetLocation(camerTransform.GetLocation() + Math::Vector3f{0.5f, 0.f, 0.f});

			const Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				camerTransform.GetLocation(),
				camerTransform.GetLocation() + camerTransform.GetForwardColumn(),
				camerTransform.GetUpColumn()
			);

			Rendering::ViewMatrices matrices = m_sceneView.GetMatrices();
			matrices = Rendering::ViewMatrices(
				viewMatrix,
				matrices.m_viewProjectionMatrices[Rendering::ViewMatrices::Type::Projection],
				camerTransform.GetLocation(),
				camerTransform.GetRotationMatrix()
			);

			renderCommandEncoder.SetViewport(renderArea);
			callback(renderCommandEncoder, matrices, renderArea);
		}
#else
		Rendering::RenderCommandEncoder renderCommandEncoder =
			commandEncoder
				.BeginRenderPass(m_sceneView.GetLogicalDevice(), renderPass, framebuffer, extent, clearValues, maximumPushConstantInstanceCount);
		callback(renderCommandEncoder, m_sceneView.GetMatrices(), extent);
#endif
	}

	void Scene3D::DoComputePass(ComputePassCallback&& callback)
	{
#if SPLIT_SCREEN_TEST
		Entity::CameraComponent* pCamera = m_sceneView.GetActiveCameraComponentSafe();
		Math::WorldTransform camerTransform = pCamera != nullptr ? pCamera->GetWorldTransform() : Math::WorldTransform(Math::Identity);
		{
			const Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				camerTransform.GetLocation(),
				camerTransform.GetLocation() + camerTransform.GetForwardColumn(),
				camerTransform.GetUpColumn()
			);

			Rendering::ViewMatrices matrices = m_sceneView.GetMatrices();
			matrices = Rendering::ViewMatrices(
				viewMatrix,
				matrices.m_viewProjectionMatrices[Rendering::ViewMatrices::Type::Projection],
				camerTransform.GetLocation(),
				camerTransform.GetRotationMatrix()
			);
			callback(matrices);
		}
		{
			camerTransform.SetLocation(camerTransform.GetLocation() + Math::Vector3f{0.5f, 0.f, 0.f});

			const Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				camerTransform.GetLocation(),
				camerTransform.GetLocation() + camerTransform.GetForwardColumn(),
				camerTransform.GetUpColumn()
			);

			Rendering::ViewMatrices matrices = m_sceneView.GetMatrices();
			matrices = Rendering::ViewMatrices(
				viewMatrix,
				matrices.m_viewProjectionMatrices[Rendering::ViewMatrices::Type::Projection],
				camerTransform.GetLocation(),
				camerTransform.GetRotationMatrix()
			);
			callback(matrices);
		}
#else
		callback(m_sceneView.GetMatrices());
#endif
	}

	Rendering::Framegraph& Scene3D::GetFramegraph()
	{
		return GetOwningWindow()->GetFramegraph();
	}

	void Scene3D::OnEnableFramegraph()
	{
		if (ngine::Scene3D* pScene = m_sceneView.GetSceneChecked())
		{
			pScene->ModifyFrameGraph(
				[&scene = *pScene]()
				{
					scene.Enable();
				}
			);
		}
	}

	void Scene3D::OnDisableFramegraph()
	{
		if (ngine::Scene3D* pScene = m_sceneView.GetSceneChecked())
		{
			pScene->ModifyFrameGraph(
				[&scene = *pScene]()
				{
					scene.Disable();
				}
			);
		}
	}

	void Scene3D::OnBeforeContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags)
	{
		if (changeFlags.AreAnySet(ContentAreaChangeFlags::SizeChanged))
		{
			m_sceneView.OnBeforeResizeRenderOutput();
		}
	}

	void Scene3D::OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags)
	{
		Widget::OnContentAreaChanged(changeFlags);
		if (changeFlags.AreAnySet(ContentAreaChangeFlags::SizeChanged))
		{
			m_sceneView.OnAfterResizeRenderOutput();
		}

		if (IsVisible())
		{
			GetOwningWindow()->InvalidateFramegraph();
		}
	}

	void Scene3D::AssignScene(ngine::Scene3D& scene)
	{
		if (m_sceneView.GetSceneChecked() != nullptr)
		{
			UnloadScene();
		}

		scene.SetMaximumUpdateRate(GetOwningWindow()->GetMaximumScreenRefreshRate());

		const Optional<SceneViewModeBase*> pMode = m_sceneView.GetCurrentMode();
		if (pMode != nullptr)
		{
			m_sceneView.ChangeMode(nullptr);
		}

		m_sceneView.AssignScene(scene);

		OnSceneChanged();

		if (pMode.IsValid() && m_sceneView.GetCurrentMode() == nullptr)
		{
			m_sceneView.ChangeMode(pMode);
		}

		ngine::PropertySource::Interface::OnDataChanged();
	}

	void Scene3D::AssignScene(UniquePtr<ngine::Scene3D>&& pScene)
	{
		Assert(pScene.IsValid());
		AssignScene(*pScene.StealOwnership());
		m_flags |= Flags::IsSceneOwned;
	}

	void Scene3D::UnloadScene()
	{
		const Optional<ngine::Scene3D*> pScene = m_sceneView.GetSceneChecked();
		const bool isSceneOwned = m_flags.IsSet(Flags::IsSceneOwned);
		if (pScene != nullptr)
		{
			if (isSceneOwned)
			{
				pScene->OnBeforeUnload();
			}

			OnSceneUnloadedInternal();

			m_sceneView.ChangeMode(nullptr);

			if (isSceneOwned)
			{
				Engine& engine = System::Get<Engine>();
				engine.OnBeforeStartFrame.Add(
					*this,
					[pScene = UniquePtr<ngine::Scene3D>::FromRaw(pScene)](Scene3D&)
					{
					}
				);
			}
		}
	}

	void Scene3D::OnSceneUnloadedInternal()
	{
		const Optional<ngine::Scene3D*> pScene = m_sceneView.GetSceneChecked();
		Assert(pScene.IsValid());

		m_sceneView.DetachCurrentScene();
		m_flags.Clear(Flags::HasSceneFinishedLoading);

		ngine::PropertySource::Interface::OnDataChanged();

		OnSceneUnloaded(*pScene);
	}

	void Scene3D::OnSceneFinishedLoading(ngine::Scene3D& scene)
	{
		m_flags |= Flags::HasSceneFinishedLoading;

		Rendering::SceneView& sceneView = GetSceneView();
		sceneView.OnSceneFinishedLoading(scene);

		ngine::PropertySource::Interface::OnDataChanged();
	}

	Optional<Input::Monitor*> Scene3D::GetFocusedInputMonitorAtCoordinates(const WindowCoordinate)
	{
		if (const Optional<SceneViewModeBase*> pMode = m_sceneView.GetCurrentMode())
		{
			return pMode->GetInputMonitor();
		}
		return nullptr;
	}

	Optional<Input::Monitor*> Scene3D::GetFocusedInputMonitor()
	{
		if (const Optional<SceneViewModeBase*> pMode = m_sceneView.GetCurrentMode())
		{
			return pMode->GetInputMonitor();
		}
		return nullptr;
	}

	ngine::DataSource::PropertyValue Scene3D::GetDataProperty(const ngine::DataSource::PropertyIdentifier identifier) const
	{
		if (ngine::DataSource::PropertyValue propertyValue = BaseType::GetDataProperty(identifier); propertyValue.HasValue())
		{
			return propertyValue;
		}

		if (Project& project = System::Get<Project>(); project.IsValid())
		{
			ngine::DataSource::PropertyValue propertyValue = project.GetDataProperty(identifier);
			if (propertyValue.HasValue())
			{
				return propertyValue;
			}
		}

		return {};
	}

	Math::Matrix4x4f Scene3D::CreateProjectionMatrix(
		const Math::Anglef fieldOfView, const Math::Vector2f renderResolution, const Math::Lengthf nearPlane, const Math::Lengthf farPlane
	) const
	{
		return static_cast<SceneViewMode&>(*m_sceneView.GetCurrentMode())
		  .CreateProjectionMatrix(fieldOfView, renderResolution, nearPlane, farPlane);
	}

	ArrayView<const Guid, uint16> Scene3D::GetSupportedDocumentAssetTypeGuids() const
	{
		constexpr Array supportedDocumentAssetTypeGuids{Scene3DAssetType::AssetFormat.assetTypeGuid};
		return supportedDocumentAssetTypeGuids.GetView();
	}

	Threading::JobBatch Scene3D::
		OpenDocumentAssetInternal(const DocumentData&, const Serialization::Data&, const Asset::DatabaseEntry&, const EnumFlags<OpenDocumentFlags>)
	{
		Assert(false, "TODO");
		return {};
	}

	void Scene3D::CloseDocument()
	{
		UnloadScene();
		GetOwningWindow()->InvalidateFramegraph();
	}

	[[maybe_unused]] const bool wasSceneDocument3DTypeRegistered = Reflection::Registry::RegisterType<Scene3D>();
	[[maybe_unused]] const bool wasSceneDocument3DComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Scene3D>>::Make());
}
