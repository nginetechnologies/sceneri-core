#pragma once

#include "PassDescription.h"

#include <Renderer/Assets/Texture/ArrayRange.h>
#include <Renderer/Assets/Texture/MipRange.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/PipelineStageFlags.h>
#include <Renderer/AccessFlags.h>
#include <Renderer/GetSupportedAccessFlags.h>
#include <Renderer/Devices/QueueFamily.h>
#include <Renderer/Wrappers/ImageSubresourceRange.h>
#include <Renderer/Wrappers/AttachmentLoadType.h>
#include <Renderer/Wrappers/AttachmentStoreType.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Function/Function.h>

namespace ngine::Rendering
{
	struct PassAttachmentReference
	{
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool operator==(const PassAttachmentReference& other) const
		{
			return passIndex == other.passIndex && attachmentIndex == other.attachmentIndex && subpassIndex == other.subpassIndex;
		}

		PassIndex passIndex{InvalidPassIndex};
		AttachmentIndex attachmentIndex{InvalidAttachmentIndex};
		SubpassIndex subpassIndex{InvalidSubpassIndex};
	};

	struct PassInfo;

	//! Keeps track of the current state (layout, queue family etc) of a resource within an image
	struct SubresourceState
	{
		SubresourceState() = default;
		SubresourceState(
			const ImageLayout imageLayout,
			const PassAttachmentReference attachmentReference,
			const EnumFlags<PipelineStageFlags> pipelineStageFlags,
			const EnumFlags<AccessFlags> accessFlags,
			const QueueFamilyIndex queueFamilyIndex
		)
			: m_imageLayout(imageLayout)
			, m_attachmentReference(attachmentReference)
			, m_pipelineStageFlags(pipelineStageFlags)
			, m_accessFlags(accessFlags)
			, m_queueFamilyIndex(queueFamilyIndex)
		{
			Assert(pipelineStageFlags.AreAnySet());
			Assert(GetSupportedAccessFlags(imageLayout).AreAllSet(accessFlags));
			Assert(GetSupportedPipelineStageFlags(imageLayout).AreAllSet(pipelineStageFlags));
			Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));
			Assert(GetSupportedPipelineStageFlags(accessFlags).AreAllSet(pipelineStageFlags));
		}

		[[nodiscard]] bool operator==(const SubresourceState other) const
		{
			return (m_imageLayout == other.m_imageLayout) & (m_queueFamilyIndex == other.m_queueFamilyIndex);
		}
		[[nodiscard]] bool operator!=(const SubresourceState other) const
		{
			return !operator==(other);
		}

		[[nodiscard]] bool WasUsed() const
		{
			return m_attachmentReference.passIndex != InvalidPassIndex;
		}

		ImageLayout m_imageLayout{ImageLayout::Undefined};
		PassAttachmentReference m_attachmentReference;
		EnumFlags<PipelineStageFlags> m_pipelineStageFlags{PipelineStageFlags::TopOfPipe};
		EnumFlags<AccessFlags> m_accessFlags{AccessFlags::None};
		QueueFamilyIndex m_queueFamilyIndex{(QueueFamilyIndex)~0};
	};

	struct SubresourceStatesBase
	{
		void Initialize(const ImageSubresourceRange fullSubresourceRange, const uint8 bucketCount);
		[[nodiscard]] PURE_NOSTATICS static uint32 GetTotalSubresourceCount(const ImageSubresourceRange fullSubresourceRange)
		{
			const uint8 aspectCount = (uint8)fullSubresourceRange.m_aspectMask.GetNumberOfSetFlags();
			return fullSubresourceRange.m_mipRange.GetCount() * fullSubresourceRange.m_arrayRange.GetCount() * aspectCount;
		}

		[[nodiscard]] PURE_NOSTATICS static uint32 GetSubresourceIndex(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const ImageSubresourceRange fullSubresourceRange,
			const uint8 bucketIndex
		)
		{
			Assert(fullSubresourceRange.m_aspectMask.IsSet(imageAspect), "Can't query subresource index for non-existent aspect");
			Assert(fullSubresourceRange.m_mipRange.Contains(MipRange{mipLevel, 1}), "Can't query subresource index for non-existent mip level");
			Assert(
				fullSubresourceRange.m_arrayRange.Contains(ArrayRange{arrayLevel, 1}),
				"Can't query subresource index for non-existent array level"
			);
			const uint8 aspectIndex = [](const ImageAspectFlags imageAspect) -> uint8
			{
				switch (imageAspect)
				{
					case ImageAspectFlags::Color:
					case ImageAspectFlags::Depth:
						return 0;
					case ImageAspectFlags::Stencil:
						return 1;

					case ImageAspectFlags::DepthStencil:
					case ImageAspectFlags::Plane0:
					case ImageAspectFlags::Plane1:
					case ImageAspectFlags::Plane2:
						ExpectUnreachable();
				}
				ExpectUnreachable();
			}(imageAspect);
			const uint32 perAspectSubresourceIndex = arrayLevel * fullSubresourceRange.m_mipRange.GetCount() + mipLevel;
			const uint32 totalSubresourceCountPerAspect = fullSubresourceRange.m_mipRange.GetCount() *
			                                              fullSubresourceRange.m_arrayRange.GetCount();
			const uint8 aspectCount = (uint8)fullSubresourceRange.m_aspectMask.GetNumberOfSetFlags();
			const uint32 totalSubresourceCountPerBucket = totalSubresourceCountPerAspect * aspectCount;
			return perAspectSubresourceIndex + totalSubresourceCountPerAspect * aspectIndex + totalSubresourceCountPerBucket * bucketIndex;
		}
		[[nodiscard]] PURE_STATICS static ImageSubresourceRange
		GetSubresourceElementRange(uint32 index, const ImageSubresourceRange fullSubresourceRange)
		{
			const uint32 totalSubresourceCountPerAspect = fullSubresourceRange.m_mipRange.GetCount() *
			                                              fullSubresourceRange.m_arrayRange.GetCount();
			const uint8 aspectCount = (uint8)fullSubresourceRange.m_aspectMask.GetNumberOfSetFlags();
			const uint32 totalSubresourceCountPerBucket = totalSubresourceCountPerAspect * aspectCount;

			[[maybe_unused]] const uint32 bucketIndex = index / totalSubresourceCountPerBucket;
			index %= totalSubresourceCountPerBucket;

			const uint32 aspectIndex = index / totalSubresourceCountPerAspect;
			Assert(aspectIndex < fullSubresourceRange.m_aspectMask.GetNumberOfSetFlags());
			index %= totalSubresourceCountPerAspect;

			const uint32 mipCount = fullSubresourceRange.m_mipRange.GetCount();
			const uint32 arrayIndex = index / mipCount;
			Assert(arrayIndex < fullSubresourceRange.m_arrayRange.GetCount());
			index %= mipCount;

			const uint32 mipIndex = index;
			Assert(mipIndex < fullSubresourceRange.m_mipRange.GetCount());

			return {
				[](const uint32 aspectIndex, const EnumFlags<ImageAspectFlags> aspectFlags) -> ImageAspectFlags
				{
					switch (aspectIndex)
					{
						case 0:
							Assert(aspectFlags.AreAnySet(ImageAspectFlags::Color | ImageAspectFlags::Depth));
							return (aspectFlags & (ImageAspectFlags::Color | ImageAspectFlags::Depth)).GetFlags();
						case 1:
							Assert(aspectFlags.IsSet(ImageAspectFlags::Stencil));
							return ImageAspectFlags::Stencil;
					}
					ExpectUnreachable();
				}(aspectIndex, fullSubresourceRange.m_aspectMask),
				MipRange{fullSubresourceRange.m_mipRange.GetIndex() + mipIndex, 1},
				ArrayRange{fullSubresourceRange.m_arrayRange.GetIndex() + arrayIndex, 1}
			};
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS const SubresourceState& GetSubresourceState(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const ImageSubresourceRange fullSubresourceRange,
			const uint8 bucketCount
		) const LIFETIME_BOUND;
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Optional<SubresourceState> GetUniformSubresourceState(
			const ImageSubresourceRange subresourceRange, const ImageSubresourceRange fullSubresourceRange, const uint8 bucketIndex
		) const;

		void SetSubresourceState(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const ImageSubresourceRange fullSubresourceRange,
			const SubresourceState subresourceState,
			const uint8 bucketIndex
		);
		void SetSubresourceState(
			const ImageSubresourceRange subresourceRange,
			const SubresourceState subresourceState,
			const ImageSubresourceRange fullSubresourceRange,
			uint8 bucketIndex
		);

		//! Invokes the callback for all resources in the range
		//! If multiple sequential resources are uniform, they are sent as one range
		template<typename Callback>
		void VisitUniformSubresourceRanges(
			const ImageSubresourceRange subresourceRange,
			Callback&& callback,
			const ImageSubresourceRange fullSubresourceRanges,
			uint8 bucketIndex
		) const;
	protected:
		Vector<SubresourceState> m_subresourceStates;
	};

	//! Keeps track of the current state (layout, queue family etc) of all resource within an image
	struct SubresourceStates : public SubresourceStatesBase
	{
		void RegisterUsedSubresourceRange(const ImageSubresourceRange subresourceRange);

		[[nodiscard]] PURE_LOCALS_AND_POINTERS ImageSubresourceRange GetSubresourceRange() const
		{
			return m_subresourceRange;
		}

		enum class Bucket : uint8
		{
			//! Current state of the attachment
			Current,
			//! Initial state of the attachment
			Initial,
			Count
		};

		[[nodiscard]] PURE_NOSTATICS static uint32 GetSubresourceIndex(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const ImageSubresourceRange fullSubresourceRange,
			const Bucket bucket
		)
		{
			return SubresourceStatesBase::GetSubresourceIndex(imageAspect, mipLevel, arrayLevel, fullSubresourceRange, (uint8)bucket);
		}
		[[nodiscard]] PURE_STATICS uint32 GetSubresourceIndex(
			const ImageAspectFlags imageAspect, const MipRange::UnitType mipLevel, const ArrayRange::UnitType arrayLevel, const Bucket bucket
		) const
		{
			return GetSubresourceIndex(imageAspect, mipLevel, arrayLevel, m_subresourceRange, bucket);
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS const SubresourceState& GetSubresourceState(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const Bucket bucket = Bucket::Current
		) const
		{
			return SubresourceStatesBase::GetSubresourceState(imageAspect, mipLevel, arrayLevel, m_subresourceRange, (uint8)bucket);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Optional<SubresourceState>
		GetUniformSubresourceState(const ImageSubresourceRange subresourceRange, const Bucket bucket = Bucket::Current) const
		{
			return SubresourceStatesBase::GetUniformSubresourceState(subresourceRange, m_subresourceRange, (uint8)bucket);
		}

		void SetSubresourceState(
			const ImageAspectFlags imageAspect,
			const MipRange::UnitType mipLevel,
			const ArrayRange::UnitType arrayLevel,
			const SubresourceState subresourceState,
			const Bucket bucket = Bucket::Current
		)
		{
			if (bucket == Bucket::Current && GetSubresourceState(imageAspect, mipLevel, arrayLevel, Bucket::Current).m_imageLayout == ImageLayout::Undefined)
			{
				SubresourceStatesBase::SetSubresourceState(
					imageAspect,
					mipLevel,
					arrayLevel,
					m_subresourceRange,
					subresourceState,
					(uint8)Bucket::Initial
				);
			}

			SubresourceStatesBase::SetSubresourceState(imageAspect, mipLevel, arrayLevel, m_subresourceRange, subresourceState, (uint8)bucket);
		}
		void SetSubresourceState(
			const ImageSubresourceRange subresourceRange, const SubresourceState subresourceState, const Bucket bucket = Bucket::Current
		)
		{
			if (bucket == Bucket::Current)
			{
				for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
				{
					for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
					{
						for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
						{
							const SubresourceState currentSubresourceState =
								GetSubresourceState(aspectFlag, mipLevelIndex, arrayLayerIndex, Bucket::Current);
							if (currentSubresourceState.m_imageLayout == ImageLayout::Undefined)
							{
								SubresourceStatesBase::SetSubresourceState(
									aspectFlag,
									mipLevelIndex,
									arrayLayerIndex,
									m_subresourceRange,
									subresourceState,
									(uint8)Bucket::Initial
								);
							}
						}
					}
				}
				SubresourceStatesBase::SetSubresourceState(subresourceRange, subresourceState, m_subresourceRange, (uint8)Bucket::Initial);
			}

			SubresourceStatesBase::SetSubresourceState(subresourceRange, subresourceState, m_subresourceRange, (uint8)bucket);
		}

		//! Invokes the callback for all resources in the range
		//! If multiple sequential resources are uniform, they are sent as one range
		template<typename Callback>
		void VisitUniformSubresourceRanges(
			const ImageSubresourceRange subresourceRange, Callback&& callback, const Bucket bucket = Bucket::Current
		) const;
	protected:
		ImageSubresourceRange m_subresourceRange;
	};
}
