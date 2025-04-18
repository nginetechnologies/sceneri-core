#include "Animation.h"
#include "AnimationAssetType.h"

#include "Wrappers/FileStream.h"
#include "Wrappers/DataStream.h"
#include "SamplingCache.h"

#include <Common/Memory/Containers/ByteView.h>
#include <Common/Math/Ratio.h>
#include <Common/Reflection/Registry.inl>

#include "3rdparty/ozz/base/io/archive.h"
#include "3rdparty/ozz/animation/runtime/sampling_job.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"

namespace ngine::Animation
{
	Animation::Animation(
		const float duration,
		const uint16 trackCount,
		const ConstStringView name,
		const size translationCount,
		const size rotationCount,
		const size scaleCount
	)
	{
		duration_ = duration;
		num_tracks_ = trackCount;

		Allocate(name.GetSize(), translationCount, rotationCount, scaleCount);

		Memory::CopyNonOverlappingElements(name_, name.GetData(), name.GetSize());
		name_[name.GetSize()] = '\0';
	}

	bool Animation::Load(const ConstByteView data)
	{
		DataStream stream(ByteView{const_cast<ByteType*>(data.GetData()), data.GetDataSize()});
		ozz::io::IArchive archive(&stream);
		if (!archive.TestTag<ozz::animation::Animation>())
		{
			return false;
		}

		// Once the tag is validated, reading cannot fail.
		archive >> static_cast<BaseType&>(*this);

		return true;
	}

	void Animation::WriteToFile(const IO::FileView outputFile) const
	{
		Assert(outputFile.IsValid());
		FileStream stream{outputFile};
		ozz::io::OArchive archive(&stream);
		archive << static_cast<const BaseType&>(*this);
	}

	void Animation::SampleAnimationAtTimeRatio(
		const Math::Ratiof ratio, SamplingCache& samplingCache, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
	) const
	{
		Assert(outTransforms.GetSize() >= num_soa_tracks());
		Assert(CanSampleAnimationAtTimeRatio(ratio, samplingCache, outTransforms));

		ozz::animation::SamplingJob samplingJob;
		samplingJob.animation = this;
		samplingJob.cache = &samplingCache;
		samplingJob.ratio = ratio;
		samplingJob.output = ozz::span<ozz::math::SoaTransform>{outTransforms.GetData(), outTransforms.GetSize()};
		samplingJob.Run();
	}

	bool Animation::IsValid() const
	{
		return (BaseType::translations().size() > 0) & (num_soa_tracks() > 0);
	}

	bool Animation::CanSampleAnimationAtTimeRatio(
		const Math::Ratiof ratio, SamplingCache& samplingCache, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
	) const
	{
		ozz::animation::SamplingJob samplingJob;
		samplingJob.animation = this;
		samplingJob.cache = &samplingCache;
		samplingJob.ratio = ratio;
		samplingJob.output = ozz::span<ozz::math::SoaTransform>{outTransforms.GetData(), outTransforms.GetSize()};
		return samplingJob.Validate();
	}

	[[maybe_unused]] const bool wasAssetTypeRegistered = Reflection::Registry::RegisterType<AnimationAssetType>();
}
