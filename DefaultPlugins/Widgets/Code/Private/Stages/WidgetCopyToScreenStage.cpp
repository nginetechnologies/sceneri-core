#include "Stages/WidgetCopyToScreenStage.h"

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Format.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Renderer.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/ImageMappingView.h>
#include <Renderer/Wrappers/ImageSubresourceRange.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Scene/ViewMatrices.h>

#include <Renderer/Assets/Texture/MipRange.h>
#include <Renderer/Assets/Texture/RenderTexture.h>

#include <Renderer/Vulkan/Includes.h>

#include <Common/System/Query.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Widgets
{
	CopyToScreenPipeline::CopyToScreenPipeline(Rendering::LogicalDevice& logicalDevice)
		: DescriptorSetLayout(
				logicalDevice,
				Array<const Rendering::DescriptorSetLayout::Binding, 2>{
					Rendering::DescriptorSetLayout::Binding::MakeSampledImage(
						0, Rendering::ShaderStage::Fragment, Rendering::SampledImageType::Float, Rendering::ImageMappingType::TwoDimensional
					),
					Rendering::DescriptorSetLayout::Binding::MakeSampler(
						1, Rendering::ShaderStage::Fragment, Rendering::SamplerBindingType::Filtering
					),
				}
					.GetView()
			)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Copy UI to screen");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, {});
	}

	void CopyToScreenPipeline::Destroy(Rendering::LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch CopyToScreenPipeline::CreatePipeline(
		Rendering::LogicalDevice& logicalDevice,
		Rendering::ShaderCache& shaderCache,
		const Rendering::RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		using namespace Rendering;

		const VertexStageInfo vertexStage{ShaderStageInfo{"17718d93-e102-456b-9db8-cb5ba6a8292f"_asset}};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::None};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add},
			AlphaAttachmentBlendState{BlendFactor::One, BlendFactor::Zero, BlendOperation::Add}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"01d8d860-8b49-44fa-9e9d-ce7dfb95f90f"_asset},
			ArrayView<const ColorTargetInfo>(colorBlendAttachment)
		};

		return CreateAsync(
			logicalDevice,
			shaderCache,
			m_pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			fragmentStage,
			Optional<const MultisamplingInfo*>{},
			Optional<const DepthStencilInfo*>{},
			Optional<const GeometryStageInfo*>{}
		);
	}

	CopyToScreenStage::CopyToScreenStage(Rendering::LogicalDevice& logicalDevice, const Guid guid)
		: Stage(logicalDevice, Threading::JobPriority::Draw)
		, m_guid(guid)
		, m_pipeline(logicalDevice)
		, m_sampler(logicalDevice)
	{
	}

	CopyToScreenStage::~CopyToScreenStage()
	{
		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread
			);
			previousEngineThread.GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(m_descriptorSet));
		}

		m_pipeline.Destroy(m_logicalDevice);
		m_sampler.Destroy(m_logicalDevice);
	}

	void CopyToScreenStage::OnRenderPassAttachmentsLoaded(
		[[maybe_unused]] const Math::Vector2ui resolution,
		[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex> colorAttachmentMappings,
		[[maybe_unused]] const ArrayView<Rendering::ImageMappingView, Rendering::FrameIndex> depthAttachmentMapping,
		[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex>
			subpassInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<ArrayView<const Rendering::ImageMappingView, uint16>, Rendering::FrameIndex>
			externalInputAttachmentMappings,
		[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> externalInputAttachmentResolutions,
		[[maybe_unused]] const ArrayView<ArrayView<const Optional<Rendering::RenderTexture*>, uint16>, Rendering::FrameIndex> colorAttachments,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
		Rendering::DescriptorSet descriptorSet;
		[[maybe_unused]] const bool allocatedDescriptorSets = engineThread.GetRenderData()
		                                                        .GetDescriptorPool(m_logicalDevice.GetIdentifier())
		                                                        .AllocateDescriptorSets(
																															m_logicalDevice,
																															ArrayView<const Rendering::DescriptorSetLayoutView>(m_pipeline),
																															ArrayView<Rendering::DescriptorSet>(descriptorSet)
																														);
		Assert(allocatedDescriptorSets);
		if (LIKELY(allocatedDescriptorSets))
		{
			Threading::EngineJobRunnerThread* pPreviousDescriptorSetLoadingThread = m_pDescriptorSetLoadingThread;
			m_pDescriptorSetLoadingThread = &engineThread;

			Array<Rendering::DescriptorSet::ImageInfo, 2> imageInfo{
				Rendering::DescriptorSet::ImageInfo{{}, externalInputAttachmentMappings[0][0], Rendering::ImageLayout::ShaderReadOnlyOptimal},
				Rendering::DescriptorSet::ImageInfo{m_sampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
			};
			Array<Rendering::DescriptorSet::UpdateInfo, 2> descriptorUpdates{
				Rendering::DescriptorSet::UpdateInfo{
					descriptorSet,
					0,
					0,
					Rendering::DescriptorType::SampledImage,
					ArrayView<const Rendering::DescriptorSet::ImageInfo>(imageInfo[0])
				},
				Rendering::DescriptorSet::UpdateInfo{
					descriptorSet,
					1,
					0,
					Rendering::DescriptorType::Sampler,
					ArrayView<const Rendering::DescriptorSet::ImageInfo>(imageInfo[1])
				}
			};
			Rendering::DescriptorSet::Update(m_logicalDevice, descriptorUpdates);

			m_descriptorSet.AtomicSwap(descriptorSet);

			if (pPreviousDescriptorSetLoadingThread != nullptr)
			{
				Threading::EngineJobRunnerThread& previousEngineThread =
					static_cast<Threading::EngineJobRunnerThread&>(*pPreviousDescriptorSetLoadingThread);
				previousEngineThread.GetRenderData().DestroyDescriptorSet(m_logicalDevice.GetIdentifier(), Move(descriptorSet));
			}
		}
	}

	void CopyToScreenStage::OnBeforeRenderPassDestroyed()
	{
		m_pipeline.PrepareForResize(m_logicalDevice);
	}

	Threading::JobBatch CopyToScreenStage::AssignRenderPass(
		const Rendering::RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui fullRenderArea,
		const uint8 subpassIndex
	)
	{
		return m_pipeline
		  .CreatePipeline(m_logicalDevice, m_logicalDevice.GetShaderCache(), renderPass, outputArea, fullRenderArea, subpassIndex);
	}

	bool CopyToScreenStage::ShouldRecordCommands() const
	{
		return m_descriptorSet.IsValid() && m_pipeline.IsValid();
	}

	void CopyToScreenStage::RecordRenderPassCommands(
		Rendering::RenderCommandEncoder& renderCommandEncoder,
		const Rendering::ViewMatrices&,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		if (m_descriptorSet.IsValid() && m_pipeline.IsValid())
		{
			renderCommandEncoder.BindPipeline(m_pipeline);
			renderCommandEncoder.BindDescriptorSets(
				m_pipeline,
				ArrayView<const Rendering::DescriptorSetView>{m_descriptorSet},
				m_pipeline.GetFirstDescriptorSetIndex()
			);

			renderCommandEncoder.Draw(6, 1);
		}
	}
}
