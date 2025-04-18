#pragma once

#include <Renderer/Buffers/DeviceMemoryView.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/CountBits.h>
#include <Common/Memory/GetIntegerType.h>

namespace ngine::Rendering
{
	struct DeviceMemoryFragment
	{
		inline static constexpr size MaximumCount = 16384;
		using SizeType = Memory::NumericSize<MaximumCount>;
		inline static constexpr size ReuseCount = 16384;
		using ReuseType = Memory::NumericSize<ReuseCount>;

		inline static constexpr size CountBitCount = Memory::GetBitWidth(MaximumCount);
		inline static constexpr size RequiredBitCount = CountBitCount + Memory::GetBitWidth(ReuseCount);
		using IdentifierStorageType = Memory::IntegerType<RequiredBitCount, false>;

		using IdentifierType = TIdentifier<IdentifierStorageType, CountBitCount>;
	};

	struct DeviceMemoryAllocation
	{
		using BlockSizeType = uint16;

		[[nodiscard]] bool IsValid() const
		{
			return m_memory.IsValid();
		}

		DeviceMemoryView m_memory;
		uint32 m_offset = 0;
		uint32 m_size = 0;
		DeviceMemoryFragment::IdentifierType m_identifier;
		uint8 m_memoryTypeIndex = 0;
		BlockSizeType m_blockIndex = 0;
	};
}
