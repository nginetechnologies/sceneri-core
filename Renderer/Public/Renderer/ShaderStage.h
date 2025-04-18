#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Math/Log2.h>

namespace ngine::Rendering
{
	enum class ShaderStage : uint32
	{
		Invalid = 0,
		Vertex = 1 << 0,
		TessellationControl = 1 << 1,
		TessellationEvaluation = 1 << 2,
		Geometry = 1 << 3,
		Fragment = 1 << 4,
		Compute = 1 << 5,
		RayGeneration = 0x00000100,
		RayAnyHit = 0x00000200,
		RayClosestHit = 0x00000400,
		RayMiss = 0x00000800,
		RayIntersection = 0x00001000,
		RayCallable = 0x00002000,
		All = Vertex | TessellationControl | TessellationEvaluation | Geometry | Fragment | Compute | RayGeneration | RayAnyHit |
		      RayClosestHit | RayMiss | RayIntersection | RayCallable,

		Count = 12
	};
	ENUM_FLAG_OPERATORS(ShaderStage);

	[[nodiscard]] constexpr uint8 GetShaderStageIndex(const ShaderStage stage)
	{
		switch (stage)
		{
			case ShaderStage::Vertex:
				return 0;
			case ShaderStage::TessellationControl:
				return 1;
			case ShaderStage::TessellationEvaluation:
				return 2;
			case ShaderStage::Geometry:
				return 3;
			case ShaderStage::Fragment:
				return 4;
			case ShaderStage::Compute:
				return 5;
			case ShaderStage::RayGeneration:
				return 6;
			case ShaderStage::RayAnyHit:
				return 7;
			case ShaderStage::RayClosestHit:
				return 8;
			case ShaderStage::RayMiss:
				return 9;
			case ShaderStage::RayIntersection:
				return 10;
			case ShaderStage::RayCallable:
				return 11;
			case ShaderStage::Invalid:
			case ShaderStage::All:
			case ShaderStage::Count:
				ExpectUnreachable();
		}
	}
}
