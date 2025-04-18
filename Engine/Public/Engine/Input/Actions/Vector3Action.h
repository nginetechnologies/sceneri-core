#pragma once

#include <Engine/Input/Actions/VectorAction.h>

namespace ngine::Input
{
	struct Vector3Action final : public VectorAction<Math::Vector3f>
	{
		using BaseType = VectorAction<Math::Vector3f>;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Input::Vector3Action>
	{
		inline static constexpr auto Type = Reflection::Reflect<Input::Vector3Action>(
			"{A596CECB-627B-49CA-90DC-E9D41A1505F0}"_guid,
			MAKE_UNICODE_LITERAL("3D Axis Action"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{
				/// TOOD: Reflect the delegates
				/*Reflection::Property
		    {
		      MAKE_UNICODE_LITERAL("On Change"),
		      MAKE_UNICODE_LITERAL("Changed"),
		      MAKE_UNICODE_LITERAL("Changed"),
		      &DirectBinaryAction::m_worldTransform
		    }*/
			},
			Reflection::Functions{Reflection::Function{
				"{A7184E89-0395-4F15-8752-A2BD9CD8DC4E}"_guid,
				MAKE_UNICODE_LITERAL("Bind Input Axis"),
				&Input::Vector3Action::BindAxisInput,
				FunctionFlags{},
				Reflection::ReturnType{"{53CFEFF6-277F-4955-879D-12126D0E18DD}"_guid, MAKE_UNICODE_LITERAL("Result")},
				Reflection::Argument{"{508246C5-CB89-4A18-966F-F81D7BBFEBF1}"_guid, MAKE_UNICODE_LITERAL("Monitor")},
				Reflection::Argument{"{8b4beacc-c184-40b3-bb22-96360952a873}"_guid, MAKE_UNICODE_LITERAL("Device Type")},
				Reflection::Argument{"{3E380004-699A-4FE1-A3A3-2EE0CC662327}"_guid, MAKE_UNICODE_LITERAL("Input Identifier")},
				Reflection::Argument{"{EC583D78-99D1-46F8-A790-2AF4843896BD}"_guid, MAKE_UNICODE_LITERAL("Direction")}
			}}
		);
	};
}
