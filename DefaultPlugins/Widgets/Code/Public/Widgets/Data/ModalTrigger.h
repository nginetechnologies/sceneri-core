#pragma once

#include <Widgets/Data/Input.h>

#include <Engine/Input/WindowCoordinate.h>

namespace ngine::Widgets::Data
{
	struct ModalFocusListener;

	struct ModalTrigger : public Widgets::Data::Input
	{
		using BaseType = Widgets::Data::Input;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		enum class Flags : uint8
		{
			//! Whether we should spawn the modal at the cursor location
			//! Otherwise it'll be spawned relative to our parent widget.
			SpawnAtCursorLocation = 1 << 0,
			SpawnOnTap = 1 << 1,
			CloseOnEvent = 1 << 2, // Closes the modal on receiving its instance guid as event
		};

		struct Initializer : public Widgets::Data::Input::Initializer
		{
			using BaseType = Widgets::Data::Input::Initializer;

			Initializer(BaseType&& baseInitializer, const Asset::Guid modalWidgetAssetGuid, const EnumFlags<Flags> flags = Flags::SpawnOnTap)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_modalWidgetAssetGuid(modalWidgetAssetGuid)
				, m_flags(flags)
			{
			}
			Asset::Guid m_modalWidgetAssetGuid;
			EnumFlags<Flags> m_flags{Flags::SpawnOnTap};
		};

		ModalTrigger(Initializer&& initializer);
		ModalTrigger(const Deserializer& deserializer);
		ModalTrigger(const ModalTrigger& templateComponent, const Cloner& cloner);
		ModalTrigger(const ModalTrigger&) = delete;
		ModalTrigger(ModalTrigger&&) = delete;
		ModalTrigger& operator=(const ModalTrigger&) = delete;
		ModalTrigger& operator=(ModalTrigger&&) = delete;
		virtual ~ModalTrigger() = default;

		void OnCreated(Widget& parent);
		void OnDestroying(Widget& parent);

		// Data::Input
		[[nodiscard]] virtual CursorResult
		OnStartTap(Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate) override;
		[[nodiscard]] virtual CursorResult
		OnEndTap(Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate) override;
		// ~Data::Input

		bool SpawnModal(Widget& owner, const WindowCoordinate clickLocation);
		bool Close(Widget& owner);
		[[nodiscard]] bool IsModalOpen() const
		{
			return m_pSpawnedWidget.IsValid();
		}
	protected:
		void OnClose()
		{
			Close(m_owner);
		}
		virtual void OnSpawnedModal([[maybe_unused]] Widget& owner, [[maybe_unused]] Widget& modalWidget)
		{
		}
		virtual void OnModalClosed([[maybe_unused]] Widget& owner, [[maybe_unused]] Widget& modalWidget)
		{
		}
	private:
		friend ModalFocusListener;
		void OnModalLostFocus(Widget& owner);
	protected:
		Widget& m_owner;
		EnumFlags<Flags> m_flags{Flags::SpawnOnTap};
		Asset::Guid m_modalWidgetAssetGuid;
		Optional<Widget*> m_pSpawnedWidget;
	};

	ENUM_FLAG_OPERATORS(ModalTrigger::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ModalTrigger>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ModalTrigger>(
			"A4198D12-7ECB-457B-9A95-F57F07AC97F7"_guid,
			MAKE_UNICODE_LITERAL("Modal Trigger"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation
		);
	};
}
