#include "Resource/ResourceManager.h"

#include <Common/System/Query.h>
#include <Common/IO/Log.h>

namespace ngine::Resource
{
	// TODO: Resource priority
	// On unload we should find the least prioritized assets that would free up the necessary space
	// Maybe it should be a weighted sort based on priority and size. Priority weighted first so higher prio is always further up in the list,
	// then order by size for equal priorities. Freeing should start with the lowest prioritized assets

	// The resource manager should solely care about memory, on CPU and GPU
	// We should also be able to mark assets as unused, which would make them first to unload
	// First example is when a scene or project is unloaded.

	// We should probably be able to group multiple allocations as well as both GPU and CPU usage on a higher level
	// For example, one mesh and its allocations treated as one. This way we deallocate everything at the same time instead of partially.

	// Examples:
	// Mesh = StaticObject 1 allocation for all vertex data on CPU
	// Also means that pooled allocations on both CPU and GPU can only be combined if they have the same or very similar priorities

	// Support having CPU and GPU memory budgets, and autorelease stuff if we request a resource with a higher priority when the limit has
	// been reached

	Manager::Manager()
	{
		System::Query::GetInstance().RegisterSystem(*this);
	}

	void Manager::OnMemoryRunningLow()
	{
		System::Get<Log>().Warning(SOURCE_LOCATION, "Running low on memory!");
	}
}
