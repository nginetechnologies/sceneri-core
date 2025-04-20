#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/ContentAreaChangeFlags.h>

#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/CachedQuery.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/SortingOrder.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>
#include <Common/Function/Callback.h>

namespace ngine::Widgets::Data
{
	struct ContentAreaListener : public Widgets::Data::Component
	{
		using BaseType = Widgets::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 8>;

		virtual ~ContentAreaListener()
		{
		}

		virtual void OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags> changeFlags) = 0;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ContentAreaListener>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ContentAreaListener>(
			"02d4df63-a2a0-42e2-9832-071dd8fb47ba"_guid,
			MAKE_UNICODE_LITERAL("Widget Data Source Consumer"),
			TypeFlags::IsAbstract | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableWriteToDisk
		);
	};
}
