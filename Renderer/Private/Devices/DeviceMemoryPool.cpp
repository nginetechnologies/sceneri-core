#include "Devices/DeviceMemoryPool.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>

#include <Common/Platform/StaticUnreachable.h>
#include <Common/Memory/Align.h>
#include <Common/Memory/IsAligned.h>
#include <Common/Memory/MemorySize.h>
#include <Common/Memory/AddressOf.h>

namespace ngine::Rendering
{
	static constexpr size NewAlignment = 4096;

	inline static constexpr Memory::Size DefaultPoolSize = 256_megabytes;
	inline static constexpr Memory::Size ExtraBlockSize = 100_megabytes;

	DeviceMemoryPool::DeviceMemoryPool(const LogicalDeviceView logicalDevice, const PhysicalDevice& physicalDevice)
	{
		if constexpr (!DISABLE_MEMORY_POOL_USAGE)
		{
			const ArrayView<const EnumFlags<MemoryFlags>, PhysicalDevice::MemoryTypeSizeType> memoryTypes = physicalDevice.GetMemoryTypes();
			if (const OptionalIterator<const EnumFlags<MemoryFlags>> deviceLocalMemory = memoryTypes.Find(MemoryFlags::DeviceLocal))
			{
				const PhysicalDevice::MemoryTypeSizeType memoryTypeIndex = memoryTypes.GetIteratorIndex(deviceLocalMemory);
				Block& block = m_memoryTypes[memoryTypeIndex].m_blocks.EmplaceBack(
					logicalDevice,
					(uint32)DefaultPoolSize.ToBytes(),
					memoryTypeIndex,
					MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress
				);
				if (UNLIKELY(!block.m_memory.IsValid()))
				{
					m_memoryTypes[memoryTypeIndex].m_blocks.Remove(&block);
				}
			}
		}
	}

	DeviceMemoryPool::~DeviceMemoryPool()
	{
		if constexpr (ENABLE_ASSERTS)
		{
			for (const MemoryType& memoryType : m_memoryTypes)
			{
				for ([[maybe_unused]] const Block& block : memoryType.m_blocks)
				{
					Assert(!block.m_memory.IsValid(), "Destroy must have been called");
				}
			}
		}
	}

	void DeviceMemoryPool::Destroy(const LogicalDeviceView logicalDevice)
	{
		for (MemoryType& memoryType : m_memoryTypes)
		{
			Threading::UniqueLock lock(memoryType.m_blockMutex);
			for (Block& block : memoryType.m_blocks)
			{
				block.m_memory.Destroy(logicalDevice);
			}

			for (DeviceMemory& deviceMemory :
			     memoryType.m_hostMappableMemoryIdentifierStorage.GetValidElementView(memoryType.m_hostMappableMemory.GetView()))
			{
				deviceMemory.Destroy(logicalDevice);
			}
		}
	}

	DeviceMemoryPool::Block::Block(
		const LogicalDeviceView logicalDevice, const uint32 size, const uint8 memoryTypeIndex, const EnumFlags<MemoryFlags> memoryFlags
	)
		: m_memory(logicalDevice, size, memoryTypeIndex, memoryFlags)
	{
		if (LIKELY(m_memory.IsValid()))
		{
			Block::Fragment::IdentifierType rootFragmentIdentifier = m_identifierStorage.AcquireIdentifier();
			Assert(rootFragmentIdentifier.IsValid());
			m_fragments[rootFragmentIdentifier] = Block::Fragment{0, size};
			m_unusedFragments.Set(rootFragmentIdentifier);
			m_availableSpace = size;
		}
	}

	template<typename TargetType, typename SourceType>
	[[nodiscard]] TargetType CheckedCast(const SourceType source)
	{
		if constexpr (TypeTraits::IsPrimitive<SourceType>)
		{
			static_assert(TypeTraits::IsPrimitive<TargetType>);
			Assert(source >= Math::NumericLimits<TargetType>::Min && source <= Math::NumericLimits<TargetType>::Max);
			return static_cast<TargetType>(source);
		}
		else
		{
			static_unreachable("Conversion is not implemented for types");
		}
	}

	[[nodiscard]] DeviceMemoryPool::Allocation TryAllocate(
		DeviceMemoryPool::Block& block,
		const typename DeviceMemoryAllocation::BlockSizeType blockIndex,
		const uint8 memoryTypeIndex,
		const uint32 allocationSize,
		const uint32 alignment
	)
	{
		using Block = DeviceMemoryPool::Block;
		using Allocation = DeviceMemoryPool::Allocation;

		Allocation newAllocation;
		for (const Block::Fragment::SizeType fragmentIndex : block.m_unusedFragments.GetSetBitsIterator())
		{
			const Block::Fragment::IdentifierType fragmentIdentifier = Block::Fragment::IdentifierType::MakeFromValidIndex(fragmentIndex);
			Block::Fragment::IdentifierType unusedFragmentIdentifier = block.m_identifierStorage.GetActiveIdentifier(fragmentIdentifier);
			Assert(unusedFragmentIdentifier.GetFirstValidIndex() == fragmentIndex);
			Assert(unusedFragmentIdentifier.GetIndex() == fragmentIdentifier.GetIndex());
			Block::Fragment& unusedFragment = block.m_fragments[unusedFragmentIdentifier];

			const typename Block::SizeType neededPadding = Memory::Align(unusedFragment.m_offset, alignment) - unusedFragment.m_offset;
			const uint32 actualAllocationSize = allocationSize + neededPadding;

			if ((unusedFragment.m_size > actualAllocationSize))
			{
				const Block::Fragment::IdentifierType newFragmentIdentifier = block.m_identifierStorage.AcquireIdentifier();
				Assert(newFragmentIdentifier.IsValid());
				Block::Fragment& newFragment = block.m_fragments[newFragmentIdentifier] = Block::Fragment{
					unusedFragment.m_offset,
					actualAllocationSize,
					unusedFragment.m_previousFragmentIdentifierIndex,
					unusedFragmentIdentifier.GetIndex()
				};

				if (newFragment.m_previousFragmentIdentifierIndex != 0)
				{
					block.m_fragments[newFragment.GetPreviousFragmentIdentifier()].m_nextFragmentIdentifierIndex = newFragmentIdentifier.GetIndex();
				}

				Assert(!block.m_usedFragments.IsSet(newFragmentIdentifier));
				Assert(!block.m_unusedFragments.IsSet(newFragmentIdentifier));
				block.m_usedFragments.Set(newFragmentIdentifier);

				unusedFragment.m_previousFragmentIdentifierIndex = newFragmentIdentifier.GetIndex();

				newAllocation =
					Allocation{block.m_memory, unusedFragment.m_offset, actualAllocationSize, newFragmentIdentifier, memoryTypeIndex, blockIndex};

				unusedFragment.m_offset += actualAllocationSize;
				unusedFragment.m_size -= actualAllocationSize;

				Assert(newFragment.m_nextFragmentIdentifierIndex == unusedFragmentIdentifier.GetIndex());
				Assert(unusedFragment.m_previousFragmentIdentifierIndex == newFragmentIdentifier.GetIndex());

				block.m_availableSpace -= actualAllocationSize;

				if constexpr (ENABLE_ASSERTS)
				{
					if (unusedFragment.m_previousFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& previousFragment = block.m_fragments[unusedFragment.GetPreviousFragmentIdentifier()];
						Assert(previousFragment.m_nextFragmentIdentifierIndex == unusedFragmentIdentifier.GetIndex());
					}

					if (unusedFragment.m_nextFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& nextFragment = block.m_fragments[unusedFragment.GetNextFragmentIdentifier()];
						Assert(nextFragment.m_previousFragmentIdentifierIndex == unusedFragmentIdentifier.GetIndex());
					}
				}

				if constexpr (ENABLE_ASSERTS)
				{
					if (newFragment.m_previousFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& previousFragment = block.m_fragments[newFragment.GetPreviousFragmentIdentifier()];
						Assert(previousFragment.m_nextFragmentIdentifierIndex == newFragmentIdentifier.GetIndex());
					}
					if (newFragment.m_nextFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& nextFragment = block.m_fragments[newFragment.GetNextFragmentIdentifier()];
						Assert(nextFragment.m_previousFragmentIdentifierIndex == newFragmentIdentifier.GetIndex());
					}
				}

				break;
			}
			else if ((unusedFragment.m_size == allocationSize) & Memory::IsAligned(unusedFragment.m_offset, alignment))
			{
				newAllocation =
					Allocation{block.m_memory, unusedFragment.m_offset, allocationSize, unusedFragmentIdentifier, memoryTypeIndex, blockIndex};

				block.m_availableSpace -= allocationSize;

				Assert(!block.m_usedFragments.IsSet(unusedFragmentIdentifier));
				block.m_usedFragments.Set(unusedFragmentIdentifier);
				Assert(block.m_unusedFragments.IsSet(unusedFragmentIdentifier));
				block.m_unusedFragments.Clear(unusedFragmentIdentifier);

				if constexpr (ENABLE_ASSERTS)
				{
					if (unusedFragment.m_previousFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& previousFragment = block.m_fragments[unusedFragment.GetPreviousFragmentIdentifier()];
						Assert(previousFragment.m_nextFragmentIdentifierIndex == unusedFragmentIdentifier.GetIndex());
					}
					if (unusedFragment.m_nextFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& nextFragment = block.m_fragments[unusedFragment.GetNextFragmentIdentifier()];
						Assert(nextFragment.m_previousFragmentIdentifierIndex == unusedFragmentIdentifier.GetIndex());
					}
				}

				break;
			}
		}

		if constexpr (ENABLE_ASSERTS)
		{
			if (newAllocation.m_identifier.IsValid())
			{
				Assert(block.m_usedFragments.IsSet(newAllocation.m_identifier));
				Assert(!block.m_unusedFragments.IsSet(newAllocation.m_identifier));
			}
		}

		return newAllocation;
	}

	DeviceMemoryPool::Allocation DeviceMemoryPool::Allocate(
		const LogicalDeviceView logicalDevice,
		const size allocationSize,
		const uint32 alignment,
		const uint8 memoryTypeIndex,
		const EnumFlags<MemoryFlags> memoryFlags
	)
	{
		if constexpr (!DISABLE_MEMORY_POOL_USAGE)
		{
			MemoryType& memoryType = m_memoryTypes[memoryTypeIndex];

			Block::SizeType finalAllocationSize = Memory::Align(CheckedCast<Block::SizeType>(allocationSize), NewAlignment);

			Allocation allocation;
			{
				// Threading::SharedLock lock(memoryType.m_blockMutex);
				Threading::UniqueLock lock(memoryType.m_blockMutex);
				typename DeviceMemoryAllocation::BlockSizeType blockIndex;
				for (Block& block : memoryType.m_blocks)
				{
					if (block.m_availableSpace < finalAllocationSize)
					{
						continue;
					}

					blockIndex = memoryType.m_blocks.GetIteratorIndex(Memory::GetAddressOf(block));

					allocation = TryAllocate(block, blockIndex, memoryTypeIndex, finalAllocationSize, alignment);
					if (allocation.m_memory.IsValid())
					{
						lock.Unlock();
						Assert(IsAllocationValid(allocation));
						Assert(allocation.m_memory.IsValid());
						return allocation;
					}
				}
			}

			typename DeviceMemoryAllocation::BlockSizeType blockIndex;
			uint32 blockAllocationSize;
			Block* __restrict pBlock;
			do
			{
				{
					if (UNLIKELY(memoryType.m_isOutOfMemory))
					{
						return {};
					}

					Threading::UniqueLock lock(memoryType.m_blockMutex);
					blockIndex = memoryType.m_blocks.GetNextAvailableIndex();

					blockAllocationSize = Math::Max((uint32)ExtraBlockSize.ToBytes(), finalAllocationSize);
					do
					{
						pBlock = &memoryType.m_blocks
						            .EmplaceBack(logicalDevice, blockAllocationSize, memoryTypeIndex, memoryFlags | MemoryFlags::AllocateDeviceAddress);
						if (LIKELY(pBlock->m_memory.IsValid()))
						{
							break;
						}
						else
						{
							memoryType.m_blocks.PopBack();
							blockAllocationSize = Math::Max(finalAllocationSize, blockAllocationSize / 2);
						}
					} while (blockAllocationSize > finalAllocationSize);

					if (!memoryType.m_blocks.IsValidIndex(blockIndex))
					{
						memoryType.m_isOutOfMemory = true;
						return {};
					}
				}

				{
					Threading::UniqueLock lock(memoryType.m_blockMutex);
					allocation = TryAllocate(memoryType.m_blocks[blockIndex], blockIndex, memoryTypeIndex, finalAllocationSize, alignment);
				}
			} while (!allocation.m_memory.IsValid());
			Assert(IsAllocationValid(allocation));
			return allocation;
		}
		else
		{
			return AllocateRaw(logicalDevice, allocationSize, memoryTypeIndex, memoryFlags);
		}
	}

	void DeviceMemoryPool::Deallocate(const Allocation allocation)
	{
		Assert(!DISABLE_MEMORY_POOL_USAGE);

		MemoryType& memoryType = m_memoryTypes[allocation.m_memoryTypeIndex];

		Threading::UniqueLock lock(memoryType.m_blockMutex);

		Block& block = memoryType.m_blocks[allocation.m_blockIndex];
		Block::Fragment& fragment = block.m_fragments[allocation.m_identifier];

		Assert(fragment.m_offset == allocation.m_offset);
		Assert(fragment.m_size == allocation.m_size);

		block.m_availableSpace += allocation.m_size;

		Assert(!block.m_unusedFragments.IsSet(allocation.m_identifier));
		Assert(block.m_usedFragments.IsSet(allocation.m_identifier));
		block.m_usedFragments.Clear(allocation.m_identifier);

		if (fragment.m_previousFragmentIdentifierIndex != 0 && !block.m_usedFragments.IsSet(fragment.GetPreviousFragmentIdentifier()))
		{
			Block::Fragment& previousFragment = block.m_fragments[fragment.GetPreviousFragmentIdentifier()];
			previousFragment.m_size += fragment.m_size;

			Assert(previousFragment.m_nextFragmentIdentifierIndex == allocation.m_identifier.GetIndex());

			if (fragment.m_nextFragmentIdentifierIndex != 0 && !block.m_usedFragments.IsSet(fragment.GetNextFragmentIdentifier()))
			{
				const Block::Fragment& nextFragment = block.m_fragments[fragment.GetNextFragmentIdentifier()];
				previousFragment.m_size += nextFragment.m_size;

				Assert(block.m_unusedFragments.IsSet(fragment.GetNextFragmentIdentifier()));
				block.m_unusedFragments.Clear(fragment.GetNextFragmentIdentifier());
				block.m_identifierStorage.ReturnIdentifier(fragment.GetNextFragmentIdentifier());

				Assert(nextFragment.m_previousFragmentIdentifierIndex == allocation.m_identifier.GetIndex());

				previousFragment.m_nextFragmentIdentifierIndex = nextFragment.m_nextFragmentIdentifierIndex;
				if (previousFragment.m_nextFragmentIdentifierIndex != 0)
				{
					block.m_fragments[previousFragment.GetNextFragmentIdentifier()].m_previousFragmentIdentifierIndex =
						fragment.m_previousFragmentIdentifierIndex;
				}

				if constexpr (ENABLE_ASSERTS)
				{
					if (previousFragment.m_previousFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& previousFragmentPrevious =
							block.m_fragments[previousFragment.GetPreviousFragmentIdentifier()];
						Assert(previousFragmentPrevious.m_nextFragmentIdentifierIndex == fragment.m_previousFragmentIdentifierIndex);
					}
					if (previousFragment.m_nextFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& nextFragmentPrevious = block.m_fragments[previousFragment.GetNextFragmentIdentifier()];
						Assert(nextFragmentPrevious.m_previousFragmentIdentifierIndex == fragment.m_previousFragmentIdentifierIndex);
					}
				}
			}
			else
			{
				previousFragment.m_nextFragmentIdentifierIndex = fragment.m_nextFragmentIdentifierIndex;
				if (previousFragment.m_nextFragmentIdentifierIndex != 0)
				{
					block.m_fragments[previousFragment.GetNextFragmentIdentifier()].m_previousFragmentIdentifierIndex =
						fragment.m_previousFragmentIdentifierIndex;
				}

				if constexpr (ENABLE_ASSERTS)
				{
					if (previousFragment.m_previousFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& previousFragmentPrevious =
							block.m_fragments[previousFragment.GetPreviousFragmentIdentifier()];
						Assert(previousFragmentPrevious.m_nextFragmentIdentifierIndex == fragment.m_previousFragmentIdentifierIndex);
					}
					if (previousFragment.m_nextFragmentIdentifierIndex != 0)
					{
						[[maybe_unused]] const Block::Fragment& nextFragmentPrevious = block.m_fragments[previousFragment.GetNextFragmentIdentifier()];
						Assert(nextFragmentPrevious.m_previousFragmentIdentifierIndex == fragment.m_previousFragmentIdentifierIndex);
					}
				}
			}

			Assert(!block.m_unusedFragments.IsSet(allocation.m_identifier));
			block.m_identifierStorage.ReturnIdentifier(allocation.m_identifier);
		}
		else if (fragment.m_nextFragmentIdentifierIndex != 0 && !block.m_usedFragments.IsSet(fragment.GetNextFragmentIdentifier()))
		{
			Block::Fragment& nextFragment = block.m_fragments[fragment.GetNextFragmentIdentifier()];
			fragment.m_size += nextFragment.m_size;

			Assert(nextFragment.m_previousFragmentIdentifierIndex == allocation.m_identifier.GetIndex());

			Assert(block.m_unusedFragments.IsSet(fragment.GetNextFragmentIdentifier()));
			block.m_unusedFragments.Clear(fragment.GetNextFragmentIdentifier());
			block.m_identifierStorage.ReturnIdentifier(fragment.GetNextFragmentIdentifier());
			Assert(block.m_identifierStorage.IsIdentifierPotentiallyValid(allocation.m_identifier));
			Assert(!block.m_unusedFragments.IsSet(allocation.m_identifier));
			block.m_unusedFragments.Set(allocation.m_identifier);
			fragment.m_nextFragmentIdentifierIndex = nextFragment.m_nextFragmentIdentifierIndex;
			if (fragment.m_nextFragmentIdentifierIndex != 0)
			{
				block.m_fragments[fragment.GetNextFragmentIdentifier()].m_previousFragmentIdentifierIndex = allocation.m_identifier.GetIndex();
			}

			if constexpr (ENABLE_ASSERTS)
			{
				if (fragment.m_previousFragmentIdentifierIndex != 0)
				{
					[[maybe_unused]] const Block::Fragment& previousFragment = block.m_fragments[fragment.GetPreviousFragmentIdentifier()];
					Assert(previousFragment.m_nextFragmentIdentifierIndex == allocation.m_identifier.GetIndex());
				}
				if (fragment.m_nextFragmentIdentifierIndex != 0)
				{
					[[maybe_unused]] const Block::Fragment& nextFragmentNext = block.m_fragments[fragment.GetNextFragmentIdentifier()];
					Assert(nextFragmentNext.m_previousFragmentIdentifierIndex == allocation.m_identifier.GetIndex());
				}
			}
		}
		else
		{
			if constexpr (ENABLE_ASSERTS)
			{
				if (fragment.m_previousFragmentIdentifierIndex != 0)
				{
					[[maybe_unused]] const Block::Fragment& previousFragment = block.m_fragments[fragment.GetPreviousFragmentIdentifier()];
					Assert(previousFragment.m_nextFragmentIdentifierIndex == allocation.m_identifier.GetIndex());
				}
				if (fragment.m_nextFragmentIdentifierIndex != 0)
				{
					[[maybe_unused]] const Block::Fragment& nextFragment = block.m_fragments[fragment.GetNextFragmentIdentifier()];
					Assert(nextFragment.m_previousFragmentIdentifierIndex == allocation.m_identifier.GetIndex());
				}
			}

			Assert(block.m_identifierStorage.IsIdentifierPotentiallyValid(allocation.m_identifier));
			Assert(!block.m_unusedFragments.IsSet(allocation.m_identifier));
			block.m_unusedFragments.Set(allocation.m_identifier);
		}
	}

	DeviceMemoryPool::Allocation DeviceMemoryPool::AllocateRaw(
		const LogicalDeviceView logicalDevice, size allocationSize, const uint8 memoryTypeIndex, const EnumFlags<MemoryFlags> memoryFlags
	)
	{
		MemoryType& memoryType = m_memoryTypes[memoryTypeIndex];

		Block::Fragment::IdentifierType fragmentIdentifier = memoryType.m_hostMappableMemoryIdentifierStorage.AcquireIdentifier();
		if (LIKELY(fragmentIdentifier.IsValid()))
		{
			Assert(!memoryType.m_hostMappableMemory[fragmentIdentifier].IsValid());
			const DeviceMemoryView deviceMemory = memoryType.m_hostMappableMemory[fragmentIdentifier] =
				DeviceMemory(logicalDevice, allocationSize, memoryTypeIndex, memoryFlags);

			return Allocation{deviceMemory, 0, (uint32)allocationSize, fragmentIdentifier, memoryTypeIndex};
		}
		return Allocation{};
	}

	void DeviceMemoryPool::DeallocateRaw(const LogicalDeviceView logicalDevice, const Allocation allocation)
	{
		MemoryType& memoryType = m_memoryTypes[allocation.m_memoryTypeIndex];
		Assert((DeviceMemoryView)memoryType.m_hostMappableMemory[allocation.m_identifier] == allocation.m_memory);
		memoryType.m_hostMappableMemory[allocation.m_identifier].Destroy(logicalDevice);
		Assert(!memoryType.m_hostMappableMemory[allocation.m_identifier].IsValid());

		memoryType.m_hostMappableMemoryIdentifierStorage.ReturnIdentifier(allocation.m_identifier);
	}

	bool DeviceMemoryPool::IsAllocationValid(const Allocation allocation) const
	{
		if (!m_memoryTypes.IsValidIndex(allocation.m_memoryTypeIndex))
		{
			return false;
		}

		const MemoryType& memoryType = m_memoryTypes[allocation.m_memoryTypeIndex];
		Threading::SharedLock lock(memoryType.m_blockMutex);

		if (!memoryType.m_blocks.IsValidIndex(allocation.m_blockIndex))
		{
			return false;
		}

		const Block& block = memoryType.m_blocks[allocation.m_blockIndex];
		if (!block.m_fragments.GetView().IsValidIndex(allocation.m_blockIndex))
		{
			return false;
		}

		const Block::Fragment& fragment = block.m_fragments[allocation.m_identifier];

		if (fragment.m_size != allocation.m_size)
		{
			return false;
		}

		if (fragment.m_offset != allocation.m_offset)
		{
			return false;
		}

		if (!block.m_usedFragments.IsSet(allocation.m_identifier))
		{
			return false;
		}
		if (block.m_unusedFragments.IsSet(allocation.m_identifier))
		{
			return false;
		}

		return true;
	}

	bool DeviceMemoryPool::IsRawAllocationValid(const Allocation allocation) const
	{
		if (!m_memoryTypes.IsValidIndex(allocation.m_memoryTypeIndex))
		{
			return false;
		}

		const MemoryType& memoryType = m_memoryTypes[allocation.m_memoryTypeIndex];
		if (!memoryType.m_hostMappableMemoryIdentifierStorage.IsIdentifierPotentiallyValid(allocation.m_identifier))
		{
			return false;
		}

		return memoryType.m_hostMappableMemory[allocation.m_identifier].IsValid();
	}
}
