#pragma once

#include "GenericIdentifier.h"
#include <Common/Memory/Containers/ForwardDeclarations/Vector.h>
#include <Common/Storage/Identifier.h>

namespace ngine::DataSource
{
	using GenericDataIndex = GenericDataIdentifier::IndexType;

	//! Represents a cached DataSource::Query, containing all matches
	//! Note: This is only valid during a LockRead / UnlockRead context, or between OnDataChanged events.
	using CachedQuery = GenericDataMask;

	//! Represents a sorted cached DataSource::Query, containing all matches in the requested order
	//! Note: This is only valid during a LockRead / LockWrite context, or between OnDataChanged events.
	using SortedQueryIndices = Vector<GenericDataIndex, GenericDataIndex>;
}
