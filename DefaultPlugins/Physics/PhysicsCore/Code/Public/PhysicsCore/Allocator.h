#pragma once

#include <PhysicsCore/3rdparty/jolt/Core/TempAllocator.h>
#include <Common/Memory/Align.h>
#include <Common/Memory/Allocators/Allocate.h>

namespace ngine::Physics::Internal
{
	/// Default implementation of the temp allocator that allocates a large block through malloc upfront
	class TempAllocator final : public JPH::TempAllocator
	{
	public:
		/// Constructs the allocator with a maximum allocatable size of inSize
		explicit TempAllocator(const uint32 inSize)
			: m_base(static_cast<uint8*>(Memory::AllocateAligned(inSize, 16)))
			, m_size(inSize)
		{
		}

		/// Destructor, frees the block
		virtual ~TempAllocator() override
		{
			Assert(m_top == 0);
			Memory::DeallocateAligned(m_base, 16);
		}

		// See: TempAllocator
		virtual void* Allocate(const uint32 inSize) override
		{
			if (inSize == 0)
			{
				return nullptr;
			}
			else
			{
				uint32 new_top = m_top + Memory::Align(inSize, 16);
				if (new_top > m_size)
				{
					Assert(false, "Physics ran out of memory!");
					return nullptr;
				}
				void* address = m_base + m_top;
				m_top = new_top;
				return address;
			}
		}

		// See: TempAllocator
		virtual void Free(void* inAddress, const uint32 inSize) override
		{
			if (inAddress == nullptr)
			{
				Assert(inSize == 0);
			}
			else
			{
				m_top -= Memory::Align(inSize, 16);
				if (m_base + m_top != inAddress)
				{
					Assert(false, "Freeing in the wrong order");
				}
			}
		}
	private:
		uint8* m_base;    ///< Base address of the memory block
		uint32 m_size;    ///< Size of the memory block
		uint32 m_top = 0; ///< Current top of the stack
	};
}
