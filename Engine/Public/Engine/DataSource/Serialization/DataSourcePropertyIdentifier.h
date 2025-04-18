#pragma once

#include "../DataSourcePropertyIdentifier.h"
#include "../DataSourceCache.h"

#include <Common/Serialization/Reader.h>

namespace ngine::DataSource
{
	inline bool Serialize(PropertyIdentifier& identifier, const Serialization::Reader serializer, Cache& cache)
	{
		ConstStringView propertyName = serializer.ReadInPlaceWithDefaultValue<ConstStringView>({});
		if (propertyName.GetSize() > 1)
		{
			// Remove brackets if present
			propertyName += propertyName[0] == '{';
			propertyName -= propertyName.GetLastElement() == '}';
		}

		identifier = cache.FindOrRegisterPropertyIdentifier(propertyName);
		return identifier.IsValid();
	}
}
