#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>

#include <Renderer/Format.h>
#include <Renderer/Assets/Texture/MipMask.h>

namespace gli
{
	class texture_cube;
	class texture2d;
}

namespace ngine
{
	namespace Asset
	{
		struct Context;
	}

	namespace Rendering
	{
		struct ImageView;
		struct Image;
		struct RenderTexture;
	}
}

namespace ngine::AssetCompiler
{
	struct RenderHelper;

	enum Distribution : uint32
	{
		Lambertian = 0,
		GGX = 1
	};

	[[nodiscard]] bool Sample(
		RenderHelper& vulkan,
		const Rendering::ImageView sourceCubemapImage,
		gli::texture_cube& cubemapOut,
		Distribution _distribution,
		const Rendering::MipMask mipMask,
		uint32 _sampleCount,
		Rendering::Format _targetFormat,
		float _lodBias
	);
	[[nodiscard]] Rendering::RenderTexture SampleCubemap(
		RenderHelper& vulkan,
		const Rendering::ImageView sourceImage,
		const Math::Vector2ui size,
		const Rendering::MipMask mipMask,
		Rendering::Format _targetFormat
	);
	[[nodiscard]] bool SampleBRDF(RenderHelper& vulkan, gli::texture2d& brdfOut, Distribution _distribution, uint32 _sampleCount);
	[[nodiscard]] bool DownloadCubemap(
		RenderHelper& _vulkan,
		Rendering::RenderTexture& sourceImage,
		const Rendering::Format sourceFormat,
		const Math::Vector2ui sourceSize,
		const uint16 sourceMipCount,
		const uint32 cubeMapByteSize,
		gli::texture_cube& cubemapOut
	);
}
