/**
 @file callbacks.c
 @brief ENet callback functions
*/
#define ENET_BUILDING_LIB 1
#include "enet/enet.h"

#include <Common/Memory/Allocators/Allocate.h>

int enet_initialize_with_callbacks(ENetVersion version, const ENetCallbacks*)
{
	if (version < ENET_VERSION_CREATE(1, 3, 0))
		return -1;
	return enet_initialize();
}

ENetVersion enet_linked_version(void)
{
	return ENET_VERSION;
}

void* enet_malloc(size_t size)
{
	return ngine::Memory::Allocate(size);
}

void enet_free(void* memory)
{
	return ngine::Memory::Deallocate(memory);
}
