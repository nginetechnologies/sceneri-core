#include "SSRStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/Vector3/Random.h>
#include <Common/Math/PseudoRandomDistributions.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>

#include <Common/System/Query.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Stages/RenderItemStage.h>
#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	SSRStage::SSRStage(SceneView& sceneView)
		: Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_pipeline(
				m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache(), m_sceneView.GetMatrices().GetDescriptorSetLayout()
			)
		, m_compositePipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
	{
	}

	SSRStage::~SSRStage()
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		m_pipeline.Destroy(logicalDevice);
		m_compositePipeline.Destroy(logicalDevice);

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread
			);
			previousEngineThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), m_descriptorSets.GetView());
		}
	}

	void SSRStage::PopulateSSRDescriptorSet(const DescriptorSetView descriptorSet)
	{
		Array<DescriptorSet::ImageInfo, (uint8)Textures::Count> imageInfo;
		Array<DescriptorSet::UpdateInfo, imageInfo.GetSize()> descriptorUpdates;
		const ArrayView<const ImageMappingView> imageMappings = m_imageMappings;

		for (uint8 i = 0; i < (uint8)Textures::Count; ++i)
		{
			Assert(imageMappings[i].IsValid());

			imageInfo[i] = DescriptorSet::ImageInfo{nullptr, imageMappings[i], ImageLayout::ShaderReadOnlyOptimal};

			descriptorUpdates[i] = DescriptorSet::UpdateInfo{
				descriptorSet,
				i,
				0,
				DescriptorType::SampledImage,
				ArrayView<const DescriptorSet::ImageInfo>(imageInfo[i]),
			};
		}

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void SSRStage::PopulateCompositeSSRDescriptorSet(const DescriptorSetView descriptorSet)
	{
		Array<DescriptorSet::ImageInfo, 2> imageInfo;
		Array<DescriptorSet::UpdateInfo, imageInfo.GetSize()> descriptorUpdates;
		const ArrayView<const ImageMappingView> imageMappings = m_compositeImageMappings;

		Assert(imageMappings[0].IsValid());

		imageInfo[0] = DescriptorSet::ImageInfo{nullptr, imageMappings[0], ImageLayout::General};
		descriptorUpdates[0] = DescriptorSet::UpdateInfo{
			descriptorSet,
			0,
			0,
			DescriptorType::StorageImage,
			ArrayView<const DescriptorSet::ImageInfo>(imageInfo[0]),
		};

		imageInfo[1] = {nullptr, imageMappings[1], ImageLayout::ShaderReadOnlyOptimal};
		descriptorUpdates[1] =
			DescriptorSet::UpdateInfo(descriptorSet, 1, 0, DescriptorType::SampledImage, ArrayView<const DescriptorSet::ImageInfo>(imageInfo[1]));

		DescriptorSet::Update(m_sceneView.GetLogicalDevice(), descriptorUpdates);
	}

	void SSRStage::OnBeforeRenderPassDestroyed()
	{
		for (ImageMappingView& imageMapping : m_imageMappings)
		{
			imageMapping = {};
		}
		for (ImageMappingView& imageMapping : m_compositeImageMappings)
		{
			imageMapping = {};
		}
	}

	bool SSRStage::ShouldRecordCommands() const
	{
		return m_imageMappings.GetView().All(
						 [](const ImageMappingView imageMapping)
						 {
							 return imageMapping.IsValid();
						 }
					 ) &
		         m_compositeImageMappings.GetView().All(
							 [](const ImageMappingView imageMapping)
							 {
								 return imageMapping.IsValid();
							 }
						 ) &
		         m_sceneView.HasActiveCamera() &&
		       m_pipeline.IsValid() && m_compositePipeline.IsValid();
	}

	void SSRStage::OnComputePassAttachmentsLoaded(
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputAttachments,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> outputInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> outputInputAttachments,
		[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> inputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, Rendering::FrameIndex> inputAttachments,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		switch (subpassIndex)
		{
			case 0:
			{
				m_imageMappings[Textures::Output] = outputAttachmentMappings[0][0];
				m_imageMappings[Textures::HDRScene] = inputAttachmentMappings[0][0];
				m_imageMappings[Textures::GBufferDepth] = inputAttachmentMappings[0][1];
				m_imageMappings[Textures::GBufferNormals] = inputAttachmentMappings[0][2];
				m_imageMappings[Textures::GBufferMaterialProperties] = inputAttachmentMappings[0][3];
			}
			break;
			case 1:
			{
				m_compositeImageMappings[0] = outputAttachmentMappings[0][0];
				m_compositeImageMappings[1] = inputAttachmentMappings[0][0];
				Threading::JobRunnerThread* pPreviousDescriptorLoadingThread = m_pDescriptorSetLoadingThread;

				LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

				Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent()
				);
				Array<DescriptorSetLayoutView, 2> descriptorSetLayouts{
					m_pipeline,
					m_compositePipeline,
				};

				Array<DescriptorSet, 2> descriptorSets;
				const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
				[[maybe_unused]] const bool allocatedDescriptorSets =
					descriptorPool.AllocateDescriptorSets(logicalDevice, descriptorSetLayouts.GetDynamicView(), descriptorSets.GetDynamicView());
				Assert(allocatedDescriptorSets);
				if (LIKELY(allocatedDescriptorSets))
				{
					m_pDescriptorSetLoadingThread = &thread;

					PopulateSSRDescriptorSet(descriptorSets[0]);
					PopulateCompositeSSRDescriptorSet(descriptorSets[1]);

					for (uint8 i = 0; i < 2; ++i)
					{
						m_descriptorSets[i].AtomicSwap(descriptorSets[i]);
					}

					if (pPreviousDescriptorLoadingThread != nullptr)
					{
						Threading::EngineJobRunnerThread& previousLoadingThread =
							static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorLoadingThread);
						previousLoadingThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), descriptorSets.GetView());
					}
				}
			}
			break;
		}
	}

	void SSRStage::RecordComputePassCommands(
		const ComputeCommandEncoderView computeCommandEncoder, const ViewMatrices& viewMatrices, const uint8 subpassIndex
	)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		computeCommandEncoder.SetDebugName(m_sceneView.GetLogicalDevice(), "SSR");
		const ComputeDebugMarker debugMarker{computeCommandEncoder, m_sceneView.GetLogicalDevice(), "SSR", "#00FF00"_color};
#endif

		switch (subpassIndex)
		{
			case 0:
			{
				const Math::Matrix4x4f& invProj = viewMatrices.GetMatrix(ViewMatrices::Type::InvertedProjection);
				const Math::Vector4f vsPos = Math::Vector4f(0.0f, 0.0f, 1.0f, 1.0f) * invProj;

				const float zNear = vsPos.z / vsPos.w;

				computeCommandEncoder.BindPipeline(m_pipeline);

				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);
				m_pipeline.Compute(
					m_sceneView.GetLogicalDevice(),
					zNear,
					Array<const DescriptorSetView, 2>(m_descriptorSets[0], viewMatrices.GetDescriptorSet()),
					computeCommandEncoder,
					renderResolution / SSRResolutionDivisor
				);
			}
			break;
			case 1:
			{
				computeCommandEncoder.BindPipeline(m_compositePipeline);

				const Math::Vector2ui renderResolution =
					(Math::Vector2ui)(Math::Vector2f(m_sceneView.GetDrawer().GetFullRenderResolution()) * UpscalingFactor);
				m_compositePipeline
					.Compute(ArrayView<const DescriptorSetView>(m_descriptorSets[1]), computeCommandEncoder, renderResolution / SSRResolutionDivisor);
			}
			break;
		}
	}
}
