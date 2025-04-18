#pragma once

#include "AccelerationStructureCommandEncoderView.h"

namespace ngine::Rendering
{
	struct CommandEncoder;
	struct CommandEncoderView;

	struct AccelerationStructureCommandEncoder : public AccelerationStructureCommandEncoderView
	{
		AccelerationStructureCommandEncoder() = default;
		AccelerationStructureCommandEncoder(const AccelerationStructureCommandEncoder&) = delete;
		AccelerationStructureCommandEncoder& operator=(const AccelerationStructureCommandEncoder&) = delete;
		AccelerationStructureCommandEncoder([[maybe_unused]] AccelerationStructureCommandEncoder&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pCommandEncoder = other.m_pCommandEncoder;
			other.m_pCommandEncoder = nullptr;
#endif
		}
		AccelerationStructureCommandEncoder& operator=(AccelerationStructureCommandEncoder&& other) noexcept;
		~AccelerationStructureCommandEncoder();

		void End();
	protected:
		AccelerationStructureCommandEncoder(const AccelerationStructureCommandEncoderView computeCommandEncoder)
			: AccelerationStructureCommandEncoderView(computeCommandEncoder)
		{
		}
		friend CommandEncoder;
		friend CommandEncoderView;
	};
}
