#pragma once

namespace ngine::Animation
{
	struct Skeleton;

	struct SkeletonJointPicker
	{
		inline static constexpr Guid TypeGuid = "{82A461E8-CA5D-4869-B53E-9BD4E81CEBF1}"_guid;

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] bool IsValid() const
		{
			return m_currentSelection.IsValid();
		}

		[[nodiscard]] bool operator==(const SkeletonJointPicker other) const
		{
			return m_currentSelection == other.m_currentSelection;
		}
		[[nodiscard]] bool operator!=(const SkeletonJointPicker other) const
		{
			return m_currentSelection != other.m_currentSelection;
		}

		Optional<const Skeleton*> m_pSkeleton;
		Guid m_currentSelection;
	};
}
