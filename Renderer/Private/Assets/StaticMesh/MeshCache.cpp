#include "Assets/StaticMesh/MeshCache.h"

#include "Assets/StaticMesh/MeshAssetType.h"
#include "Assets/StaticMesh/StaticMesh.h"
#include "Assets/StaticMesh/StaticObject.h"
#include "Assets/StaticMesh/RenderMesh.h"
#include "Devices/LogicalDevice.h"

#include <Common/System/Query.h>
#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetType.inl>

#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Threading/Semaphore.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Commands/UnifiedCommandBuffer.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>
#include <Renderer/Assets/StaticMesh/Primitives/Arc.h>
#include <Renderer/Assets/StaticMesh/Primitives/Arrow.h>
#include <Renderer/Assets/StaticMesh/Primitives/Box.h>
#include <Renderer/Assets/StaticMesh/Primitives/Capsule.h>
#include <Renderer/Assets/StaticMesh/Primitives/Cone.h>
#include <Renderer/Assets/StaticMesh/Primitives/Cylinder.h>
#include <Renderer/Assets/StaticMesh/Primitives/Plane.h>
#include <Renderer/Assets/StaticMesh/Primitives/Triangle.h>
#include <Renderer/Assets/StaticMesh/Primitives/Pyramid.h>
#include <Renderer/Assets/StaticMesh/Primitives/Sphere.h>
#include <Renderer/Assets/StaticMesh/Primitives/Torus.h>
#include <Renderer/Assets/StaticMesh/MeshSceneTag.h>
#include <Renderer/Renderer.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Asset/VirtualAsset.h>
#include <Common/Assert/Validate.h>
#include <Common/IO/Log.h>

namespace ngine::Rendering
{
	[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& MeshCache::GetRenderer()
	{
		return Memory::GetOwnerFromMember(*this, &Renderer::m_meshCache);
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& MeshCache::GetRenderer() const
	{
		return Memory::GetConstOwnerFromMember(*this, &Renderer::m_meshCache);
	}

	StaticMeshInfo::StaticMeshInfo() = default;
	StaticMeshInfo::StaticMeshInfo(StaticMeshGlobalLoadingCallback&& callback)
		: m_globalLoadingCallback(Forward<StaticMeshGlobalLoadingCallback>(callback))
	{
	}
	StaticMeshInfo::StaticMeshInfo(StaticMeshGlobalLoadingCallback&& callback, UniquePtr<StaticMesh> pMesh)
		: m_globalLoadingCallback(Forward<StaticMeshGlobalLoadingCallback>(callback))
		, m_pMesh(Forward<UniquePtr<StaticMesh>>(pMesh))
	{
	}
	StaticMeshInfo::~StaticMeshInfo() = default;

	MeshCache::MeshCache()
	{
		RegisterAssetModifiedCallback(System::Get<Asset::Manager>());

		System::Get<Engine>().OnRendererInitialized.Add(
			this,
			[](MeshCache& cache)
			{
				cache.GetRenderer().OnLogicalDeviceCreated.Add(cache, &MeshCache::OnLogicalDeviceCreated);

				cache.CreateProceduralMeshes();
			}
		);
	}

	MeshCache::~MeshCache()
	{
		IterateElements(
			m_meshData.GetView(),
			[](MeshData* pMeshData)
			{
				if (pMeshData != nullptr)
				{
					delete pMeshData;
				}
			}
		);
	}

	inline UniquePtr<RenderMesh> CreateDummyMesh(LogicalDevice& logicalDevice)
	{
		CommandBuffer transferCommandBuffer;
		CommandBuffer graphicsCommandBuffer;

		struct Data
		{
			Semaphore transferSignalSemaphore;
			EncodedCommandBufferView encodedTransferCommandBuffer;
			EncodedCommandBufferView encodedGraphicsCommandBuffer;
		};

		UniquePtr<Data> pData = UniquePtr<Data>::Make();

		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();

		CommandEncoder transferCommandEncoder;
		CommandEncoder graphicsCommandEncoder;

		{
			{
				const CommandPoolView transferCommandPool =
					thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer);
				transferCommandBuffer =
					CommandBuffer(logicalDevice, transferCommandPool, logicalDevice.GetCommandQueue(Rendering::QueueFamily::Transfer));
				transferCommandEncoder = transferCommandBuffer.BeginEncoding(logicalDevice);
			}

			const bool isUnifiedGraphicsAndTransferQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics) ==
			                                               logicalDevice.GetCommandQueue(QueueFamily::Transfer);
			if (!isUnifiedGraphicsAndTransferQueue)
			{
				graphicsCommandBuffer = CommandBuffer(
					logicalDevice,
					thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
					logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
				);
				graphicsCommandEncoder = graphicsCommandBuffer.BeginEncoding(logicalDevice);
			}
		}

		StagingBuffer stagingBuffer;
		UniquePtr<RenderMesh> result =
			UniquePtr<RenderMesh>::Make(RenderMesh::Dummy, logicalDevice, transferCommandEncoder, graphicsCommandEncoder, stagingBuffer);

		const EncodedCommandBuffer encodedTransferCommandBuffer = transferCommandEncoder.StopEncoding();
		EncodedCommandBuffer encodedGraphicsCommandBuffer;

		if (graphicsCommandEncoder.IsValid())
		{
			encodedGraphicsCommandBuffer = graphicsCommandEncoder.StopEncoding();
		}

		pData->encodedTransferCommandBuffer = encodedTransferCommandBuffer;
		pData->encodedGraphicsCommandBuffer = encodedGraphicsCommandBuffer;

		pData->transferSignalSemaphore = Semaphore(logicalDevice);
		Data& data = *pData;

		{
			QueueSubmissionParameters parameters;
			parameters.m_signalSemaphores = encodedGraphicsCommandBuffer.IsValid()
			                                  ? ArrayView<const SemaphoreView>{pData->transferSignalSemaphore}
			                                  : ArrayView<const SemaphoreView>();
			parameters.m_finishedCallback = [&thread,
			                                 &logicalDevice,
			                                 stagingBuffer = Move(stagingBuffer),
			                                 pData = Move(pData),
			                                 transferCommandBuffer = Move(transferCommandBuffer),
			                                 graphicsCommandBuffer = Move(graphicsCommandBuffer)]() mutable
			{
				Threading::Job& destroyBuffersCallback = Threading::CreateCallback(
					[&logicalDevice, stagingBuffer = Move(stagingBuffer), pData = Move(pData)](Threading::JobRunnerThread&) mutable
					{
						if (stagingBuffer.IsValid())
						{
							stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
						}
						pData->transferSignalSemaphore.Destroy(logicalDevice);
					},
					Threading::JobPriority::DeallocateResourcesMin
				);

				thread.QueueExclusiveCallbackFromAnyThread(
					Threading::JobPriority::DeallocateResourcesMin,
					[transferCommandBuffer = Move(transferCommandBuffer),
				   graphicsCommandBuffer = Move(graphicsCommandBuffer),
				   &logicalDevice](Threading::JobRunnerThread& thread) mutable
					{
						Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);

						transferCommandBuffer.Destroy(
							logicalDevice,
							engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer)
						);
						graphicsCommandBuffer.Destroy(
							logicalDevice,
							engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
						);
					}
				);
				destroyBuffersCallback.Queue(System::Get<Threading::JobManager>());
			};

			if (data.encodedGraphicsCommandBuffer.IsValid())
			{
				parameters.m_submittedCallback = [finishedCallback = Move(parameters.m_finishedCallback), &data, &logicalDevice]() mutable
				{
					QueueSubmissionParameters parameters;
					parameters.m_waitSemaphores = ArrayView<const SemaphoreView>{data.transferSignalSemaphore};
					static constexpr EnumFlags<PipelineStageFlags> waitStageMask = PipelineStageFlags::Transfer;
					parameters.m_waitStagesMasks = ArrayView<const EnumFlags<PipelineStageFlags>>{waitStageMask};
					parameters.m_finishedCallback = Move(finishedCallback);
					logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
						.Queue(
							Threading::JobPriority::CoreRenderStageResources,
							ArrayView<const EncodedCommandBufferView, uint16>(data.encodedGraphicsCommandBuffer),
							Move(parameters)
						);
				};
			}

			logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer)
				.Queue(
					Threading::JobPriority::CoreRenderStageResources,
					ArrayView<const EncodedCommandBufferView, uint16>(data.encodedTransferCommandBuffer),
					Move(parameters)
				);
		}

		return result;
	}

	struct MeshAddresses
	{
		uint64 vertexNormalsBufferAddress;
		uint64 vertexTextureCoordinatesBufferAddress;
		uint64 indicesBufferAddress;
		uint64 pad;
	};

	void MeshCache::OnLogicalDeviceCreated(LogicalDevice& logicalDevice)
	{
		UniquePtr<PerLogicalDeviceData>& pDeviceData = m_perLogicalDeviceData[logicalDevice.GetIdentifier()];
		pDeviceData.CreateInPlace();
		PerLogicalDeviceData& deviceData = *pDeviceData;

		deviceData.m_pDummyMesh = CreateDummyMesh(logicalDevice);

		const EnumFlags<Rendering::PhysicalDeviceFeatures> physicalDeviceFeatures = logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
		const bool supportsDynamicBufferIndexing = physicalDeviceFeatures.AreAllSet(
			Rendering::PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings | Rendering::PhysicalDeviceFeatures::RuntimeDescriptorArrays |
			Rendering::PhysicalDeviceFeatures::BufferDeviceAddress | Rendering::PhysicalDeviceFeatures::AccelerationStructure
		);
		if (supportsDynamicBufferIndexing)
		{
			deviceData.m_meshesDescriptorSetLayout =
				DescriptorSetLayout(logicalDevice, Array{DescriptorSetLayout::Binding::MakeStorageBuffer(0, ShaderStage::All)});
#if RENDERER_OBJECT_DEBUG_NAMES
			deviceData.m_meshesDescriptorSetLayout.SetDebugName(logicalDevice, "Meshes Indirect");
#endif

			Threading::EngineJobRunnerThread& currentJobRunner = *Threading::EngineJobRunnerThread::GetCurrent();

			const DescriptorPoolView descriptorPool = currentJobRunner.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
			[[maybe_unused]] const bool allocatedTexturesDescriptorSet = descriptorPool.AllocateDescriptorSets(
				logicalDevice,
				ArrayView<const DescriptorSetLayoutView>{deviceData.m_meshesDescriptorSetLayout},
				ArrayView<DescriptorSet>{deviceData.m_meshesDescriptorSet}
			);
			Assert(allocatedTexturesDescriptorSet);
			deviceData.m_pMeshesDescriptorPoolLoadingThread = &currentJobRunner;

			deviceData.m_meshesBuffer = StorageBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(MeshAddresses) * StaticMeshIdentifier::MaximumCount
			);

			{
				Array bufferInfos{
					DescriptorSet::BufferInfo{deviceData.m_meshesBuffer, 0, sizeof(MeshAddresses) * StaticMeshIdentifier::MaximumCount}
				};

				DescriptorSet::Update(
					logicalDevice,
					Array{
						DescriptorSet::UpdateInfo{deviceData.m_meshesDescriptorSet, 0, 0, DescriptorType::StorageBuffer, bufferInfos.GetDynamicView()}
					}
				);
			}
		}

		logicalDevice.OnDestroyed.Add(
			*this,
			[](MeshCache& meshCache, LogicalDevice& logicalDevice, const LogicalDeviceIdentifier deviceIdentifier)
			{
				PerLogicalDeviceData& deviceData = *meshCache.m_perLogicalDeviceData[deviceIdentifier];

				if (deviceData.m_pMeshesDescriptorPoolLoadingThread != nullptr)
				{
					Rendering::JobRunnerData& jobRunnerData = deviceData.m_pMeshesDescriptorPoolLoadingThread->GetRenderData();
					jobRunnerData.DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(deviceData.m_meshesDescriptorSet));
					jobRunnerData.DestroyDescriptorSetLayout(logicalDevice.GetIdentifier(), Move(deviceData.m_meshesDescriptorSetLayout));
					jobRunnerData.DestroyBuffer(logicalDevice.GetIdentifier(), Move(deviceData.m_meshesBuffer));
				}

				meshCache.IterateElements(
					deviceData.m_meshes.GetView(),
					[&logicalDevice](UniquePtr<RenderMesh>& pMesh)
					{
						if (pMesh != nullptr)
						{
							pMesh->Destroy(logicalDevice);
						}
					}
				);

				deviceData.m_pDummyMesh->Destroy(logicalDevice);
				meshCache.m_perLogicalDeviceData[deviceIdentifier].DestroyElement();
			}
		);
	}

	void MeshCache::CreateProceduralMeshes()
	{
		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		// Arc
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);
				Assert(meshInfo.m_pMesh.IsValid());

				Math::Anglef angle = 90_degrees;
				constexpr Math::Lengthf halfHeight = 0.5_meters;
				constexpr Math::Radiusf outer = 1.0_meters;
				constexpr Math::Radiusf inner = 0.5_meters;
				constexpr uint16 sideCount = 16u;

				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Arc::Create(angle, halfHeight, outer, inner, sideCount)));
				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			assetManager.RegisterAsset(
				ngine::Primitives::ArcMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Arc"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					Asset::Guid{},
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
			BaseType::RegisterAsset(
				ngine::Primitives::ArcMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
		}

		// Cylinder
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				constexpr uint8 sideCount = 8;
				constexpr uint8 segmentCount = 2;

				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf radius = 0.5_meters;
				constexpr Math::Lengthf halfHeight = 0.5_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Cylinder::Create(radius, halfHeight, segmentCount, sideCount)));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::CylinderMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::CylinderMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Cylinder"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					Asset::Guid{},
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Plane
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				StaticObject
					staticObject(Memory::ConstructWithSize, Memory::Uninitialized, Primitives::Plane::VertexCount, Primitives::Plane::IndexCount);

				constexpr Math::Radiusf radius = 0.5_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Plane::Create(Math::Vector2f(radius.GetMeters()))));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::PlaneMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::PlaneMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Plane"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::PlaneMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Triangle
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				StaticObject staticObject(
					Memory::ConstructWithSize,
					Memory::Uninitialized,
					Primitives::Triangle::VertexCount,
					Primitives::Triangle::IndexCount
				);

				constexpr Math::Radiusf radius = 0.5_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Triangle::Create(Math::Vector2f(radius.GetMeters()))));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::TriangleMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::TriangleMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Triangle"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					Asset::Guid{},
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);

			BaseType::RegisterAsset(
				ngine::Primitives::ScreenSpaceMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::ScreenSpaceMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(
						MAKE_PATH("Screen Space Mesh"),
						MeshSceneAssetType::AssetFormat.binaryFileExtension,
						Asset::VirtualAsset::FileExtension
					),
					UnicodeString{},
					UnicodeString{},
					Asset::Guid{},
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Capsule
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				constexpr uint8 sideCount = 8;
				constexpr uint8 segmentCount = 2;

				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf radius = 0.5_meters;
				constexpr Math::Lengthf height = 1_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Primitives::Capsule::Create(radius, height, segmentCount, sideCount));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::CapsuleMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::CapsuleMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Capsule"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::CapsuleMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Cone
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				constexpr uint8 sideCount = 8;

				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf radius = 0.5_meters;
				constexpr Math::Lengthf height = 1_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Primitives::Cone::Create(radius, height, sideCount));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::ConeMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::ConeMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Cone"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::ConeMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Box
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);
				Assert(meshInfo.m_pMesh.IsValid());

				constexpr Math::Radiusf radius = 0.5_meters;
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Box::CreateUniform(radius)));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::BoxMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::BoxMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Box"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::BoxMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Pyramid
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);
				Assert(meshInfo.m_pMesh.IsValid());

				constexpr Math::Radiusf radius = 0.5_meters;
				constexpr Math::Lengthf height = 1_meters;

				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Pyramid::Create(radius, height)));
				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::PyramidMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::PyramidMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Pyramid"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					Asset::Guid{},
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Sphere
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				constexpr uint8 latitudes = 16;
				constexpr uint8 longitudes = 16;

				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf radius = 0.5_meters;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Sphere::Create(radius, latitudes, longitudes)));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::SphereMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::SphereMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Sphere"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::SphereMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Torus
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf radius = 0.5_meters;
				constexpr Math::Lengthf thickness = 0.2_meters;
				constexpr uint8 sideCount = 8;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Torus::Create(radius, thickness, sideCount)));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::TorusMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::TorusMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Torus"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::TorusMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}

		// Arrow
		{
			auto globalLoadingCallback = [](const StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

				const StaticMeshInfo& meshInfo = meshCache.GetAssetData(identifier);

				constexpr Math::Radiusf shaftRadius = 0.05_meters;
				constexpr Math::Radiusf shaftHeight = 0.75_meters;
				constexpr Math::Radiusf tipRadius = 0.3_meters;
				constexpr Math::Radiusf tipHeight = 0.25_meters;
				constexpr uint8 sideCount = 8;

				Assert(meshInfo.m_pMesh.IsValid());
				meshInfo.m_pMesh->SetStaticObjectData(Move(Primitives::Arrow::Create(shaftRadius, tipRadius, shaftHeight, tipHeight, sideCount)));

				meshCache.OnMeshLoaded(identifier);

				return {};
			};

			BaseType::RegisterAsset(
				ngine::Primitives::ArrowMeshAssetGuid,
				[globalLoadingCallback = Move(globalLoadingCallback)](const StaticMeshIdentifier identifier, const Asset::Guid) mutable
				{
					return StaticMeshInfo{
						Forward<decltype(globalLoadingCallback)>(globalLoadingCallback),
						UniquePtr<StaticMesh>::Make(identifier, identifier)
					};
				}
			);
			assetManager.RegisterAsset(
				ngine::Primitives::ArrowMeshAssetGuid,
				Asset::DatabaseEntry{
					MeshSceneAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(MAKE_PATH("Arrow"), MeshSceneAssetType::AssetFormat.binaryFileExtension, Asset::VirtualAsset::FileExtension),
					UnicodeString{},
					UnicodeString{},
					ngine::Primitives::ArrowMeshThumbnailAssetGuid,
					Array{Tag::Guid{Tags::MeshPart}}.GetDynamicView()
				},
				Asset::Identifier{}
			);
		}
	}

	StaticMeshIdentifier MeshCache::Create(StaticMeshGlobalLoadingCallback&& globalLoadingCallback, const EnumFlags<StaticMeshFlags> flags)
	{
		const Asset::Guid guid = Asset::Guid::Generate();

		return BaseType::RegisterAsset(
			guid,
			[globalLoadingCallback = Forward<StaticMeshGlobalLoadingCallback>(globalLoadingCallback),
		   flags](const StaticMeshIdentifier identifier, const Guid) mutable -> StaticMeshInfo
			{
				return StaticMeshInfo{Move(globalLoadingCallback), UniquePtr<StaticMesh>::Make(identifier, identifier, flags)};
			}
		);
	}

	void MeshCache::Remove(const StaticMeshIdentifier identifier)
	{
		MeshData* pMeshData = m_meshData[identifier];
		if (pMeshData != nullptr && m_meshData[identifier].CompareExchangeStrong(pMeshData, nullptr))
		{
			delete pMeshData;
		}
		m_loadingMeshes.Clear(identifier);

		for (Optional<LogicalDevice*> pLogicalDevice : System::Get<Renderer>().GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				const UniquePtr<PerLogicalDeviceData>& pPerDeviceData = m_perLogicalDeviceData[pLogicalDevice->GetIdentifier()];
				if (pPerDeviceData.IsValid())
				{
					{
						UniquePtr<RenderMesh> pMesh = Move(pPerDeviceData->m_meshes[identifier]);
						if (pMesh.IsValid())
						{
							pMesh->Destroy(*pLogicalDevice);
						}
					}
					pPerDeviceData->m_loadingRenderMeshes.Clear(identifier);

					{
						Threading::UniqueLock lock(pPerDeviceData->m_meshRequesterMutex);
						auto it = pPerDeviceData->m_meshRequesterMap.Find(identifier);
						if (it != pPerDeviceData->m_meshRequesterMap.end())
						{
							pPerDeviceData->m_meshRequesterMap.Remove(it);
						}
					}
				}
			}
		}

		BaseType::DeregisterAsset(identifier);
	}

	struct LoadStaticMeshGlobalDataFromDiskJob : public Threading::Job
	{
		LoadStaticMeshGlobalDataFromDiskJob(const StaticMeshIdentifier identifier, const Threading::JobPriority priority)
			: Threading::Job(priority)
			, m_identifier(identifier)
		{
		}
		LoadStaticMeshGlobalDataFromDiskJob(const LoadStaticMeshGlobalDataFromDiskJob&) = delete;
		LoadStaticMeshGlobalDataFromDiskJob& operator=(const LoadStaticMeshGlobalDataFromDiskJob&) = delete;
		LoadStaticMeshGlobalDataFromDiskJob(LoadStaticMeshGlobalDataFromDiskJob&&) = delete;
		LoadStaticMeshGlobalDataFromDiskJob& operator=(LoadStaticMeshGlobalDataFromDiskJob&&) = delete;
		virtual ~LoadStaticMeshGlobalDataFromDiskJob() = default;

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final
		{
			const Asset::Guid assetGuid = System::Get<Rendering::Renderer>().GetMeshCache().GetAssetGuid(m_identifier);
			Threading::Job* pAsyncJob = System::Get<Asset::Manager>().RequestAsyncLoadAssetBinary(
				assetGuid,
				GetPriority(),
				[this](const ConstByteView data) mutable
				{
					MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
					if (LIKELY(data.HasElements()))
					{
						const StaticMeshInfo& meshInfo = meshCache.GetAssetData(m_identifier);

						LogWarningIf(!meshInfo.m_pMesh.IsValid(), "Mesh was invalid!");

						StaticObject staticObject(data);
						if (LIKELY(staticObject.IsValid()))
						{
							meshInfo.m_pMesh->SetStaticObjectData(Move(staticObject));

							meshCache.OnMeshLoaded(m_identifier);
						}
						else
						{
							meshCache.OnMeshLoadingFailed(m_identifier);
						}
					}
					else
					{
						// Temporary, doesn't compile on clang inlined for some reason.
						auto errorCallback = [this, &meshCache]()
						{
							LogWarning(
								"Mesh load failed: Asset {0} with guid {1} binary load failure",
								m_identifier.GetIndex(),
								meshCache.GetAssetGuid(m_identifier)
							);
							meshCache.OnMeshLoadingFailed(m_identifier);
						};

						COLD_ERROR_LOGIC(errorCallback);
					}
					SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					delete this;
				}
			);

			if (pAsyncJob != nullptr)
			{
				pAsyncJob->Queue(thread);
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread&) override final
		{
			return Result::AwaitExternalFinish;
		}
	protected:
		const StaticMeshIdentifier m_identifier;
	};

	static Threading::JobBatch LoadDefaultMeshGlobalDataFromDisk(const StaticMeshIdentifier identifier)
	{
		return Threading::JobBatch(new LoadStaticMeshGlobalDataFromDiskJob(identifier, Threading::JobPriority::LoadMeshData));
	}

	StaticMeshIdentifier MeshCache::RegisterAsset(const Asset::Guid guid)
	{
		const StaticMeshIdentifier identifier = BaseType::RegisterAsset(
			guid,
			[](const StaticMeshIdentifier identifier, const Guid) mutable -> StaticMeshInfo
			{
				return StaticMeshInfo{LoadDefaultMeshGlobalDataFromDisk, UniquePtr<StaticMesh>::Make(identifier, identifier)};
			}
		);
		return identifier;
	}

	StaticMeshIdentifier MeshCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const StaticMeshIdentifier identifier, const Asset::Guid)
			{
				return StaticMeshInfo{LoadDefaultMeshGlobalDataFromDisk, UniquePtr<StaticMesh>::Make(identifier, identifier)};
			}
		);
	}

	struct LoadStaticMeshPerLogicalDeviceDataJob : public Threading::Job
	{
		enum class LoadStatus : uint8
		{
			AwaitingLoad,
			AwaitingRenderMeshCreation,
			AwaitingTransferStart,
			AwaitingTransferCompletion
		};

		LoadStaticMeshPerLogicalDeviceDataJob(
			const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice, const Threading::JobPriority priority
		)
			: Threading::Job(priority)
			, m_identifier(identifier)
			, m_logicalDevice(logicalDevice)
		{
		}
		LoadStaticMeshPerLogicalDeviceDataJob(const LoadStaticMeshPerLogicalDeviceDataJob&) = delete;
		LoadStaticMeshPerLogicalDeviceDataJob& operator=(const LoadStaticMeshPerLogicalDeviceDataJob&) = delete;
		LoadStaticMeshPerLogicalDeviceDataJob(LoadStaticMeshPerLogicalDeviceDataJob&&) = delete;
		LoadStaticMeshPerLogicalDeviceDataJob& operator=(LoadStaticMeshPerLogicalDeviceDataJob&&) = delete;
		virtual ~LoadStaticMeshPerLogicalDeviceDataJob()
		{
			m_transferSignalSemaphore.Destroy(m_logicalDevice);
		}

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread&) override final
		{
			switch (m_status)
			{
				case LoadStatus::AwaitingTransferStart:
				{
					m_status = LoadStatus::AwaitingTransferCompletion;

					QueueSubmissionParameters parameters;
					parameters.m_signalSemaphores = m_graphicsCommandBuffer.IsValid() ? ArrayView<const SemaphoreView>{m_transferSignalSemaphore}
					                                                                  : ArrayView<const SemaphoreView>{};

					if (m_graphicsCommandBuffer.IsValid())
					{
						parameters.m_submittedCallback = [this]()
						{
							QueueSubmissionParameters parameters;
							parameters.m_waitSemaphores = ArrayView<const SemaphoreView>{m_transferSignalSemaphore};
							static constexpr EnumFlags<PipelineStageFlags> waitStageMask = PipelineStageFlags::Transfer;
							parameters.m_waitStagesMasks = ArrayView<const EnumFlags<PipelineStageFlags>>{waitStageMask};
							parameters.m_finishedCallback = [this]
							{
								Queue(System::Get<Threading::JobManager>());
							};
							m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
								.Queue(GetPriority(), ArrayView<const EncodedCommandBufferView, uint16>(m_graphicsCommandBuffer), Move(parameters));
						};
					}
					else
					{
						parameters.m_finishedCallback = [this]()
						{
							Queue(System::Get<Threading::JobManager>());
						};
					}

					m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer)
						.Queue(GetPriority(), ArrayView<const EncodedCommandBufferView, uint16>(m_transferCommandBuffer), Move(parameters));
				}
				break;
				case LoadStatus::AwaitingRenderMeshCreation:
				{
#if RENDERER_WEBGPU
					m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer)
						.QueueCallback(
							[this]()
							{
								const StaticMesh& staticMesh = *m_logicalDevice.GetRenderer().GetMeshCache().FindMesh(m_identifier);
								const ArrayView<const Index, Index> indices = staticMesh.GetIndices();

								m_renderMesh = RenderMesh(
									m_logicalDevice,
									m_transferCommandEncoder,
									m_graphicsCommandEncoder,
									staticMesh.GetVertexCount(),
									staticMesh.GetVertexData(),
									indices,
									m_stagingBuffer,
									staticMesh.ShouldAllowCpuVertexAccess()
								);
								Queue(System::Get<Threading::JobManager>());
							}
						);
#endif
				}
				break;
				default:
					break;
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread& thread) override final
		{
			switch (m_status)
			{
				case LoadStatus::AwaitingLoad:
				{
					const Optional<const StaticMesh*> pStaticMesh = m_logicalDevice.GetRenderer().GetMeshCache().FindMesh(m_identifier);
					Assert(pStaticMesh.IsValid());
					if (UNLIKELY(!pStaticMesh.IsValid()))
					{
						return Result::FinishedAndDelete;
					}

					// TODO: Delay LoadStaticMeshPerLogicalDeviceDataJob queuing until pStaticMesh->HasFinishedLoading will return true so we don't
					// have to check it
					if (!pStaticMesh->HasFinishedLoading())
					{
						return Result::TryRequeue;
					}

					if (UNLIKELY(pStaticMesh->DidLoadingFail()))
					{
						return Result::FinishedAndDelete;
					}
					Assert(pStaticMesh->IsLoaded());

					Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
					m_transferCommandBuffer = UnifiedCommandBuffer(
						m_logicalDevice,
						engineThread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer),
						m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Transfer)
					);

					m_transferCommandEncoder = m_transferCommandBuffer.BeginEncoding(m_logicalDevice);

					const bool isUnifiedGraphicsAndTransferQueue = m_logicalDevice.GetCommandQueue(QueueFamily::Graphics) ==
					                                               m_logicalDevice.GetCommandQueue(QueueFamily::Transfer);
					if (!isUnifiedGraphicsAndTransferQueue)
					{
						m_graphicsCommandBuffer = UnifiedCommandBuffer(
							m_logicalDevice,
							engineThread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
							m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
						);
						m_graphicsCommandEncoder = m_graphicsCommandBuffer.BeginEncoding(m_logicalDevice);
					}
					m_pCommandBufferThread = &thread;

					m_status = LoadStatus::AwaitingRenderMeshCreation;
#if RENDERER_WEBGPU
					return Result::AwaitExternalFinish;
#else
					const ArrayView<const Index, Index> indices = pStaticMesh->GetIndices();

					m_renderMesh = RenderMesh(
						m_logicalDevice,
						m_transferCommandEncoder,
						m_graphicsCommandEncoder,
						pStaticMesh->GetVertexCount(),
						pStaticMesh->GetVertexData(),
						indices,
						m_stagingBuffer,
						pStaticMesh->ShouldAllowCpuVertexAccess()
					);
#endif
				}
					[[fallthrough]];
				case LoadStatus::AwaitingRenderMeshCreation:
				{
					m_transferCommandBuffer.StopEncoding();
					if (m_graphicsCommandBuffer.IsValid())
					{
						m_graphicsCommandBuffer.StopEncoding();
					}

					m_status = LoadStatus::AwaitingTransferStart;
					return Result::AwaitExternalFinish;
				}
				case LoadStatus::AwaitingTransferCompletion:
				{
					RenderMesh previousMesh;
					m_logicalDevice.GetRenderer()
						.GetMeshCache()
						.OnRenderMeshLoaded(m_logicalDevice.GetIdentifier(), m_identifier, Move(m_renderMesh), previousMesh);

					if (previousMesh.IsValid())
					{
						previousMesh.Destroy(m_logicalDevice);
					}

					if (m_stagingBuffer.IsValid())
					{
						thread.QueueCallbackFromThread(
							Threading::JobPriority::DeallocateResourcesMin,
							[stagingBuffer = Move(m_stagingBuffer), &logicalDevice = m_logicalDevice](Threading::JobRunnerThread&) mutable
							{
								stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
							}
						);
					}

					m_pCommandBufferThread->QueueExclusiveCallbackFromAnyThread(
						Threading::JobPriority::DeallocateResourcesMin,
						[transferCommandBuffer = Move(m_transferCommandBuffer),
					   graphicsCommandEncoder = Move(m_graphicsCommandBuffer),
					   &logicalDevice = m_logicalDevice](Threading::JobRunnerThread& thread) mutable
						{
							Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
							transferCommandBuffer.Destroy(
								logicalDevice,
								engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer)
							);
							graphicsCommandEncoder.Destroy(
								logicalDevice,
								engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
							);
						}
					);

					return Result::FinishedAndDelete;
				}
				default:
					ExpectUnreachable();
			}
		}
	protected:
		LoadStatus m_status = LoadStatus::AwaitingLoad;
		UnifiedCommandBuffer m_transferCommandBuffer;
		UnifiedCommandBuffer m_graphicsCommandBuffer;
		Threading::JobRunnerThread* m_pCommandBufferThread = nullptr;
		CommandEncoderView m_graphicsCommandEncoder;
		CommandEncoderView m_transferCommandEncoder;
		StagingBuffer m_stagingBuffer;
		RenderMesh m_renderMesh;

		const StaticMeshIdentifier m_identifier;
		LogicalDevice& m_logicalDevice;
		Semaphore m_transferSignalSemaphore = Semaphore(m_logicalDevice);
	};

	[[nodiscard]] inline Threading::Job& CreateStaticRenderMeshLoadingJob(
		const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice, const Threading::JobPriority priority
	)
	{
		return *(new LoadStaticMeshPerLogicalDeviceDataJob(identifier, logicalDevice, priority));
	}

	bool MeshCache::IsLoadingStaticMesh(const StaticMeshIdentifier identifier) const
	{
		return m_loadingMeshes.IsSet(identifier);
	}

	MeshCache::MeshData& MeshCache::GetOrCreateMeshData(const StaticMeshIdentifier identifier)
	{
		Threading::Atomic<MeshData*>& meshData = m_meshData[identifier];

		if (meshData.Load() != nullptr)
		{
			return *meshData;
		}
		else
		{
			MeshData* pExpected = nullptr;
			MeshData* pNewValue = new MeshData();
			if (meshData.CompareExchangeStrong(pExpected, pNewValue))
			{
				return *pNewValue;
			}
			else
			{
				delete pNewValue;
				return *pExpected;
			}
		}
	}

	Threading::JobBatch MeshCache::TryLoadStaticMesh(const StaticMeshIdentifier identifier, MeshLoadListenerData&& newListenerData)
	{
		MeshData& meshData = GetOrCreateMeshData(identifier);
		meshData.m_onLoadedCallback.Emplace(Forward<MeshLoadListenerData>(newListenerData));

		const Optional<const StaticMesh*> pMesh = FindMesh(identifier);
		Assert(pMesh.IsValid());
		if (UNLIKELY(pMesh.IsInvalid()))
		{
			return {};
		}

		if (pMesh->IsLoaded() || pMesh->DidLoadingFail())
		{
			[[maybe_unused]] const bool wasExecuted = meshData.m_onLoadedCallback.Execute(newListenerData.m_identifier, identifier);
			Assert(wasExecuted);
		}
		else
		{
			if (m_loadingMeshes.Set(identifier))
			{
				if (!pMesh->IsLoaded())
				{
					const StaticMeshGlobalLoadingCallback& loadingCallback = GetAssetData(identifier).m_globalLoadingCallback;
					if (Threading::JobBatch jobBatch = loadingCallback(identifier))
					{
						Assert(m_loadingMeshes.IsSet(identifier));
						return jobBatch;
					}
					else
					{
						Assert(!m_loadingMeshes.IsSet(identifier));
					}
				}
				else
				{
					[[maybe_unused]] const bool wasCleared = m_loadingMeshes.Clear(identifier);
					Assert(wasCleared);
					[[maybe_unused]] const bool wasExecuted = meshData.m_onLoadedCallback.Execute(newListenerData.m_identifier, identifier);
					Assert(wasExecuted);
				}
			}
			else
			{
			}
		}

		return {};
	}

	Threading::JobBatch MeshCache::TryReloadStaticMesh(const StaticMeshIdentifier identifier)
	{
		if (m_loadingMeshes.Set(identifier))
		{
			const StaticMeshGlobalLoadingCallback& loadingCallback = GetAssetData(identifier).m_globalLoadingCallback;
			if (Threading::JobBatch jobBatch = loadingCallback(identifier))
			{
				Assert(m_loadingMeshes.IsSet(identifier));
				return jobBatch;
			}
			else
			{
				Assert(!m_loadingMeshes.IsSet(identifier));
			}
		}
		return {};
	}

	Threading::JobBatch MeshCache::LoadRenderMeshData(const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice)
	{
		Threading::JobBatch intermediateStage{Threading::JobBatch::IntermediateStage};
		Threading::IntermediateStage& finishedLoadingStage = Threading::CreateIntermediateStage();
		finishedLoadingStage.AddSubsequentStage(intermediateStage.GetFinishedStage());

		if (Threading::JobBatch loadStaticMeshJobBatch = TryLoadStaticMesh(identifier, MeshLoadListenerData{*this, [pFinishedLoadingStage = Threading::Atomic<Threading::IntermediateStage*>(&finishedLoadingStage)](MeshCache&, const StaticMeshIdentifier) mutable
			{
                Threading::IntermediateStage* pStage = pFinishedLoadingStage;
                if(pStage != nullptr && pFinishedLoadingStage.CompareExchangeStrong(pStage, nullptr))
                {
                    pStage->SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
                }
                return EventCallbackResult::Remove;
			}}))
		{
			PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[logicalDevice.GetIdentifier()];
			if (perDeviceData.m_meshRequesterMap.Contains(identifier) && perDeviceData.m_loadingRenderMeshes.Set(identifier))
			{
				Threading::Job& loadRenderMeshJob =
					CreateStaticRenderMeshLoadingJob(identifier, logicalDevice, Threading::JobPriority::CreateRenderMesh);

				intermediateStage.QueueAsNewFinishedStage(loadRenderMeshJob);
				loadStaticMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);

				return loadStaticMeshJobBatch;
			}

			loadStaticMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);

			Assert(m_loadingMeshes.IsSet(identifier));
			return loadStaticMeshJobBatch;
		}
		else
		{
			PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[logicalDevice.GetIdentifier()];
			if (perDeviceData.m_meshRequesterMap.Contains(identifier) && perDeviceData.m_loadingRenderMeshes.Set(identifier))
			{
				Threading::Job& loadRenderMeshJob =
					CreateStaticRenderMeshLoadingJob(identifier, logicalDevice, Threading::JobPriority::CreateRenderMesh);
				intermediateStage.QueueAsNewFinishedStage(loadRenderMeshJob);
				loadStaticMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);

				return loadStaticMeshJobBatch;
			}
		}

		return {};
	}

	Threading::JobBatch MeshCache::ReloadRenderMeshData(const StaticMeshIdentifier identifier, LogicalDevice& logicalDevice)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[logicalDevice.GetIdentifier()];
		if (perDeviceData.m_meshRequesterMap.Contains(identifier) && perDeviceData.m_loadingRenderMeshes.Set(identifier))
		{
			return Threading::JobBatch(CreateStaticRenderMeshLoadingJob(identifier, logicalDevice, Threading::JobPriority::CreateRenderMesh));
		}
		return {};
	}

	void MeshCache::OnMeshLoaded(const StaticMeshIdentifier identifier)
	{
		[[maybe_unused]] const bool cleared = m_loadingMeshes.Clear(identifier);
		Assert(cleared);
		GetAssetData(identifier).m_pMesh->OnLoaded();

		m_meshData[identifier]->m_onLoadedCallback.ExecuteAndClear(identifier);
	}

	void MeshCache::OnMeshLoadingFailed(const StaticMeshIdentifier identifier)
	{
		[[maybe_unused]] const bool cleared = m_loadingMeshes.Clear(identifier);
		Assert(cleared);
		GetAssetData(identifier).m_pMesh->OnLoadingFailed();

		m_meshData[identifier]->m_onLoadedCallback.ExecuteAndClear(identifier);
	}

	Threading::JobBatch MeshCache::TryLoadRenderMesh(
		const LogicalDeviceIdentifier deviceIdentifier,
		const StaticMeshIdentifier identifier,
		RenderMeshLoadListenerData&& newListenerData,
		const EnumFlags<MeshLoadFlags> flags
	)
	{
		LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		auto getMeshRequesters = [&perDeviceData, identifier]() -> PerLogicalDeviceData::MeshRequesters&
		{
			PerLogicalDeviceData::MeshRequesters* pMeshRequesters;
			{
				Threading::SharedLock readLock(perDeviceData.m_meshRequesterMutex);
				decltype(perDeviceData.m_meshRequesterMap)::iterator it = perDeviceData.m_meshRequesterMap.Find(identifier);
				if (it != perDeviceData.m_meshRequesterMap.end())
				{
					pMeshRequesters = it->second;
				}
				else
				{
					readLock.Unlock();
					Threading::UniqueLock writeLock(perDeviceData.m_meshRequesterMutex);
					it = perDeviceData.m_meshRequesterMap.Find(identifier);
					if (it != perDeviceData.m_meshRequesterMap.end())
					{
						pMeshRequesters = it->second;
					}
					else
					{
						pMeshRequesters = perDeviceData.m_meshRequesterMap
						                    .Emplace(StaticMeshIdentifier(identifier), UniqueRef<PerLogicalDeviceData::MeshRequesters>::Make())
						                    ->second;
					}
				}
			}

			return *pMeshRequesters;
		};

		PerLogicalDeviceData::MeshRequesters& meshRequesters = getMeshRequesters();
		meshRequesters.m_onLoadedCallback.Emplace(Forward<RenderMeshLoadListenerData>(newListenerData));

		UniquePtr<RenderMesh>& pRenderMesh = perDeviceData.m_meshes[identifier];
		if (pRenderMesh.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = meshRequesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, *pRenderMesh, {});
			Assert(wasExecuted);
		}
		else if (const Optional<const StaticMesh*> pStaticMesh = FindMesh(identifier); pStaticMesh.IsValid() && pStaticMesh->IsLoaded())
		{
			if (perDeviceData.m_loadingRenderMeshes.Set(identifier))
			{
				if (pRenderMesh.IsValid())
				{
					// Do nothing, mesh requester will have been used
					[[maybe_unused]] const bool cleared = perDeviceData.m_loadingRenderMeshes.Clear(identifier);
					Assert(cleared);

					[[maybe_unused]] const bool wasExecuted =
						meshRequesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, *pRenderMesh, {});
					Assert(wasExecuted);
				}
				else
				{
					if (flags.IsSet(MeshLoadFlags::LoadDummy))
					{
						[[maybe_unused]] const bool wasExecuted = meshRequesters.m_onLoadedCallback.Execute(
							newListenerData.m_identifier,
							*perDeviceData.m_pDummyMesh,
							LoadedMeshFlags::IsDummy
						);
						Assert(wasExecuted);
					}

					return Threading::JobBatch(CreateStaticRenderMeshLoadingJob(identifier, logicalDevice, Threading::JobPriority::CreateRenderMesh));
				}
			}
			else if (flags.IsSet(MeshLoadFlags::LoadDummy))
			{
				[[maybe_unused]] const bool wasExecuted =
					meshRequesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, *perDeviceData.m_pDummyMesh, LoadedMeshFlags::IsDummy);
				Assert(wasExecuted);
			}
		}
		else
		{
			if (flags.IsSet(MeshLoadFlags::LoadDummy))
			{
				[[maybe_unused]] const bool wasExecuted =
					meshRequesters.m_onLoadedCallback.Execute(newListenerData.m_identifier, *perDeviceData.m_pDummyMesh, LoadedMeshFlags::IsDummy);
				Assert(wasExecuted);
			}
			return LoadRenderMeshData(identifier, logicalDevice);
		}

		return {};
	}

	bool MeshCache::RemoveRenderMeshListener(
		const LogicalDeviceIdentifier deviceIdentifier,
		const StaticMeshIdentifier identifier,
		const RenderMeshListenerIdentifier listenerIdentifier
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
		PerLogicalDeviceData::MeshRequesters* pMeshRequesters;
		{
			Threading::SharedLock requestersReadLock(perDeviceData.m_meshRequesterMutex);
			decltype(perDeviceData.m_meshRequesterMap)::iterator it = perDeviceData.m_meshRequesterMap.Find(identifier);
			if (UNLIKELY(it == perDeviceData.m_meshRequesterMap.end()))
			{
				return false;
			}

			pMeshRequesters = it->second;
		}

		return pMeshRequesters->m_onLoadedCallback.Remove(listenerIdentifier);
	}

	void MeshCache::OnRenderMeshLoaded(
		const LogicalDeviceIdentifier deviceIdentifier, const StaticMeshIdentifier identifier, RenderMesh&& renderMesh, RenderMesh& previousMesh
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		if (perDeviceData.m_meshes[identifier].IsValid())
		{
			previousMesh = Move(*perDeviceData.m_meshes[identifier]);
		}

		perDeviceData.m_meshes[identifier].CreateInPlace(Move(renderMesh));
		[[maybe_unused]] const bool cleared = perDeviceData.m_loadingRenderMeshes.Clear(identifier);
		Assert(cleared);

		RenderMesh& storedRenderMesh = *perDeviceData.m_meshes[identifier];

		LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);

		if (perDeviceData.m_meshesBuffer.IsValid())
		{
			const BufferView meshBuffer = storedRenderMesh.GetVertexBuffer();
			const Rendering::Index vertexCount = storedRenderMesh.GetVertexCount();

			const uint64 normalsOffset = Memory::Align(sizeof(Rendering::VertexPosition) * vertexCount, alignof(Rendering::VertexNormals));
			const uint64 textureCoordinatesOffset =
				Memory::Align(normalsOffset + sizeof(Rendering::VertexNormals) * vertexCount, alignof(Rendering::VertexTextureCoordinate));
			const uint64 vertexBufferAddress = meshBuffer.GetDeviceAddress(logicalDevice);

			MeshAddresses addresses{
				vertexBufferAddress + normalsOffset,
				vertexBufferAddress + textureCoordinatesOffset,
				storedRenderMesh.GetIndexBuffer().GetDeviceAddress(logicalDevice)
			};

			StagingBuffer stagingBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(MeshAddresses),
				StagingBuffer::Flags::TransferSource
			);
			stagingBuffer.MapAndCopyFrom(
				logicalDevice,
				QueueFamily::Transfer,
				Array{DataToBuffer{0, ConstByteView::Make(addresses)}},
				Math::Range<size>::Make(0, sizeof(MeshAddresses))
			);

			{
				Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
				const Rendering::CommandPoolView commandPool =
					thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer);
				Rendering::SingleUseCommandBuffer
					commandBuffer(logicalDevice, commandPool, thread, Rendering::QueueFamily::Transfer, Threading::JobPriority::CreateRenderMesh);

				const CommandEncoderView commandEncoder = commandBuffer;
				{
					const BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
					blitCommandEncoder.RecordCopyBufferToBuffer(
						stagingBuffer,
						perDeviceData.m_meshesBuffer,
						Array{BufferCopy{0, sizeof(MeshAddresses) * identifier.GetFirstValidIndex(), sizeof(MeshAddresses)}}
					);
				}

				commandBuffer.OnFinished = [&logicalDevice, stagingBuffer = Move(stagingBuffer)]() mutable
				{
					stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				};
			}
		}

		{
			Threading::SharedLock readLock(perDeviceData.m_meshRequesterMutex);
			const decltype(perDeviceData.m_meshRequesterMap)::const_iterator it = perDeviceData.m_meshRequesterMap.Find(identifier);
			if (it != perDeviceData.m_meshRequesterMap.end())
			{
				PerLogicalDeviceData::MeshRequesters& __restrict requesters = *it->second;
				requesters.m_onLoadedCallback(storedRenderMesh, {});
			}
		}
	}

	bool MeshCache::IsMeshLoaded(const StaticMeshIdentifier identifier) const
	{
		const StaticMeshInfo& meshInfo = GetAssetData(identifier);
		return meshInfo.m_pMesh.IsValid() && meshInfo.m_pMesh->IsLoaded();
	}

	StaticMeshIdentifier MeshCache::Clone(const StaticMeshIdentifier masterMeshIdentifier, const EnumFlags<StaticMeshFlags> flags)
	{
		struct CloneMasterMeshJob final : public Threading::Job
		{
			CloneMasterMeshJob(
				Rendering::MeshCache& meshCache,
				const Rendering::StaticMeshIdentifier masterMeshIdentifier,
				const Rendering::StaticMeshIdentifier meshIdentifier
			)
				: Job(Priority::CloneMeshData)
				, m_meshCache(meshCache)
				, m_masterMeshIdentifier(masterMeshIdentifier)
				, m_meshIdentifier(meshIdentifier)
			{
			}
			virtual ~CloneMasterMeshJob() = default;

			virtual Result OnExecute(Threading::JobRunnerThread&) override
			{
				const Rendering::StaticMeshInfo& masterMeshInfo = m_meshCache.GetAssetData(m_masterMeshIdentifier);
				Assert(masterMeshInfo.m_pMesh.IsValid());

				if (masterMeshInfo.m_pMesh->IsLoaded())
				{
					const Rendering::StaticMeshInfo& meshInfo = m_meshCache.GetAssetData(m_meshIdentifier);
					if (LIKELY(meshInfo.m_pMesh.IsValid()))
					{
						meshInfo.m_pMesh->SetStaticObjectData(Rendering::StaticObject(masterMeshInfo.m_pMesh->GetStaticObjectData()));
						m_meshCache.OnMeshLoaded(m_meshIdentifier);
					}
					else
					{
						m_meshCache.OnMeshLoadingFailed(m_meshIdentifier);
					}
				}
				else
				{
					Assert(masterMeshInfo.m_pMesh->DidLoadingFail());
					m_meshCache.OnMeshLoadingFailed(m_meshIdentifier);
				}
				return Result::FinishedAndDelete;
			}
		protected:
			Rendering::MeshCache& m_meshCache;
			const Rendering::StaticMeshIdentifier m_masterMeshIdentifier;
			const Rendering::StaticMeshIdentifier m_meshIdentifier;
		};

		auto globalLoadingcallback = [this, masterMeshIdentifier](const Rendering::StaticMeshIdentifier identifier)
		{
			Threading::JobBatch intermediateStage{Threading::JobBatch::IntermediateStage};
			Threading::IntermediateStage& finishedLoadingStage = Threading::CreateIntermediateStage();
			finishedLoadingStage.AddSubsequentStage(intermediateStage.GetFinishedStage());

			Threading::JobBatch loadMasterMeshJobBatch = TryLoadStaticMesh(
				masterMeshIdentifier,
				MeshLoadListenerData{
					*this,
					[pFinishedLoadingStage = Threading::Atomic<Threading::IntermediateStage*>(&finishedLoadingStage
			     )](MeshCache&, [[maybe_unused]] const Rendering::StaticMeshIdentifier identifier) mutable
					{
						Threading::IntermediateStage* pStage = pFinishedLoadingStage;
						if (pStage != nullptr && pFinishedLoadingStage.CompareExchangeStrong(pStage, nullptr))
						{
							pStage->SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						}
						return EventCallbackResult::Remove;
					}
				}
			);
			Threading::Job* pCloneJob = new CloneMasterMeshJob(*this, masterMeshIdentifier, identifier);
			intermediateStage.QueueAsNewFinishedStage(*pCloneJob);
			loadMasterMeshJobBatch.QueueAsNewFinishedStage(intermediateStage);
			return loadMasterMeshJobBatch;
		};

		const Asset::Guid guid = Asset::Guid::Generate();
		return BaseType::RegisterAsset(
			guid,
			[globalLoadingCallback = Move(globalLoadingcallback),
		   masterMeshIdentifier,
		   flags](const StaticMeshIdentifier identifier, const Guid) mutable -> StaticMeshInfo
			{
				return StaticMeshInfo{
					Move(globalLoadingCallback),
					UniquePtr<StaticMesh>::Make(identifier, masterMeshIdentifier, flags | StaticMeshFlags::IsClone)
				};
			}
		);
	}

	Threading::JobBatch MeshCache::ReloadMesh(const IdentifierType identifier, LogicalDevice& logicalDevice)
	{
		return ReloadRenderMeshData(identifier, logicalDevice);
	}

	Threading::JobBatch MeshCache::ReloadMesh(const IdentifierType identifier)
	{
		if (IsMeshLoaded(identifier))
		{
			Threading::JobBatch loadStaticMeshJobBatch = TryReloadStaticMesh(identifier);

			Threading::JobBatch renderMeshesJobBatch;
			for (Optional<LogicalDevice*> pLogicalDevice : GetRenderer().GetLogicalDevices())
			{
				if (pLogicalDevice.IsValid())
				{
					Threading::JobBatch deviceJobBatch = ReloadRenderMeshData(identifier, *pLogicalDevice);
					renderMeshesJobBatch.QueueAfterStartStage(deviceJobBatch);
				}
			}

			loadStaticMeshJobBatch.QueueAsNewFinishedStage(renderMeshesJobBatch);
			return loadStaticMeshJobBatch;
		}
		return {};
	}

	void MeshCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		Threading::JobBatch reloadBatch = ReloadMesh(identifier);
		if (reloadBatch.IsValid())
		{
			reloadBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[this, identifier](Threading::JobRunnerThread&)
				{
					[[maybe_unused]] const bool wasCleared = m_reloadingAssets.Clear(identifier);
					Assert(wasCleared);
				},
				Threading::JobPriority::FileChangeDetection
			));

			Threading::JobRunnerThread::GetCurrent()->Queue(reloadBatch);
		}
	}
}
