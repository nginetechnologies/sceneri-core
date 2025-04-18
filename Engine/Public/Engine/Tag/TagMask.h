#pragma once

#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

#include <Engine/Tag/TagIdentifier.h>

#include <Engine/DataSource/GenericIdentifier.h>

namespace ngine::Tag
{
	struct StageCache;
	struct Registry;

	//! A mask matching any number of tags
	//! Used to filter assets or components
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

		bool Serialize(const Serialization::Reader, Registry& registry);
		bool Serialize(Serialization::Writer, const Registry& registry) const;
	};

	struct AllowedMask : public Mask
	{
		using Mask::Mask;
		using Mask::operator=;
	};

	struct RequiredMask : public Mask
	{
		using Mask::Mask;
		using Mask::operator=;
	};

	struct DisallowedMask : public Mask
	{
		using Mask::Mask;
		using Mask::operator=;
	};

	struct Query
	{
		[[nodiscard]] inline bool IsActive() const
		{
			return m_allowedItems.IsValid() || m_disallowedItems.IsValid() || m_requiredFilterMask.AreAnySet() ||
			       m_disallowedFilterMask.AreAnySet() || m_allowedFilterMask.AreAnySet();
		}

		Tag::AllowedMask m_allowedFilterMask;
		Tag::RequiredMask m_requiredFilterMask;
		Tag::DisallowedMask m_disallowedFilterMask;
		Optional<DataSource::GenericDataMask> m_allowedItems;
		Optional<DataSource::GenericDataMask> m_disallowedItems;
	};
}
