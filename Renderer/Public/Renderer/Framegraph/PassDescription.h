#pragma once

#include <Renderer/Assets/Texture/RenderTargetTemplateIdentifier.h>
#include <Renderer/Wrappers/ImageSubresourceRange.h>
#include <Renderer/Wrappers/ImageMappingType.h>
#include <Renderer/Commands/ClearValue.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Color.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Bitset.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Rendering
{
	struct Stage;
	struct Framegraph;
	struct SceneViewDrawer;

	using PassIndex = uint16;
	inline static constexpr PassIndex InvalidPassIndex = Math::NumericLimits<PassIndex>::Max;
	using SubpassIndex = uint8;
	inline static constexpr SubpassIndex InvalidSubpassIndex = Math::NumericLimits<SubpassIndex>::Max;
	using SubpassMask = uint64;
	using StageIndex = uint16;
	inline static constexpr StageIndex InvalidStageIndex = Math::NumericLimits<StageIndex>::Max;
	using AttachmentIndex = uint8;
	inline static constexpr AttachmentIndex InvalidAttachmentIndex = Math::NumericLimits<AttachmentIndex>::Max;
	inline static constexpr AttachmentIndex MaximumAttachmentCount = 64;
	using AttachmentMask = Bitset<MaximumAttachmentCount>;

	enum class FramegraphAttachmentFlags : uint8
	{
		//! Indicates that the attachment should be cleared to a pre-requested value before the owning stage starts
		Clear = 1 << 0,
		//! Indicates that the attachment can be read from, if a prior stage stored it
		CanRead = 1 << 1,
		//! Indicates that the attachment will be read from, implying it must have valid data from a prior stage
		MustRead = CanRead | (1 << 2),
		//! Indicates that the attachment stores results if a next stage in the framegraph is found to use it
		CanStore = 1 << 3,
		//! Indicates that the attachment stores results and that the framegraph must guarantee storing happens no matter what
		MustStore = CanStore | (1 << 4)
	};
	ENUM_FLAG_OPERATORS(FramegraphAttachmentFlags);

	using AttachmentIdentifier = RenderTargetTemplateIdentifier;

	struct FramegraphAttachmentDescription
	{
		FramegraphAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const ImageSubresourceRange subresourceRange,
			const EnumFlags<FramegraphAttachmentFlags> flags
		)
			: m_identifier(identifier)
			, m_size(size)
			, m_subresourceRange(subresourceRange)
			, m_flags(flags)
		{
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_identifier.IsValid();
		}

		AttachmentIdentifier m_identifier;
		Math::Vector2ui m_size;
		ImageSubresourceRange m_subresourceRange;
		EnumFlags<FramegraphAttachmentFlags> m_flags;
	};

	struct ColorAttachmentDescription : public FramegraphAttachmentDescription
	{
		ColorAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const MipRange mipRange = MipRange(0, 1),
			const ArrayRange arrayRange = ArrayRange(0, 1),
			const EnumFlags<FramegraphAttachmentFlags> flags = FramegraphAttachmentFlags::CanStore,
			const Optional<Math::Color> clearValue = Invalid,
			const ImageMappingType mappingType = ImageMappingType::TwoDimensional
		)
			: FramegraphAttachmentDescription{identifier, size, ImageSubresourceRange{ImageAspectFlags::Color, mipRange, arrayRange}, flags | (FramegraphAttachmentFlags::Clear * clearValue.IsValid())}
			, m_clearValue(clearValue)
			, m_mappingType(mappingType)
		{
		}

		Optional<Math::Color> m_clearValue;
		ImageMappingType m_mappingType;
	};

	struct DepthAttachmentDescription : public FramegraphAttachmentDescription
	{
		DepthAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const MipRange mipRange = MipRange(0, 1),
			const ArrayRange arrayRange = ArrayRange(0, 1),
			const EnumFlags<FramegraphAttachmentFlags> flags = FramegraphAttachmentFlags::CanStore,
			const Optional<DepthValue> clearValue = Invalid,
			const ImageMappingType mappingType = ImageMappingType::TwoDimensional
		)
			: FramegraphAttachmentDescription{identifier, size, ImageSubresourceRange{ImageAspectFlags::Depth, mipRange, arrayRange}, flags | (FramegraphAttachmentFlags::Clear * clearValue.IsValid())}
			, m_clearValue(clearValue)
			, m_mappingType(mappingType)
		{
		}

		Optional<DepthValue> m_clearValue;
		ImageMappingType m_mappingType;
	};
	struct StencilAttachmentDescription : public FramegraphAttachmentDescription
	{
		StencilAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const MipRange mipRange = MipRange(0, 1),
			const ArrayRange arrayRange = ArrayRange(0, 1),
			const EnumFlags<FramegraphAttachmentFlags> flags = FramegraphAttachmentFlags::CanStore,
			const Optional<StencilValue> clearValue = Invalid,
			const ImageMappingType mappingType = ImageMappingType::TwoDimensional
		)
			: FramegraphAttachmentDescription{identifier, size, ImageSubresourceRange{ImageAspectFlags::Stencil, mipRange, arrayRange}, flags | (FramegraphAttachmentFlags::Clear * clearValue.IsValid())}
			, m_clearValue(clearValue)
			, m_mappingType(mappingType)
		{
		}

		Optional<StencilValue> m_clearValue;
		ImageMappingType m_mappingType{ImageMappingType::TwoDimensional};
	};

	struct InputAttachmentDescription : public FramegraphAttachmentDescription
	{
		InputAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const ImageSubresourceRange subresourceRange,
			const ImageMappingType mappingType = ImageMappingType::TwoDimensional
		)
			: FramegraphAttachmentDescription(identifier, size, subresourceRange, FramegraphAttachmentFlags::MustRead)
			, m_mappingType(mappingType)
		{
		}

		ImageMappingType m_mappingType{ImageMappingType::TwoDimensional};
	};

	struct OutputAttachmentDescription : public FramegraphAttachmentDescription
	{
		OutputAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const ImageSubresourceRange subresourceRange,
			const EnumFlags<FramegraphAttachmentFlags> flags = {}
		)
			: FramegraphAttachmentDescription(identifier, size, subresourceRange, flags | FramegraphAttachmentFlags::CanStore)
		{
		}
	};

	struct InputOutputAttachmentDescription : public FramegraphAttachmentDescription
	{
		InputOutputAttachmentDescription(
			const AttachmentIdentifier identifier,
			const Math::Vector2ui size,
			const ImageSubresourceRange subresourceRange,
			const EnumFlags<FramegraphAttachmentFlags> flags = {}
		)
			: FramegraphAttachmentDescription(
					identifier, size, subresourceRange, flags | FramegraphAttachmentFlags::MustRead | FramegraphAttachmentFlags::CanStore
				)
		{
		}
	};

	enum class StageType : uint8
	{
		//! Stage rendered within a render pass.
		//! Allows for automatic merging / subpass setup
		RenderPass,
		//! Stage rendered within a render pass with explicit subpasses
		//! Note: Still allows for additional merging / subpass setup.
		ExplicitRenderPass,
		//! Generic stage that can perform any action.
		//! Avoid as this bypasses the vast majority of optimizations.
		Generic,
		//! Stage that supports compute operations
		Compute
	};

	struct RenderPassDescriptionBase
	{
		[[nodiscard]] Optional<const FramegraphAttachmentDescription*> GetAttachmentDescription(AttachmentIndex& attachmentIndex) const
		{
			if (attachmentIndex < m_colorAttachments.GetSize())
			{
				return m_colorAttachments[attachmentIndex];
			}
			attachmentIndex -= m_colorAttachments.GetSize();
			if (attachmentIndex == 0)
			{
				if (m_depthAttachment.IsValid())
				{
					return *m_depthAttachment;
				}
				else if (m_stencilAttachment.IsValid())
				{
					return *m_stencilAttachment;
				}
			}
			attachmentIndex -= m_depthAttachment.IsValid();
			attachmentIndex -= m_stencilAttachment.IsValid() &&
			                   (m_depthAttachment.IsInvalid() || m_stencilAttachment->m_identifier != m_depthAttachment->m_identifier);
			return Invalid;
		}

		Math::Rectangleui m_renderArea;
		ArrayView<const ColorAttachmentDescription, AttachmentIndex> m_colorAttachments;
		Optional<DepthAttachmentDescription> m_depthAttachment;
		Optional<StencilAttachmentDescription> m_stencilAttachment;
	};

	struct RenderPassDescription : public RenderPassDescriptionBase
	{
		[[nodiscard]] const FramegraphAttachmentDescription& GetAttachmentDescription(AttachmentIndex attachmentIndex) const
		{
			if (const Optional<const FramegraphAttachmentDescription*> pAttachmentDescription = RenderPassDescriptionBase::GetAttachmentDescription(attachmentIndex))
			{
				return *pAttachmentDescription;
			}
			Assert(attachmentIndex < m_inputAttachments.GetSize());
			return m_inputAttachments[attachmentIndex];
		}

		ArrayView<const InputAttachmentDescription, AttachmentIndex> m_inputAttachments;
	};

	struct RenderSubpassDescription
	{
		ConstStringView m_name;
		ArrayView<const ReferenceWrapper<Stage>, StageIndex> m_stages;
		ArrayView<const AttachmentIndex, AttachmentIndex> m_colorAttachmentIndices;
		AttachmentIndex m_depthAttachmentIndex = InvalidAttachmentIndex;
		AttachmentIndex m_stencilAttachmentIndex = InvalidAttachmentIndex;
		ArrayView<const AttachmentIndex, AttachmentIndex> m_subpassInputAttachmentIndices;
		ArrayView<const AttachmentIndex, AttachmentIndex> m_externalInputAttachmentIndices;
	};

	struct ExplicitRenderPassDescription : public RenderPassDescriptionBase
	{
		[[nodiscard]] const FramegraphAttachmentDescription& GetAttachmentDescription(AttachmentIndex attachmentIndex) const
		{
			if (const Optional<const FramegraphAttachmentDescription*> pAttachmentDescription = RenderPassDescriptionBase::GetAttachmentDescription(attachmentIndex))
			{
				return *pAttachmentDescription;
			}
			Assert(attachmentIndex < m_externalInputAttachments.GetSize());
			return m_externalInputAttachments[attachmentIndex];
		}

		ArrayView<const InputAttachmentDescription, AttachmentIndex> m_externalInputAttachments;
		ArrayView<const RenderSubpassDescription, SubpassIndex> m_subpassDescriptions;
	};

	struct SubpassAttachmentReference
	{
		AttachmentIndex m_attachmentIndex;
		MipRange m_mipRange{0, 1};
		ArrayRange m_arrayRange{0, 1};
		ImageMappingType m_mappingType{ImageMappingType::TwoDimensional};
	};
	using SubpassInputAttachmentReference = SubpassAttachmentReference;
	using SubpassOutputAttachmentReference = SubpassAttachmentReference;
	using SubpassInputOutputAttachmentReference = SubpassAttachmentReference;

	struct GenericSubpassDescription
	{
		ConstStringView m_name;
		ArrayView<const SubpassOutputAttachmentReference, AttachmentIndex> m_outputAttachments;
		ArrayView<const SubpassInputOutputAttachmentReference, AttachmentIndex> m_inputOutputAttachments;
		ArrayView<const SubpassInputAttachmentReference, AttachmentIndex> m_inputAttachments;
	};

	struct GenericPassDescription
	{
		ArrayView<const OutputAttachmentDescription, AttachmentIndex> m_outputAttachments;
		ArrayView<const InputOutputAttachmentDescription, AttachmentIndex> m_inputOutputAttachments;
		ArrayView<const InputAttachmentDescription, AttachmentIndex> m_inputAttachments;
		GenericSubpassDescription m_subpass; // TODO: Support multiple
	};

	using ComputeSubpassDescription = GenericSubpassDescription;

	struct ComputePassDescription
	{
		[[nodiscard]] const FramegraphAttachmentDescription& GetAttachmentDescription(AttachmentIndex attachmentIndex) const
		{
			if (attachmentIndex < m_outputAttachments.GetSize())
			{
				return m_outputAttachments[attachmentIndex];
			}
			attachmentIndex -= m_outputAttachments.GetSize();
			if (attachmentIndex < m_inputOutputAttachments.GetSize())
			{
				return m_inputOutputAttachments[attachmentIndex];
			}
			attachmentIndex -= m_inputOutputAttachments.GetSize();
			Assert(attachmentIndex < m_inputAttachments.GetSize());
			return m_inputAttachments[attachmentIndex];
		}

		ArrayView<const OutputAttachmentDescription, AttachmentIndex> m_outputAttachments;
		ArrayView<const InputOutputAttachmentDescription, AttachmentIndex> m_inputOutputAttachments;
		ArrayView<const InputAttachmentDescription, AttachmentIndex> m_inputAttachments;
		ArrayView<const ComputeSubpassDescription, SubpassIndex> m_subpassDescriptions;
	};

	struct StageDescription
	{
		StageDescription(StageDescription&& other)
			: m_name(other.m_name)
			, m_previousStageIndex(other.m_previousStageIndex)
			, m_pStage(other.m_pStage)
			, m_pSceneViewDrawer(other.m_pSceneViewDrawer)
			, m_type(other.m_type)
		{
			switch (other.m_type)
			{
				case StageType::RenderPass:
					m_renderPassDescription = Move(other.m_renderPassDescription);
					break;
				case StageType::ExplicitRenderPass:
					m_explicitRenderPassDescription = Move(other.m_explicitRenderPassDescription);
					break;
				case StageType::Generic:
					m_genericPassDescription = Move(other.m_genericPassDescription);
					break;
				case StageType::Compute:
					m_computePassDescription = Move(other.m_computePassDescription);
					break;
			}
		}
		~StageDescription()
		{
			switch (m_type)
			{
				case StageType::RenderPass:
					m_renderPassDescription.~RenderPassDescription();
					break;
				case StageType::ExplicitRenderPass:
					m_explicitRenderPassDescription.~ExplicitRenderPassDescription();
					break;
				case StageType::Generic:
					m_genericPassDescription.~GenericPassDescription();
					break;
				case StageType::Compute:
					m_computePassDescription.~ComputePassDescription();
					break;
			}
		}
	protected:
		friend struct RenderPassStageDescription;
		friend Framegraph;

		StageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			RenderPassDescription&& passDescription
		)
			: m_name(name)
			, m_previousStageIndex(previousStageIndex)
			, m_pStage(pStage)
			, m_pSceneViewDrawer(pSceneViewDrawer)
			, m_type(StageType::RenderPass)
			, m_renderPassDescription{Forward<RenderPassDescription>(passDescription)}
		{
		}
		StageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			ExplicitRenderPassDescription&& explicitPassDescription
		)
			: m_name(name)
			, m_previousStageIndex(previousStageIndex)
			, m_pStage(pStage)
			, m_pSceneViewDrawer(pSceneViewDrawer)
			, m_type(StageType::ExplicitRenderPass)
			, m_explicitRenderPassDescription{Forward<ExplicitRenderPassDescription>(explicitPassDescription)}
		{
		}
		StageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			GenericPassDescription&& genericPassDescription
		)
			: m_name(name)
			, m_previousStageIndex(previousStageIndex)
			, m_pStage(pStage)
			, m_pSceneViewDrawer(pSceneViewDrawer)
			, m_type(StageType::Generic)
			, m_genericPassDescription{Forward<GenericPassDescription>(genericPassDescription)}
		{
		}
		StageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			ComputePassDescription&& computePassDescription
		)
			: m_name(name)
			, m_previousStageIndex(previousStageIndex)
			, m_pStage(pStage)
			, m_pSceneViewDrawer(pSceneViewDrawer)
			, m_type(StageType::Compute)
			, m_computePassDescription{Forward<ComputePassDescription>(computePassDescription)}
		{
		}
		StageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			const StageType stageType
		)
			: m_name(name)
			, m_previousStageIndex(previousStageIndex)
			, m_pStage(pStage)
			, m_pSceneViewDrawer(pSceneViewDrawer)
			, m_type(stageType)
		{
		}
		StageDescription(const StageDescription&) = delete;

		ConstStringView m_name;
		StageIndex m_previousStageIndex;
		Optional<Stage*> m_pStage;
		Optional<SceneViewDrawer*> m_pSceneViewDrawer;
		StageType m_type;
		union
		{
			RenderPassDescription m_renderPassDescription;
			ExplicitRenderPassDescription m_explicitRenderPassDescription;
			GenericPassDescription m_genericPassDescription;
			ComputePassDescription m_computePassDescription;
		};
		// TODO: Specify buffers here too
	};

	struct RenderPassStageDescription : public StageDescription
	{
		RenderPassStageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			const Math::Rectangleui renderArea,
			const ArrayView<const ColorAttachmentDescription, AttachmentIndex> colorAttachments,
			const Optional<DepthAttachmentDescription> depthAttachment = {},
			const Optional<StencilAttachmentDescription> stencilAttachment = {},
			const ArrayView<const InputAttachmentDescription, AttachmentIndex> inputAttachments = {}
		)
			: StageDescription{
					name,
					previousStageIndex,
					pStage,
					pSceneViewDrawer,
					RenderPassDescription{renderArea, colorAttachments, depthAttachment, stencilAttachment, inputAttachments}
				}
		{
		}
	};

	struct ExplicitRenderPassStageDescription : public StageDescription
	{
		ExplicitRenderPassStageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			const Math::Rectangleui renderArea,
			const ArrayView<const ColorAttachmentDescription, AttachmentIndex> colorAttachments,
			const Optional<DepthAttachmentDescription> depthAttachment,
			const Optional<StencilAttachmentDescription> stencilAttachment,
			const ArrayView<const InputAttachmentDescription, AttachmentIndex> externalInputAttachments,
			const ArrayView<const RenderSubpassDescription, SubpassIndex> subpassDescriptions
		)
			: StageDescription{
					name,
					previousStageIndex,
					pStage,
					pSceneViewDrawer,
					ExplicitRenderPassDescription{
						renderArea, colorAttachments, depthAttachment, stencilAttachment, externalInputAttachments, subpassDescriptions
					}
				}
		{
		}
	};

	struct ComputePassStageDescription : public StageDescription
	{
		ComputePassStageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			const Optional<Stage*> pStage,
			const Optional<SceneViewDrawer*> pSceneViewDrawer,
			const ArrayView<const OutputAttachmentDescription, AttachmentIndex> outputAttachments,
			const ArrayView<const InputOutputAttachmentDescription, AttachmentIndex> inputOutputAttachments,
			const ArrayView<const InputAttachmentDescription, AttachmentIndex> inputAttachments,
			const ArrayView<const ComputeSubpassDescription, SubpassIndex> computeSubpassDescriptions
		)
			: StageDescription{
					name,
					previousStageIndex,
					pStage,
					pSceneViewDrawer,
					ComputePassDescription{outputAttachments, inputOutputAttachments, inputAttachments, computeSubpassDescriptions}
				}
		{
		}
	};

	struct GenericStageDescription : StageDescription
	{
		GenericStageDescription(
			const ConstStringView name,
			const StageIndex previousStageIndex,
			Stage& stage,
			const ArrayView<const OutputAttachmentDescription, AttachmentIndex> outputAttachments = {},
			const ArrayView<const InputOutputAttachmentDescription, AttachmentIndex> inputOutputAttachments = {},
			const ArrayView<const InputAttachmentDescription, AttachmentIndex> inputAttachments = {},
			const GenericSubpassDescription& subpassDescription = {}
		)
			: StageDescription{
					name,
					previousStageIndex,
					stage,
					Invalid,
					GenericPassDescription{outputAttachments, inputOutputAttachments, inputAttachments, subpassDescription}
				}
		{
		}
	};
}
