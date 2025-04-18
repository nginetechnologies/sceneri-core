#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Common/Guid.h>

namespace ngine::Entity::Data
{
	struct InstanceGuid final : public Component
	{
		InstanceGuid(const Deserializer& deserializer) = delete;
		InstanceGuid(const InstanceGuid&, const Cloner&) = delete;

		InstanceGuid(Guid guid)
			: m_guid(guid)
		{
		}

		[[nodiscard]] Guid GetGuid() const
		{
			return m_guid;
		}

		[[nodiscard]] operator Guid() const
		{
			return m_guid;
		}

		InstanceGuid& operator=(Guid guid)
		{
			m_guid = guid;
			return *this;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::InstanceGuid>;
	private:
		Guid m_guid;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::InstanceGuid>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::InstanceGuid>(
			"b62f0023-333e-44c7-a531-4a99d5998415"_guid,
			MAKE_UNICODE_LITERAL("Instance Guid"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
