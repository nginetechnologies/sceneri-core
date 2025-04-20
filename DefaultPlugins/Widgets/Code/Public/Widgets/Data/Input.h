#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/SetCursorCallback.h>
#include <Widgets/InputResults.h>
#include <Widgets/LocalWidgetCoordinate.h>
#include <Widgets/ContextMenuBuilder.h>

#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/Actions/DeleteTextType.h>
#include <Engine/Input/Actions/MoveTextCursorFlags.h>
#include <Engine/Entity/ApplyAssetFlags.h>
#include <Engine/Entity/ApplicableData.h>

#include <Common/Math/Vector2.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::Widgets::Data
{
	struct Input : public Component
	{
		using DeviceIdentifier = ngine::Input::DeviceIdentifier;
		using SetCursorCallback = Widgets::SetCursorCallback;
		using CursorResult = Widgets::CursorResult;
		using PanResult = Widgets::PanResult;

		using InstanceIdentifier = TIdentifier<uint32, 10>;
		using BaseType = Component;

		using BaseType::BaseType;
		Input(const Input&) = delete;
		Input(Input&&) = delete;
		Input& operator=(const Input&) = delete;
		Input& operator=(Input&&) = delete;
		virtual ~Input() = default;

		[[nodiscard]] virtual CursorResult
		OnStartTap([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnEndTap([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnCancelTap([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnDoubleTap([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}
		[[nodiscard]] virtual PanResult OnStartLongPress(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			[[maybe_unused]] const Optional<uint16> touchRadius
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnMoveLongPress(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			[[maybe_unused]] const Optional<uint16> touchRadius
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnEndLongPress([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnCancelLongPress([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier)
		{
			return {};
		}
		[[nodiscard]] virtual PanResult
		OnStartPan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnMovePan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnEndPan(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2f velocity
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnCancelPan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnHover([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, SetCursorCallback&)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnStartScroll(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2i delta
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnScroll(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2i delta
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult OnEndScroll(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2f velocity
		)
		{
			return {};
		}
		[[nodiscard]] virtual CursorResult
		OnCancelScroll([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}

		[[nodiscard]] virtual ContextMenuBuilder
		OnSpawnContextMenu([[maybe_unused]] Widget& owner, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
		{
			return {};
		}

		[[nodiscard]] virtual DragAndDropResult
		OnStartDragWidgetOverThis([[maybe_unused]] Widget& owner, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&)
		{
			return {};
		}
		[[nodiscard]] virtual DragAndDropResult OnCancelDragWidgetOverThis([[maybe_unused]] Widget& owner)
		{
			return {};
		}
		[[nodiscard]] virtual DragAndDropResult
		OnMoveDragWidgetOverThis([[maybe_unused]] Widget& owner, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&)
		{
			return {};
		}
		[[nodiscard]] virtual DragAndDropResult OnReleaseDragWidgetOverThis(
			[[maybe_unused]] Widget& owner, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget
		)
		{
			return {};
		}

		virtual bool
		CanApplyAtPoint([[maybe_unused]] const Widget& owner, const Entity::ApplicableData&, [[maybe_unused]] const LocalWidgetCoordinate coordinate, const EnumFlags<Entity::ApplyAssetFlags>)
			const
		{
			return false;
		}

		virtual bool
		ApplyAtPoint([[maybe_unused]] Widget& owner, const Entity::ApplicableData&, [[maybe_unused]] const LocalWidgetCoordinate coordinate, const EnumFlags<Entity::ApplyAssetFlags>)
		{
			return false;
		}

		virtual void OnTextInput([[maybe_unused]] Widget& owner, const ConstUnicodeStringView)
		{
		}
		virtual void OnMoveTextCursor([[maybe_unused]] Widget& owner, [[maybe_unused]] const EnumFlags<ngine::Input::MoveTextCursorFlags>)
		{
		}
		virtual void OnApplyTextInput([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnAbortTextInput([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnDeleteTextInput([[maybe_unused]] Widget& owner, const ngine::Input::DeleteTextType)
		{
		}
		virtual bool HasEditableText([[maybe_unused]] const Widget& owner) const
		{
			return false;
		}

		virtual void OnCopyToPasteboard([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnPasteFromPasteboard([[maybe_unused]] Widget& owner)
		{
		}

		virtual void OnTabForward([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnTabBack([[maybe_unused]] Widget& owner)
		{
		}

		virtual void OnReceivedInputFocus([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnLostInputFocus([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnChildReceivedInputFocus([[maybe_unused]] Widget& owner)
		{
		}
		virtual void OnChildLostInputFocus([[maybe_unused]] Widget& owner)
		{
		}

		virtual void OnChildAdded(
			[[maybe_unused]] Widget& owner,
			[[maybe_unused]] Widget& child,
			[[maybe_unused]] const uint32 childIndex,
			[[maybe_unused]] const Optional<uint16> preferredChildIndex
		)
		{
		}
		virtual void
		OnChildRemoved([[maybe_unused]] Widget& owner, [[maybe_unused]] Widget& child, [[maybe_unused]] const uint32 previousChildIndex)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Input>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Input>(
			"3a6f3566-3261-4184-8eb6-391b40bf0539"_guid,
			MAKE_UNICODE_LITERAL("Widget Input"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
