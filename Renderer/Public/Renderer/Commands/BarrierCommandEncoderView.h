#pragma once

#include <Renderer/Commands/CommandEncoderView.h>

namespace ngine::Rendering
{
	//! View into a command encoder that can record barriers and transition images
	struct TRIVIAL_ABI BarrierCommandEncoderView
	{
		BarrierCommandEncoderView() = default;

		BarrierCommandEncoderView(const CommandEncoderView commandEncoder)
			: m_commandEncoder(commandEncoder)
		{
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_commandEncoder.IsValid();
		}

		//! Transitions the specified images to new layouts
		//! Currently taking more arguments than required while migrating from old barrier setup, remove when validated working
	protected:
		CommandEncoderView m_commandEncoder;
	};
}
