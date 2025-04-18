#pragma once

#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

#include "Identifier.h"

namespace ngine::Asset
{
	struct Manager;

	struct Mask : public IdentifierMask<Identifier>
	{
		using BaseType = IdentifierMask<Identifier>;
		using BaseType::BaseType;
		using BaseType::operator=;
		Mask(const BaseType& other)
			: BaseType(other)
		{
		}
		Mask& operator=(const BaseType& other)
		{
			BaseType::operator=(other);
			return *this;
		}
		Mask(const Mask&) = default;
		Mask& operator=(const Mask&) = default;
		Mask(Mask&&) = default;
		Mask& operator=(Mask&&) = default;

		bool Serialize(const Serialization::Reader, Manager& registry);
		bool Serialize(Serialization::Writer, const Manager& registry) const;
	};
}
