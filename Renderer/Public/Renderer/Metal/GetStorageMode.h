#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Devices/MemoryFlags.h>

namespace ngine::Rendering::Metal
{
#if RENDERER_METAL
	[[nodiscard]] inline MTLStorageMode GetStorageMode(const EnumFlags<MemoryFlags> memoryFlags)
	{
		if (memoryFlags.IsNotSet(MemoryFlags::HostVisible))
		{
			if (memoryFlags.IsSet(MemoryFlags::LazilyAllocated))
			{
				return MTLStorageModeMemoryless;
			}
			else
			{
				return MTLStorageModePrivate;
			}
		}
		else if (memoryFlags.IsSet(MemoryFlags::HostCoherent))
		{
			return MTLStorageModeShared;
		}
		else
		{
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			return MTLStorageModeManaged;
#else
			return MTLStorageModeShared;
#endif
		}
	}
#endif
}
