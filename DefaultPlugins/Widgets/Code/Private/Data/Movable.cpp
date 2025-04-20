#include <Widgets/Data/Movable.h>
#include <Widgets/Widget.h>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	Movable::Movable([[maybe_unused]] const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
	}
	Movable::~Movable()
	{
	}

	PanResult Movable::OnStartPan(
		Widget& owner,
		const Input::DeviceIdentifier,
		const LocalWidgetCoordinate,
		const Math::Vector2f,
		const Optional<uint16>,
		SetCursorCallback& pointer
	)
	{
		pointer.SetOverridableCursor(Rendering::CursorType::Hand);

		return PanResult{&owner, PanType::MoveWidget};
	}

	CursorResult Movable::OnMovePan(
		Widget& owner,
		const Input::DeviceIdentifier,
		const LocalWidgetCoordinate,
		const Math::Vector2i delta,
		const Math::Vector2f,
		const Optional<uint16>,
		SetCursorCallback& pointer
	)
	{
		pointer.SetOverridableCursor(Rendering::CursorType::Hand);

		owner.Reposition(owner.GetPosition() + delta);

		return CursorResult{&owner};
	}

	CursorResult Movable::OnEndPan(Widget& owner, const Input::DeviceIdentifier, const LocalWidgetCoordinate, const Math::Vector2f)
	{
		return CursorResult{&owner};
	}

	[[maybe_unused]] const bool wasMovableTypeRegistered = Reflection::Registry::RegisterType<Movable>();
	[[maybe_unused]] const bool wasMovableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Movable>>::Make());
}
