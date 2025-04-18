#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	enum class PipelineStageFlags : uint32
	{
		/* Maps to VkPipelineStageFlags */
		None = 0,
		TopOfPipe = 1,
		//! Where indirect draws and raytraces are consumed
		DrawIndirect = 2,
		//! Where vertex and index buffers are consumed
		VertexInput = 4,
		//! Vertex shader
		VertexShader = 8,
		//! Tessellation control shader
		TessellationControlShader = 16,
		//! Tessellation evaluation shader
		TessellationEvaluationShader = 32,
		//! Geometry shader
		GeometryShader = 64,
		//! Fragment shader
		FragmentShader = 128,
		//! Where pre-fragment shading depth and stencil tests are performed. Also includes render pass load for depth stencil attachments.
		EarlyFragmentTests = 256,
		//! Where post-fragment shading depth and stencil tests are performed. Includes render pass store for depth stencil attachments.
		LateFragmentTests = 512,
		//! Where post-blending final color values are output, as well as render pass load and store operations for color attachments.
		ColorAttachmentOutput = 1024,
		//! Compute shader
		ComputeShader = 2048,
		//! Includes all copy commands, resolving, cleairing and blitting
		Transfer = 4096,
		BottomOfPipe = 8192,
		Host = 16384,
		AccelerationStructureBuild = 0x02000000,
		RaytracingShader = 0x00200000,
		ShadingRateAttachmentRead = 0x00400000,

		AllRenderPass = VertexInput | VertexShader | TessellationControlShader | TessellationEvaluationShader | GeometryShader |
		                FragmentShader | EarlyFragmentTests | LateFragmentTests | ColorAttachmentOutput,

		All = TopOfPipe | DrawIndirect | AllRenderPass | ComputeShader | Transfer | BottomOfPipe | Host | AccelerationStructureBuild |
		      RaytracingShader | ShadingRateAttachmentRead,

		AllGraphics = 0x00008000,
		AllCommands = 0x00010000
	};

	ENUM_FLAG_OPERATORS(PipelineStageFlags);
}
