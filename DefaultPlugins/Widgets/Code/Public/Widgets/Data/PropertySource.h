#pragma once

#include <Widgets/Data/Component.h>
#include <Engine/DataSource/PropertySourceIdentifier.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/Variant.h>

namespace ngine::PropertySource
{
	struct Dynamic;
}

namespace ngine::Widgets::Data
{
	struct PropertySource : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		using BaseType = Widgets::Data::Component;

		enum class Flags : uint8
		{
			IsDynamic = 1 << 0
		};

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, const ngine::PropertySource::Identifier propertySourceIdentifier)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_propertySource(propertySourceIdentifier)
			{
			}
			Initializer(BaseType&& baseInitializer, ngine::PropertySource::Dynamic& propertySource)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_propertySource(ReferenceWrapper<ngine::PropertySource::Dynamic>(propertySource))
			{
			}

			Variant<ngine::PropertySource::Identifier, ReferenceWrapper<ngine::PropertySource::Dynamic>> m_propertySource;
		};
		PropertySource(Initializer&& initializer);
		PropertySource(const Deserializer& deserializer);
		PropertySource(const PropertySource& templateComponent, const Cloner& cloner);

		PropertySource(const PropertySource&) = delete;
		PropertySource& operator=(const PropertySource&) = delete;
		PropertySource(PropertySource&&) = delete;
		PropertySource& operator=(PropertySource&&) = delete;
		~PropertySource();

		void OnCreated(Widget& owner);
		void OnParentCreated(Widget& owner);
		void OnDestroying(Widget& owner);

		void OnDataChanged(Widget& owner);
		void SetPropertySource(Widget& owner, ngine::PropertySource::Identifier propertySourceIdentifier);

		[[nodiscard]] ngine::PropertySource::Identifier GetPropertySourceIdentifier() const
		{
			return m_propertySourceIdentifier;
		}

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		ngine::PropertySource::Identifier m_propertySourceIdentifier;
		Guid m_propertySourceGuid;
		EnumFlags<Flags> m_flags;
	};
	ENUM_FLAG_OPERATORS(PropertySource::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::PropertySource>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::PropertySource>(
			"f1304ae9-8026-4f5d-a65f-4faf6567eab2"_guid, MAKE_UNICODE_LITERAL("Widget Property Source"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
