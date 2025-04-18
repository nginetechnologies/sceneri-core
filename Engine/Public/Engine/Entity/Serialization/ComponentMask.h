#pragma once

#include "../ComponentMask.h"
#include "../Component.h"
#include "ComponentSoftReference.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Entity
{
	inline bool ComponentMask::Serialize(const Serialization::Reader serializer, Manager& manager)
	{
		for (const Serialization::Reader referenceSerializer : serializer.GetArrayView())
		{
			ComponentSoftReference softReference;
			if (softReference.SerializeInPlace(serializer, componentRegistry, sceneRegistry))
			{
			}

			Asset::Guid guid;
			if (guidSerializer.SerializeInPlace(guid))
			{
				const Identifier identifier = manager.GetAssetIdentifier(guid);
				Assert(identifier.IsValid());
				if (LIKELY(identifier.IsValid()))
				{
					Set(identifier);
				}
			}
		}

		return true;
	}

	inline bool ComponentMask::Serialize(Serialization::Writer serializer, const Manager& manager) const
	{
		if (BaseType::AreNoneSet())
		{
			return false;
		}

		using BitIndexType = typename BaseType::BitIndexType;
		const BitIndexType numSetBits = GetNumberOfSetBits();
		if (numSetBits == 0)
		{
			return false;
		}

		Serialization::Value& value = serializer.GetValue();
		value = Serialization::Value(rapidjson::Type::kArrayType);
		value.Reserve((rapidjson::SizeType)numSetBits, serializer.GetDocument().GetAllocator());

		IterateSetBits(
			[&manager, &value, &serializer](const BitIndexType index)
			{
				const Guid guid = manager.GetAssetGuid(Identifier::MakeFromValidIndex(index));

				Serialization::Value guidValue;
				Serialization::Writer writer(guidValue, serializer.GetData());
				Assert(guid.IsValid());
				if (LIKELY(writer.SerializeInPlace(guid)))
				{
					value.PushBack(Move(guidValue), serializer.GetDocument().GetAllocator());
				}
				return true;
			}
		);

		return value.Size() > 0;
	}
}
