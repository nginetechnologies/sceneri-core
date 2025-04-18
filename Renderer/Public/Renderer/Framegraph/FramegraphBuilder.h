#pragma once

#include <Renderer/Framegraph/Framegraph.h>

namespace ngine::Rendering
{
	struct FramegraphBuilder
	{
		[[nodiscard]] StageIndex GetNextAvailableStageIndex() const
		{
			return stages.GetNextAvailableIndex();
		}
		void EmplaceStage(StageDescription&& stageDescription)
		{
			stages.EmplaceBack(Forward<StageDescription>(stageDescription));
		}
		[[nodiscard]] ArrayView<const StageDescription, StageIndex> GetStages() const
		{
			return stages.GetView();
		}

		ArrayView<const ColorAttachmentDescription, AttachmentIndex>
		EmplaceColorAttachments(const ArrayView<const ColorAttachmentDescription, AttachmentIndex> emplacedColorAttachments)
		{
			colorAttachments.CopyEmplaceRangeBack(emplacedColorAttachments);
			return colorAttachments
			  .GetSubView(colorAttachments.GetSize() - emplacedColorAttachments.GetSize(), emplacedColorAttachments.GetSize());
		}

		ArrayView<const InputAttachmentDescription, AttachmentIndex>
		EmplaceInputAttachments(const ArrayView<const InputAttachmentDescription, AttachmentIndex> emplacedInputAttachments)
		{
			inputAttachments.CopyEmplaceRangeBack(emplacedInputAttachments);
			return inputAttachments
			  .GetSubView(inputAttachments.GetSize() - emplacedInputAttachments.GetSize(), emplacedInputAttachments.GetSize());
		}

		ArrayView<const InputOutputAttachmentDescription, AttachmentIndex>
		EmplaceInputOutputAttachments(const ArrayView<const InputOutputAttachmentDescription, AttachmentIndex> emplacedInputOutputAttachments)
		{
			inputOutputAttachments.CopyEmplaceRangeBack(emplacedInputOutputAttachments);
			return inputOutputAttachments
			  .GetSubView(inputOutputAttachments.GetSize() - emplacedInputOutputAttachments.GetSize(), emplacedInputOutputAttachments.GetSize());
		}

		ArrayView<const OutputAttachmentDescription, AttachmentIndex>
		EmplaceOutputAttachments(const ArrayView<const OutputAttachmentDescription, AttachmentIndex> emplacedOutputAttachments)
		{
			outputAttachments.CopyEmplaceRangeBack(emplacedOutputAttachments);
			return outputAttachments
			  .GetSubView(outputAttachments.GetSize() - emplacedOutputAttachments.GetSize(), emplacedOutputAttachments.GetSize());
		}

		ArrayView<const RenderSubpassDescription, PassIndex>
		EmplaceRenderSubpass(const ArrayView<const RenderSubpassDescription, PassIndex> emplacedRenderSubpasses)
		{
			renderSubpasses.CopyEmplaceRangeBack(emplacedRenderSubpasses);
			return renderSubpasses.GetSubView(renderSubpasses.GetSize() - emplacedRenderSubpasses.GetSize(), emplacedRenderSubpasses.GetSize());
		}

		ArrayView<const SubpassAttachmentReference, AttachmentIndex> EmplaceSubpassAttachmentReferences(
			const ArrayView<const SubpassAttachmentReference, AttachmentIndex> emplacedSubpassAttachmentReferences
		)
		{
			subpassAttachmentReferences.CopyEmplaceRangeBack(emplacedSubpassAttachmentReferences);
			return subpassAttachmentReferences.GetSubView(
				subpassAttachmentReferences.GetSize() - emplacedSubpassAttachmentReferences.GetSize(),
				emplacedSubpassAttachmentReferences.GetSize()
			);
		}

		ArrayView<const ComputeSubpassDescription, PassIndex>
		EmplaceComputeSubpass(const ArrayView<const ComputeSubpassDescription, PassIndex> emplacedComputeSubpasses)
		{
			computeSubpasses.CopyEmplaceRangeBack(emplacedComputeSubpasses);
			return computeSubpasses
			  .GetSubView(computeSubpasses.GetSize() - emplacedComputeSubpasses.GetSize(), emplacedComputeSubpasses.GetSize());
		}

		ArrayView<const AttachmentIndex, AttachmentIndex>
		EmplaceAttachmentIndices(const ArrayView<const AttachmentIndex, AttachmentIndex> emplacedAttachmentIndices)
		{
			attachmentIndices.CopyEmplaceRangeBack(emplacedAttachmentIndices);
			return attachmentIndices
			  .GetSubView(attachmentIndices.GetSize() - emplacedAttachmentIndices.GetSize(), emplacedAttachmentIndices.GetSize());
		}
	protected:
		InlineVector<StageDescription, 100, StageIndex> stages;
		FlatVector<ColorAttachmentDescription, 128, AttachmentIndex> colorAttachments;
		FlatVector<InputAttachmentDescription, 32, AttachmentIndex> inputAttachments;
		FlatVector<InputOutputAttachmentDescription, 32, AttachmentIndex> inputOutputAttachments;
		FlatVector<OutputAttachmentDescription, 96, AttachmentIndex> outputAttachments;
		FlatVector<RenderSubpassDescription, 32, PassIndex> renderSubpasses;
		FlatVector<SubpassAttachmentReference, 128, AttachmentIndex> subpassAttachmentReferences;
		FlatVector<ComputeSubpassDescription, 128, PassIndex> computeSubpasses;
		FlatVector<AttachmentIndex, 128, AttachmentIndex> attachmentIndices;
	};
}
