#pragma once

#include <Engine/Tag/TagMask.h>

namespace ngine::Tag
{
	struct MaskProperty
	{
		bool Serialize(const Serialization::Reader);
		bool Serialize(Serialization::Writer) const;

		[[nodiscard]] inline bool operator==(const MaskProperty& other) const
		{
			return m_mask == other.m_mask;
		}
		[[nodiscard]] inline bool operator!=(const MaskProperty& other) const
		{
			return m_mask != other.m_mask;
		}

		Mask m_mask;
		Registry& m_registry;
	};

	//! Property that allows adding / removing tags from a component
	struct ModifiableMaskProperty : public MaskProperty
	{
		inline static constexpr Guid TypeGuid = "{9584A99A-D329-400B-A579-B4B9AA88154D}"_guid;
	};
	//! Property that exposes adding / removing tags to be queried from components
	struct QueryableMaskProperty : public MaskProperty
	{
		inline static constexpr Guid TypeGuid = "{24C89909-35A0-4C2B-B0EC-BD15C5CF5E7B}"_guid;
	};
}
