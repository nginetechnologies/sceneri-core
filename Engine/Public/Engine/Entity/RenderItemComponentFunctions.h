#pragma once

#include <Engine/Entity/Component3D.h>

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>

namespace ngine::Entity
{
	void EnableStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier identifier);
	void DisableStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier identifier);
	void ResetStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier identifier);
	[[nodiscard]] bool IsStageEnabled(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier identifier);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Entity::EnableStage>
	{
		static constexpr auto Function = Reflection::Function{
			"91ac4e06-bb97-453e-8a4b-c234cc13d645"_guid,
			MAKE_UNICODE_LITERAL("Enable Stage"),
			&Entity::EnableStage,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"f8cd1282-4718-41e7-8954-df0169d707d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"78a0de43-3643-4c92-bd1e-0a8f044de00e"_guid, MAKE_UNICODE_LITERAL("Stage")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::DisableStage>
	{
		static constexpr auto Function = Reflection::Function{
			"f2b9c3e7-a64d-4829-b117-e20a5abc6448"_guid,
			MAKE_UNICODE_LITERAL("Disable Stage"),
			&Entity::DisableStage,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"f8cd1282-4718-41e7-8954-df0169d707d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"78a0de43-3643-4c92-bd1e-0a8f044de00e"_guid, MAKE_UNICODE_LITERAL("Stage")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::ResetStage>
	{
		static constexpr auto Function = Reflection::Function{
			"9b1ec071-b283-4110-af16-337e9dfc3eab"_guid,
			MAKE_UNICODE_LITERAL("Reset Stage"),
			&Entity::ResetStage,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"f8cd1282-4718-41e7-8954-df0169d707d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"78a0de43-3643-4c92-bd1e-0a8f044de00e"_guid, MAKE_UNICODE_LITERAL("Stage")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::IsStageEnabled>
	{
		static constexpr auto Function = Reflection::Function{
			"b2ecc6fb-b00b-4792-bae0-78663e982e4a"_guid,
			MAKE_UNICODE_LITERAL("Is Stage Enabled"),
			&Entity::IsStageEnabled,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"7fc58576-61eb-4aa8-a87f-ec619435d3b7"_guid, MAKE_UNICODE_LITERAL("Enabled")},
			Reflection::Argument{
				"f8cd1282-4718-41e7-8954-df0169d707d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"78a0de43-3643-4c92-bd1e-0a8f044de00e"_guid, MAKE_UNICODE_LITERAL("Stage")}
		};
	};
}
