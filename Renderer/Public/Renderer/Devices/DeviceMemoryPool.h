#pragma once

#include <Renderer/Buffers/DeviceMemory.h>
#include <Renderer/Devices/DeviceMemoryAllocation.h>

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Memory/Bitset.h>
#include <Common/Memory/Containers/Vector.h>

#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicBool.h>

namespace ngine::Rendering
{
	struct DeviceMemoryView;
	struct LogicalDeviceView;
	struct PhysicalDevice;

#define DISABLE_MEMORY_POOL_USAGE 0

	struct DeviceMemoryPool
	{
		struct Block
		{
			Block(
				const LogicalDeviceView logicalDevice, const uint32 size, const uint8 memoryTypeIndex, const EnumFlags<MemoryFlags> memoryFlags
			);

			using SizeType = uint32;
			inline static constexpr SizeType Alignment = 16;

			DeviceMemory m_memory;
			uint32 m_availableSpace = 0;

			struct Fragment
			{
				using IdentifierType = typename DeviceMemoryFragment::IdentifierType;
				using SizeType = typename DeviceMemoryFragment::SizeType;
				using ReuseType = typename DeviceMemoryFragment::ReuseType;
				using IdentifierStorageType = typename DeviceMemoryFragment::IdentifierStorageType;
				inline static constexpr size MaximumCount = DeviceMemoryFragment::MaximumCount;
				inline static constexpr size ReuseCount = DeviceMemoryFragment::ReuseCount;
				inline static constexpr size CountBitCount = DeviceMemoryFragment::CountBitCount;
				inline static constexpr size RequiredBitCount = DeviceMemoryFragment::RequiredBitCount;

				[[nodiscard]] IdentifierType GetPreviousFragmentIdentifier() const
				{
					return Block::Fragment::IdentifierType::MakeFromIndex(m_previousFragmentIdentifierIndex);
				}
				[[nodiscard]] IdentifierType GetNextFragmentIdentifier() const
				{
					return Block::Fragment::IdentifierType::MakeFromIndex(m_nextFragmentIdentifierIndex);
				}

				Block::SizeType m_offset;
				Block::SizeType m_size;
				typename IdentifierType::IndexType m_previousFragmentIdentifierIndex;
				typename IdentifierType::IndexType m_nextFragmentIdentifierIndex;
			};

			TSaltedIdentifierStorage<Fragment::IdentifierType> m_identifierStorage;
			TIdentifierArray<Fragment, Fragment::IdentifierType> m_fragments;
			IdentifierMask<Fragment::IdentifierType> m_usedFragments;
			IdentifierMask<Fragment::IdentifierType> m_unusedFragments;
		};

		using Allocation = DeviceMemoryAllocation;

		DeviceMemoryPool() = default;
		DeviceMemoryPool(const LogicalDeviceView logicalDevice, const PhysicalDevice& physicalDevice);
		~DeviceMemoryPool();

		void Destroy(const LogicalDeviceView logicalDevice);

		[[nodiscard]] Allocation Allocate(
			const LogicalDeviceView logicalDevice,
			size allocationSize,
			const uint32 alignment,
			const uint8 memoryTypeIndex,
			const EnumFlags<MemoryFlags> memoryFlags
		);
		void Deallocate(const Allocation allocation);
		[[nodiscard]] Allocation AllocateRaw(
			const LogicalDeviceView logicalDevice, size allocationSize, const uint8 memoryTypeIndex, const EnumFlags<MemoryFlags> memoryFlags
		);
		void DeallocateRaw(const LogicalDeviceView logicalDevice, const Allocation allocation);

		[[nodiscard]] bool IsAllocationValid(const Allocation allocation) const;
		[[nodiscard]] bool IsRawAllocationValid(const Allocation allocation) const;
	protected:
		struct HostMappableMemory
		{
			DeviceMemory m_memory;
		};

		struct MemoryType
		{
			mutable Threading::SharedMutex m_blockMutex;

			inline static constexpr uint16 InitialBlockCount = 2;
			Vector<Block, typename DeviceMemoryAllocation::BlockSizeType> m_blocks;
			Threading::Atomic<bool> m_isOutOfMemory = false;

			TSaltedIdentifierStorage<Block::Fragment::IdentifierType> m_hostMappableMemoryIdentifierStorage;
			TIdentifierArray<DeviceMemory, Block::Fragment::IdentifierType> m_hostMappableMemory;
		};

		Array<MemoryType, 32> m_memoryTypes;
	};
}
