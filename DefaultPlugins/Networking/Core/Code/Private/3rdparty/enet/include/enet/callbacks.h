/**
 @file  callbacks.h
 @brief ENet callbacks
*/
#ifndef __ENET_CALLBACKS_H__
#define __ENET_CALLBACKS_H__

#include <stdlib.h>

typedef struct _ENetCallbacks
{
	void* (*malloc)(size_t size);
	void (*free)(void* memory);
	void (*no_memory)(void);
} ENetCallbacks;

/** @defgroup callbacks ENet internal callbacks
    @{
    @ingroup private
*/
extern void* enet_malloc(size_t);
extern void enet_free(void*);

/** @} */

#endif /* __ENET_CALLBACKS_H__ */
