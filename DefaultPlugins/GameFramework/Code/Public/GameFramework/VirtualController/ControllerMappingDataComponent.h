#pragma once

#include <Widgets/Data/Component.h>

#include <Engine/Input/Devices/Gamepad/GamepadInput.h>

#include <Common/Reflection/Type.h>
#include <Common/Memory/DynamicBitset.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Memory/UnicodeCharType.h>
#include <Common/Reflection/TypeInterface.h>

namespace ngine::GameFramework::Data
{
	struct ControllerMappingDataComponent final : public Widgets::Data::Component
	{
		static constexpr Guid TypeGuid = "692387ca-552c-4398-ac35-832a090a0894"_guid;

		using BaseType = Widgets::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 8>;

		ControllerMappingDataComponent(const Deserializer& deserializer);
		ControllerMappingDataComponent(const ControllerMappingDataComponent& templateComponent, const Cloner& cloner);

		ControllerMappingDataComponent(const ControllerMappingDataComponent&) = delete;
		ControllerMappingDataComponent(ControllerMappingDataComponent&&) = delete;
		ControllerMappingDataComponent& operator=(const ControllerMappingDataComponent&) = delete;
		ControllerMappingDataComponent& operator=(ControllerMappingDataComponent&&) = delete;

		Vector<Input::GamepadInput::Button> GetButtons() const;
		bool HasAxisInput() const;
		bool HasLeftTrigger() const;
		bool HasRightTrigger() const;
	private:
		DynamicBitset<uint32> m_inputMappings;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::GameFramework::Data::ControllerMappingDataComponent>
	{
		static constexpr auto Type = Reflection::Reflect<ngine::GameFramework::Data::ControllerMappingDataComponent>(
			GameFramework::Data::ControllerMappingDataComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Controller Mapping Data Component"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation
		);
	};
}
