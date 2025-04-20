#pragma once

#include <Widgets/Data/Input.h>

namespace ngine::Widgets::Data
{
	struct Movable : public Input
	{
		using InstanceIdentifier = TIdentifier<uint32, 8>;
		using BaseType = Input;

		Movable(const Deserializer& deserializer);
		~Movable();

		[[nodiscard]] virtual PanResult
		OnStartPan(Widget& owner, const Input::DeviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&)
			override final;
		[[nodiscard]] virtual CursorResult
		OnMovePan(Widget& owner, const Input::DeviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2i delta, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&)
			override final;
		[[nodiscard]] virtual CursorResult OnEndPan(
			Widget& owner, const Input::DeviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2f velocity
		) override final;
	protected:
		friend struct Reflection::ReflectedType<Movable>;
		Widget& m_owner;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Movable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Movable>(
			"a2dec77d-e6d8-4021-b096-b96b06ecdfb4"_guid,
			MAKE_UNICODE_LITERAL("Widget Movable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
