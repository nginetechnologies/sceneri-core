#pragma once

#include "EventInfo.h"

#include <Engine/Event/Identifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyMask.h>
#include <Engine/Tag/TagMask.h>
#include <Engine/Asset/Mask.h>
#include <Engine/Entity/ComponentSoftReferences.h>

#include <Common/IO/URI.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Guid.h>

namespace ngine::Widgets
{
	struct Widget;
	struct DataSourceProperties;

	struct EventData
	{
		struct Tags
		{
			InlineVector<Tag::Identifier::IndexType, 1> tags;
			DataSource::PropertyMask dynamicPropertyMask;
			InlineVector<Tag::Identifier::IndexType, 1> dynamicTags;
		};
		struct Assets
		{
			InlineVector<Asset::Identifier::IndexType, 1> assets;
			DataSource::PropertyMask dynamicPropertyMask;
			InlineVector<Asset::Identifier::IndexType, 1> dynamicAssets;
		};
		struct Identifiers
		{
			InlineVector<DataSource::GenericDataIdentifier::IndexType, 1> data;
			DataSource::PropertyMask dynamicPropertyMask;
			InlineVector<DataSource::GenericDataIdentifier::IndexType, 1> dynamicData;
		};
		struct Components
		{
			Entity::ComponentSoftReferences componentSoftReferences;
			DataSource::PropertyMask dynamicPropertyMask;
			Entity::ComponentSoftReferences dynamicComponentSoftReferences;
		};
		struct URIs
		{
			InlineVector<IO::URI, 1> m_uris;
			DataSource::PropertyMask dynamicPropertyMask;
		};
		struct Strings
		{
			InlineVector<UnicodeString, 1> m_strings;
		};
		struct Guids
		{
			InlineVector<Guid, 1> m_guids;
			DataSource::PropertyMask dynamicPropertyMask;
		};
		struct ThisWidget
		{
		};
		struct ParentWidget
		{
		};
		struct AssetRootWidget
		{
		};

		using ArgumentValue = Variant<
			UniquePtr<Tags>,
			UniquePtr<Assets>,
			UniquePtr<Identifiers>,
			UniquePtr<Components>,
			URIs,
			Strings,
			Guids,
			ThisWidget,
			ParentWidget,
			AssetRootWidget>;
		using ArgumentContainer = Vector<ArgumentValue, uint8>;

		EventData() = default;
		EventData(
			const Events::Identifier identifier,
			const ngine::DataSource::PropertyIdentifier dynamicPropertyIdentifier,
			const EventInfo info,
			ArgumentContainer&& arguments
		)
			: m_identifier(identifier)
			, m_dynamicPropertyIdentifier(dynamicPropertyIdentifier)
			, m_info(info)
			, m_arguments(Forward<ArgumentContainer>(arguments))
		{
		}
		EventData(const EventData& other, Widget& owner);
		EventData(EventData&&) = default;
		EventData& operator=(const EventData&) = delete;
		EventData& operator=(EventData&&) = default;

		void NotifyAll(Widget& owner) const;
		void UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties);

		Events::Identifier m_identifier;
		ngine::DataSource::PropertyIdentifier m_dynamicPropertyIdentifier;
		EventInfo m_info;

		ArgumentContainer m_arguments;
	};
}
