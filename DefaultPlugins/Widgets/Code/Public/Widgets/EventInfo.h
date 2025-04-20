#pragma once

#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Common/Memory/Variant.h>
#include <Common/Guid.h>

namespace ngine::Widgets
{
	enum class EventInstanceGuidType : uint8
	{
		ThisWidgetParentAssetRootInstanceGuid,
		ThisWidgetAssetRootInstanceGuid,
		ThisWidgetParentInstanceGuid,
		ThisWidgetInstanceGuid
	};
	using EventInfo = Variant<Guid, EventInstanceGuidType, ngine::DataSource::PropertyIdentifier>;
}
