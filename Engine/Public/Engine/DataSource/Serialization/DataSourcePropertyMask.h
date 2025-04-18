#pragma once

#include "../DataSourcePropertyMask.h"
#include "../DataSourceCache.h"

#include <Common/Serialization/Reader.h>

namespace ngine::DataSource
{
	inline bool PropertyMask::Serialize(const Serialization::Reader serializer, Cache& cache)
	{
		for (const Serialization::Reader elementSerializer : serializer.GetArrayView())
		{
			if (const Optional<ConstStringView> propertyName = elementSerializer.ReadInPlace<ConstStringView>())
			{
				ConstStringView value = *propertyName;
				if (value[0] == '{' && value.GetLastElement() == '}')
				{
					// Remove bracket
					value++;
					value--;

					const Identifier identifier = cache.FindOrRegisterPropertyIdentifier(value);
					Assert(identifier.IsValid());
					Set(identifier);
				}
			}
			else if (const Optional<PropertyIdentifier::IndexType> propertyIndex = elementSerializer.ReadInPlace<PropertyIdentifier::IndexType>())
			{
				Set(PropertyIdentifier::MakeFromValidIndex(*propertyIndex));
			}
		}

		return true;
	}
}
