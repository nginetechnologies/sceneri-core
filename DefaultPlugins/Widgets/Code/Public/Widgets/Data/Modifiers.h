#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Style/Modifier.h>

#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Widgets
{
	enum class ModifierProperty : uint8
	{
		Disabled,
		Hidden,
		Active,
		Toggled,
		Ignored,
		Required,
		Valid,
		Count
	};

	struct ModifierProperties
	{
		[[nodiscard]] bool AreAnyValid() const
		{
			return m_properties.GetView().Any(
				[](const ngine::DataSource::PropertyIdentifier propertyIdentifier)
				{
					return propertyIdentifier.IsValid();
				}
			);
		}

		void SetPropertyIdentifier(const ModifierProperty propertyType, const ngine::DataSource::PropertyIdentifier identifier)
		{
			m_properties[propertyType] = identifier;
		}

		[[nodiscard]] ngine::DataSource::PropertyIdentifier GetPropertyIdentifier(const ModifierProperty propertyType) const
		{
			return m_properties[propertyType];
		}
	private:
		Array<ngine::DataSource::PropertyIdentifier, (uint8)ModifierProperty::Count, ModifierProperty> m_properties;
	};

	struct DataSourceProperties;
}

namespace ngine::Widgets::Data
{
	struct Modifiers final : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		using BaseType = Widgets::Data::Component;

		Modifiers(Initializer&& initializer);
		Modifiers(const Deserializer& deserializer);
		Modifiers(const Modifiers& templateComponent, const Cloner& cloner);

		Modifiers(const Modifiers&) = delete;
		Modifiers& operator=(const Modifiers&) = delete;
		Modifiers(Modifiers&&) = delete;
		Modifiers& operator=(Modifiers&&) = delete;
		~Modifiers();

		[[nodiscard]] bool HasDynamicProperties() const
		{
			return m_pModifierProperties.IsValid();
		}

		[[nodiscard]] EnumFlags<Style::Modifier> GetModifiers() const
		{
			return m_modifiers;
		}

		void ToggleModifiers(Widget& owner, const EnumFlags<Style::Modifier> modifiers)
		{
			m_modifiers ^= modifiers;
			OnModifiersChangedInternal(owner, modifiers);
		}
		void SetModifiers(Widget& owner, const EnumFlags<Style::Modifier> modifiers)
		{
			m_modifiers |= modifiers;
			OnModifiersChangedInternal(owner, modifiers);
		}
		void ClearModifiers(Widget& owner, const EnumFlags<Style::Modifier> modifiers)
		{
			m_modifiers &= ~modifiers;
			OnModifiersChangedInternal(owner, modifiers);
		}

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		friend Widget;
		static void OnModifiersChangedInternal(Widget& widget, const EnumFlags<Style::Modifier> modifiers);

		void UpdateFromDataSourceInternal(Widget& owner, const DataSourceProperties& dataSourceProperties);
	protected:
		EnumFlags<Style::Modifier> m_modifiers;
		UniquePtr<ModifierProperties> m_pModifierProperties;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Modifiers>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Modifiers>(
			"{C4DCA150-A06C-4AB6-A949-03FADC4AD49D}"_guid, MAKE_UNICODE_LITERAL("Widget Modifiers"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
