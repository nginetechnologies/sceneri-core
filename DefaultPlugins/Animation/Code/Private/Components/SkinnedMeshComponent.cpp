#include "Components/SkinnedMeshComponent.h"
#include "Components/SkeletonComponent.h"
#include "MeshSkinAssetType.h"
#include "Plugin.h"
#include "AnimationAssetType.h"
#include "SkeletonAssetType.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/RenderItem/VisibilityListener.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#include "MeshSkin.h"
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/RenderMesh.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Renderer.h>
#include <Renderer/Buffers/DeviceMemoryView.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/AddressOf.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Animation
{
	SkinnedMeshComponent::SkinnedMeshComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer(initializer),
				System::Get<Rendering::Renderer>().GetMeshCache().Clone(initializer.m_staticMeshIdentifier),
				initializer.m_materialInstanceIdentifier
			})
	{
		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>(initializer.GetSceneRegistry()))
		{
			pSkeletonComponent->OnToggledUpdate.Add(
				*this,
				[](SkinnedMeshComponent& component)
				{
					component.TryEnableUpdate();
				}
			);
		}
	}

	SkinnedMeshComponent::SkinnedMeshComponent(const SkinnedMeshComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
		, m_masterMeshIdentifier(templateComponent.m_masterMeshIdentifier)
		, m_meshSkinIdentifier(templateComponent.m_meshSkinIdentifier)
	{
		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>(cloner.GetSceneRegistry()))
		{
			pSkeletonComponent->OnToggledUpdate.Add(
				*this,
				[](SkinnedMeshComponent& component)
				{
					component.TryEnableUpdate();
				}
			);
		}

		// Create a unique mesh
		SetMesh(*System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(m_masterMeshIdentifier).m_pMesh);
		CloneMesh(false);
	}

	SkinnedMeshComponent::SkinnedMeshComponent(const Deserializer& deserializer)
		: SkinnedMeshComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SkinnedMeshComponent>().ToString().GetView())
			)
	{
	}

	SkinnedMeshComponent::SkinnedMeshComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: StaticMeshComponent(deserializer)
		, m_masterMeshIdentifier(GetMeshIdentifier())
		, m_meshSkinIdentifier(Plugin::GetInstance()->GetMeshSkinCache().FindOrRegisterAsset(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Guid>("skin", Guid{}) : Guid{}
			))
	{
		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>(deserializer.GetSceneRegistry()))
		{
			pSkeletonComponent->OnToggledUpdate.Add(
				*this,
				[](SkinnedMeshComponent& component)
				{
					component.TryEnableUpdate();
				}
			);
		}

		// Create a unique mesh
		CloneMesh(false);
	}

	SkinnedMeshComponent::~SkinnedMeshComponent() = default;

	void SkinnedMeshComponent::OnCreated()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentSoftReference componentReference{*this, sceneRegistry};

		[[maybe_unused]] const Optional<Entity::Data::RenderItem::VisibilityListener*> pVisibilityListener =
			CreateDataComponent<Entity::Data::RenderItem::VisibilityListener>(
				[componentReference, &sceneRegistry](Rendering::LogicalDevice& logicalDevice, const Threading::JobBatch& jobBatch)
				{
					if (const Optional<SkinnedMeshComponent*> pComponent = componentReference.Find<SkinnedMeshComponent>(sceneRegistry))
					{
						if (pComponent->m_canLoadSkin == true)
						{
							pComponent->LoadMeshSkin(logicalDevice, jobBatch);
						}
					}
				}
			);
		Assert(pVisibilityListener.IsValid());
	}

	void SkinnedMeshComponent::OnDestroying()
	{
		if (HasParent())
		{
			if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>())
			{
				pSkeletonComponent->OnToggledUpdate.Remove(this);
			}
		}
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkinnedMeshComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkinnedMeshComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}

		UnloadMeshSkin();
	}

	void SkinnedMeshComponent::OnAttachedToNewParent()
	{
		BaseType::OnAttachedToNewParent();

		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>())
		{
			if (!pSkeletonComponent->OnToggledUpdate.Contains(this))
			{
				pSkeletonComponent->OnToggledUpdate.Add(
					*this,
					[](SkinnedMeshComponent& component)
					{
						component.TryEnableUpdate();
					}
				);
			}
		}

		TryEnableUpdate();
	}

	void SkinnedMeshComponent::OnBeforeDetachFromParent()
	{
		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>())
		{
			pSkeletonComponent->OnToggledUpdate.Remove(this);
		}

		TryEnableUpdate();
	}

	bool SkinnedMeshComponent::CanApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	) const
	{
		if (StaticMeshComponent::CanApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (GetParent().Is<SkeletonComponent>())
			{
				// If we have a skeleton attached, indicate that we are compatible with animations and skeletons
				if (pAssetReference->GetTypeGuid() == AnimationAssetType::AssetFormat.assetTypeGuid)
				{
					// TODO: Validate that the animation is compatible with the skeleton
					return true;
				}
				else if (pAssetReference->GetTypeGuid() == SkeletonAssetType::AssetFormat.assetTypeGuid)
				{
					// TODO: Validate that the skeleton is compatible with this skinned mesh
					return true;
				}
			}

			return pAssetReference->GetTypeGuid() == MeshSkinAssetType::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool SkinnedMeshComponent::ApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	)
	{
		if (StaticMeshComponent::ApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == MeshSkinAssetType::AssetFormat.assetTypeGuid)
			{
				SetSkinnedMesh(*pAssetReference);
				return true;
			}
			else if (pAssetReference->GetTypeGuid() == SkeletonAssetType::AssetFormat.assetTypeGuid || pAssetReference->GetTypeGuid() == AnimationAssetType::AssetFormat.assetTypeGuid)
			{
				if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>())
				{
					if (pSkeletonComponent->ApplyAtPoint(*pAssetReference, coordinate, applyFlags))
					{
						TryEnableUpdate();
						return true;
					}
				}
			}
		}

		return false;
	}

	void SkinnedMeshComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		const Asset::Picker mesh = GetStaticMesh();
		if (callback(mesh.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}

		const Asset::Picker materialInstance = GetMaterialInstance();
		if (callback(materialInstance.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}

		const Asset::Picker skinnedMesh = GetSkinnedMesh();
		if (callback(skinnedMesh.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}

		if (const Optional<SkeletonComponent*> pSkeletonComponent = GetParent().AsExactType<SkeletonComponent>())
		{
			pSkeletonComponent->IterateAttachedItems(allowedTypes, callback);
		}
	}

	void SkinnedMeshComponent::SetSkinnedMesh(const SkinnedMeshPicker asset)
	{
		MeshSkinCache& meshSkinCache = Plugin::GetInstance()->GetMeshSkinCache();
		const MeshSkinIdentifier newMeshSkinModifier = meshSkinCache.FindOrRegisterAsset(asset.GetAssetGuid());
		if (newMeshSkinModifier != m_meshSkinIdentifier)
		{
			UnloadMeshSkin();

			m_meshSkinIdentifier = newMeshSkinModifier;
			TryEnableUpdate();
		}
	}

	SkinnedMeshComponent::SkinnedMeshPicker SkinnedMeshComponent::GetSkinnedMesh() const
	{
		MeshSkinCache& meshSkinCache = Plugin::GetInstance()->GetMeshSkinCache();
		return SkinnedMeshPicker{
			m_meshSkinIdentifier.IsValid() ? meshSkinCache.GetAssetGuid(m_meshSkinIdentifier) : Asset::Guid{},
			MeshSkinAssetType::AssetFormat.assetTypeGuid
		};
	}

	bool SkinnedMeshComponent::CanEnableUpdate() const
	{
		if (IsDisabled())
		{
			return false;
		}

		if (!HasParent())
		{
			return false;
		}

		if (!GetParent().Is<SkeletonComponent>())
		{
			return false;
		}

		if (m_pMeshSkin == nullptr)
		{
			return false;
		}

		const SkeletonComponent& skeletonComponent = static_cast<const SkeletonComponent&>(GetParent());
		const Optional<const Skeleton*> pSkeleton = skeletonComponent.GetSkeletonInstance().GetSkeleton();
		if (pSkeleton.IsInvalid())
		{
			return false;
		}

		const ngine::Animation::JointIndex jointCount = pSkeleton->GetJointCount();
		if (UNLIKELY(jointCount < m_pMeshSkin->GetHighestJointRemappingIndex()))
		{
			return false;
		}

		if (!skeletonComponent.GetSkeletonInstance().GetModelSpaceMatrices().HasElements() || !pSkeleton->GetJointBindPoses().HasElements())
		{
			return false;
		}

		if (m_pMeshSkin == nullptr || !m_pMeshSkin->IsValid())
		{
			return false;
		}

		if (m_skinningMatrices.IsEmpty() || m_mappedTargetVertices.IsEmpty())
		{
			return false;
		}

		if (skeletonComponent.GetSkeletonInstance().GetModelSpaceMatrices().IsEmpty())
		{
			return false;
		}

		if (skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses().IsEmpty())
		{
			return false;
		}

		if (skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount() < m_pMeshSkin->GetHighestJointRemappingIndex())
		{
			return false;
		}

		const Optional<const Rendering::StaticMesh*> pMesh = GetMesh();
		if (pMesh.IsInvalid() || !pMesh->IsLoaded())
		{
			return false;
		}

		if (!m_pMeshSkin->ValidateVertexBuffers(pMesh->GetVertexPositions(), pMesh->GetVertexNormals()))
		{
			return false;
		}

		using PairType = typename decltype(m_mappedTargetVertices)::PairType;
		for (const PairType& mappedTargetVerticesPair : m_mappedTargetVertices)
		{
			const TargetVertices& __restrict targetVertices = mappedTargetVerticesPair.second;

			if (!m_pMeshSkin->ValidateVertexBuffers(
						{targetVertices.pPositions, targetVertices.m_vertexCount},
						{targetVertices.pNormals, targetVertices.m_vertexCount}
					))
			{
				return false;
			}
		}

		return true;
	}

	// TODO: Don't trigger if this mesh is not visible.
	// Later we'd need to have a worst case bounding box, combined from all animation frames
	// This would be used for the visibility check.
	void SkinnedMeshComponent::TryEnableUpdate()
	{
		Threading::SharedLock lock(m_mappedTargetVerticesMutex);

		if (CanEnableUpdate())
		{
			bool expected = false;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, true))
			{
				Entity::ComponentTypeSceneData<SkinnedMeshComponent>& skinnedMeshTypeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkinnedMeshComponent>&>(*GetTypeSceneData());
				Entity::ComponentTypeSceneData<SkeletonComponent>& skeletonTypeSceneData =
					*GetSceneRegistry().FindComponentTypeData<SkeletonComponent>();
				skinnedMeshTypeSceneData.EnableUpdate(*this);
				skeletonTypeSceneData.GetUpdateStage()->AddSubsequentStage(*skinnedMeshTypeSceneData.GetUpdateStage(), GetSceneRegistry());
			}
		}
		else
		{
			bool expected = true;
			if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
			{
				Entity::ComponentTypeSceneData<SkinnedMeshComponent>& typeSceneData =
					static_cast<Entity::ComponentTypeSceneData<SkinnedMeshComponent>&>(*GetTypeSceneData());
				typeSceneData.DisableUpdate(*this);
			}
		}
	}

	void SkinnedMeshComponent::OnEnable()
	{
		TryEnableUpdate();
	}

	void SkinnedMeshComponent::OnDisable()
	{
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkinnedMeshComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkinnedMeshComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}
	}

	void SkinnedMeshComponent::LoadMeshSkin(Rendering::LogicalDevice& logicalDevice, const Threading::JobBatch& jobBatch)
	{
		Assert(m_canLoadSkin);

		if (m_pMeshSkin != nullptr)
		{
			return;
		}

		if (m_meshSkinIdentifier.IsInvalid())
		{
			return;
		}

		m_canLoadSkin = false;

		MeshSkinCache& meshSkinCache = Plugin::GetInstance()->GetMeshSkinCache();
		m_pMeshSkin = meshSkinCache.GetMeshSkin(m_meshSkinIdentifier);
		Assert(m_pMeshSkin != nullptr);

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentSoftReference componentReference{*this, sceneRegistry};

		Threading::Job* pLoadMeshSkinJob = meshSkinCache.TryLoadMeshSkin(
			m_meshSkinIdentifier,
			System::Get<Asset::Manager>(),
			MeshSkinCache::LoadListenerData{
				this,
				[componentReference, &sceneRegistry](SkinnedMeshComponent&, const MeshSkinIdentifier)
				{
					if (const Optional<SkinnedMeshComponent*> pComponent = componentReference.Find<SkinnedMeshComponent>(sceneRegistry))
					{
						Assert(!pComponent->IsDestroying(sceneRegistry));
						pComponent->m_skinningMatrices.Resize(pComponent->m_pMeshSkin->GetJointRemappingIndices().GetSize());

						pComponent->TryEnableUpdate();
					}
				}
			}
		);
		if (pLoadMeshSkinJob != nullptr)
		{
			pLoadMeshSkinJob->Queue(System::Get<Threading::JobManager>());
		}

		Rendering::MeshCache& meshCache = logicalDevice.GetRenderer().GetMeshCache();
		const Rendering::StaticMeshIdentifier meshIdentifier = GetMeshIdentifier();

		Threading::JobBatch setupMeshSkinJobBatch = meshCache.TryLoadRenderMesh(
			logicalDevice.GetIdentifier(),
			meshIdentifier,
			Rendering::MeshCache::RenderMeshLoadListenerData{
				this,
				[componentReference,
		     &sceneRegistry,
		     &meshCache,
		     meshIdentifier,
		     &logicalDevice](SkinnedMeshComponent&, const Rendering::RenderMeshView, const EnumFlags<Rendering::MeshCache::LoadedMeshFlags>)
				{
					if (const Optional<SkinnedMeshComponent*> pComponent = componentReference.Find<SkinnedMeshComponent>(sceneRegistry))
					{
						Assert(!pComponent->IsDestroying(sceneRegistry));

						Optional<Rendering::RenderMesh*> pRenderMesh = meshCache.GetRenderMesh(logicalDevice.GetIdentifier(), meshIdentifier);
						Assert(pRenderMesh.IsValid());
						if (LIKELY(pRenderMesh.IsValid()))
						{
							Rendering::RenderMesh& renderMesh = *pRenderMesh;
							const Rendering::Index vertexCount = renderMesh.GetVertexCount();
							Rendering::Buffer& vertexBuffer = renderMesh.GetVertexBuffer();

							const Rendering::StaticMesh& mesh = *meshCache.GetAssetData(meshIdentifier).m_pMesh;

							const ArrayView<const Rendering::VertexPosition, Rendering::Index> sourceVertexPositions = mesh.GetVertexPositions();
							const ArrayView<const Rendering::VertexNormals, Rendering::Index> sourceVertexNormals = mesh.GetVertexNormals();
							const uint32 normalsOffset = static_cast<uint32>(
								reinterpret_cast<uintptr>(sourceVertexNormals.begin().Get()) -
								reinterpret_cast<uintptr>(sourceVertexPositions.begin().Get())
							);

							{
								Threading::UniqueLock lock(pComponent->m_mappedTargetVerticesMutex);

								auto it = pComponent->m_mappedTargetVertices.Find(logicalDevice.GetIdentifier());
								if (it == pComponent->m_mappedTargetVertices.end())
								{
#if SUPPORT_SKINNED_MESH_STAGING_BUFFER
									Rendering::StagingBuffer stagingBuffer(
										logicalDevice,
										logicalDevice.GetPhysicalDevice(),
										logicalDevice.GetDeviceMemoryPool(),
										normalsOffset + sourceVertexNormals.GetDataSize(),
										Rendering::StagingBuffer::Flags::TransferSource
									);

									const Rendering::BufferView bufferView{vertexBuffer.operator Rendering::BufferView()};
									[[maybe_unused]] const bool wasExecutedAsynchronously = stagingBuffer.MapToHostMemoryAsync(
										logicalDevice,
										Math::Range<size>::Make((size)0, stagingBuffer.GetSize()),
										Rendering::Buffer::MapMemoryFlags::Write | Rendering::Buffer::MapMemoryFlags::KeepMapped,
										[&it,
							       lock = Move(lock),
							       pComponent,
							       logicalDeviceIdentifier = logicalDevice.GetIdentifier(),
							       vertexCount,
							       normalsOffset,
							       vertexBuffer = bufferView,
							       &stagingBuffer](
											const Rendering::Buffer::MapMemoryStatus status,
											const ByteView data,
											[[maybe_unused]] const bool executedAsynchronously
										) mutable
										{
											Assert(!executedAsynchronously);
											Assert(status == Rendering::Buffer::MapMemoryStatus::Success);
											if (LIKELY(status == Rendering::Buffer::MapMemoryStatus::Success))
											{
												it = pComponent->m_mappedTargetVertices.Emplace(
													logicalDeviceIdentifier,
													TargetVertices{
														vertexCount,
														Move(stagingBuffer),
														vertexBuffer,
														reinterpret_cast<Rendering::VertexPosition*>(data.GetData()),
														reinterpret_cast<Rendering::VertexNormals*>(data.GetData() + normalsOffset),
													}
												);
											}

											lock.Unlock();
											pComponent->TryEnableUpdate();
											pComponent->m_canLoadSkin = false;
										}
									);
#else
									FixedSizeVector<ByteType> buffer = FixedSizeVector<ByteType>(
										Memory::ConstructWithSize,
										Memory::Uninitialized,
										(uint32)(normalsOffset + sourceVertexNormals.GetDataSize())
									);
									Rendering::VertexPosition* pPositions = reinterpret_cast<Rendering::VertexPosition*>(buffer.GetData());
									Rendering::VertexNormals* pNormals = reinterpret_cast<Rendering::VertexNormals*>(buffer.GetData() + normalsOffset);

									it = pComponent->m_mappedTargetVertices.Emplace(
										logicalDevice.GetIdentifier(),
										TargetVertices{
											vertexCount,
											Move(buffer),
											vertexBuffer,
											pPositions,
											pNormals,
										}
									);

									lock.Unlock();
									pComponent->TryEnableUpdate();
									pComponent->m_canLoadSkin = false;
#endif
								}
							}
						}
					}
					return EventCallbackResult::Remove;
				}
			},
			Rendering::MeshLoadFlags::Default & ~Rendering::MeshLoadFlags::LoadDummy
		);
		if (setupMeshSkinJobBatch.IsValid())
		{
			if (jobBatch.IsValid())
			{
				jobBatch.GetFinishedStage().AddSubsequentStage(setupMeshSkinJobBatch.GetStartStage());
			}
			else
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadMeshSkin);
				}
			}
		}
	}

	void SkinnedMeshComponent::UnloadMeshSkin()
	{
		bool expected = true;
		if (m_isUpdateEnabled.CompareExchangeStrong(expected, false))
		{
			Entity::ComponentTypeSceneData<SkinnedMeshComponent>& typeSceneData =
				static_cast<Entity::ComponentTypeSceneData<SkinnedMeshComponent>&>(*GetTypeSceneData());
			typeSceneData.DisableUpdate(*this);
		}

		MeshSkinCache& meshSkinCache = Plugin::GetInstance()->GetMeshSkinCache();
		const MeshSkinIdentifier meshSkinIdentifier = m_meshSkinIdentifier;

		meshSkinCache.RemoveListener(meshSkinIdentifier, this);

		m_pMeshSkin = nullptr;
		m_meshSkinIdentifier = {};

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::Renderer::LogicalDeviceView logicalDevices = renderer.GetLogicalDevices();
		Rendering::MeshCache& meshCache = renderer.GetMeshCache();

		const Optional<const Rendering::StaticMesh*> pMesh = GetMesh();
		const Rendering::StaticMeshIdentifier meshIdentifier = pMesh.IsValid() ? pMesh->GetIdentifier() : Rendering::StaticMeshIdentifier{};

		Threading::SharedLock lock(m_mappedTargetVerticesMutex);
#if SUPPORT_SKINNED_MESH_STAGING_BUFFER
		{
			using PairType = typename decltype(m_mappedTargetVertices)::PairType;
			for (PairType& mappedTargetVerticesPair : m_mappedTargetVertices)
			{
				const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = mappedTargetVerticesPair.first;
				TargetVertices& __restrict targetVertices = mappedTargetVerticesPair.second;

				Rendering::LogicalDevice& logicalDevice = *logicalDevices[logicalDeviceIdentifier];

				targetVertices.m_stagingBuffer.UnmapFromHostMemory(logicalDevice);
				Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
				thread.GetRenderData().DestroyBuffer(logicalDeviceIdentifier, Move(targetVertices.m_stagingBuffer));
			}
		}
#endif

		for (const Optional<Rendering::LogicalDevice*> pLogicalDevice : logicalDevices)
		{
			if (pLogicalDevice.IsValid())
			{
				meshCache.RemoveRenderMeshListener(pLogicalDevice->GetIdentifier(), meshIdentifier, this);
			}
		}

		m_canLoadSkin = true;
	}

	void SkinnedMeshComponent::Update()
	{
		const Optional<const Rendering::StaticMesh*> pMesh = GetMesh();
		Assert(pMesh.IsValid() && pMesh->IsLoaded());
		if (UNLIKELY_ERROR(pMesh.IsInvalid()))
		{
			return;
		}

		Threading::SharedLock lock(m_mappedTargetVerticesMutex);

		Assert(CanEnableUpdate());
		// TODO: Fix this possibility when parent is null
		if (UNLIKELY(!CanEnableUpdate()))
		{
			return;
		}

		const SkeletonComponent& skeletonComponent = static_cast<const SkeletonComponent&>(GetParent());

		using PairType = typename decltype(m_mappedTargetVertices)::PairType;
		// TODO: Only generate once, and then copy to other devices
		for (const PairType& mappedTargetVerticesPair : m_mappedTargetVertices)
		{
			const TargetVertices& __restrict targetVertices = mappedTargetVerticesPair.second;

			m_pMeshSkin->ProcessSkinning(
				skeletonComponent.GetSkeletonInstance(),
				m_skinningMatrices,
				pMesh->GetVertexPositions().GetData(),
				targetVertices.pPositions,
				pMesh->GetVertexNormals().GetData(),
				targetVertices.pNormals
			);

			const Rendering::LogicalDeviceIdentifier deviceIdentifier = mappedTargetVerticesPair.first;

			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			Rendering::Renderer::LogicalDeviceView logicalDevices = renderer.GetLogicalDevices();
			Rendering::LogicalDevice& logicalDevice = *logicalDevices[deviceIdentifier];

			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			const Rendering::CommandPoolView commandPool =
				thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer);
			Rendering::SingleUseCommandBuffer
				commandBuffer(logicalDevice, commandPool, thread, Rendering::QueueFamily::Transfer, Threading::JobPriority::CreateRenderMesh);

			const Rendering::CommandEncoderView commandEncoder = commandBuffer;
			const Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
#if SUPPORT_SKINNED_MESH_STAGING_BUFFER
			blitCommandEncoder.RecordCopyBufferToBuffer(
				targetVertices.m_stagingBuffer,
				targetVertices.m_targetBuffer,
				Array{Rendering::BufferCopy{0, 0, targetVertices.m_stagingBuffer.GetSize()}}
			);
#else
			Optional<Rendering::StagingBuffer> stagingBuffer;
			blitCommandEncoder.RecordCopyDataToBuffer(
				logicalDevice,
				Rendering::QueueFamily::Graphics,
				Array<const Rendering::DataToBufferBatch, 1>{Rendering::DataToBufferBatch{
					targetVertices.m_targetBuffer,
					Array<const Rendering::DataToBuffer, 1>{Rendering::DataToBuffer{0, ConstByteView(targetVertices.m_buffer.GetView())}}
				}},
				stagingBuffer
			);
			if (stagingBuffer.IsValid())
			{
				commandBuffer.OnFinished = [&logicalDevice, stagingBuffer = Move(*stagingBuffer)]() mutable
				{
					stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				};
			}
#endif
		}
	}

	[[maybe_unused]] const bool wasSkinnedMeshRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SkinnedMeshComponent>>::Make());
	[[maybe_unused]] const bool wasSkinnedMeshTypeRegistered = Reflection::Registry::RegisterType<SkinnedMeshComponent>();
}
