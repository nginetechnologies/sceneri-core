#pragma once

#include "DataSourcePropertyIdentifier.h"
#include "PropertySourceIdentifier.h"
#include "ForwardDeclarations/PropertyValue.h"

#include <Common/Memory/ForwardDeclarations/Any.h>
#include <Common/Storage/Identifier.h>

namespace ngine::PropertySource
{
	using DataSource::PropertyIdentifier;
	using DataSource::PropertyValue;
	using Identifier = DataSource::PropertyIdentifier;

	//! Interface to a property source implementation
	//! A property source contains any number of properties that can be queried
	struct Interface
	{
		using PropertyValue = PropertySource::PropertyValue;

		Interface(const Identifier identifier)
			: m_identifier(identifier)
		{
		}
		virtual ~Interface() = default;

		[[nodiscard]] Identifier GetIdentifier() const
		{
			return m_identifier;
		}

		//! Called before starting access to lock changing of data
		virtual void LockRead()
		{
		}
		//! Called after finishing access to unlock changing of data
		virtual void UnlockRead()
		{
		}

		[[nodiscard]] virtual PropertyValue GetDataProperty(const PropertyIdentifier identifier) const = 0;
	protected:
		void OnDataChanged();
	protected:
		Identifier m_identifier;
	};
}
