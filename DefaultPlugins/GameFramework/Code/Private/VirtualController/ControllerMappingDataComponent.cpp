#include "VirtualController/ControllerMappingDataComponent.h"

#include <Widgets/Widget.h>
#include <Widgets/Data/Component.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Reflection/TypeDeserializer.h>
#include <Common/Serialization/Reader.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Reflection/Registry.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework::Data
{
	struct GamepadInputButtonMapping
	{
		Input::GamepadInput::Button button;
		Guid instanceGuid;
	};

	static constexpr Array s_GamepadInputButtonMappings = {
		GamepadInputButtonMapping{Input::GamepadInput::Button::A, "eb5691da-422a-43df-be01-fe9381781b25"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::B, "2a84d8c4-198c-472e-828f-f8a32b0dd0b2"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::X, "d7f79723-e5b9-4fbf-856e-6f51c663fec8"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::Y, "12d84736-2f90-4b1c-bed7-926a9bdb740a"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::LeftShoulder, "5529ee63-fe56-4e5b-a268-5b75f3d182b8"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::RightShoulder, "e2080eae-6cb5-4f71-b253-80cd6e122180"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::LeftThumbstick, "71e77faa-1ac9-4681-88ac-e660539f371c"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::RightThumbstick, "81f1c8e0-f3d8-477c-8293-16e90027fe48"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::Menu, "e2559280-12b3-4b40-8773-a5fd27242625"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::Home, "22e34e4b-770b-4640-aaea-3ae3a738c8a8"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::DirectionPadLeft, "72f4886b-2fae-4f70-9258-2fbe16314922"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::DirectionPadRight, "260b52bf-3e69-41be-be64-fd5268c46bd5"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::DirectionPadUp, "a75e9e7b-0643-4c1c-97e1-1111a74d1dc3"_guid},
		GamepadInputButtonMapping{Input::GamepadInput::Button::DirectionPadDown, "84d841c5-5cea-46e3-ba67-3102bc547ceb"_guid}
	};

	static constexpr Guid s_GamepadAxisInstanceGuid = "cc712c0c-db50-465d-ae76-dd40b1586656"_guid;
	static constexpr Guid s_GamepadAnalogLeftGuid = "6dc70ae2-c2e5-4425-8575-328cb5424f6d"_guid;
	static constexpr Guid s_GamepadAnalogRightGuid = "85f23fba-1e8b-4df5-8792-2045118716c7"_guid;

	static constexpr uint32 s_axisBit = s_GamepadInputButtonMappings.GetSize();
	static constexpr uint32 s_leftTriggerBit = s_axisBit + 1;
	static constexpr uint32 s_rightTriggerBit = s_leftTriggerBit + 1;

	ControllerMappingDataComponent::ControllerMappingDataComponent(const Deserializer& deserializer)
	{
		const Vector<Guid> guids = deserializer.m_reader.ReadWithDefaultValue<Vector<Guid>>("guids", {});
		for (Guid guid : guids)
		{
			uint32 buttonBit = 0;
			for (const GamepadInputButtonMapping& mapping : s_GamepadInputButtonMappings)
			{
				if (guid == mapping.instanceGuid)
				{
					m_inputMappings.Set(buttonBit);
					break;
				}
				++buttonBit;
			}

			if (guid == s_GamepadAxisInstanceGuid)
			{
				m_inputMappings.Set(s_axisBit);
			}
			if (guid == s_GamepadAnalogLeftGuid)
			{
				m_inputMappings.Set(s_leftTriggerBit);
			}
			if (guid == s_GamepadAnalogRightGuid)
			{
				m_inputMappings.Set(s_rightTriggerBit);
			}
		}
	}

	ControllerMappingDataComponent::ControllerMappingDataComponent(const ControllerMappingDataComponent& templateComponent, const Cloner&)
		: m_inputMappings(templateComponent.m_inputMappings)
	{
	}

	Vector<Input::GamepadInput::Button> ControllerMappingDataComponent::GetButtons() const
	{
		Vector<Input::GamepadInput::Button> buttons(Memory::Reserve, (uint32)m_inputMappings.GetNumberOfSetBits());
		for (uint8 i = 0; i < s_GamepadInputButtonMappings.GetSize(); ++i)
		{
			if (m_inputMappings.IsSet(i))
			{
				buttons.EmplaceBack(s_GamepadInputButtonMappings[i].button);
			}
		}
		return buttons;
	}

	bool ControllerMappingDataComponent::HasAxisInput() const
	{
		return m_inputMappings.IsSet(s_axisBit);
	}

	bool ControllerMappingDataComponent::HasLeftTrigger() const
	{
		return m_inputMappings.IsSet(s_leftTriggerBit);
	}

	bool ControllerMappingDataComponent::HasRightTrigger() const
	{
		return m_inputMappings.IsSet(s_rightTriggerBit);
	}
}

namespace ngine
{
	[[maybe_unused]] const bool wasControllerMappingDataComponentTypeRegistered =
		Reflection::Registry::RegisterType<GameFramework::Data::ControllerMappingDataComponent>();
	[[maybe_unused]] const bool wasControllerMappingDataComponentComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<GameFramework::Data::ControllerMappingDataComponent>>::Make());
}
