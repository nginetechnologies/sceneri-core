#pragma once

#include "JointIndex.h"

#include "3rdparty/ozz/animation/runtime/skeleton.h"
#include "3rdparty/ozz/base/io/archive.h"
#include "3rdparty/ozz/base/maths/soa_transform.h"

#include <Common/Asset/AssetFormat.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>

#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::IO
{
	struct FileView;
}

namespace ngine
{
	extern template struct UnorderedMap<Guid, Animation::JointIndex, Guid::Hash>;
}

namespace ngine::Animation
{
	struct Skeleton : protected ozz::animation::Skeleton
	{
		using BaseType = ozz::animation::Skeleton;

		using BaseType::BaseType;
		using BaseType::operator=;

		Skeleton(BaseType&& base)
			: BaseType(Forward<BaseType>(base))
		{
		}

		bool Load(const ConstByteView data);
		bool Load(const IO::FileView file);
		void Save(const IO::FileView file) const;

		char* Reserve(const size nameCharacterCount, const size jointCount)
		{
			return BaseType::Allocate(nameCharacterCount, jointCount);
		}

		[[nodiscard]] JointIndex GetJointCount() const
		{
			return (JointIndex)BaseType::num_joints();
		}

		[[nodiscard]] bool IsValid() const
		{
			return GetJointBindPoses().HasElements() & (GetJointCount() > 0);
		}

		[[nodiscard]] JointIndex GetStructureOfArraysJointCount() const
		{
			return (JointIndex)BaseType::num_soa_joints();
		}

		[[nodiscard]] ArrayView<ozz::math::SoaTransform, JointIndex> GetJointBindPoses()
		{
			return {joint_bind_poses_.data(), (JointIndex)joint_bind_poses_.size()};
		}

		[[nodiscard]] ArrayView<const ozz::math::SoaTransform, JointIndex> GetJointBindPoses() const
		{
			return {joint_bind_poses_.data(), (JointIndex)joint_bind_poses_.size()};
		}

		[[nodiscard]] ArrayView<char*, JointIndex> GetJointNames()
		{
			return {joint_names_.data(), (JointIndex)joint_names_.size()};
		}

		[[nodiscard]] ArrayView<const char* const, JointIndex> GetJointNames() const
		{
			return {joint_names_.data(), (JointIndex)joint_names_.size()};
		}

		[[nodiscard]] ArrayView<int16_t, JointIndex> GetJointParents()
		{
			return {joint_parents_.data(), (JointIndex)joint_parents_.size()};
		}

		[[nodiscard]] ArrayView<int16_t, JointIndex> GetJointParents() const
		{
			return {joint_parents_.data(), (JointIndex)joint_parents_.size()};
		}

		[[nodiscard]] Optional<JointIndex> FindJointIndex(const Guid jointGuid) const
		{
			const auto it = m_jointLookupMap.Find(jointGuid);
			if (it != m_jointLookupMap.end())
			{
				return it->second;
			}
			return Invalid;
		}
		[[nodiscard]] Guid FindJointGuid(const JointIndex index) const
		{
			for (auto it = m_jointLookupMap.begin(), endIt = m_jointLookupMap.end(); it != endIt; ++it)
			{
				if (it->second == index)
				{
					return it->first;
				}
			}
			return {};
		}

		[[nodiscard]] BaseType& GetOzzType()
		{
			return *this;
		}
		[[nodiscard]] const BaseType& GetOzzType() const
		{
			return *this;
		}

		bool Serialize(const Serialization::Reader reader);

		mutable ThreadSafe::Event<void(void*), 24> OnChanged;
	protected:
		UnorderedMap<Guid, JointIndex, Guid::Hash> m_jointLookupMap;
	};
}
