#pragma once

#include "PipelineStageFlags.h"
#include "AccessFlags.h"
#include "ImageLayout.h"

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	[[nodiscard]] constexpr inline EnumFlags<AccessFlags> GetSupportedAccessFlags(const PipelineStageFlags stage)
	{
		switch (stage)
		{
			case PipelineStageFlags::None:
				return AccessFlags::None;

			case PipelineStageFlags::TopOfPipe:
				return AccessFlags::MemoryRead;

			case PipelineStageFlags::DrawIndirect:
				return AccessFlags::IndirectCommandRead;

			case PipelineStageFlags::VertexInput:
				return AccessFlags::MemoryReadWrite | AccessFlags::IndexRead | AccessFlags::VertexRead | AccessFlags::UniformRead;

			case PipelineStageFlags::VertexShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead;
			case PipelineStageFlags::TessellationControlShader:
			case PipelineStageFlags::TessellationEvaluationShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead;
			case PipelineStageFlags::GeometryShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead;
			case PipelineStageFlags::FragmentShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead | AccessFlags::InputAttachmentRead |
				       AccessFlags::ColorAttachmentRead | AccessFlags::DepthStencilRead;
			case PipelineStageFlags::ComputeShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead;
			case PipelineStageFlags::RaytracingShader:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite | AccessFlags::UniformRead |
				       AccessFlags::AccelerationStructureRead;

			case PipelineStageFlags::ShadingRateAttachmentRead:
				return AccessFlags::ShadingRateImageRead;

			case PipelineStageFlags::EarlyFragmentTests:
				return AccessFlags::MemoryReadWrite | AccessFlags::DepthStencilReadWrite;
			case PipelineStageFlags::LateFragmentTests:
				return AccessFlags::MemoryReadWrite | AccessFlags::DepthStencilReadWrite;

			case PipelineStageFlags::ColorAttachmentOutput:
				return AccessFlags::MemoryReadWrite | AccessFlags::ColorAttachmentReadWrite;

			case PipelineStageFlags::Transfer:
				return AccessFlags::MemoryReadWrite | AccessFlags::TransferReadWrite;
			case PipelineStageFlags::AccelerationStructureBuild:
				return AccessFlags::MemoryReadWrite | AccessFlags::AccelerationStructureRead | AccessFlags::AccelerationStructureWrite;

			case PipelineStageFlags::BottomOfPipe:
				return AccessFlags::MemoryReadWrite;

			case PipelineStageFlags::Host:
				return AccessFlags::HostReadWrite;
			case PipelineStageFlags::AllGraphics:
			case PipelineStageFlags::AllRenderPass:
				return AccessFlags::AllGraphicsReadWrite;
			case PipelineStageFlags::AllCommands:
			case PipelineStageFlags::All:
				return AccessFlags::AllReadWrite;
		}
		ExpectUnreachable();
	}
	[[nodiscard]] constexpr inline EnumFlags<AccessFlags> GetSupportedAccessFlags(const EnumFlags<PipelineStageFlags> stages)
	{
		EnumFlags<AccessFlags> accessFlags;
		for (const PipelineStageFlags stage : stages)
		{
			accessFlags |= GetSupportedAccessFlags(stage);
		}
		return accessFlags;
	}

	[[nodiscard]] constexpr inline EnumFlags<PipelineStageFlags> GetSupportedPipelineStageFlags(const AccessFlags accessFlag)
	{
		switch (accessFlag)
		{
			case AccessFlags::None:
				return PipelineStageFlags::TopOfPipe | PipelineStageFlags::BottomOfPipe;

			case AccessFlags::IndirectCommandRead:
				return PipelineStageFlags::DrawIndirect;

			case AccessFlags::IndexRead:
			case AccessFlags::VertexRead:
				return PipelineStageFlags::VertexInput;

			case AccessFlags::UniformRead:
			case AccessFlags::ShaderRead:
			case AccessFlags::ShaderWrite:
			case AccessFlags::ShaderReadWrite:
				return PipelineStageFlags::VertexShader | PipelineStageFlags::TessellationControlShader |
				       PipelineStageFlags::TessellationEvaluationShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader |
				       PipelineStageFlags::ComputeShader;

			case AccessFlags::InputAttachmentRead:
				return PipelineStageFlags::FragmentShader;

			case AccessFlags::ColorAttachmentRead:
			case AccessFlags::ColorAttachmentWrite:
			case AccessFlags::ColorAttachmentReadWrite:
				return PipelineStageFlags::ColorAttachmentOutput;

			case AccessFlags::DepthStencilRead:
				return PipelineStageFlags::EarlyFragmentTests;
			case AccessFlags::DepthStencilWrite:
				return PipelineStageFlags::LateFragmentTests;
			case AccessFlags::DepthStencilReadWrite:
				return PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests;

			case AccessFlags::TransferRead:
			case AccessFlags::TransferWrite:
			case AccessFlags::TransferReadWrite:
				return PipelineStageFlags::Transfer;

			case AccessFlags::HostRead:
			case AccessFlags::HostWrite:
			case AccessFlags::HostReadWrite:
				return PipelineStageFlags::Host;

			case AccessFlags::MemoryRead:
			case AccessFlags::MemoryWrite:
			case AccessFlags::MemoryReadWrite:
				return PipelineStageFlags::TopOfPipe;

			case AccessFlags::AccelerationStructureRead:
			case AccessFlags::AccelerationStructureReadWrite:
				return PipelineStageFlags::AccelerationStructureBuild | PipelineStageFlags::RaytracingShader | PipelineStageFlags::FragmentShader;
			case AccessFlags::AccelerationStructureWrite:
				return PipelineStageFlags::AccelerationStructureBuild;

			case AccessFlags::ShadingRateImageRead:
				return PipelineStageFlags::ShadingRateAttachmentRead;

			case AccessFlags::AllGraphicsRead:
			case AccessFlags::AllGraphicsWrite:
			case AccessFlags::AllGraphicsReadWrite:
			case AccessFlags::AllRead:
			case AccessFlags::AllWrite:
			case AccessFlags::AllReadWrite:
			{
				EnumFlags<PipelineStageFlags> pipelineStageFlags;
				for (const AccessFlags nestedFlag : EnumFlags<AccessFlags>{accessFlag})
				{
					pipelineStageFlags |= GetSupportedPipelineStageFlags(nestedFlag);
				}
				return pipelineStageFlags;
			}
		}
		ExpectUnreachable();
	}
	[[nodiscard]] constexpr inline EnumFlags<PipelineStageFlags> GetSupportedPipelineStageFlags(const EnumFlags<AccessFlags> accessFlags)
	{
		EnumFlags<PipelineStageFlags> pipelineStageFlags;
		for (const AccessFlags accessFlag : accessFlags)
		{
			pipelineStageFlags |= GetSupportedPipelineStageFlags(accessFlag);
		}
		pipelineStageFlags |= GetSupportedPipelineStageFlags(AccessFlags::None) * (accessFlags == AccessFlags::None);
		return pipelineStageFlags;
	}

	[[nodiscard]] constexpr inline EnumFlags<AccessFlags> GetSupportedAccessFlags(const ImageLayout imageLayout)
	{
		switch (imageLayout)
		{
			case ImageLayout::Undefined:
			case ImageLayout::PresentSource:
				return {};
			case ImageLayout::General:
				return AccessFlags::MemoryReadWrite | AccessFlags::ShaderReadWrite;
			case ImageLayout::ColorAttachmentOptimal:
				return AccessFlags::ColorAttachmentReadWrite;
			case ImageLayout::DepthStencilAttachmentOptimal:
			case ImageLayout::DepthReadOnlyStencilAttachmentOptimal:
			case ImageLayout::DepthAttachmentOptimal:
			case ImageLayout::StencilAttachmentOptimal:
				return AccessFlags::DepthStencilReadWrite;
			case ImageLayout::DepthStencilReadOnlyOptimal:
			case ImageLayout::DepthAttachmentStencilReadOnlyOptimal:
			case ImageLayout::DepthReadOnlyOptimal:
			case ImageLayout::StencilReadOnlyOptimal:
				return AccessFlags::DepthStencilRead | AccessFlags::ShaderRead;
			case ImageLayout::ShaderReadOnlyOptimal:
				return AccessFlags::ShaderRead | AccessFlags::InputAttachmentRead;
			case ImageLayout::TransferSourceOptimal:
				return AccessFlags::TransferRead;
			case ImageLayout::TransferDestinationOptimal:
				return AccessFlags::TransferWrite;
			case ImageLayout::Preinitialized:
				return AccessFlags::HostWrite;
		}
		ExpectUnreachable();
	}
	[[nodiscard]] constexpr inline EnumFlags<PipelineStageFlags> GetSupportedPipelineStageFlags(const ImageLayout imageLayout)
	{
		switch (imageLayout)
		{
			case ImageLayout::Undefined:
				return PipelineStageFlags::TopOfPipe;
			case ImageLayout::PresentSource:
				return PipelineStageFlags::BottomOfPipe;
			case ImageLayout::General:
				return PipelineStageFlags::ComputeShader;
			case ImageLayout::ColorAttachmentOptimal:
				return PipelineStageFlags::ColorAttachmentOutput;
			case ImageLayout::DepthStencilAttachmentOptimal:
			case ImageLayout::DepthReadOnlyStencilAttachmentOptimal:
			case ImageLayout::DepthAttachmentOptimal:
			case ImageLayout::StencilAttachmentOptimal:
				return PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests;
			case ImageLayout::DepthStencilReadOnlyOptimal:
			case ImageLayout::DepthAttachmentStencilReadOnlyOptimal:
			case ImageLayout::DepthReadOnlyOptimal:
			case ImageLayout::StencilReadOnlyOptimal:
				return PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::ComputeShader | PipelineStageFlags::FragmentShader |
				       PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader;
			case ImageLayout::ShaderReadOnlyOptimal:
				return PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader |
				       PipelineStageFlags::ComputeShader;
			case ImageLayout::TransferSourceOptimal:
			case ImageLayout::TransferDestinationOptimal:
				return PipelineStageFlags::Transfer;
			case ImageLayout::Preinitialized:
				return PipelineStageFlags::Host;
		}
		ExpectUnreachable();
	}
}
