#pragma once

#include <Renderer/Wrappers/SamplerView.h>
#include <Renderer/Wrappers/AddressMode.h>
#include <Renderer/Wrappers/FilterMode.h>
#include <Renderer/Wrappers/MipmapMode.h>
#include <Renderer/Wrappers/CompareOperation.h>
#include <Common/Math/Range.h>

namespace ngine::Rendering
{
	struct PhysicalDeviceView;
	struct LogicalDeviceView;

	struct Sampler : public SamplerView
	{
		Sampler() = default;
		Sampler(
			const LogicalDeviceView logicalDevice,
			const AddressMode addressMode = AddressMode::Repeat,
			const FilterMode filterMode = FilterMode::Linear,
			const CompareOperation compareOp = CompareOperation::AlwaysSucceed,
			const Math::Range<int16> mipRange = Math::Range<int16>::MakeStartToEnd(0, 1000)
		);
		Sampler(
			const LogicalDeviceView logicalDevice,
			const AddressMode addressMode,
			const FilterMode filterMode,
			const MipmapMode mipmapMode,
			const CompareOperation compareOp,
			const Math::Range<int16> mipRange
		);
		Sampler(const Sampler&) = delete;
		Sampler& operator=(const Sampler&) = delete;
		Sampler([[maybe_unused]] Sampler&& other)
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			m_pSampler = other.m_pSampler;
			other.m_pSampler = 0;
#endif
		}
		Sampler& operator=(Sampler&&);
		~Sampler();

		void Destroy(const LogicalDeviceView logicalDevice);
	};

	enum class ChromaLocation : uint8
	{
		Even,
		Midpoint
	};

#define SUPPORT_YCBCR_TO_RGB_SAMPLER (RENDERER_VULKAN && !PLATFORM_ANDROID) || RENDERER_METAL

	struct SamplerConvertYCbCrToRGB : public Sampler
	{
		SamplerConvertYCbCrToRGB(
			const LogicalDeviceView logicalDevice,
			const ChromaLocation chromaLocation,
			const AddressMode addressMode = AddressMode::ClampToEdge,
			const FilterMode filterMode = FilterMode::Linear,
			const Math::Range<uint16> mipRange = Math::Range<uint16>::MakeStartToEnd(0, 1000)
		);
		SamplerConvertYCbCrToRGB(const SamplerConvertYCbCrToRGB&) = delete;
		SamplerConvertYCbCrToRGB& operator=(const SamplerConvertYCbCrToRGB&) = delete;
		SamplerConvertYCbCrToRGB(SamplerConvertYCbCrToRGB&& other)
			: Sampler(Forward<Sampler>(static_cast<Sampler&&>(other)))
#if RENDERER_VULKAN
			, m_samplerConversion(other.m_samplerConversion)
#endif
		{
#if RENDERER_VULKAN
			other.m_samplerConversion = 0;
#endif
		}
		SamplerConvertYCbCrToRGB& operator=(SamplerConvertYCbCrToRGB&& other);

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN
			return Sampler::IsValid() & (m_samplerConversion != 0);
#else
			return Sampler::IsValid();
#endif
		}

		void Destroy(const LogicalDeviceView logicalDevice);
	protected:
#if RENDERER_VULKAN
		VkSamplerYcbcrConversion m_samplerConversion;
#endif
	};
}
