#pragma once

#include "DataSourcePropertyIdentifier.h"

#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>

namespace ngine::DataSource
{
	struct Cache;

	struct PropertyMask : public IdentifierMask<PropertyIdentifier>
	{
		using BaseType = IdentifierMask<PropertyIdentifier>;
		using BaseType::BaseType;
		using BaseType::operator=;

		bool Serialize(const Serialization::Reader, Cache& cache);
	};
}
