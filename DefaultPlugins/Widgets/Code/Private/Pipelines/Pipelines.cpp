#include "Pipelines/Pipelines.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Descriptors/DescriptorSetView.h>

#include <Common/Math/Color.h>
#include <Common/Math/Min.h>
#include <Common/Math/Max.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering
{
	Pipelines::Pipelines(LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> imageDescriptorSetLayouts)
		: m_rectanglePipeline(logicalDevice)
		, m_borderRectanglePipeline(logicalDevice)
		, m_gradientRectanglePipeline(logicalDevice)
		, m_gridRectanglePipeline(logicalDevice)
		, m_linePipeline(logicalDevice)

		, m_uniformRoundedRectanglePipeline(logicalDevice)
		, m_uniformRoundedBorderRectanglePipeline(logicalDevice)
		, m_uniformRoundedGradientRectanglePipeline(logicalDevice)

		, m_roundedRectanglePipeline(logicalDevice)
		, m_roundedBorderRectanglePipeline(logicalDevice)
		, m_roundedGradientRectanglePipeline(logicalDevice)

		, m_circlePipeline(logicalDevice)
		, m_circleLinearGradientPipeline(logicalDevice)
		, m_circleConicGradientPipeline(logicalDevice)

		, m_fontPipeline(logicalDevice, imageDescriptorSetLayouts)
		, m_fontGradientPipeline(logicalDevice)

		, m_coloredIconPipeline(logicalDevice, imageDescriptorSetLayouts)
		, m_roundedImagePipeline(logicalDevice, imageDescriptorSetLayouts)
	{
	}

	void Pipelines::Destroy(LogicalDevice& logicalDevice)
	{
		m_rectanglePipeline.Destroy(logicalDevice);
		m_borderRectanglePipeline.Destroy(logicalDevice);
		m_gradientRectanglePipeline.Destroy(logicalDevice);
		m_gridRectanglePipeline.Destroy(logicalDevice);
		m_linePipeline.Destroy(logicalDevice);

		m_uniformRoundedRectanglePipeline.Destroy(logicalDevice);
		m_uniformRoundedBorderRectanglePipeline.Destroy(logicalDevice);
		m_uniformRoundedGradientRectanglePipeline.Destroy(logicalDevice);

		m_roundedRectanglePipeline.Destroy(logicalDevice);
		m_roundedBorderRectanglePipeline.Destroy(logicalDevice);
		m_roundedGradientRectanglePipeline.Destroy(logicalDevice);

		m_circlePipeline.Destroy(logicalDevice);
		m_circleConicGradientPipeline.Destroy(logicalDevice);
		m_circleConicGradientPipeline.Destroy(logicalDevice);

		m_fontPipeline.Destroy(logicalDevice);
		m_fontGradientPipeline.Destroy(logicalDevice);

		m_coloredIconPipeline.Destroy(logicalDevice);
		m_roundedImagePipeline.Destroy(logicalDevice);
	}

	void Pipelines::PrepareForResize(const LogicalDeviceView logicalDevice)
	{
		m_rectanglePipeline.PrepareForResize(logicalDevice);
		m_borderRectanglePipeline.PrepareForResize(logicalDevice);
		m_gradientRectanglePipeline.PrepareForResize(logicalDevice);
		m_gridRectanglePipeline.PrepareForResize(logicalDevice);
		m_linePipeline.PrepareForResize(logicalDevice);

		m_uniformRoundedRectanglePipeline.PrepareForResize(logicalDevice);
		m_uniformRoundedBorderRectanglePipeline.PrepareForResize(logicalDevice);
		m_uniformRoundedGradientRectanglePipeline.PrepareForResize(logicalDevice);

		m_roundedRectanglePipeline.PrepareForResize(logicalDevice);
		m_roundedBorderRectanglePipeline.PrepareForResize(logicalDevice);
		m_roundedGradientRectanglePipeline.PrepareForResize(logicalDevice);

		m_circlePipeline.PrepareForResize(logicalDevice);
		m_circleLinearGradientPipeline.PrepareForResize(logicalDevice);
		m_circleConicGradientPipeline.PrepareForResize(logicalDevice);

		m_fontPipeline.PrepareForResize(logicalDevice);
		m_fontGradientPipeline.PrepareForResize(logicalDevice);

		m_coloredIconPipeline.PrepareForResize(logicalDevice);
		m_roundedImagePipeline.PrepareForResize(logicalDevice);
	}

	Threading::JobBatch Pipelines::Create(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		Threading::JobBatch jobBatch;

		{
			Threading::JobBatch pipelineJobBatch =
				m_rectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_borderRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_gradientRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_gridRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_linePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		{
			Threading::JobBatch pipelineJobBatch =
				m_uniformRoundedRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_uniformRoundedBorderRectanglePipeline
					.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_uniformRoundedGradientRectanglePipeline
					.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		{
			Threading::JobBatch pipelineJobBatch =
				m_roundedRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_roundedBorderRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_roundedGradientRectanglePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		{
			Threading::JobBatch pipelineJobBatch =
				m_circlePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_circleLinearGradientPipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_circleConicGradientPipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		{
			Threading::JobBatch pipelineJobBatch =
				m_fontPipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_fontGradientPipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		{
			Threading::JobBatch pipelineJobBatch =
				m_coloredIconPipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}
		{
			Threading::JobBatch pipelineJobBatch =
				m_roundedImagePipeline.CreatePipeline(logicalDevice, shaderCache, renderPass, outputArea, renderArea, subpassIndex);
			jobBatch.QueueAfterStartStage(pipelineJobBatch);
		}

		return jobBatch;
	}
}
