#pragma once

#include <Renderer/Assets/Texture/RenderTargetTemplateIdentifier.h>

#include <Common/Asset/Guid.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Length.h>
#include <Common/Storage/ForwardDeclarations/Identifier.h>
#include <Common/Function/ForwardDeclarations/Function.h>

namespace ngine::Rendering
{
	struct StageBase;
	struct Stage;
	struct StartFrameStage;
	struct PresentStage;
	struct CommandEncoderView;
	struct RenderCommandEncoderView;
	struct RenderCommandEncoder;
	struct RenderPassView;
	struct FramebufferView;
	struct ClearValue;
	struct ViewMatrices;
	struct ImageView;
	struct ImageMappingView;
	struct Framegraph;

	struct SceneViewDrawer
	{
		virtual ~SceneViewDrawer() = default;

		inline static constexpr Asset::Guid DefaultDepthStencilRenderTargetGuid = "2C517D62-2F66-41B5-AE2E-52690B275120"_asset;

		[[nodiscard]] virtual Framegraph& GetFramegraph() = 0;

		using RenderPassCallback = Function<void(RenderCommandEncoder&, const ViewMatrices&, const Math::Rectangleui renderArea), 24>;
		virtual void DoRenderPass(
			const CommandEncoderView commandEncoder,
			const RenderPassView renderPass,
			const FramebufferView framebuffer,
			const Math::Rectangleui extent,
			const ArrayView<const ClearValue, uint8> clearValues,
			RenderPassCallback&& callback,
			const uint32 maximumPushConstantInstanceCount
		) = 0;
		using ComputePassCallback = Function<void(const ViewMatrices&), 24>;
		virtual void DoComputePass(ComputePassCallback&& callback) = 0;

		//! Returns the resolution that we'll be rendering at in each render pass
		[[nodiscard]] virtual Math::Vector2ui GetRenderResolution() const = 0;
		//! Returns the area within the render output that this drawer can render to
		[[nodiscard]] virtual Math::Rectangleui GetFullRenderArea() const = 0;
		//! Returns the resolution within the render output that this drawer can render to
		[[nodiscard]] Math::Vector2ui GetFullRenderResolution() const
		{
			return GetFullRenderArea().GetSize();
		}
		[[nodiscard]] virtual Math::Matrix4x4f CreateProjectionMatrix(
			const Math::Anglef fieldOfView, const Math::Vector2f renderResolution, const Math::Lengthf nearPlane, const Math::Lengthf farPlane
		) const = 0;
	};
}
