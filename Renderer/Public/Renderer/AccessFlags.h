#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	enum class AccessFlags : uint32
	{
		None = 0,

		/* Maps to VkAccessFlags */
		IndirectCommandRead = 1,
		IndexRead = 2,
		VertexRead = 4,
		UniformRead = 8,
		InputAttachmentRead = 16,

		ShaderRead = 32,
		ShaderWrite = 64,
		ShaderReadWrite = ShaderRead | ShaderWrite,

		ColorAttachmentRead = 128,
		ColorAttachmentWrite = 256,
		ColorAttachmentReadWrite = ColorAttachmentRead | ColorAttachmentWrite,

		DepthStencilRead = 512,
		DepthStencilWrite = 1024,
		DepthStencilReadWrite = DepthStencilRead | DepthStencilWrite,

		TransferRead = 2048,
		TransferWrite = 4096,
		TransferReadWrite = TransferRead | TransferWrite,

		HostRead = 8192,
		HostWrite = 16384,
		HostReadWrite = HostRead | HostWrite,

		MemoryRead = 32768,
		MemoryWrite = 65536,
		MemoryReadWrite = MemoryRead | MemoryWrite,

		AccelerationStructureRead = 0x00200000,
		AccelerationStructureWrite = 0x00400000,
		AccelerationStructureReadWrite = AccelerationStructureRead | AccelerationStructureWrite,

		ShadingRateImageRead = 0x00800000,

		AllGraphicsRead = IndexRead | VertexRead | UniformRead | InputAttachmentRead | ShaderRead | ColorAttachmentRead | DepthStencilRead |
		                  AccelerationStructureRead | ShadingRateImageRead,
		AllGraphicsWrite = ShaderWrite | ColorAttachmentWrite | DepthStencilWrite | AccelerationStructureWrite,
		AllGraphicsReadWrite = AllGraphicsRead | AllGraphicsWrite,

		AllRead = AllGraphicsRead | TransferRead | HostRead | MemoryRead,
		AllWrite = AllGraphicsWrite | TransferWrite | HostWrite | MemoryWrite,
		AllReadWrite = AllRead | AllWrite
	};

	ENUM_FLAG_OPERATORS(AccessFlags);
}
