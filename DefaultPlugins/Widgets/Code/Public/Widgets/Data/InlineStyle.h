#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Style/ValueTypeIdentifier.h>
#include <Widgets/Style/Modifier.h>

#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets::Style
{
	struct Entry;
	struct Value;
}

namespace ngine::Widgets::Data
{
	struct InlineStyle final : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 16>;

		using BaseType = Widgets::Data::Component;

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, UniquePtr<Style::Entry>&& pEntry)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_pEntry(Forward<UniquePtr<Style::Entry>>(pEntry))
			{
			}

			UniquePtr<Style::Entry> m_pEntry;
		};
		InlineStyle(Initializer&& initializer);
		InlineStyle(const Deserializer& deserializer);
		InlineStyle(const InlineStyle& templateComponent, const Cloner& cloner);

		InlineStyle(const InlineStyle&) = delete;
		InlineStyle& operator=(const InlineStyle&) = delete;
		InlineStyle(InlineStyle&&) = delete;
		InlineStyle& operator=(InlineStyle&&) = delete;
		~InlineStyle();

		[[nodiscard]] const Style::Entry& GetEntry() const
		{
			return *m_pEntry;
		}

		void SetEntry(Widget& widget, Style::Entry&& entry);
		void EmplaceValue(Widget& widget, Style::Value&& value, const EnumFlags<Style::Modifier> modifiers);
		void ClearValue(Widget& widget, const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers);
		void ParseEntry(Widget& widget, const ConstStringView string);

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		UniquePtr<Style::Entry> m_pEntry;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::InlineStyle>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::InlineStyle>(
			"24A138E8-1085-40AF-A228-D15AE772A2F3"_guid, MAKE_UNICODE_LITERAL("Widget Inline Style"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
