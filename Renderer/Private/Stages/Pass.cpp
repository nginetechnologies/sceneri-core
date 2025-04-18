#include "Stages/Pass.h"
#include "Stages/PresentStage.h"

#include "Scene/SceneView.h"

#include <Engine/Asset/AssetManager.h>

#include <Commands/CommandEncoderView.h>
#include <Commands/RenderCommandEncoder.h>
#include <Devices/LogicalDevice.h>
#include <Wrappers/ImageMappingView.h>
#include <Assets/Texture/MipRange.h>
#include <Assets/Texture/RenderTexture.h>
#include <Assets/Texture/RenderTargetAsset.h>
#include <RenderOutput/RenderOutput.h>

#include <Renderer.h>
#include <Renderer/Wrappers/SubpassDescription.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Framegraph/Framegraph.h>
#include <Renderer/Scene/SceneViewDrawer.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>

#include <Common/Serialization/SerializedData.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	Pass::Pass(
		LogicalDevice& logicalDevice,
		RenderOutput& renderOutput,
		Framegraph& framegraph,
		ClearValues&& clearValues,
		const uint8 subpassCount
#if STAGE_DEPENDENCY_PROFILING
		,
		String&& debugName
#endif
	)
		: Stage(logicalDevice, Threading::JobPriority::Draw)
		, m_renderOutput(renderOutput)
		, m_framegraph(framegraph)
		, m_clearColors(Forward<ClearValues>(clearValues))
#if STAGE_DEPENDENCY_PROFILING
		, m_debugName(Forward<String>(debugName))
#endif
		, m_subpassStages(Memory::ConstructWithSize, Memory::DefaultConstruct, subpassCount)
	{
	}

	Pass::~Pass()
	{
		const LogicalDeviceView logicalDevice = m_logicalDevice;

		for (Framebuffer& framebuffer : m_framebuffers)
		{
			framebuffer.Destroy(logicalDevice);
		}

		m_renderPass.Destroy(logicalDevice);
	}

	Threading::JobBatch Pass::Initialize(
		LogicalDevice& logicalDevice,
		const ArrayView<const AttachmentDescription, uint8> attachmentDescriptions,
		const ArrayView<const SubpassDescription, uint8> subpassDescriptions,
		const ArrayView<const SubpassDependency, uint8> subpassDependencies,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui fullRenderArea
	)
	{
		m_renderPass = RenderPass(logicalDevice, attachmentDescriptions, subpassDescriptions, subpassDependencies);
		m_renderArea = fullRenderArea;

		Threading::JobBatch jobBatch;
		const RenderPassView renderPass = m_renderPass;
		for (SubpassStages& subpassStages : m_subpassStages.GetView())
		{
			const uint8 subpassIndex = m_subpassStages.GetIteratorIndex(&subpassStages);
			for (Stage& stage : subpassStages)
			{
				Threading::JobBatch stageJobBatch = stage.AssignRenderPass(renderPass, outputArea, fullRenderArea, subpassIndex);
				jobBatch.QueueAfterStartStage(stageJobBatch);
			}
		}
		return jobBatch;
	}

	void Pass::OnPassAttachmentsLoaded(
		LogicalDevice& logicalDevice,
		const ArrayView<ArrayView<const ImageMappingView, uint16>, Rendering::FrameIndex> attachmentMappings,
		const Math::Vector2ui framebufferSize
	)
	{
		Assert(m_renderPass.IsValid());
		m_framebuffers.Resize(attachmentMappings.GetSize());
		for (FrameIndex i = 0; i < attachmentMappings.GetSize(); ++i)
		{
			m_framebuffers[i] = Framebuffer(logicalDevice, m_renderPass, attachmentMappings[i], framebufferSize);
		}
	}

	void Pass::OnBeforeRenderPassDestroyed()
	{
		Stage::OnBeforeRenderPassDestroyed();

		for (SubpassStages& subpassStages : m_subpassStages.GetView())
		{
			subpassStages.Clear();
		}

		for (Framebuffer& framebuffer : m_framebuffers)
		{
			framebuffer.Destroy(m_logicalDevice);
		}
	}

	void Pass::AddStage(Stage& stage, const uint8 subpassIndex)
	{
		Assert(m_subpassStages.GetSize() > subpassIndex);
		m_subpassStages[subpassIndex].EmplaceBack(stage);
		stage.SetManagedByRenderPass();
	}

	bool Pass::ShouldRecordCommands() const
	{
		return m_renderPass.IsValid() & m_framebuffers.GetView().All(
																			[](const FramebufferView framebuffer)
																			{
																				return framebuffer.IsValid();
																			}
																		);
	}

	void Pass::OnBeforeRecordCommands(const CommandEncoderView commandEncoder)
	{
		for (SubpassStages& subpassStages : m_subpassStages.GetView())
		{
			for (Stage& stage : subpassStages)
			{
				[[maybe_unused]] const bool wasSkipped = stage.EvaluateShouldSkip();

				stage.OnBeforeRecordCommands(commandEncoder);
			}
		}
	}

	void Pass::RecordCommands(const CommandEncoderView commandEncoder)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		const DebugMarker debugMarker{commandEncoder, m_logicalDevice, m_debugName, "#FF0000"_color};
#endif

		const uint32 maximumPushConstantInstanceCount = m_subpassStages.GetView().Count(
			[](const SubpassStages& subpassStages)
			{
				return subpassStages.GetView().Count(
					[](const Stage& stage)
					{
						return stage.GetMaximumPushConstantInstanceCount();
					}
				);
			}
		);

		const FrameImageId frameImageId = m_framegraph.GetAcquireRenderOutputImageStage().GetFrameImageId();
		const Math::Rectangleui renderArea = m_renderArea;
		auto doRenderPass =
			[this](RenderCommandEncoder& renderCommandEncoder, const ViewMatrices& viewMatrices, const Math::Rectangleui renderArea)
		{
			const uint8 subpassIndex = 0;
			RecordRenderPassCommands(renderCommandEncoder, viewMatrices, renderArea, subpassIndex);
		};

		if (m_pSceneViewDrawer.IsValid())
		{
			m_pSceneViewDrawer->DoRenderPass(
				commandEncoder,
				m_renderPass,
				m_framebuffers[(FrameIndex)frameImageId],
				renderArea,
				m_clearColors.GetView(),
				Move(doRenderPass),
				maximumPushConstantInstanceCount
			);
		}
		else
		{
			Rendering::RenderCommandEncoder renderCommandEncoder = commandEncoder.BeginRenderPass(
				m_logicalDevice,
				m_renderPass,
				m_framebuffers[(FrameIndex)frameImageId],
				renderArea,
				m_clearColors.GetView(),
				maximumPushConstantInstanceCount
			);
			ViewMatrices viewMatrices;
			viewMatrices.Assign(
				Math::Identity,
				Math::Identity,
				Math::Zero,
				Math::Identity,
				m_renderArea.GetSize(),
				m_renderOutput.GetOutputArea().GetSize()
			);
			doRenderPass(renderCommandEncoder, viewMatrices, renderArea);
		}
	}

	void Pass::RecordRenderPassCommands(
		RenderCommandEncoder& renderCommandEncoder,
		const ViewMatrices& viewMatrices,
		const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 mainSubpassIndex
	)
	{
		auto runStages = [](
											 const ArrayView<ReferenceWrapper<Stage>> stages,
											 RenderCommandEncoder& renderCommandEncoder,
											 const ViewMatrices& viewMatrices,
											 const Math::Rectangleui renderArea,
											 const uint8 subpassIndex,
											 [[maybe_unused]] LogicalDevice& logicalDevice
										 )
		{
			for (Stage& stage : stages)
			{
				if (!stage.WasSkipped())
				{
#if RENDERER_OBJECT_DEBUG_NAMES
					RenderDebugMarker debugMarker{renderCommandEncoder, logicalDevice, stage.GetDebugName(), "#ffffff"_color};
#endif

					// TODO: do it asynchronously and concurrently
					stage.RecordRenderPassCommands(renderCommandEncoder, viewMatrices, renderArea, subpassIndex);
				}
			}
		};

		{

#if RENDERER_OBJECT_DEBUG_NAMES
			const RenderDebugMarker debugMarker{renderCommandEncoder, m_logicalDevice, m_debugName, "#FF0000"_color};
#endif
			runStages(m_subpassStages[0].GetView(), renderCommandEncoder, viewMatrices, renderArea, 0, m_logicalDevice);
		}

		for (SubpassStages& subpassStages : m_subpassStages.GetView().GetSubViewFrom(1))
		{
			renderCommandEncoder.StartNextSubpass(m_clearColors);
#if RENDERER_OBJECT_DEBUG_NAMES
			const RenderDebugMarker debugMarker{renderCommandEncoder, m_logicalDevice, m_debugName, "#FF0000"_color};
#endif

			const uint8 subpassIndex = m_subpassStages.GetIteratorIndex(&subpassStages);
			runStages(subpassStages.GetView(), renderCommandEncoder, viewMatrices, renderArea, subpassIndex, m_logicalDevice);
		}
	}

	void Pass::OnAfterRecordCommands(const CommandEncoderView commandEncoder)
	{
		for (SubpassStages& subpassStages : m_subpassStages.GetView())
		{
			for (Stage& stage : subpassStages)
			{
				stage.OnAfterRecordCommands(commandEncoder);
			}
		}
	}

	EnumFlags<PipelineStageFlags> Pass::GetPipelineStageFlags() const
	{
		EnumFlags<PipelineStageFlags> stageFlags;
		for (const SubpassStages& subpassStages : m_subpassStages.GetView())
		{
			for (Stage& stage : subpassStages)
			{
				stageFlags |= stage.GetPipelineStageFlags() * (!stage.WasSkipped());
			}
		}

		if (stageFlags.AreNoneSet())
		{
			stageFlags = PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::LateFragmentTests;
		}

		return stageFlags;
	}
}
