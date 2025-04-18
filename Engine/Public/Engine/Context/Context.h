#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/HierarchyComponentBase.h>

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Context::Data
{
	struct Component : public Entity::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 12>;
		using BaseType = Entity::Data::Component;

		[[nodiscard]] Guid FindGuid(const Guid guid) const;
		[[nodiscard]] Guid GetOrEmplaceGuid(const Guid guid);

		Component(Initializer&& initializer);
		Component(const Component& templateContext, const Cloner& cloner);
		virtual ~Component();
	protected:
		friend struct Reflection::ReflectedType<Component>;

		UnorderedMap<Guid, Guid, Guid::Hash> m_lookupMap;
		mutable Threading::SharedMutex m_mutex;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Context::Data::Component>
	{
		inline static constexpr auto Type = Reflection::Reflect<Context::Data::Component>(
			"e76b5537-0514-4393-9bb3-c06147077145"_guid, MAKE_UNICODE_LITERAL("Context"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
