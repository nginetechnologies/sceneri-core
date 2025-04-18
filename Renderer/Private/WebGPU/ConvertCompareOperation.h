#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Wrappers/CompareOperation.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUCompareFunction ConvertCompareOperation(const CompareOperation operation)
	{
		switch (operation)
		{
			case CompareOperation::AlwaysFail:
				return WGPUCompareFunction_Never;
			case CompareOperation::Less:
				return WGPUCompareFunction_Less;
			case CompareOperation::Equal:
				return WGPUCompareFunction_Equal;
			case CompareOperation::LessOrEqual:
				return WGPUCompareFunction_LessEqual;
			case CompareOperation::Greater:
				return WGPUCompareFunction_Greater;
			case CompareOperation::NotEqual:
				return WGPUCompareFunction_NotEqual;
			case CompareOperation::GreaterOrEqual:
				return WGPUCompareFunction_GreaterEqual;
			case CompareOperation::AlwaysSucceed:
				return WGPUCompareFunction_Always;
		}
		ExpectUnreachable();
	}
#endif
}
