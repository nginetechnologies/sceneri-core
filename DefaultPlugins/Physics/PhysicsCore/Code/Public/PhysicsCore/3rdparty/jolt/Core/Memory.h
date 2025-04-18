// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Common/Memory/Allocators/Allocate.h>

JPH_NAMESPACE_BEGIN

#ifndef JPH_DISABLE_CUSTOM_ALLOCATOR

/// Macro to override the new and delete functions
#define JPH_OVERRIDE_NEW_DELETE \
	JPH_INLINE void* operator new(size_t inCount) \
	{ \
		return JPH::Allocate(inCount); \
	} \
	JPH_INLINE void operator delete(void* inPointer) noexcept \
	{ \
		JPH::Free(inPointer); \
	} \
	JPH_INLINE void* operator new[](size_t inCount) \
	{ \
		return JPH::Allocate(inCount); \
	} \
	JPH_INLINE void operator delete[](void* inPointer) noexcept \
	{ \
		JPH::Free(inPointer); \
	} \
	JPH_INLINE void* operator new(size_t inCount, align_val_t inAlignment) \
	{ \
		return JPH::AlignedAllocate(inCount, static_cast<size_t>(inAlignment)); \
	} \
	JPH_INLINE void operator delete(void* inPointer, align_val_t inAlignment) noexcept \
	{ \
		JPH::AlignedFree(inPointer, static_cast<size_t>(inAlignment)); \
	} \
	JPH_INLINE void* operator new[](size_t inCount, align_val_t inAlignment) \
	{ \
		return JPH::AlignedAllocate(inCount, static_cast<size_t>(inAlignment)); \
	} \
	JPH_INLINE void operator delete[](void* inPointer, align_val_t inAlignment) noexcept \
	{ \
		JPH::AlignedFree(inPointer, static_cast<size_t>(inAlignment)); \
	}

#else

// Don't override new/delete
#define JPH_OVERRIDE_NEW_DELETE

#endif // !JPH_DISABLE_CUSTOM_ALLOCATOR

// Directly define the allocation functions
[[nodiscard]] inline void* Allocate(size_t inSize)
{
	return ngine::Memory::Allocate(inSize);
}
inline void Free(void* inBlock)
{
	ngine::Memory::Deallocate(inBlock);
}
[[nodiscard]] inline void* AlignedAllocate(size_t inSize, size_t inAlignment)
{
	return ngine::Memory::AllocateAligned(inSize, inAlignment);
}
inline void AlignedFree(void* inBlock, size_t inAlignment)
{
	ngine::Memory::DeallocateAligned(inBlock, inAlignment);
}

JPH_NAMESPACE_END
