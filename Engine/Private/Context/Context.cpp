#include "Context/Context.h"
#include "Context/Reference.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Common/Reflection/Registry.inl>

namespace ngine::Context::Data
{
	Component::Component(Initializer&&)
	{
	}

	Component::Component(const Component& templateComponent, const Cloner&)
		: m_lookupMap(Memory::Reserve, templateComponent.m_lookupMap.GetSize())
	{
	}

	Component::~Component() = default;

	Guid Component::GetOrEmplaceGuid(const Guid guid)
	{
		Threading::UniqueLock lock(m_mutex);
		auto it = m_lookupMap.Find(guid);
		if (it == m_lookupMap.end())
		{
			it = m_lookupMap.Emplace(Guid(guid), Guid::Generate());
		}

		return it->second;
	}

	Guid Component::FindGuid(const Guid guid) const
	{
		Threading::SharedLock lock(m_mutex);
		auto it = m_lookupMap.Find(guid);
		if (it != m_lookupMap.end())
		{
			return it->second;
		}

		return {};
	}

	Reference::Reference(Initializer&& initializer)
		: BaseType()
		, m_componentIdentifier(initializer.m_componentIdentifier)
	{
	}
	Reference::~Reference() = default;

	Optional<Component*> Reference::GetComponent(Entity::SceneRegistry& registry) const
	{
		return registry.FindComponentTypeData<Context::Data::Component>()->GetComponentImplementation(m_componentIdentifier);
	}

	[[maybe_unused]] const bool wasContextTypeRegistered = Reflection::Registry::RegisterType<Context::Data::Component>();
	[[maybe_unused]] const bool wasContextComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Context::Data::Component>>::Make());
	[[maybe_unused]] const bool wasContextReferenceTypeRegistered = Reflection::Registry::RegisterType<Context::Data::Reference>();
	[[maybe_unused]] const bool wasContextReferenceComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Context::Data::Reference>>::Make());
}
