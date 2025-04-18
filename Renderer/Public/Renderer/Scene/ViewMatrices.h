#pragma once

#include <Renderer/Buffers/UniformBuffer.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>

#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Matrix3x3.h>
#include <Common/Math/WorldCoordinate.h>

namespace ngine::Threading
{
	struct JobRunnerThread;
}

namespace ngine::Rendering
{
	struct CommandEncoderView;
	struct PerFrameStagingBuffer;

	struct ViewMatrices
	{
		enum class Type : uint8
		{
			View,                             // 1 << 0 / 1
			Projection,                       // 1 << 1 / 2
			ViewProjection,                   // 1 << 2 / 4
			ViewProjectionCylinderBillboard,  // 1 << 3 / 8
			ViewProjectionSphericalBillboard, // 1 << 4 / 16
			InvertedViewProjection,           // 1 << 5 / 32
			InvertedView,                     // 1 << 6 / 64
			InvertedProjection,               // 1 << 7 // 128
			Count
		};

		struct Data
		{
			Data() = default;

			Array<Math::Matrix4x4f, (uint8)Type::Count, Type> m_matrices{Memory::InitializeAll, Math::Identity};
			Math::Vector4f m_viewLocationAndTime{Math::Zero};
			Math::Matrix3x3f m_viewRotation{Math::Identity};
			Math::Matrix3x3f m_invertedViewRotation{Math::Identity};
			Math::Vector4ui m_renderAndOutputResolution;
			Math::Vector4f m_jitterOffset;
		};

		ViewMatrices() = default;
		ViewMatrices(LogicalDevice& logicalDevice);

		void Destroy(LogicalDevice& logicalDevice);

		void StartFrame(
			const uint8 frameIndex,
			const uint64 frameCounter,
			const float time,
			LogicalDevice& logicalDevice,
			const CommandQueueView graphicsCommandQueue,
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void Assign(
			Math::Matrix4x4f viewMatrix,
			const Math::Matrix4x4f& projectionMatrix,
			const Math::WorldCoordinate viewLocation,
			const Math::Matrix3x3f viewRotation,
			const Math::Vector2ui renderResolution,
			const Math::Vector2ui outputResolution
		);
		void ResetPreviousFrames()
		{
			for (Data& data : m_data)
			{
				data = m_latestData;
			}
		}

		void OnCameraPropertiesChanged(
			Math::Matrix4x4f viewMatrix,
			const Math::Matrix4x4f& projectionMatrix,
			const Math::WorldCoordinate viewLocation,
			const Math::Matrix3x3f viewRotation,
			const Math::Vector2ui renderResolution,
			const Math::Vector2ui outputResolution,
			const uint8 frameIndex
		)
		{
			Assign(viewMatrix, projectionMatrix, viewLocation, viewRotation, renderResolution, outputResolution);
			m_data[frameIndex] = m_latestData;
		}
		void OnCameraTransformChanged(
			Math::Matrix4x4f viewMatrix, const Math::WorldCoordinate viewLocation, const Math::Matrix3x3f viewRotation, const uint8 frameIndex
		)
		{
			Assign(
				viewMatrix,
				m_latestData.m_matrices[Type::Projection],
				viewLocation,
				viewRotation,
				{
					m_latestData.m_renderAndOutputResolution.x,
					m_latestData.m_renderAndOutputResolution.y,
				},
				{m_latestData.m_renderAndOutputResolution.z, m_latestData.m_renderAndOutputResolution.w}
			);
			m_data[frameIndex] = m_latestData;
		}

		[[nodiscard]] const Math::Matrix4x4f& GetMatrix(const Type type) const
		{
			return m_latestData.m_matrices[type];
		}
		[[nodiscard]] Math::WorldCoordinate GetLocation() const
		{
			return {m_latestData.m_viewLocationAndTime.x, m_latestData.m_viewLocationAndTime.y, m_latestData.m_viewLocationAndTime.z};
		}
		[[nodiscard]] Math::Vector2f GetLocation2D() const
		{
			return {m_latestData.m_viewLocationAndTime.x, m_latestData.m_viewLocationAndTime.y};
		}
		[[nodiscard]] const Math::Matrix3x3f& GetRotation() const
		{
			return m_latestData.m_viewRotation;
		}
		[[nodiscard]] const Math::Matrix3x3f& GetInvertedRotation() const
		{
			return m_latestData.m_invertedViewRotation;
		}

		[[nodiscard]] Math::Vector2ui GetRenderResolution() const
		{
			return {m_latestData.m_renderAndOutputResolution.x, m_latestData.m_renderAndOutputResolution.y};
		}
		[[nodiscard]] Math::Vector2ui GetOutputResolution() const
		{
			return {m_latestData.m_renderAndOutputResolution.z, m_latestData.m_renderAndOutputResolution.w};
		}

		[[nodiscard]] const Math::Matrix4x4f& GetPreviousMatrix(const Type type) const
		{
			const uint8 previousFrameIndex = (m_frameIndex + (Rendering::MaximumConcurrentFrameCount - 1)) %
			                                 Rendering::MaximumConcurrentFrameCount;
			return m_data[previousFrameIndex].m_matrices[type];
		}
		[[nodiscard]] Math::WorldCoordinate GetPreviousLocation() const
		{
			const uint8 previousFrameIndex = (m_frameIndex - (Rendering::MaximumConcurrentFrameCount - 1)) %
			                                 Rendering::MaximumConcurrentFrameCount;
			const Data& previousData = m_data[previousFrameIndex];
			return Math::WorldCoordinate{
				previousData.m_viewLocationAndTime.x,
				previousData.m_viewLocationAndTime.y,
				previousData.m_viewLocationAndTime.z
			};
		}
		[[nodiscard]] const Math::Matrix3x3f& GetPreviousRotation() const
		{
			const uint8 previousFrameIndex = (m_frameIndex - (Rendering::MaximumConcurrentFrameCount - 1)) %
			                                 Rendering::MaximumConcurrentFrameCount;
			return m_data[previousFrameIndex].m_viewRotation;
		}
		[[nodiscard]] const Math::Matrix3x3f& GetPreviousInvertedRotation() const
		{
			const uint8 previousFrameIndex = (m_frameIndex - (Rendering::MaximumConcurrentFrameCount - 1)) %
			                                 Rendering::MaximumConcurrentFrameCount;
			return m_data[previousFrameIndex].m_invertedViewRotation;
		}

		[[nodiscard]] DescriptorSetLayoutView GetDescriptorSetLayout() const
		{
			return m_descriptorSetLayout;
		}
		[[nodiscard]] DescriptorSetView GetDescriptorSet() const
		{
			return m_descriptorSet;
		}
		[[nodiscard]] BufferView GetBuffer() const
		{
			return m_buffer;
		}
	protected:
		Data m_latestData;
		Array<Data, MaximumConcurrentFrameCount> m_data;
		uint8 m_frameIndex;

		DescriptorSetLayout m_descriptorSetLayout;
		DescriptorSet m_descriptorSet;
		Optional<Threading::JobRunnerThread*> m_pDescriptorSetLoadingThread;
		UniformBuffer m_buffer;
	};
}
