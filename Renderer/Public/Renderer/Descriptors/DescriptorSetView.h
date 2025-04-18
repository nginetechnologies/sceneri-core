#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/Containers/ArrayView.h>

#include <Renderer/Descriptors/DescriptorType.h>
#include <Renderer/Wrappers/SamplerView.h>
#include <Renderer/Wrappers/ImageMappingView.h>
#include <Renderer/Wrappers/AccelerationStructureView.h>
#include <Renderer/Buffers/BufferView.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/Constants.h>

namespace ngine::Rendering
{
	namespace Internal
	{
		struct DescriptorSetData;
	}

	struct LogicalDeviceView;
	struct PrimitiveAccelerationStructureView;

	struct TRIVIAL_ABI DescriptorSetView
	{
#if RENDERER_VULKAN
		using BindingIndexType = uint32;
#else
		using BindingIndexType = uint16;
#endif

		DescriptorSetView() = default;

#if RENDERER_VULKAN
		DescriptorSetView(const VkDescriptorSet pDescriptorSet)
			: m_pDescriptorSet(pDescriptorSet)
		{
		}

		[[nodiscard]] operator VkDescriptorSet() const
		{
			return m_pDescriptorSet;
		}
#elif RENDERER_METAL || RENDERER_WEBGPU
		DescriptorSetView(Internal::DescriptorSetData* pDescriptorSet)
			: m_pDescriptorSet(pDescriptorSet)
		{
		}

		[[nodiscard]] operator Internal::DescriptorSetData*() const
		{
			return m_pDescriptorSet;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pDescriptorSet != 0;
#else
			return false;
#endif
		}
		[[nodiscard]] bool operator==(const DescriptorSetView& other) const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pDescriptorSet == other.m_pDescriptorSet;
#else
			return false;
#endif
		}

		struct ImageInfo
		{
			ImageInfo() = default;

			ImageInfo(const SamplerView sampler, const ImageMappingView mapping, const ImageLayout imageLayout)
				: m_sampler(sampler)
				, m_imageView(mapping)
				, m_imageLayout(imageLayout)
			{
				Assert(sampler.IsValid() || mapping.IsValid());
			}

			SamplerView m_sampler;
			ImageMappingView m_imageView;
			ImageLayout m_imageLayout;
		};

		struct BufferInfo
		{
			BufferInfo() = default;

			BufferInfo(const BufferView buffer, const size offset, const size range)
				: m_buffer(buffer)
				, m_offset(offset)
				, m_range(range)
			{
				Assert(buffer.IsValid());
			}

			BufferView m_buffer;
			size m_offset;
			size m_range;
		};

		struct UpdateInfo;
		struct CopyInfo;

		void BindIndirectAccelerationStructures(
			const BindingIndexType bindingIndex, const ArrayView<const PrimitiveAccelerationStructureView> primitiveAccelerationStructures
		) const;

		[[nodiscard]] static bool ValidateUpdate(const ArrayView<const UpdateInfo, uint8> descriptorUpdates);

		static void Update(const LogicalDeviceView logicalDevice, const ArrayView<const UpdateInfo, uint8> descriptorUpdates);
		static void UpdateAndCopy(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const UpdateInfo, uint8> descriptorUpdates,
			const ArrayView<const CopyInfo, uint8> descriptorCopies
		);
		static void Copy(const LogicalDeviceView logicalDevice, const ArrayView<const CopyInfo, uint8> descriptorCopies);
	protected:
#if RENDERER_VULKAN
		VkDescriptorSet m_pDescriptorSet = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::DescriptorSetData* m_pDescriptorSet{nullptr};
#endif
	};

	struct DescriptorSetView::UpdateInfo
	{
		friend DescriptorSetView;
#if RENDERER_VULKAN
		uint32 m_vulkanType = 35;
		const void* m_pNext = nullptr;
#endif
	public:
		UpdateInfo() = default;
		UpdateInfo(
			const DescriptorSetView set,
			const BindingIndexType bindingIndex,
			const uint32 arrayIndex,
			const DescriptorType type,
			const ArrayView<const ImageInfo, uint32> images
		)
			: m_set(set)
			, m_bindingIndex(bindingIndex)
			, m_arrayIndex(arrayIndex)
			, m_count(images.GetSize())
#if RENDERER_VULKAN
			, m_type(static_cast<uint32>(type))
#else
			, m_type(type)
#endif
			, m_pImageInfo(images.GetData())
		{
		}
		UpdateInfo(
			const DescriptorSetView set,
			const BindingIndexType bindingIndex,
			const uint32 arrayIndex,
			const DescriptorType type,
			const ArrayView<const BufferInfo, uint32> buffers
		)
			: m_set(set)
			, m_bindingIndex(bindingIndex)
			, m_arrayIndex(arrayIndex)
			, m_count(buffers.GetSize())
#if RENDERER_VULKAN
			, m_type(static_cast<uint32>(type))
#else
			, m_type(type)
#endif
			, m_pBufferInfo(buffers.GetData())
		{
		}
#if !RENDERER_VULKAN && RENDERER_SUPPORTS_RAYTRACING
		UpdateInfo(const DescriptorSetView set, const BindingIndexType bindingIndex, const AccelerationStructureView accelerationStructure)
			: m_set(set)
			, m_bindingIndex(bindingIndex)
			, m_arrayIndex(0)
			, m_count(1)
#if RENDERER_VULKAN
			, m_type(static_cast<uint32>(DescriptorType::AccelerationStructure))
#else
			, m_type(DescriptorType::AccelerationStructure)
#endif
			, m_accelerationStructure(accelerationStructure)
		{
		}
#endif

		DescriptorSetView m_set;
		BindingIndexType m_bindingIndex;
		uint32 m_arrayIndex;
		uint32 m_count;
#if RENDERER_VULKAN
		uint32 m_type;
#else
		DescriptorType m_type;
#endif
		Optional<const ImageInfo*> m_pImageInfo = nullptr;
		Optional<const BufferInfo*> m_pBufferInfo = nullptr;
		void* m_pTexelBufferView = nullptr;
#if !RENDERER_VULKAN && RENDERER_SUPPORTS_RAYTRACING
		AccelerationStructureView m_accelerationStructure;
#endif
	};

	struct DescriptorSetView::CopyInfo
	{
		struct Set
		{
			DescriptorSetView m_set;
			BindingIndexType m_bindingIndex;
			uint32 m_arrayIndex;
		};
	protected:
		friend DescriptorSetView;
#if RENDERER_VULKAN
		uint32 m_vulkanType = 36;
		const void* m_pNext = nullptr;
#endif
	public:
		CopyInfo() = default;
		CopyInfo(const Set source, const Set target, const BindingIndexType count)
			: m_source(source)
			, m_target(target)
			, m_count(count)
		{
		}

		Set m_source;
		Set m_target;
		BindingIndexType m_count;
	};
}
