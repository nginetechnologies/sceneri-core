#pragma once

#include <Common/Plugin/Plugin.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/ForwardDeclarations/Optional.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/Identifier.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/EnumFlags.h>

#include <Common/Function/Function.h>
#include <Common/Reflection/TypeDefinition.h>

#include "WidgetTypeIdentifier.h"
#include "Style/StylesheetCache.h"
#include "WidgetCache.h"

namespace ngine::Widgets
{
	struct Manager final : public Plugin
	{
		inline static constexpr Guid Guid = "AAEA95B0-9B18-442D-B043-0629481E8C95"_guid;

		Manager(Application&);
		virtual ~Manager();

		[[nodiscard]] const WidgetCache& GetWidgetCache() const
		{
			return m_widgetCache;
		}
		[[nodiscard]] WidgetCache& GetWidgetCache()
		{
			return m_widgetCache;
		}

		[[nodiscard]] const Style::StylesheetCache& GetStylesheetCache() const
		{
			return m_stylesheetCache;
		}
		[[nodiscard]] Style::StylesheetCache& GetStylesheetCache()
		{
			return m_stylesheetCache;
		}
	protected:
		WidgetCache m_widgetCache;
		Style::StylesheetCache m_stylesheetCache;
	};
}
