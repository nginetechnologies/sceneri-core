#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Style/StylesheetIdentifier.h>

#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Widgets::Style
{
	struct Entry;
	struct ComputedStylesheet;
}

namespace ngine::Widgets::Data
{
	struct Stylesheet;

	struct ExternalStyle final : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		using BaseType = Widgets::Data::Component;

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, const Asset::Guid stylesheetEntryGuid = {})
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_stylesheetEntryGuid(stylesheetEntryGuid)
			{
			}

			StylesheetIdentifier m_stylesheetIdentifier;
			Asset::Guid m_stylesheetEntryGuid;
		};
		ExternalStyle(Initializer&& initializer);
		ExternalStyle(const Deserializer& deserializer);
		ExternalStyle(const ExternalStyle& templateComponent, const Cloner& cloner);

		ExternalStyle(const ExternalStyle&) = delete;
		ExternalStyle& operator=(const ExternalStyle&) = delete;
		ExternalStyle(ExternalStyle&&) = delete;
		ExternalStyle& operator=(ExternalStyle&&) = delete;
		~ExternalStyle();

		[[nodiscard]] Optional<const Style::Entry*> GetEntry() const
		{
			return m_pStyle;
		}

		void SetEntry(Widget& widget, const Asset::Guid stylesheetEntryGuid);

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		friend Stylesheet;

		void ApplyStyleEntry(Widget& widget, const ArrayView<const ReferenceWrapper<const Style::ComputedStylesheet>> stylesheets);
	protected:
		Asset::Guid m_stylesheetEntryGuid;
		Optional<const Style::Entry*> m_pStyle;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ExternalStyle>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ExternalStyle>(
			"BCF069C9-E63C-4ED0-B977-9A1B5EB0B440"_guid, MAKE_UNICODE_LITERAL("Widget External Style"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
