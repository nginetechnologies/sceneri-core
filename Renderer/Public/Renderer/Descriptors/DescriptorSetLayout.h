#pragma once

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

#include <Renderer/Descriptors/DescriptorSetLayoutView.h>
#include <Renderer/Constants.h>

#include <Renderer/Descriptors/DescriptorType.h>
#include <Renderer/ShaderStage.h>
#include <Renderer/Format.h>
#include <Renderer/Wrappers/ImageMappingType.h>

#include <Renderer/Metal/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct SamplerView;

	enum class SampledImageType : uint8
	{
		Undefined = 0,
		Float = 1,
		UnfilterableFloat = 2,
		Depth = 3,
		SignedInteger = 4,
		UnsignedInteger = 5
	};

	enum class SamplerBindingType : uint8
	{
		Undefined = 0,
		Filtering = 1,
		NonFiltering = 2,
		Comparison = 3
	};

	enum class StorageTextureAccess : uint8
	{
		Undefined = 0,
		WriteOnly = 1,
		ReadOnly = 2,
		ReadWrite = 3
	};

	struct DescriptorSetLayout : public DescriptorSetLayoutView
	{
		enum class Flags : uint8
		{
			UpdateAfterBind = 1 << 1
		};

		struct Binding
		{
			enum class Flags : uint32
			{
				UpdateAfterBind = 1 << 0,
				PartiallyBound = 1 << 2,
				ShaderWrite = (1 << 3) * RENDERER_WEBGPU
			};

			[[nodiscard]] static constexpr Binding MakeSampler(
				const uint32 index,
				const EnumFlags<ShaderStage> shaderStages,
				const SamplerBindingType samplerBindingType,
				const uint32 arrayElementCount = 1,
				const Optional<SamplerView*> pSamplers = Invalid
			)
			{
				return Binding{
					index,
					DescriptorType::Sampler,
					arrayElementCount,
					shaderStages,
					pSamplers,
					Format::Invalid,
					SampledImageType::Undefined,
					ImageMappingType::TwoDimensional,
					samplerBindingType
				};
			}
			[[nodiscard]] static constexpr Binding MakeCombinedImageSampler(
				const uint32 index,
				const EnumFlags<ShaderStage> shaderStages,
				const SampledImageType sampledImageType,
				const ImageMappingType imageMappingType,
				const uint32 arrayElementCount = 1,
				const Optional<SamplerView*> pSamplers = Invalid
			)
			{
				return Binding{
					index,
					DescriptorType::CombinedImageSampler,
					arrayElementCount,
					shaderStages,
					pSamplers,
					Format::Invalid,
					sampledImageType,
					imageMappingType
				};
			}
			[[nodiscard]] static constexpr Binding MakeSampledImage(
				const uint32 index,
				const EnumFlags<ShaderStage> shaderStages,
				const SampledImageType sampledImageType,
				const ImageMappingType imageMappingType,
				const uint32 arrayElementCount = 1
			)
			{
				return Binding{
					index,
					DescriptorType::SampledImage,
					arrayElementCount,
					shaderStages,
					Invalid,
					Format::Invalid,
					sampledImageType,
					imageMappingType
				};
			}
			[[nodiscard]] static constexpr Binding MakeStorageImage(
				const uint32 index, const EnumFlags<ShaderStage> shaderStages, const Format format, const StorageTextureAccess storageTextureAccess
			)
			{
				return Binding{
					index,
					DescriptorType::StorageImage,
					1,
					shaderStages,
					Invalid,
					format,
					SampledImageType::Undefined,
					ImageMappingType::TwoDimensional,
					SamplerBindingType::Undefined,
					storageTextureAccess
				};
			}
			[[nodiscard]] static constexpr Binding MakeUniformBuffer(const uint32 index, const EnumFlags<ShaderStage> shaderStages)
			{
				return Binding{index, DescriptorType::UniformBuffer, 1, shaderStages};
			}
			[[nodiscard]] static constexpr Binding MakeUniformBufferDynamic(const uint32 index, const EnumFlags<ShaderStage> shaderStages)
			{
				return Binding{index, DescriptorType::UniformBufferDynamic, 1, shaderStages};
			}
			[[nodiscard]] static constexpr Binding MakeStorageBuffer(const uint32 index, const EnumFlags<ShaderStage> shaderStages)
			{
				return Binding{index, DescriptorType::StorageBuffer, 1, shaderStages};
			}
			[[nodiscard]] static constexpr Binding MakeStorageBufferDynamic(const uint32 index, const EnumFlags<ShaderStage> shaderStages)
			{
				return Binding{index, DescriptorType::StorageBufferDynamic, 1, shaderStages};
			}
			[[nodiscard]] static constexpr Binding MakeInputAttachment(
				const uint32 index,
				const EnumFlags<ShaderStage> shaderStages,
				const SampledImageType sampledImageType,
				const ImageMappingType imageMappingType
			)
			{
				return Binding{
					index,
					DescriptorType::InputAttachment,
					1,
					shaderStages,
					Invalid,
					Format::Invalid,
					sampledImageType,
					imageMappingType
				};
			}
			[[nodiscard]] static constexpr Binding MakeAccelerationStructure(const uint32 index, const EnumFlags<ShaderStage> shaderStages)
			{
				return Binding{index, DescriptorType::AccelerationStructure, 1, shaderStages};
			}
		protected:
			constexpr Binding(
				const uint32 index,
				const DescriptorType type,
				const uint32 count,
				const EnumFlags<ShaderStage> shaderStages,
				const Optional<SamplerView*> pSamplers = Invalid,
				[[maybe_unused]] const Format format = Format::Invalid,
				[[maybe_unused]] const SampledImageType sampledImageType = SampledImageType::Undefined,
				[[maybe_unused]] const ImageMappingType imageMappingType = ImageMappingType::TwoDimensional,
				[[maybe_unused]] const SamplerBindingType samplerBindingType = SamplerBindingType::Undefined,
				[[maybe_unused]] const StorageTextureAccess storageTextureAccess = StorageTextureAccess::Undefined
			)
				: m_index(index)
				, m_type(type)
				, m_count(count)
				, m_shaderStages(shaderStages)
				, m_pSamplers(pSamplers)
#if RENDERER_WEBGPU
				, m_format(format)
				, m_sampledImageType(sampledImageType)
				, m_imageMappingType(imageMappingType)
				, m_samplerBindingType(samplerBindingType)
				, m_storageTextureAccess(storageTextureAccess)
#endif
			{
			}
		public:
			uint32 m_index;
			DescriptorType m_type;
			uint32 m_count;
			EnumFlags<ShaderStage> m_shaderStages;
			const SamplerView* m_pSamplers = nullptr;
#if RENDERER_WEBGPU
			Format m_format;
			SampledImageType m_sampledImageType;
			ImageMappingType m_imageMappingType;
			SamplerBindingType m_samplerBindingType;
			StorageTextureAccess m_storageTextureAccess;
#endif
		};

		DescriptorSetLayout() = default;
		DescriptorSetLayout(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const Binding, uint8> bindings,
			const EnumFlags<Flags> flags = {},
			const ArrayView<const EnumFlags<Binding::Flags>, uint8> bindingFlags = {}
		);
		DescriptorSetLayout(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout& operator=(const DescriptorSetLayout&) = delete;
		DescriptorSetLayout([[maybe_unused]] DescriptorSetLayout&& other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pLayout = other.m_pLayout;
			other.m_pLayout = 0;
#endif
		}
		DescriptorSetLayout& operator=(DescriptorSetLayout&&);
		~DescriptorSetLayout();

		void Destroy(const LogicalDeviceView logicalDevice);
	};

	ENUM_FLAG_OPERATORS(DescriptorSetLayout::Binding::Flags);
}
