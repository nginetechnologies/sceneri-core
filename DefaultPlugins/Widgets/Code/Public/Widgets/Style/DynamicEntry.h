#pragma once

#include <Widgets/Style/Entry.h>

namespace ngine::DataSource
{
	struct Cache;
}

namespace ngine::Widgets::Style
{
	struct DynamicEntry
	{
		Style::Entry m_dynamicEntry;
		Style::Entry m_populatedEntry;
	};
}
