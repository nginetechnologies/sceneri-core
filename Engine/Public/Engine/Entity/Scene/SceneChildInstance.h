#pragma once

#include <Engine/Entity/Data/Component.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Entity
{
	struct SceneChildInstance final : public Entity::Data::Component
	{
		SceneChildInstance() = default;
		SceneChildInstance(const Deserializer& deserializer)
			: m_sourceTemplateInstanceGuid(*deserializer.m_reader.Read<Guid>("templateInstanceGuid"))
			, m_parentTemplateInstanceGuid(m_sourceTemplateInstanceGuid)
			, m_flags(deserializer.m_reader.ReadWithDefaultValue<Flags>("flags", {}))
		{
		}
		SceneChildInstance(const SceneChildInstance& templateComponent, const Cloner&)
			: m_sourceTemplateInstanceGuid(templateComponent.m_sourceTemplateInstanceGuid)
			, m_parentTemplateInstanceGuid(templateComponent.m_parentTemplateInstanceGuid)
			, m_flags(templateComponent.m_flags)
		{
		}
		SceneChildInstance(const Guid templateInstanceGuid, const Guid parentTemplateInstanceGuid)
			: m_sourceTemplateInstanceGuid(templateInstanceGuid)
			, m_parentTemplateInstanceGuid(parentTemplateInstanceGuid)
		{
		}

		Guid m_sourceTemplateInstanceGuid;
		Guid m_parentTemplateInstanceGuid;

		enum class Flags : uint8
		{
			NoTemplateOverrides = 1 << 0,
			Removed = 1 << 1
		};
		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(SceneChildInstance::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::SceneChildInstance>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::SceneChildInstance>(
			"{1C6D4883-40C7-476D-95EE-90A82135974B}"_guid,
			MAKE_UNICODE_LITERAL("Scene Child Instance"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Template Instance Guid"),
					"templateInstanceGuid",
					"{DFD78D18-B39B-4FB6-A3F2-1314D5A00A2F}"_guid,
					MAKE_UNICODE_LITERAL("Scene Child Instance"),
					&Entity::SceneChildInstance::m_sourceTemplateInstanceGuid
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Flags"),
					"flags",
					"{98145AA6-4FE7-4FA5-8134-1D80F63615B6}"_guid,
					MAKE_UNICODE_LITERAL("Scene Child Instance"),
					&Entity::SceneChildInstance::m_flags
				)
			}
		);
	};
}
