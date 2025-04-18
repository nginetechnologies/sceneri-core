#include "Scene/SceneView.h"
#include "Scene/SceneViewDrawer.h"

#include "Stages/StartFrameStage.h"
#include "Stages/OctreeTraversalStage.h"
#include "Stages/LateStageVisibilityCheckStage.h"
#include "Stages/PresentStage.h"
#include "Stages/RenderItemStage.h"
#include "Stages/MaterialsStage.h"
#include "Framegraph/Framegraph.h"
#include "Jobs/QueueSubmissionJob.h"

#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Primitives/CullingFrustum.h>
#include <Common/Math/Primitives/Transform/BoundingBox.h>

#include <Engine/Engine.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/BoundingBox.h>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/RenderItem/TransformChangeTracker.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Threading/JobRunnerThread.h>

#include <Renderer/Renderer.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering
{
	ViewMatrices::ViewMatrices(LogicalDevice& logicalDevice)
		: m_descriptorSetLayout(
				logicalDevice,
				Array{DescriptorSetLayout::Binding::MakeUniformBuffer(0, ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Compute)}
					.GetDynamicView()
			)
		, m_buffer(logicalDevice, logicalDevice.GetPhysicalDevice(), logicalDevice.GetDeviceMemoryPool(), sizeof(Data) * 2)
	{
		Threading::EngineJobRunnerThread& currentJobRunnerThread = *Threading::EngineJobRunnerThread::GetCurrent();
		const DescriptorPoolView descriptorPool = currentJobRunnerThread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
		[[maybe_unused]] const bool wasAllocated = descriptorPool.AllocateDescriptorSets(
			logicalDevice,
			Array<const DescriptorSetLayoutView, 1>{m_descriptorSetLayout},
			ArrayView<DescriptorSet, uint8>{m_descriptorSet}
		);
		Assert(wasAllocated);
		m_pDescriptorSetLoadingThread = &currentJobRunnerThread;

		Array<DescriptorSet::BufferInfo, 1> bufferInfo{DescriptorSet::BufferInfo{m_buffer, 0, sizeof(Data) * 2}};

		Array<DescriptorSet::UpdateInfo, 1> updateInfo{
			DescriptorSet::UpdateInfo{m_descriptorSet, 0u, 0u, DescriptorType::UniformBuffer, bufferInfo}
		};

		DescriptorSet::Update(logicalDevice, updateInfo);
	}

	void ViewMatrices::Destroy(LogicalDevice& logicalDevice)
	{
		m_buffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());

		Threading::EngineJobRunnerThread& descriptorSetLoadingThread =
			static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread);
		descriptorSetLoadingThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_descriptorSet));
		descriptorSetLoadingThread.GetRenderData().DestroyDescriptorSetLayout(logicalDevice.GetIdentifier(), Move(m_descriptorSetLayout));
	}

	void ViewMatrices::StartFrame(
		const uint8 frameIndex,
		const uint64 frameCounter,
		const float time,
		LogicalDevice& logicalDevice,
		const CommandQueueView graphicsCommandQueue,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		m_latestData.m_viewLocationAndTime.w = time;

		// Push jitter data
		constexpr float jitterData[32] = {0.500000f, 0.333333f, 0.250000f, 0.666667f, 0.750000f, 0.111111f, 0.125000f, 0.444444f,
		                                  0.625000f, 0.777778f, 0.375000f, 0.222222f, 0.875000f, 0.555556f, 0.062500f, 0.888889f,
		                                  0.562500f, 0.037037f, 0.312500f, 0.370370f, 0.812500f, 0.703704f, 0.187500f, 0.148148f,
		                                  0.687500f, 0.481481f, 0.437500f, 0.814815f, 0.937500f, 0.259259f, 0.031250f, 0.592593f};
		uint64 jitterIdx = frameCounter % 16; // 0-15

		const Math::Vector2d sourceJitter{jitterData[jitterIdx * 2], jitterData[jitterIdx * 2 + 1]};
		const Math::Vector2d viewResolution = (Math::Vector2d
		)Math::Vector2ui{m_latestData.m_renderAndOutputResolution.x, m_latestData.m_renderAndOutputResolution.y};
		// Jitter adjusted so we jitter in positive and negative directions + do not jitter more than the size of a pixel
		const Math::Vector2d jitterOffset = ((sourceJitter * 2.0f) - Math::Vector2d{1.0f}) / viewResolution * 0.5f;

		m_latestData.m_jitterOffset = {(float)jitterOffset.x, (float)jitterOffset.y, 0.0f, 0.0f};

		m_data[frameIndex] = m_latestData;
		m_frameIndex = frameIndex;
		const uint8 previousFrameIndex = (frameIndex + (Rendering::MaximumConcurrentFrameCount - 1)) % Rendering::MaximumConcurrentFrameCount;

		Array<Data, 2> data{m_latestData, m_data[previousFrameIndex]};
		perFrameStagingBuffer.CopyToBuffer(logicalDevice, graphicsCommandQueue, graphicsCommandEncoder, ConstByteView::Make(data), m_buffer);
	}

	void ViewMatrices::Assign(
		Math::Matrix4x4f viewMatrix,
		const Math::Matrix4x4f& projectionMatrix,
		const Math::WorldCoordinate viewLocation,
		const Math::Matrix3x3f viewRotation,
		const Math::Vector2ui renderResolution,
		const Math::Vector2ui outputResolution
	)
	{
		m_latestData.m_viewLocationAndTime.x = viewLocation.x;
		m_latestData.m_viewLocationAndTime.y = viewLocation.y;
		m_latestData.m_viewLocationAndTime.z = viewLocation.z;
		m_latestData.m_viewRotation = viewRotation;
		m_latestData.m_invertedViewRotation = viewRotation.GetInverted();

		m_latestData.m_matrices[ViewMatrices::Type::View] = viewMatrix;
		m_latestData.m_matrices[ViewMatrices::Type::Projection] = projectionMatrix;
		m_latestData.m_matrices[ViewMatrices::Type::ViewProjection] = viewMatrix * projectionMatrix;
		m_latestData.m_matrices[ViewMatrices::Type::InvertedViewProjection] =
			m_latestData.m_matrices[ViewMatrices::Type::ViewProjection].GetInverted();
		m_latestData.m_matrices[ViewMatrices::Type::InvertedView] = m_latestData.m_matrices[ViewMatrices::Type::View].GetInverted();
		m_latestData.m_matrices[ViewMatrices::Type::InvertedProjection] = m_latestData.m_matrices[ViewMatrices::Type::Projection].GetInverted();

		viewMatrix.m_rows[0][0] = 1;
		viewMatrix.m_rows[0][1] = 0;
		viewMatrix.m_rows[0][2] = 0;

		viewMatrix.m_rows[2][0] = 0;
		viewMatrix.m_rows[2][1] = 0;
		viewMatrix.m_rows[2][2] = 1;
		m_latestData.m_matrices[ViewMatrices::Type::ViewProjectionCylinderBillboard] = viewMatrix * projectionMatrix;

		viewMatrix.m_rows[1][0] = 0;
		viewMatrix.m_rows[1][1] = 1;
		viewMatrix.m_rows[1][2] = 0;
		m_latestData.m_matrices[ViewMatrices::Type::ViewProjectionSphericalBillboard] = viewMatrix * projectionMatrix;

		m_latestData.m_renderAndOutputResolution = {renderResolution.x, renderResolution.y, outputResolution.x, outputResolution.y};
	}

	SceneView::SceneView(LogicalDevice& logicalDevice, SceneViewDrawer& drawer, RenderOutput& output, const EnumFlags<Flags> flags)
		: SceneViewBase(logicalDevice, drawer, output, flags)
		, m_pMaterialsStage(Memory::ConstructInPlace, *this)
		, m_pOctreeTraversalStage(UniqueRef<OctreeTraversalStage>::Make(*this))
		, m_pLateStageVisibilityCheckStage(UniqueRef<LateStageVisibilityCheckStage>::Make(*this))
		, m_transformBuffer(logicalDevice, Entity::RenderItemIdentifier::MaximumCount)
	{
		OnCreated();
	}

	SceneView::SceneView(
		LogicalDevice& logicalDevice,
		SceneViewDrawer& drawer,
		Widgets::Document::Scene3D& sceneWidget,
		RenderOutput& output,
		const EnumFlags<Flags> flags
	)
		: SceneViewBase(logicalDevice, drawer, output, flags)
		, m_pSceneWidget(&sceneWidget)
		, m_pMaterialsStage(Memory::ConstructInPlace, *this)
		, m_pOctreeTraversalStage(UniqueRef<OctreeTraversalStage>::Make(*this))
		, m_pLateStageVisibilityCheckStage(UniqueRef<LateStageVisibilityCheckStage>::Make(*this))
		, m_transformBuffer(logicalDevice, Entity::RenderItemIdentifier::MaximumCount)
	{
		OnCreated();
	}

	SceneView::~SceneView()
	{
		if (m_pCurrentScene != nullptr)
		{
			DetachCurrentScene();
		}

		m_transformBuffer.Destroy(m_logicalDevice);
		m_renderMaterialCache.Destroy(m_logicalDevice, m_logicalDevice.GetRenderer().GetMaterialCache());
	}

	uint8 SceneView::GetCurrentFrameIndex() const
	{
		return System::Get<Engine>().GetCurrentFrameIndex();
	}

	Entity::CameraComponent& SceneView::GetActiveCameraComponent() const
	{
		return static_cast<Entity::CameraComponent&>(*m_pActiveCameraComponent);
	}

	Optional<Entity::CameraComponent*> SceneView::GetActiveCameraComponentSafe() const
	{
		return static_cast<Entity::CameraComponent*>(m_pActiveCameraComponent.Get());
	}

	Math::WorldCoordinate SceneView::GetWorldLocation() const
	{
		Expect(m_pActiveCameraComponent != nullptr);
		Entity::CameraComponent* pCameraComponent = GetActiveCameraComponentSafe();
		Assert(pCameraComponent != nullptr);
		if (LIKELY(pCameraComponent != nullptr))
		{
			return pCameraComponent->GetWorldLocation();
		}
		else
		{
			return Math::WorldCoordinate{Math::Zero};
		}
	}

	void SceneView::ProcessLateStageAddedRenderItem(Entity::HierarchyComponentBase& renderItem)
	{
		ProcessComponentFromOctreeTraversal(renderItem);
	}

	void SceneView::ProcessEnabledRenderItem(Entity::HierarchyComponentBase& renderItem)
	{
		ProcessComponentFromOctreeTraversal(renderItem);
	}

	void SceneView::ProcessLateStageChangedRenderItemTransform(Entity::HierarchyComponentBase& renderItem)
	{
		ProcessComponentFromOctreeTraversal(renderItem);
	}

	void
	SceneView::StartOctreeTraversal(const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer)
	{
		SceneViewBase::StartTraversal();
		m_viewMatrices.StartFrame(
			GetCurrentFrameIndex(),
			GetScene().GetFrameCounter(),
			GetScene().GetTime().GetSeconds(),
			m_logicalDevice,
			m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
		m_transformBuffer.StartFrame(graphicsCommandEncoder);
	}

	void SceneView::StartLateStageOctreeVisibilityCheck()
	{
		SceneViewBase::StartTraversal();
	}

	SceneView::TraversalResult SceneView::ProcessComponentFromOctreeTraversal(Entity::HierarchyComponentBase& component)
	{
		Scene& scene = *GetSceneChecked();
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();

		const Optional<Entity::Data::Flags*> pFlagsComponent =
			sceneRegistry.GetCachedSceneData<Entity::Data::Flags>().GetComponentImplementation(componentIdentifier);
		if (UNLIKELY_ERROR(pFlagsComponent.IsInvalid()))
		{
			return TraversalResult::RemainedHidden;
		}

		const EnumFlags<Entity::ComponentFlags> renderItemFlags = *pFlagsComponent;

		if (renderItemFlags.IsSet(Entity::ComponentFlags::IsDestroying))
		{
			return TraversalResult::RemainedHidden;
		}

		const Math::WorldTransform& __restrict renderItemWorldTransform =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>().GetComponentImplementationUnchecked(componentIdentifier);
		const Math::BoundingBox& __restrict renderItemBoundingBox =
			sceneRegistry.GetCachedSceneData<Entity::Data::BoundingBox>().GetComponentImplementationUnchecked(componentIdentifier);

		const Math::WorldBoundingBox renderItemWorldBoundingBox = Math::Transform(renderItemWorldTransform, renderItemBoundingBox);

		const bool isVisible = renderItemFlags.AreNoneSet(Entity::ComponentFlags::IsDisabledFromAnySource) &
		                       renderItemWorldBoundingBox.IsVisibleFromFrustum();
		return SceneViewBase::ProcessComponent(sceneRegistry, componentIdentifier, component, isVisible);
	}

	void SceneView::NotifyOctreeTraversalRenderStages(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		Scene& scene = *GetSceneChecked();
		Rendering::LogicalDevice& logicalDevice = m_logicalDevice;
		const CommandQueueView graphicsCommandQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics);
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount = scene.GetMaximumUsedRenderItemCount();

		Entity::RenderItemMask& newlyVisibleRenderItems = SceneViewBase::GetNewlyVisibleRenderItems();
		if (newlyVisibleRenderItems.AreAnySet())
		{
			m_transformBuffer.OnRenderItemsBecomeVisible(
				scene.GetEntitySceneRegistry(),
				logicalDevice,
				newlyVisibleRenderItems,
				maximumUsedRenderItemCount,
				SceneViewBase::GetRenderItemComponentIdentifiers(),
				graphicsCommandQueue,
				graphicsCommandEncoder,
				perFrameStagingBuffer
			);
		}

		Entity::RenderItemMask& changedTransformRenderItems = SceneViewBase::GetChangedTransformRenderItems();
		if (changedTransformRenderItems.AreAnySet())
		{
			m_transformBuffer.OnVisibleRenderItemTransformsChanged(
				scene.GetEntitySceneRegistry(),
				logicalDevice,
				changedTransformRenderItems,
				maximumUsedRenderItemCount,
				SceneViewBase::GetRenderItemComponentIdentifiers(),
				graphicsCommandQueue,
				graphicsCommandEncoder,
				perFrameStagingBuffer
			);
		}

		SceneViewBase::NotifyRenderStages(scene, graphicsCommandEncoder, perFrameStagingBuffer);
	}

	void SceneView::OnOctreeTraversalFinished(
		const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		SceneViewBase::OnTraversalFinished(*GetSceneChecked());

		NotifyOctreeTraversalRenderStages(graphicsCommandEncoder, perFrameStagingBuffer);
	}

	void SceneView::OnEnableFramegraph(SceneBase& sceneBase, const Optional<Entity::HierarchyComponentBase*> pCamera)
	{
		Scene& scene = static_cast<Scene&>(sceneBase);

		Framegraph& framegraph = m_drawer.GetFramegraph();

		const Optional<Rendering::Stage*> pOctreeTraversalPass = framegraph.GetStagePass(*m_pOctreeTraversalStage);
		const Optional<Rendering::Stage*> pLatestageVisibilityCheckPass = framegraph.GetStagePass(*m_pLateStageVisibilityCheckStage);

		if (LIKELY(pOctreeTraversalPass.IsValid() && pLatestageVisibilityCheckPass.IsValid()))
		{
			if (!scene.GetEntitySceneRegistry().GetDynamicRenderUpdatesFinishedStage().IsDirectlyFollowedBy(*pLatestageVisibilityCheckPass))
			{
				scene.GetEntitySceneRegistry().GetDynamicRenderUpdatesFinishedStage().AddSubsequentStage(*pLatestageVisibilityCheckPass);
				scene.GetEntitySceneRegistry().GetDynamicRenderUpdatesFinishedStage().AddSubsequentStage(*pOctreeTraversalPass);
				scene.GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(*pLatestageVisibilityCheckPass);

				pOctreeTraversalPass->AddSubsequentCpuStage(scene.GetRootComponent().GetOctreeCleanupJob());

				pLatestageVisibilityCheckPass->AddSubsequentCpuStage(scene.GetRootComponent().GetOctreeCleanupJob());
				pLatestageVisibilityCheckPass->AddSubsequentCpuStage(scene.GetEndFrameStage());
				pLatestageVisibilityCheckPass->AddSubsequentCpuStage(scene.GetDestroyComponentsStage());
			}
		}

		if (pCamera.IsValid())
		{
			OnCameraAssignedInternal(pCamera->AsExpected<Entity::CameraComponent>(), Invalid);
		}
	}

	void SceneView::OnDisableFramegraph(SceneBase& sceneBase, const Optional<Entity::HierarchyComponentBase*> pCamera)
	{
		Scene& scene = static_cast<Scene&>(sceneBase);

		Framegraph& framegraph = m_drawer.GetFramegraph();
		const Optional<Rendering::Stage*> pOctreeTraversalPass = framegraph.GetStagePass(*m_pOctreeTraversalStage);
		const Optional<Rendering::Stage*> pLatestageVisibilityCheckPass = framegraph.GetStagePass(*m_pLateStageVisibilityCheckStage);

		Assert(pOctreeTraversalPass.IsValid());
		Assert(pLatestageVisibilityCheckPass.IsValid());
		if (LIKELY(pOctreeTraversalPass.IsValid() && pLatestageVisibilityCheckPass.IsValid()))
		{
			if (scene.GetEntitySceneRegistry().GetDynamicRenderUpdatesFinishedStage().IsDirectlyFollowedBy(*pLatestageVisibilityCheckPass))
			{
				scene.GetEntitySceneRegistry()
					.GetDynamicRenderUpdatesFinishedStage()
					.RemoveSubsequentStage(*pLatestageVisibilityCheckPass, Invalid, Threading::StageBase::RemovalFlags{});
				scene.GetEntitySceneRegistry()
					.GetDynamicRenderUpdatesFinishedStage()
					.RemoveSubsequentStage(*pOctreeTraversalPass, Invalid, Threading::StageBase::RemovalFlags{});
				scene.GetEntitySceneRegistry()
					.GetDynamicLateUpdatesFinishedStage()
					.RemoveSubsequentStage(*pLatestageVisibilityCheckPass, Invalid, Threading::StageBase::RemovalFlags{});

				pOctreeTraversalPass
					->RemoveSubsequentCpuStage(scene.GetRootComponent().GetOctreeCleanupJob(), Invalid, Threading::StageBase::RemovalFlags{});

				pLatestageVisibilityCheckPass
					->RemoveSubsequentCpuStage(scene.GetRootComponent().GetOctreeCleanupJob(), Invalid, Threading::StageBase::RemovalFlags{});
				pLatestageVisibilityCheckPass->RemoveSubsequentCpuStage(scene.GetEndFrameStage(), Invalid, Threading::StageBase::RemovalFlags{});
				pLatestageVisibilityCheckPass
					->RemoveSubsequentCpuStage(scene.GetDestroyComponentsStage(), Invalid, Threading::StageBase::RemovalFlags{});
			}

			if (pCamera.IsValid())
			{
				Optional<Entity::ComponentTypeSceneDataInterface*> pCameraSceneData = pCamera->GetTypeSceneData();
				if (Optional<Threading::StageBase*> pCameraUpdateStage = pCameraSceneData->GetUpdateStage())
				{
					if (pCameraUpdateStage->IsDirectlyFollowedBy(*pOctreeTraversalPass))
					{
						pCameraUpdateStage->RemoveSubsequentStage(*pOctreeTraversalPass, Invalid, Threading::StageBase::RemovalFlags{});
					}
				}
			}
		}
	}

	void SceneView::AssignScene(Scene& scene)
	{
		m_pActiveCameraComponent = nullptr;
		m_flags.ClearFlags(Flags::HasCamera);

		OnSceneAssigned(scene);
	}

	void SceneView::AssignCamera(Entity::CameraComponent& newCamera)
	{
		Assert(m_pActiveCameraComponent != &newCamera);

		Optional<Entity::CameraComponent*> pPreviousCameraComponent = GetActiveCameraComponentSafe();
		if (pPreviousCameraComponent != nullptr)
		{
			pPreviousCameraComponent->OnFarPlaneChangedEvent.Remove(this);
			pPreviousCameraComponent->OnNearPlaneChangedEvent.Remove(this);
			pPreviousCameraComponent->OnFieldOfViewChangedEvent.Remove(this);
			pPreviousCameraComponent->OnTransfromChangedEvent.Remove(this);
			pPreviousCameraComponent->OnDestroyedEvent.Remove(this);
		}

		m_pActiveCameraComponent = &newCamera;

		newCamera.OnFarPlaneChangedEvent.Add(*this, &SceneView::OnActiveCameraPropertiesChanged);
		newCamera.OnNearPlaneChangedEvent.Add(*this, &SceneView::OnActiveCameraPropertiesChanged);
		newCamera.OnFieldOfViewChangedEvent.Add(*this, &SceneView::OnActiveCameraPropertiesChanged);
		newCamera.OnTransfromChangedEvent.Add(*this, &SceneView::OnActiveCameraPropertiesChanged);
		newCamera.OnDestroyedEvent.Add(*this, &SceneView::OnActiveCameraDestroyed);

		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::HasCamera);
		if (previousFlags.AreAllSet(Flags::CanEnable))
		{
			OnCameraAssignedInternal(newCamera, pPreviousCameraComponent);
		}

		OnActiveCameraPropertiesChanged();
		m_viewMatrices.ResetPreviousFrames();
	}

	void SceneView::OnCameraAssignedInternal(Entity::CameraComponent& newCamera, const Optional<Entity::CameraComponent*> pPreviousCamera)
	{
		Expect(m_pCurrentScene != nullptr);
		Scene& scene = *GetSceneChecked();

		Optional<Entity::ComponentTypeSceneDataInterface*> pPreviousCameraSceneData = pPreviousCamera.IsValid()
		                                                                                ? pPreviousCamera->GetTypeSceneData()
		                                                                                : nullptr;

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentSoftReference cameraReference = Entity::ComponentSoftReference(newCamera, sceneRegistry);

		scene.ModifyFrameGraph(
			[this, pPreviousCameraSceneData, cameraReference, &scene]()
			{
				Framegraph& framegraph = m_drawer.GetFramegraph();
				const Optional<Stage*> pOctreeTraversalPass = framegraph.GetStagePass(*m_pOctreeTraversalStage);

				if (LIKELY(pOctreeTraversalPass.IsValid()))
				{
					if (LIKELY(pPreviousCameraSceneData.IsValid()))
					{
						if (Optional<Threading::StageBase*> pCameraUpdateStage = pPreviousCameraSceneData->GetUpdateStage())
						{
							if (pCameraUpdateStage->IsDirectlyFollowedBy(*pOctreeTraversalPass))
							{
								Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
								pCameraUpdateStage->RemoveSubsequentStage(*pOctreeTraversalPass, thread, Threading::StageBase::RemovalFlags{});
							}
						}
					}

					if (IsEnabled())
					{
						if (Entity::Component3D* pNewCameraComponent = cameraReference.Find<Entity::Component3D>(scene.GetEntitySceneRegistry()))
						{
							Optional<Entity::ComponentTypeSceneDataInterface*> pCameraSceneData = pNewCameraComponent->GetTypeSceneData();
							if (LIKELY(pCameraSceneData.IsValid()))
							{
								if (Optional<Threading::StageBase*> pCameraUpdateStage = pCameraSceneData->GetUpdateStage())
								{
									if (pCameraUpdateStage->HasDependencies())
									{
										pCameraUpdateStage->AddSubsequentStage(*pOctreeTraversalPass);
									}
								}
							}
						}
					}
				}
			}
		);
	}

	void SceneView::DetachCurrentScene()
	{
		Assert(!IsEnabled());

		m_transformBuffer.Reset();

		m_pLateStageVisibilityCheckStage->ClearQueue();

		OnSceneDetached();

		if (const Optional<Entity::CameraComponent*> pCameraComponent = GetActiveCameraComponentSafe())
		{
			pCameraComponent->OnFarPlaneChangedEvent.Remove(this);
			pCameraComponent->OnNearPlaneChangedEvent.Remove(this);
			pCameraComponent->OnFieldOfViewChangedEvent.Remove(this);
			pCameraComponent->OnTransfromChangedEvent.Remove(this);
			pCameraComponent->OnDestroyedEvent.Remove(this);
			m_pActiveCameraComponent = nullptr;
			m_flags.ClearFlags(Flags::HasCamera);
		}
	}

	bool SceneView::HasStartedCulling() const
	{
		return m_pOctreeTraversalStage->HasStartedCulling();
	}

	Framegraph& SceneView::GetFramegraph() const
	{
		return m_drawer.GetFramegraph();
	}

	Stage& SceneView::GetOctreeTraversalStage() const
	{
		return *m_pOctreeTraversalStage;
	}

	Stage& SceneView::GetLateStageVisibilityCheckStage() const
	{
		return *m_pLateStageVisibilityCheckStage;
	}

	void SceneView::OnActiveCameraPropertiesChanged()
	{
		Entity::CameraComponent* pCameraComponent = GetActiveCameraComponentSafe();
		Assert(pCameraComponent != nullptr);
		if (LIKELY(pCameraComponent != nullptr))
		{
			Entity::SceneRegistry& sceneRegistry = GetScene().GetEntitySceneRegistry();
			const Math::Anglef fieldOfView = pCameraComponent->GetFieldOfView();
			const Math::Vector2ui renderResolution = m_drawer.GetRenderResolution();
			const Math::Lengthf nearPlane = pCameraComponent->GetNearPlane();
			const Math::Lengthf farPlane = pCameraComponent->GetFarPlane();
			const Math::WorldTransform cameraTransform = pCameraComponent->GetWorldTransform(sceneRegistry);
			Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				cameraTransform.GetLocation(),
				cameraTransform.GetLocation() + cameraTransform.GetForwardColumn(),
				cameraTransform.GetUpColumn()
			);
			const Math::Vector3f cameraScale = cameraTransform.GetScale();
			viewMatrix =
				Math::Matrix4x4f{Math::Matrix3x4f{
					Math::Matrix3x3f{Math::Vector3f{cameraScale.x, 0, 0}, Math::Vector3f{0, cameraScale.y, 0}, Math::Vector3f{0, 0, cameraScale.z}}
				}} *
				viewMatrix;

			const Math::Matrix4x4f projectionMatrix =
				m_drawer.CreateProjectionMatrix(fieldOfView, (Math::Vector2f)renderResolution, nearPlane, farPlane);
			const uint8 frameIndex = GetCurrentFrameIndex();

			const Math::Vector2ui outputResolution = m_output.GetOutputArea().GetSize();
			m_viewMatrices.OnCameraPropertiesChanged(
				viewMatrix,
				projectionMatrix,
				cameraTransform.GetLocation(),
				cameraTransform.GetRotationMatrix(),
				renderResolution,
				outputResolution,
				frameIndex
			);

			m_pLateStageVisibilityCheckStage->QueueActiveCameraPropertiesChanged();
		}
	}

	void SceneView::OnActiveCameraTransformChanged()
	{
		Entity::CameraComponent* pCameraComponent = GetActiveCameraComponentSafe();
		Assert(pCameraComponent != nullptr);
		if (LIKELY(pCameraComponent != nullptr))
		{
			Entity::SceneRegistry& sceneRegistry = GetScene().GetEntitySceneRegistry();
			const Math::WorldTransform cameraTransform = pCameraComponent->GetWorldTransform(sceneRegistry);
			Math::Matrix4x4f viewMatrix = Math::Matrix4x4f::CreateLookAt(
				cameraTransform.GetLocation(),
				cameraTransform.GetLocation() + cameraTransform.GetForwardColumn(),
				cameraTransform.GetUpColumn()
			);
			const Math::Vector3f cameraScale = cameraTransform.GetScale();
			viewMatrix =
				Math::Matrix4x4f{Math::Matrix3x4f{
					Math::Matrix3x3f{Math::Vector3f{cameraScale.x, 0, 0}, Math::Vector3f{0, cameraScale.y, 0}, Math::Vector3f{0, 0, cameraScale.z}}
				}} *
				viewMatrix;
			const uint8 frameIndex = GetCurrentFrameIndex();
			m_viewMatrices.OnCameraTransformChanged(viewMatrix, cameraTransform.GetLocation(), cameraTransform.GetRotationMatrix(), frameIndex);

			m_pLateStageVisibilityCheckStage->QueueActiveCameraTransformChanged();
		}
	}

	void SceneView::OnActiveCameraDestroyed()
	{
		if (m_flags.TryClearFlags(Flags::HasCamera))
		{
			Entity::CameraComponent* pCameraComponent = GetActiveCameraComponentSafe();
			Assert(pCameraComponent != nullptr);

			Optional<Entity::ComponentTypeSceneDataInterface*> pCameraSceneData = pCameraComponent->GetTypeSceneData();
			if (Optional<Threading::StageBase*> pCameraUpdateStage = pCameraSceneData->GetUpdateStage())
			{
				Framegraph& framegraph = m_drawer.GetFramegraph();
				if (const Optional<Stage*> pOctreeTraversalPass = framegraph.GetStagePass(*m_pOctreeTraversalStage))
				{
					if (pCameraUpdateStage->IsDirectlyFollowedBy(*pOctreeTraversalPass))
					{
						Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
						pCameraUpdateStage->RemoveSubsequentStage(*pOctreeTraversalPass, thread, Threading::StageBase::RemovalFlags{});
					}
				}
			}

			m_pActiveCameraComponent = nullptr;
		}
	}

	void SceneView::OnBeforeResizeRenderOutput()
	{
		if (IsEnabled())
		{
			GetFramegraph().WaitForProcessingFramesToFinish(Rendering::AllFramesMask);
		}
	}

	void SceneView::OnAfterResizeRenderOutput()
	{
		if (m_pActiveCameraComponent != nullptr)
		{
			OnActiveCameraPropertiesChanged();
		}
	}

	void SceneView::OnRenderItemAdded(Entity::HierarchyComponentBase& renderItem)
	{
		if (HasStartedCulling() && renderItem.IsEnabled())
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForLateStageAdd(renderItem);
		}
	}

	void SceneView::OnRenderItemRemoved(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		m_pLateStageVisibilityCheckStage->QueueRenderItemForRemoval(renderItemIdentifier);
	}

	void SceneView::OnRenderItemEnabled(Entity::HierarchyComponentBase& renderItem)
	{
		if (HasStartedCulling())
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemEnabled(renderItem);
		}
	}

	void SceneView::OnRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		if (HasStartedCulling())
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemDisabled(renderItemIdentifier);
		}
	}

	void SceneView::OnRenderItemTransformChanged(Entity::HierarchyComponentBase& renderItem)
	{
		if (HasStartedCulling())
		{
			m_pLateStageVisibilityCheckStage->QueueChangedRenderItemTransformForLateStageCulling(renderItem);
		}
	}

	void
	SceneView::OnRenderItemStageMaskEnabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageMaskEnable(renderItemIdentifier, stages);
		}
	}

	void
	SceneView::OnRenderItemStageMaskDisabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageMaskDisable(renderItemIdentifier, stages);
		}
	}

	void SceneView::OnRenderItemStageMaskChanged(
		const Entity::RenderItemIdentifier renderItemIdentifier,
		const RenderItemStageMask& enabledStages,
		const RenderItemStageMask& disabledStages
	) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageMaskChanged(renderItemIdentifier, enabledStages, disabledStages);
		}
	}

	void
	SceneView::OnRenderItemStageMaskReset(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages)
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageMaskReset(renderItemIdentifier, resetStages);
		}
	}

	void SceneView::OnRenderItemStageEnabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageEnable(renderItemIdentifier, stageIdentifier);
		}
	}

	void SceneView::OnRenderItemStageDisabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageDisable(renderItemIdentifier, stageIdentifier);
		}
	}

	void SceneView::OnRenderItemStageReset(
		const Entity::RenderItemIdentifier renderItemIdentifier, const Rendering::SceneRenderStageIdentifier stageIdentifier
	) const
	{
		if (SceneViewBase::IsRenderItemVisible(renderItemIdentifier))
		{
			m_pLateStageVisibilityCheckStage->QueueRenderItemForStageReset(renderItemIdentifier, stageIdentifier);
		}
	}

	PURE_STATICS Optional<Scene3D*> SceneView::GetSceneChecked() const
	{
		return static_cast<Scene3D*>(m_pCurrentScene.Get());
	}

	PURE_STATICS Scene3D& SceneView::GetScene() const
	{
		return static_cast<Scene3D&>(*GetSceneChecked());
	}

	PURE_STATICS Optional<SceneData*> SceneView::GetSceneData() const
	{
		if (const Optional<Scene*> pScene = GetSceneChecked())
		{
			return pScene->GetRenderData();
		}
		else
		{
			return Invalid;
		}
	}

	Math::Vector2ui SceneView::GetRenderResolution() const
	{
		return m_drawer.GetRenderResolution();
	}

	Math::CullingFrustum<float> SceneView::GetCullingFrustum() const
	{
		Entity::CameraComponent* pCameraComponent = GetActiveCameraComponentSafe();
		Assert(pCameraComponent != nullptr);
		if (LIKELY(pCameraComponent != nullptr))
		{
			const float fieldOfView = pCameraComponent->GetFieldOfView().GetRadians();
			const Math::Vector2ui renderResolution = m_drawer.GetRenderResolution();
			const float aspectRatio = static_cast<float>(renderResolution.x) / static_cast<float>(renderResolution.y);
			const float nearPlane = pCameraComponent->GetNearPlane().GetUnits();
			const float farPlane = pCameraComponent->GetFarPlane().GetUnits();

			const float tanHalfVerticalFov = Math::Tan(fieldOfView * 0.5f);
			return {
				aspectRatio * nearPlane * tanHalfVerticalFov,
				nearPlane * tanHalfVerticalFov,
				nearPlane,
				farPlane,
			};
		}
		else
		{
			return {0.f, 0.f, 0.f, 0.f};
		}
	}
}
