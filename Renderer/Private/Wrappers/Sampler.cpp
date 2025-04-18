#include <Renderer/Wrappers/Sampler.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Devices/PhysicalDeviceView.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include "Metal/ConvertCompareOperation.h"
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Window/Window.h>
#include "WebGPU/ConvertCompareOperation.h"

#include <Common/Assert/Assert.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] MTLSamplerAddressMode ConvertAddressMode(const AddressMode addressMode)
	{
		switch (addressMode)
		{
			case AddressMode::Repeat:
				return MTLSamplerAddressModeRepeat;
			case AddressMode::RepeatMirrored:
				return MTLSamplerAddressModeMirrorRepeat;
			case AddressMode::ClampToEdge:
				return MTLSamplerAddressModeClampToEdge;
			case AddressMode::ClampToBorder:
				return MTLSamplerAddressModeClampToBorderColor;
			case AddressMode::ClampToEdgeMirrored:
				return MTLSamplerAddressModeMirrorClampToEdge;
		}
	}

	[[nodiscard]] MTLSamplerMinMagFilter ConvertMinMagFilter(const FilterMode filterMode)
	{
		switch (filterMode)
		{
			case FilterMode::Nearest:
				return MTLSamplerMinMagFilterNearest;
			case FilterMode::Linear:
				return MTLSamplerMinMagFilterLinear;
		}
	}

	[[nodiscard]] MTLSamplerMipFilter ConvertMipFilter(const MipmapMode mipmapMode)
	{
		switch (mipmapMode)
		{
			case MipmapMode::Nearest:
				return MTLSamplerMipFilterNearest;
			case MipmapMode::Linear:
				return MTLSamplerMipFilterLinear;
		}
	}
#endif

#if RENDERER_WEBGPU
	[[nodiscard]] WGPUAddressMode ConvertAddressMode(const AddressMode addressMode)
	{
		switch (addressMode)
		{
			case AddressMode::Repeat:
				return WGPUAddressMode_Repeat;
			case AddressMode::RepeatMirrored:
				return WGPUAddressMode_MirrorRepeat;
			case AddressMode::ClampToEdge:
				return WGPUAddressMode_ClampToEdge;
			case AddressMode::ClampToBorder:
				return WGPUAddressMode_ClampToEdge;
			case AddressMode::ClampToEdgeMirrored:
				return WGPUAddressMode_ClampToEdge;
		}
		ExpectUnreachable();
	}

	[[nodiscard]] WGPUFilterMode ConvertMinMagFilter(const FilterMode filterMode)
	{
		switch (filterMode)
		{
			case FilterMode::Nearest:
				return WGPUFilterMode_Nearest;
			case FilterMode::Linear:
				return WGPUFilterMode_Linear;
		}
		ExpectUnreachable();
	}

	[[nodiscard]] WGPUMipmapFilterMode ConvertMipFilter(const MipmapMode mipmapMode)
	{
		switch (mipmapMode)
		{
			case MipmapMode::Nearest:
				return WGPUMipmapFilterMode_Nearest;
			case MipmapMode::Linear:
				return WGPUMipmapFilterMode_Linear;
		}
		ExpectUnreachable();
	}
#endif

	[[nodiscard]] SamplerView CreateSimpleSampler(
		const LogicalDeviceView logicalDevice,
		const AddressMode addressMode,
		const FilterMode filterMode,
		const MipmapMode mipmapMode,
		const CompareOperation compareOp,
		const Math::Range<int16> mipRange
	)
	{
#if RENDERER_VULKAN
		const VkSamplerCreateInfo samplerInfo = {
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			nullptr,
			0,
			static_cast<VkFilter>(filterMode),
			static_cast<VkFilter>(filterMode),
			// filterMode == FilterMode::Nearest ? VK_SAMPLER_MIPMAP_MODE_NEAREST : VK_SAMPLER_MIPMAP_MODE_LINEAR,
			static_cast<VkSamplerMipmapMode>(mipmapMode),
			static_cast<VkSamplerAddressMode>(addressMode),
			static_cast<VkSamplerAddressMode>(addressMode),
			static_cast<VkSamplerAddressMode>(addressMode),
			0.f,
			VK_FALSE,
			1,
			(VkBool32)(compareOp != CompareOperation::AlwaysSucceed ? VK_TRUE : VK_FALSE),
			static_cast<VkCompareOp>(compareOp),
			(float)mipRange.GetMinimum(),
			(float)mipRange.GetMaximum(),
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			VK_FALSE
		};

		VkSampler sampler;
		[[maybe_unused]] const VkResult samplerCreationResult = vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &sampler);
		Assert(samplerCreationResult == VK_SUCCESS);
		return sampler;
#elif RENDERER_METAL
		MTLSamplerDescriptor* samplerDescriptor = [MTLSamplerDescriptor new];

		samplerDescriptor.sAddressMode = ConvertAddressMode(addressMode);
		samplerDescriptor.tAddressMode = ConvertAddressMode(addressMode);
		samplerDescriptor.rAddressMode = ConvertAddressMode(addressMode);
		samplerDescriptor.magFilter = ConvertMinMagFilter(filterMode);
		samplerDescriptor.minFilter = ConvertMinMagFilter(filterMode);
		samplerDescriptor.mipFilter = ConvertMipFilter(mipmapMode);
		samplerDescriptor.lodMinClamp = mipRange.GetMinimum();
		samplerDescriptor.lodMaxClamp = mipRange.GetMaximum();
		samplerDescriptor.compareFunction = ConvertCompareOperation(compareOp);
		samplerDescriptor.supportArgumentBuffers = YES;
		samplerDescriptor.maxAnisotropy = 1;
		samplerDescriptor.normalizedCoordinates = true;

		return [(id<MTLDevice>)logicalDevice newSamplerStateWithDescriptor:samplerDescriptor];
#elif RENDERER_WEBGPU
		const WGPUSamplerDescriptor descriptor{
			nullptr,
#if RENDERER_WEBGPU_DAWN
			WGPUStringView { nullptr, 0 },
#else
			nullptr,
#endif
			ConvertAddressMode(addressMode),
			ConvertAddressMode(addressMode),
			ConvertAddressMode(addressMode),
			ConvertMinMagFilter(filterMode),
			ConvertMinMagFilter(filterMode),
			ConvertMipFilter(mipmapMode),
			(float)mipRange.GetMinimum(),
			(float)mipRange.GetMaximum(),
			compareOp != CompareOperation::AlwaysSucceed ? ConvertCompareOperation(compareOp) : WGPUCompareFunction_Undefined,
			1
		};

		SamplerView sampler;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[logicalDevice, &descriptor, &sampler]()
			{
				sampler = wgpuDeviceCreateSampler(logicalDevice, &descriptor);
				if (LIKELY(sampler.IsValid()))
				{
#if RENDERER_WEBGPU_DAWN
					wgpuSamplerAddRef(sampler);
#else
					wgpuSamplerReference(sampler);
#endif
				}
			}
		);
		return sampler;
#else
		return {};
#endif
	}

	Sampler::Sampler(
		const LogicalDeviceView logicalDevice,
		const AddressMode addressMode,
		const FilterMode filterMode,
		const CompareOperation compareOp,
		const Math::Range<int16> mipRange
	)
		: SamplerView(CreateSimpleSampler(
				logicalDevice,
				addressMode,
				filterMode,
				(filterMode == FilterMode::Nearest ? MipmapMode::Nearest : MipmapMode::Linear),
				compareOp,
				mipRange
			))
	{
	}

	Sampler::Sampler(
		const LogicalDeviceView logicalDevice,
		const AddressMode addressMode,
		const FilterMode filterMode,
		const MipmapMode mipmapMode,
		const CompareOperation compareOp,
		const Math::Range<int16> mipRange
	)
		: SamplerView(CreateSimpleSampler(logicalDevice, addressMode, filterMode, mipmapMode, compareOp, mipRange))
	{
	}

	Sampler& Sampler::operator=([[maybe_unused]] Sampler&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pSampler = other.m_pSampler;
		other.m_pSampler = 0;
#endif
		return *this;
	}

	Sampler::~Sampler()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void Sampler::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroySampler(logicalDevice, m_pSampler, nullptr);
		m_pSampler = 0;
#elif RENDERER_METAL
		m_pSampler = nullptr;
#elif RENDERER_WEBGPU
		if (m_pSampler != nullptr)
		{
			Rendering::Window::QueueOnWindowThread(
				[pSampler = m_pSampler]()
				{
					wgpuSamplerRelease(pSampler);
				}
			);
			m_pSampler = nullptr;
		}
#endif
	}

	SamplerConvertYCbCrToRGB::SamplerConvertYCbCrToRGB(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const ChromaLocation chromaLocation,
		[[maybe_unused]] const AddressMode addressMode,
		[[maybe_unused]] const FilterMode filterMode,
		[[maybe_unused]] const Math::Range<uint16> mipRange
	)
	{
#if SUPPORT_YCBCR_TO_RGB_SAMPLER && RENDERER_VULKAN
		{
			const VkSamplerYcbcrConversionCreateInfo conversionInfo{
				VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_CREATE_INFO,
				nullptr,
				VK_FORMAT_G8_B8R8_2PLANE_420_UNORM,
				VK_SAMPLER_YCBCR_MODEL_CONVERSION_YCBCR_709,
				VK_SAMPLER_YCBCR_RANGE_ITU_FULL,
				VkComponentMapping{
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY,
					VK_COMPONENT_SWIZZLE_IDENTITY
				},
				static_cast<VkChromaLocation>(chromaLocation),
				static_cast<VkChromaLocation>(chromaLocation),
				VK_FILTER_LINEAR,
				VK_FALSE
			};

			const VkResult result = vkCreateSamplerYcbcrConversion(logicalDevice, &conversionInfo, nullptr, &m_samplerConversion);
			if (UNLIKELY(result != VK_SUCCESS))
			{
				return;
			}
		}

		const VkSamplerYcbcrConversionInfo conversionInfo{VK_STRUCTURE_TYPE_SAMPLER_YCBCR_CONVERSION_INFO, nullptr, m_samplerConversion};

		const VkSamplerCreateInfo samplerInfo = {
			VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO,
			&conversionInfo,
			0,
			static_cast<VkFilter>(filterMode),
			static_cast<VkFilter>(filterMode),
			VK_SAMPLER_MIPMAP_MODE_LINEAR,
			static_cast<VkSamplerAddressMode>(addressMode),
			static_cast<VkSamplerAddressMode>(addressMode),
			static_cast<VkSamplerAddressMode>(addressMode),
			0.f,
			VK_FALSE,
			1,
			VK_FALSE,
			static_cast<VkCompareOp>(CompareOperation::AlwaysSucceed),
			(float)mipRange.GetMinimum(),
			(float)mipRange.GetMaximum(),
			VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE,
			VK_FALSE
		};
		[[maybe_unused]] const VkResult samplerCreationResult = vkCreateSampler(logicalDevice, &samplerInfo, nullptr, &m_pSampler);
#endif
	}

	void SamplerConvertYCbCrToRGB::Destroy(const LogicalDeviceView logicalDevice)
	{
		Sampler::Destroy(logicalDevice);
#if SUPPORT_YCBCR_TO_RGB_SAMPLER && RENDERER_VULKAN
		vkDestroySamplerYcbcrConversion(logicalDevice, m_samplerConversion, nullptr);
#endif
	}

	SamplerConvertYCbCrToRGB& SamplerConvertYCbCrToRGB::operator=(SamplerConvertYCbCrToRGB&& other)
	{
		Sampler::operator=(static_cast<Sampler&&>(other));
#if RENDERER_VULKAN
		m_samplerConversion = other.m_samplerConversion;
		other.m_samplerConversion = 0;
#endif
		return *this;
	}
}
