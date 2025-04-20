#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/EventInfo.h>

#include <Engine/Event/Identifier.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Events
{
	struct Manager;
}

namespace ngine::Widgets::Data
{
	struct SubscribedEvents : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		using BaseType = Widgets::Data::Component;

		SubscribedEvents(const Deserializer& deserializer);
		SubscribedEvents(const SubscribedEvents& templateComponent, const Cloner& cloner);

		SubscribedEvents(const SubscribedEvents&) = delete;
		SubscribedEvents& operator=(const SubscribedEvents&) = delete;
		SubscribedEvents(SubscribedEvents&&) = delete;
		SubscribedEvents& operator=(SubscribedEvents&&) = delete;
		~SubscribedEvents();

		void OnDestroying();

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);

		[[nodiscard]] static Guid GetEventInstanceGuid(Widget& owner, const EventInstanceGuidType type);
		using ResolvedEvent = Variant<Events::Identifier, ngine::DataSource::PropertyIdentifier>;
		[[nodiscard]] static ResolvedEvent ResolveEvent(Widget& owner, const EventInfo& eventInfo);
	protected:
		enum class EventTypeDefinition : uint8
		{
			Invalid,
			SetDataAllowedTags,
			SetDataRequiredTags,
			SetDataAllowedItems,
			ClearDataAllowedItems,
			FocusLayoutData,
			Hide,
			Show,
			ToggleVisiblity,
			SetIgnored,
			ClearIgnored,
			ToggleIgnored,
			Disable,
			Enable,
			On,
			Off,
			Toggle,
			Activate,
			Deactivate,
			ToggleActive,
			OpenContextMenu,
			OpenAssets,
			EditAssets,
			OpenAssetInOS,
			CloneAssetsFromTemplate,
			OpenURIs,
			SetText,
			Count
		};
		using EventMask = Memory::UnsignedNumericSize<1 << (uint8)EventTypeDefinition::Count>;
		struct Event
		{
			EventInfo info;
			Events::Identifier identifier;
			EventMask mask{0};
		};

		[[nodiscard]] static EventTypeDefinition ParseEventType(const ConstStringView eventTypeName);
		[[nodiscard]] static ConstStringView GetEventTypeName(const EventTypeDefinition eventType);

		[[nodiscard]] bool TryRegisterEvent(
			Widget& owner, const EventTypeDefinition eventType, const Events::Identifier eventIdentifier, Events::Manager& eventManager
		);
	protected:
		Vector<Event, uint16> m_events;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::SubscribedEvents>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::SubscribedEvents>(
			"{165D3D15-7897-42CD-9BB4-AB82EDBABF04}"_guid, MAKE_UNICODE_LITERAL("Subscribed Event Data"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
