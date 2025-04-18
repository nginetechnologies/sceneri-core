#pragma once

#include "3rdparty/ozz/animation/runtime/animation.h"
#define OZZ_INCLUDE_PRIVATE_HEADER 1
#include "3rdparty/ozz/animation/runtime/animation_keyframe.h"
#undef OZZ_INCLUDE_PRIVATE_HEADER

#include <Common/Asset/AssetFormat.h>
#include <Common/Math/ForwardDeclarations/Ratio.h>
#include <Common/Time/Duration.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/Forward.h>
#include <Renderer/Index.h>

namespace ozz::math
{
	struct SoaTransform;
}

namespace ngine::IO
{
	struct FileView;
};

namespace ngine::Animation
{
	struct SamplingCache;

	struct Animation : protected ozz::animation::Animation
	{
		using BaseType = ozz::animation::Animation;
		using BaseType::BaseType;
		using BaseType::operator=;

		Animation(ozz::animation::Animation&& animation)
			: ozz::animation::Animation(Forward<ozz::animation::Animation>(animation))
		{
		}

		Animation(
			const float duration,
			const uint16 trackCount,
			const ConstStringView name,
			const size translationCount,
			const size rotationCount,
			const size scaleCount
		);

		[[nodiscard]] bool IsValid() const;

		bool Load(const ConstByteView data);
		void WriteToFile(const IO::FileView outputFile) const;

		[[nodiscard]] bool CanSampleAnimationAtTimeRatio(
			const Math::Ratiof ratio, SamplingCache& samplingCache, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
		) const;
		void SampleAnimationAtTimeRatio(
			const Math::Ratiof ratio, SamplingCache& samplingCache, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
		) const;

		[[nodiscard]] Time::Durationf GetDuration() const
		{
			return Time::Durationf::FromSeconds(BaseType::duration());
		}

		[[nodiscard]] uint16 GetTrackCount() const
		{
			return (uint16)BaseType::num_tracks();
		}

		[[nodiscard]] uint16 GetSoATrackCount() const
		{
			return (uint16)BaseType::num_soa_tracks();
		}

		using KeyIndexType = uint32;
		[[nodiscard]] ArrayView<ozz::animation::Float3Key, KeyIndexType> GetTranslationKeys()
		{
			return {translations_.data(), (KeyIndexType)translations_.size()};
		}
		[[nodiscard]] ArrayView<const ozz::animation::Float3Key, KeyIndexType> GetTranslationKeys() const
		{
			return {translations_.data(), (KeyIndexType)translations_.size()};
		}
		[[nodiscard]] ArrayView<ozz::animation::QuaternionKey, KeyIndexType> GetRotationKeys()
		{
			return {rotations_.data(), (KeyIndexType)rotations_.size()};
		}
		[[nodiscard]] ArrayView<const ozz::animation::QuaternionKey, KeyIndexType> GetRotationKeys() const
		{
			return {rotations_.data(), (KeyIndexType)rotations_.size()};
		}
		[[nodiscard]] ArrayView<ozz::animation::Float3Key, KeyIndexType> GetScaleKeys()
		{
			return {scales_.data(), (KeyIndexType)scales_.size()};
		}
		[[nodiscard]] ArrayView<const ozz::animation::Float3Key, KeyIndexType> GetScaleKeys() const
		{
			return {scales_.data(), (KeyIndexType)scales_.size()};
		}

		[[nodiscard]] BaseType& GetOzzType()
		{
			return *this;
		}
		[[nodiscard]] const BaseType& GetOzzType() const
		{
			return *this;
		}
	};
}
