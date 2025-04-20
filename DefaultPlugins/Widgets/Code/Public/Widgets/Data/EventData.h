#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/EventData.h>
#include <Widgets/EventType.h>

namespace ngine::Widgets
{
	struct DataSourceProperties;
}

namespace ngine::Widgets::Data
{
	struct EventData : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		using BaseType = Widgets::Data::Component;

		EventData(const Deserializer& deserializer);
		EventData(const EventData& templateComponent, const Cloner& cloner);

		EventData(const EventData&) = delete;
		EventData& operator=(const EventData&) = delete;
		EventData(EventData&&) = delete;
		EventData& operator=(EventData&&) = delete;
		~EventData();

		[[nodiscard]] static Widgets::EventData DeserializeEvent(Widget& owner, Serialization::Reader eventReader);

		[[nodiscard]] bool HasEvents(const EventType eventType) const
		{
			return m_events[(uint8)eventType].HasElements();
		}

		[[nodiscard]] bool HasDynamicEvents() const
		{
			return m_events.GetView().Any(
				[](const EventContainer& eventContainer)
				{
					return eventContainer.GetView().Any(
						[](const Widgets::EventData& eventData)
						{
							return eventData.m_dynamicPropertyIdentifier.IsValid();
						}
					);
				}
			);
		}

		void NotifyAll(Widget& owner, const EventType eventType);

		void UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties);

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		using EventContainer = Vector<Widgets::EventData, uint16>;
		Array<EventContainer, (uint8)EventType::Count> m_events;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::EventData>
	{
		// TODO:Cloning, deserialization & write to disk
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::EventData>(
			"{A767E127-2304-4441-8AED-9869D59A8F69}"_guid, MAKE_UNICODE_LITERAL("Event Data"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
