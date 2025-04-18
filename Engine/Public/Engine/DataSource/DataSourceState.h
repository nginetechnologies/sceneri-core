#pragma once

#include "DataSourceStateIdentifier.h"
#include "DataSourceIdentifier.h"
#include "ForwardDeclarations/PropertyValue.h"
#include "DataSourcePropertyIdentifier.h"

#include <Common/Storage/Identifier.h>
#include <Common/Memory/ForwardDeclarations/AnyView.h>

namespace ngine::DataSource
{
	using Data = ConstAnyView;

	//! Interface to a state within a data source
	//! Commonly used to represent selection state
	struct State
	{
		using Identifier = StateIdentifier;

		State(const Guid stateGuid, const DataSource::Identifier dataSourceIDentifier);
		virtual ~State();

		[[nodiscard]] virtual PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const = 0;

		[[nodiscard]] Identifier GetIdentifier() const
		{
			return m_identifier;
		}

		void OnDataChanged();
	protected:
		const Identifier m_identifier;
		DataSource::Identifier m_dataSourceIdentifier;
	};
}
