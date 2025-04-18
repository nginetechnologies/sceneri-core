#pragma once

#include <Renderer/ImageLayout.h>
#include <Renderer/UsageFlags.h>
#include <Renderer/Framegraph/SubresourceStates.h>

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Memory/CountBits.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Threading/AtomicInteger.h>

#include <Renderer/Format.h>
#include <Renderer/Wrappers/Image.h>
#include <Renderer/Assets/Texture/ArrayRange.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Assets/Texture/MipRange.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandEncoderView;
	struct StagingBuffer;

	struct RenderTexture : public Image
	{
		enum DummyType
		{
			Dummy
		};
		enum RenderTargetType
		{
			RenderTarget
		};

		RenderTexture() = default;
		RenderTexture(
			const DummyType,
			LogicalDevice& logicalDevice,
			CommandEncoderView commandEncoder,
			StagingBuffer& stagingBufferOut,
			const EnumFlags<Flags> flags,
			const EnumFlags<UsageFlags> usageFlags,
			const ArrayRange::UnitType arraySize,
			const Math::Color color
		);
		RenderTexture(
			const DummyType,
			LogicalDevice& logicalDevice,
			const Math::Vector2ui resolution,
			const Format format,
			CommandEncoderView commandEncoder,
			StagingBuffer& stagingBufferOut,
			const EnumFlags<Flags> flags,
			const EnumFlags<UsageFlags> usageFlags,
			const ArrayRange::UnitType arraySize,
			const ConstByteView data
		);
		RenderTexture(
			const RenderTargetType,
			LogicalDevice& logicalDevice,
			const Math::Vector2ui resolution,
			const Format format,
			const SampleCount sampleCount,
			const EnumFlags<Flags> flags,
			const EnumFlags<UsageFlags> usageFlags,
			const MipMask totalMipMask,
			const ArrayRange::UnitType numArrayLayers
		);
		RenderTexture(
			LogicalDevice& logicalDevice,
			const PhysicalDevice& physicalDevice,
			const Format format,
			const SampleCount sampleCount,
			const EnumFlags<Flags> flags,
			const Math::Vector3ui resolution,
			const EnumFlags<UsageFlags> usageFlags,
			const ImageLayout initialLayout,
			const MipMask totalMipMask,
			const MipMask availableMipMask,
			const ArrayRange::UnitType numArrayLayers
		);
		RenderTexture(const RenderTexture&) = delete;
		RenderTexture& operator=(const RenderTexture&) = delete;
		RenderTexture(RenderTexture&& other) = default;
		RenderTexture& operator=(RenderTexture&& other) = default;
		~RenderTexture() = default;

		[[nodiscard]] Format GetFormat() const
		{
			return m_format;
		}
		[[nodiscard]] EnumFlags<UsageFlags> GetUsageFlags() const
		{
			return m_usageFlags;
		}
		[[nodiscard]] MipMask::StoredType GetTotalMipCount() const
		{
			return Memory::GetNumberOfSetBits(m_totalMipsMask);
		}
		[[nodiscard]] MipMask::StoredType GetLoadedMipCount() const
		{
			return Memory::GetNumberOfSetBits(m_loadedMipsMask.Load());
		}
		[[nodiscard]] ArrayRange::UnitType GetTotalArrayCount() const
		{
			return m_totalArrayCount;
		}
		[[nodiscard]] ImageSubresourceRange GetTotalSubresourceRange() const;

		void OnMipsLoaded(const MipMask newMips)
		{
			Assert((m_totalMipsMask & newMips.GetValue()) == newMips.GetValue(), "Loading mips not part of texture");
			Assert((m_loadedMipsMask.Load() & newMips.GetValue()) == 0, "Loaded duplicate mips");
			m_loadedMipsMask |= newMips.GetValue();
		}
		[[nodiscard]] MipMask GetLoadedMipMask() const
		{
			return MipMask{m_loadedMipsMask.Load()};
		}
		[[nodiscard]] MipMask GetTotalMipMask() const
		{
			return MipMask{m_totalMipsMask};
		}
		[[nodiscard]] MipRange GetAvailableMipRange() const
		{
			return GetLoadedMipMask().GetRange(*GetTotalMipMask().GetLastIndex() + 1);
		}
		[[nodiscard]] bool HasLoadedAllMips() const
		{
			return GetLoadedMipCount() == GetTotalMipCount();
		}

		[[nodiscard]] const SubresourceStatesBase& GetSubresourceStates() const
		{
			return m_subresourceStates;
		}
		[[nodiscard]] SubresourceStatesBase& GetSubresourceStates()
		{
			return m_subresourceStates;
		}
	protected:
		Format m_format;
		EnumFlags<UsageFlags> m_usageFlags;

		MipMask::StoredType m_totalMipsMask;
		Threading::Atomic<MipMask::StoredType> m_loadedMipsMask{0};
		ArrayRange::UnitType m_totalArrayCount = 1;

		SubresourceStatesBase m_subresourceStates;
	};
}
