#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Style/ValueTypeIdentifier.h>
#include <Widgets/Style/Modifier.h>

#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets::Style
{
	struct DynamicEntry;
	struct Value;
}

namespace ngine::Widgets::Data
{
	struct DynamicStyle final : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		using BaseType = Widgets::Data::Component;

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, UniquePtr<Style::DynamicEntry>&& pEntry)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_pEntry(Forward<UniquePtr<Style::DynamicEntry>>(pEntry))
			{
			}

			UniquePtr<Style::DynamicEntry> m_pEntry;
		};
		DynamicStyle(Initializer&& initializer);
		DynamicStyle(const Deserializer& deserializer);
		DynamicStyle(const DynamicStyle& templateComponent, const Cloner& cloner);

		DynamicStyle(const DynamicStyle&) = delete;
		DynamicStyle& operator=(const DynamicStyle&) = delete;
		DynamicStyle(DynamicStyle&&) = delete;
		DynamicStyle& operator=(DynamicStyle&&) = delete;
		~DynamicStyle();

		[[nodiscard]] Style::DynamicEntry& GetEntry() const
		{
			return *m_pEntry;
		}

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);

		void SetEntry(Widget& widget, Style::Entry&& entry);
		void EmplaceValue(Widget& widget, Style::Value&& value, const EnumFlags<Style::Modifier> modifiers);
		void ClearValue(Widget& widget, const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers);
	protected:
		UniquePtr<Style::DynamicEntry> m_pEntry;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::DynamicStyle>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::DynamicStyle>(
			"5EB15848-85C8-4221-9D1F-069A7956229F"_guid, MAKE_UNICODE_LITERAL("Widget Dynamic Style"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
