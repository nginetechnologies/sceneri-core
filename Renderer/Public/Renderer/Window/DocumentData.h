#pragma once

#include <Engine/Asset/Identifier.h>

#include <Common/Memory/Variant.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>
#include <Common/Asset/Guid.h>
#include <Common/Network/Address.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Widgets
{
	struct DocumentData
	{
		using Reference = Variant<IO::Path, IO::URI, Asset::Guid, Asset::Identifier, Network::Address>;

		DocumentData() = default;

		DocumentData(Reference&& reference)
			: m_reference(Forward<Reference>(reference))
		{
		}

		DocumentData(Reference&& reference, Reference&& projectReference)
			: m_reference(Forward<Reference>(reference))
			, m_projectReference(Forward<Reference>(projectReference))
		{
		}

		[[nodiscard]] bool HasValue() const
		{
			return m_reference.HasValue();
		}

		template<typename Type>
		[[nodiscard]] Optional<const Type*> Get() const
		{
			return m_reference.Get<Type>();
		}

		template<typename Callback>
		constexpr auto Visit(Callback&& callback) const
		{
			return m_reference.Visit(Forward<Callback>(callback));
		}
		template<typename... Callbacks>
		constexpr auto Visit(Callbacks&&... callbacks) const
		{
			return m_reference.Visit(Forward<Callbacks>(callbacks)...);
		}

		//! Mandatory reference specifying where to find the document
		Reference m_reference;
		//! Optional reference to a parent that should be loaded first
		Reference m_projectReference;
	};
}
