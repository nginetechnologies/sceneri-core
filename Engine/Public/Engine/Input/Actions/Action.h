#pragma once

#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/Monitor.h>
#include <Common/Reflection/Type.h>

namespace ngine::Input
{
	struct ActionMonitor;
	struct DeviceType;

	struct Action : protected Monitor
	{
		Action() = default;
		Action(const Action&) = delete;
		Action& operator=(const Action&) = delete;
		Action(Action&&) = delete;
		Action& operator=(Action&&) = delete;

		void BindInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier);
		void UnbindInput(ActionMonitor& monitor, const InputIdentifier inputIdentifier);

		[[nodiscard]] virtual bool IsExclusive() const
		{
			return false;
		}
	protected:
		friend ActionMonitor;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Input::Action>
	{
		inline static constexpr auto Type = Reflection::Reflect<Input::Action>(
			"{08FBC0BD-3B0A-4899-AFFC-05C63DA52876}"_guid,
			MAKE_UNICODE_LITERAL("Action"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{Reflection::Function{
				"{38C38D54-EFC8-4FCD-82A8-37C2459C8713}"_guid,
				MAKE_UNICODE_LITERAL("Bind Input"),
				&Input::Action::BindInput,
				FunctionFlags{},
				Reflection::ReturnType{"{53CFEFF6-277F-4955-879D-12126D0E18DD}"_guid, MAKE_UNICODE_LITERAL("Result")},
				Reflection::Argument{"{508246C5-CB89-4A18-966F-F81D7BBFEBF1}"_guid, MAKE_UNICODE_LITERAL("Monitor")},
				Reflection::Argument{"{8b4beacc-c184-40b3-bb22-96360952a873}"_guid, MAKE_UNICODE_LITERAL("Device Type")},
				Reflection::Argument{"{3E380004-699A-4FE1-A3A3-2EE0CC662327}"_guid, MAKE_UNICODE_LITERAL("Input Identifier")}
			}}
		);
	};
}
