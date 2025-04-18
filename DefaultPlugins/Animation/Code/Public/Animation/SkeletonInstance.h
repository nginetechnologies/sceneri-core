#pragma once

#include "Skeleton.h"

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/Vector.h>

#include <Common/Math/Matrix4x4.h>
#include <Common/Math/ScaledQuaternion.h>

namespace ngine::Animation
{
	struct SkeletonInstance
	{
		SkeletonInstance() = default;

		SkeletonInstance(const Skeleton& skeleton)
			: m_pSkeleton(skeleton)
			, m_modelSpaceMatrices(Memory::ConstructWithSize, Memory::Uninitialized, skeleton.GetJointCount())
			, m_sampledTransforms(Memory::ConstructWithSize, Memory::Uninitialized, m_pSkeleton->GetStructureOfArraysJointCount())
		{
			skeleton.OnChanged.Add(*this, &SkeletonInstance::OnSkeletonChanged);
		}
		SkeletonInstance(const SkeletonInstance& other)
			: m_pSkeleton(other.m_pSkeleton)
			, m_modelSpaceMatrices(other.m_modelSpaceMatrices)
			, m_sampledTransforms(other.m_sampledTransforms)
		{
			if (m_pSkeleton.IsValid())
			{
				m_pSkeleton->OnChanged.Add(*this, &SkeletonInstance::OnSkeletonChanged);
			}
		}
		SkeletonInstance(SkeletonInstance&& other)
			: m_pSkeleton(other.m_pSkeleton)
			, m_modelSpaceMatrices(Move(other.m_modelSpaceMatrices))
			, m_sampledTransforms(Move(other.m_sampledTransforms))
		{
			if (m_pSkeleton.IsValid())
			{
				m_pSkeleton->OnChanged.Add(*this, &SkeletonInstance::OnSkeletonChanged);
			}
		}
		SkeletonInstance& operator=(const SkeletonInstance& other)
		{
			m_modelSpaceMatrices = other.m_modelSpaceMatrices;
			m_sampledTransforms = other.m_sampledTransforms;
			if (m_pSkeleton.IsValid())
			{
				m_pSkeleton->OnChanged.Remove(this);
			}
			if (other.m_pSkeleton.IsValid())
			{
				m_pSkeleton = other.m_pSkeleton;
				m_pSkeleton->OnChanged.Add(*this, &SkeletonInstance::OnSkeletonChanged);
			}
			return *this;
		}
		SkeletonInstance& operator=(SkeletonInstance&& other)
		{
			m_modelSpaceMatrices = Move(other.m_modelSpaceMatrices);
			m_sampledTransforms = Move(other.m_sampledTransforms);
			if (m_pSkeleton.IsValid())
			{
				m_pSkeleton->OnChanged.Remove(this);
			}
			if (other.m_pSkeleton.IsValid())
			{
				m_pSkeleton = other.m_pSkeleton;
				m_pSkeleton->OnChanged.Add(*this, &SkeletonInstance::OnSkeletonChanged);
			}
			return *this;
		}
		~SkeletonInstance()
		{
			if (m_pSkeleton.IsValid())
			{
				m_pSkeleton->OnChanged.Remove(this);
			}
		}

		static void ProcessLocalToModelSpace(
			const Skeleton& skeleton,
			const ArrayView<const ozz::math::SoaTransform, uint16> transforms,
			const ArrayView<Math::Matrix4x4f, uint16> modelSpaceMatricesOut
		);

		void ProcessLocalToModelSpace(const ArrayView<const ozz::math::SoaTransform, uint16> transforms)
		{
			ProcessLocalToModelSpace(*m_pSkeleton, transforms, m_modelSpaceMatrices);
		}

		[[nodiscard]] Optional<const Skeleton*> GetSkeleton() const
		{
			return m_pSkeleton;
		}

		[[nodiscard]] ArrayView<const Math::Matrix4x4f, uint16> GetModelSpaceMatrices() const
		{
			return m_modelSpaceMatrices;
		}

		[[nodiscard]] ArrayView<ozz::math::SoaTransform, uint16> GetSampledTransforms()
		{
			return m_sampledTransforms;
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_pSkeleton.IsValid() && m_pSkeleton->IsValid() && m_modelSpaceMatrices.HasElements() && m_sampledTransforms.HasElements();
		}
	private:
		void OnSkeletonChanged()
		{
			m_modelSpaceMatrices.Resize(m_pSkeleton->GetJointCount());
			m_sampledTransforms.Resize(m_pSkeleton->GetStructureOfArraysJointCount());
		}
	protected:
		Optional<const Skeleton*> m_pSkeleton;
		Vector<Math::Matrix4x4f, uint16> m_modelSpaceMatrices;
		Vector<ozz::math::SoaTransform, uint16> m_sampledTransforms;
	};
}
