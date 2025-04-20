#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Style/StylesheetIdentifier.h>

#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/InlineVector.h>

namespace ngine::Widgets::Style
{
	struct ComputedStylesheet;
	struct StylesheetCache;
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Widgets::Data
{
	struct ExternalStyle;

	struct Stylesheet final : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		using BaseType = Widgets::Data::Component;

		using StylesheetIdentifiers = InlineVector<StylesheetIdentifier, 1>;
		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, StylesheetIdentifiers&& stylesheetIdentifiers)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_stylesheetIdentifiers(Forward<StylesheetIdentifiers>(stylesheetIdentifiers))
			{
			}

			StylesheetIdentifiers m_stylesheetIdentifiers;
		};
		Stylesheet(Initializer&& initializer);
		Stylesheet(const Deserializer& deserializer);
		Stylesheet(const Stylesheet& templateComponent, const Cloner& cloner);

		Stylesheet(const Stylesheet&) = delete;
		Stylesheet& operator=(const Stylesheet&) = delete;
		Stylesheet(Stylesheet&&) = delete;
		Stylesheet& operator=(Stylesheet&&) = delete;
		~Stylesheet();

		[[nodiscard]] Optional<Threading::Job*> AddStylesheet(Widget& widget, const StylesheetIdentifier stylesheetIdentifier);

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		friend ExternalStyle;

		[[nodiscard]] Optional<Threading::Job*>
		LoadStylesheet(const StylesheetIdentifier stylesheetIdentifier, Widget& widget, Entity::SceneRegistry& sceneRegistry);

		using ParentStylesheets = InlineVector<ReferenceWrapper<const Style::ComputedStylesheet>, 6>;
		static void GetParentStylesheets(
			ParentStylesheets& stylesheets,
			Style::StylesheetCache& stylesheetCache,
			Widget& widget,
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData
		);
		[[nodiscard]] static ParentStylesheets GetStylesheets(
			Style::StylesheetCache& stylesheetCache,
			Widget& widget,
			const Optional<Stylesheet*> pStylesheet,
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData
		);

		void ApplyStylesheetsRecursiveInternal(
			Widget& widget,
			const Optional<ExternalStyle*> pExternalStyle,
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
		);
		static void ApplyStylesheetRecursiveInternal(
			Widget& widget,
			const ArrayView<const ReferenceWrapper<const Style::ComputedStylesheet>> stylesheets,
			const Optional<ExternalStyle*> pExternalStyle,
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
		);
		static void ApplyParentStylesheets(
			Widget& widget,
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
		);
	protected:
		StylesheetIdentifiers m_stylesheetIdentifiers;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Stylesheet>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Stylesheet>(
			"c0b9e78e-e570-442c-8e48-b58a696547fc"_guid, MAKE_UNICODE_LITERAL("Widget Stylesheets"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
