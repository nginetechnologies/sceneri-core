#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Wrappers/CompareOperation.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] inline constexpr MTLCompareFunction ConvertCompareOperation(const CompareOperation operation)
	{
		switch (operation)
		{
			case CompareOperation::AlwaysFail:
				return MTLCompareFunctionNever;
			case CompareOperation::Less:
				return MTLCompareFunctionLess;
			case CompareOperation::Equal:
				return MTLCompareFunctionEqual;
			case CompareOperation::LessOrEqual:
				return MTLCompareFunctionLessEqual;
			case CompareOperation::Greater:
				return MTLCompareFunctionGreater;
			case CompareOperation::NotEqual:
				return MTLCompareFunctionNotEqual;
			case CompareOperation::GreaterOrEqual:
				return MTLCompareFunctionGreaterEqual;
			case CompareOperation::AlwaysSucceed:
				return MTLCompareFunctionAlways;
		}
	}
#endif
}
