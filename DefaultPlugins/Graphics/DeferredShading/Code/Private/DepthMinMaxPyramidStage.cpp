#include "DepthMinMaxPyramidStage.h"
#include "ShadowsStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Renderer.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Stages/RenderItemStage.h>
#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	DepthMinMaxPyramidStage::DepthMinMaxPyramidStage(SceneView& sceneView, const Optional<ShadowsStage*> pShadowsStage)
		: Rendering::Stage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_pShadowsStage(pShadowsStage)
		, m_initialPipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
		, m_pipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetShaderCache())
	{
	}

	DepthMinMaxPyramidStage::~DepthMinMaxPyramidStage()
	{
		Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		m_pipeline.Destroy(logicalDevice);

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousLoadingThread =
				static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread);
			previousLoadingThread.GetRenderData().DestroyDescriptorSets(logicalDevice.GetIdentifier(), m_reductionDescriptorSets.GetView());
		}

		m_initialPipeline.Destroy(logicalDevice);
		m_pipeline.Destroy(logicalDevice);
	}

	void DepthMinMaxPyramidStage::OnBeforeRenderPassDestroyed()
	{
		if (m_pDescriptorSetLoadingThread != nullptr && m_reductionDescriptorSets.GetView().All([](const DescriptorSetView descriptorSet) { return descriptorSet.IsValid(); }))
		{
			Threading::EngineJobRunnerThread& previousLoadingThread =
				static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread);
			previousLoadingThread.GetRenderData().DestroyDescriptorSets(m_logicalDevice.GetIdentifier(), m_reductionDescriptorSets.GetView());
		}
		m_reductionDescriptorSets = {};
	}

	void DepthMinMaxPyramidStage::OnComputePassAttachmentsLoaded(
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
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		switch (subpassIndex)
		{
			case 0:
			{
				if (m_reductionDescriptorSets.IsEmpty())
				{
					Math::Vector2ui renderResolution = inputAttachmentResolutions[0];
					renderResolution = Math::Max(renderResolution >> Math::Vector2ui{1}, Math::Vector2ui{1u});
					const Rendering::MipMask mipMask = Rendering::MipMask::FromSizeAllToLargest(renderResolution);
					const uint8 mipCount = uint8(mipMask.GetSize());

					Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent(
					));
					FlatVector<DescriptorSetLayoutView, 32> descriptorSetLayouts(Memory::ConstructWithSize, Memory::Uninitialized, mipCount);
					descriptorSetLayouts[0] = m_initialPipeline;
					for (uint8 mip = 1; mip < mipCount; ++mip)
					{
						descriptorSetLayouts[mip] = m_pipeline;
					}

					m_reductionDescriptorSets.Resize(mipCount);
					const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
					[[maybe_unused]] const bool allocatedDescriptorSets =
						descriptorPool.AllocateDescriptorSets(logicalDevice, descriptorSetLayouts.GetView(), m_reductionDescriptorSets);

					Assert(allocatedDescriptorSets);
					if (LIKELY(allocatedDescriptorSets))
					{
						m_pDescriptorSetLoadingThread = &thread;
					}
				}

				const Rendering::DescriptorSetView initialReductionDescriptorSet = m_reductionDescriptorSets[0];
				Assert(initialReductionDescriptorSet.IsValid());
				if (LIKELY(initialReductionDescriptorSet.IsValid()))
				{
					const ImageMappingView depthBufferImageMapping = inputAttachmentMappings[0][0];
					const ImageMappingView depthMinMaxFirstImageMapping = outputAttachmentMappings[0][0];
					const Array<DescriptorSet::ImageInfo, 2> imageInfos{
						DescriptorSet::ImageInfo{{}, depthBufferImageMapping, ImageLayout::DepthStencilReadOnlyOptimal},
						DescriptorSet::ImageInfo{{}, depthMinMaxFirstImageMapping, ImageLayout::General},
					};
					const Array<DescriptorSet::UpdateInfo, 2> descriptorUpdates{
						DescriptorSet::UpdateInfo{initialReductionDescriptorSet, 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(0, 1)},
						DescriptorSet::UpdateInfo{initialReductionDescriptorSet, 1, 0, DescriptorType::StorageImage, imageInfos.GetSubView(1, 1)},
					};
					DescriptorSet::Update(logicalDevice, descriptorUpdates.GetView());
				}
			}
			break;
			default:
			{
				Assert(m_reductionDescriptorSets.HasElements());

				const Rendering::DescriptorSetView reductionDescriptorSet = m_reductionDescriptorSets[subpassIndex];
				Assert(reductionDescriptorSet.IsValid());
				if (LIKELY(reductionDescriptorSet.IsValid()))
				{
					const ImageMappingView inputImageMapping = inputAttachmentMappings[0][0];
					const ImageMappingView depthMinMaxTargetMapping = outputAttachmentMappings[0][0];
					const Array<DescriptorSet::ImageInfo, 2> imageInfos{
						DescriptorSet::ImageInfo{{}, inputImageMapping, ImageLayout::ShaderReadOnlyOptimal},
						DescriptorSet::ImageInfo{{}, depthMinMaxTargetMapping, ImageLayout::General},
					};
					const Array<DescriptorSet::UpdateInfo, 2> descriptorUpdates{
						DescriptorSet::UpdateInfo{reductionDescriptorSet, 0, 0, DescriptorType::SampledImage, imageInfos.GetSubView(0, 1)},
						DescriptorSet::UpdateInfo{reductionDescriptorSet, 1, 0, DescriptorType::StorageImage, imageInfos.GetSubView(1, 1)},
					};
					DescriptorSet::Update(logicalDevice, descriptorUpdates.GetView());
				}
			}
			break;
		}
	}

	bool DepthMinMaxPyramidStage::ShouldRecordCommands() const
	{
		return m_pShadowsStage.IsValid() && m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized && m_sceneView.HasActiveCamera() &&
		       m_initialPipeline.IsValid() && m_pipeline.IsValid() && m_reductionDescriptorSets.HasElements() &&
		       m_reductionDescriptorSets.GetView().All(
						 [](const DescriptorSetView descriptorSet)
						 {
							 return descriptorSet.IsValid();
						 }
					 );
	}

	void DepthMinMaxPyramidStage::RecordComputePassCommands(
		const ComputeCommandEncoderView computeCommandEncoder, const ViewMatrices& viewMatrices, const uint8 subpassIndex
	)
	{
		Math::Vector2ui resolution = (Math::Vector2ui)((Math::Vector2f)viewMatrices.GetRenderResolution() * UpscalingFactor);
		resolution = Math::Max(resolution >> Math::Vector2ui{1}, Math::Vector2ui{1u});

		switch (subpassIndex)
		{
			case 0:
			{
				computeCommandEncoder.BindPipeline(m_initialPipeline);
				m_initialPipeline.Compute(ArrayView<const DescriptorSetView>(m_reductionDescriptorSets[0]), computeCommandEncoder, resolution);
			}
			break;
			case 1:
				computeCommandEncoder.BindPipeline(m_pipeline);
				[[fallthrough]];
			default:
			{
				resolution = Math::Max(resolution >> Math::Vector2ui{1}, Math::Vector2ui{1u});
				m_pipeline.Compute(ArrayView<const DescriptorSetView>(m_reductionDescriptorSets[subpassIndex]), computeCommandEncoder, resolution);
			}
			break;
		}
	}
}
