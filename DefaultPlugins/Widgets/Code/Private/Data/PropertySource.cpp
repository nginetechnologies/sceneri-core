#include <Widgets/Data/PropertySource.h>
#include <Widgets/WidgetScene.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/DataSource/DynamicPropertySource.h>
#include <Engine/DataSource/PropertySourceInterface.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Context/Utils.h>

#include <Widgets/Widget.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	PropertySource::PropertySource(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
	{
		ngine::PropertySource::Cache& propertySourceCache = System::Get<ngine::DataSource::Cache>().GetPropertySourceCache();
		if (const Optional<ngine::PropertySource::Identifier*> propertySourceIdentifier = initializer.m_propertySource.Get<ngine::PropertySource::Identifier>())
		{
			m_propertySourceIdentifier = *propertySourceIdentifier;
			m_propertySourceGuid = propertySourceCache.FindGuid(*propertySourceIdentifier);
		}
		else if (const Optional<ReferenceWrapper<ngine::PropertySource::Dynamic>*> pPropertySource = initializer.m_propertySource.Get<ReferenceWrapper<ngine::PropertySource::Dynamic>>())
		{
			ngine::PropertySource::Dynamic& propertySource = *pPropertySource;
			m_propertySourceIdentifier = propertySource.GetIdentifier();
			m_propertySourceGuid = propertySourceCache.FindGuid(m_propertySourceIdentifier);
			m_flags |= Flags::IsDynamic;
		}

		m_propertySourceGuid = propertySourceCache.FindGuid(m_propertySourceIdentifier);
		if (!initializer.GetParent().GetRootScene().IsTemplate())
		{
			propertySourceCache.AddOnChangedListener(
				m_propertySourceIdentifier,
				ngine::PropertySource::Cache::OnChangedListenerData{
					this,
					[&owner = initializer.GetParent()](Data::PropertySource& propertySourceComponent)
					{
						propertySourceComponent.OnDataChanged(owner);
					}
				}
			);
		}
	}

	PropertySource::PropertySource(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

		ngine::DataSource::Identifier propertySourceIdentifier;
		bool isDynamic = false;
		if (Optional<Guid> propertySourceGuid = deserializer.m_reader.ReadInPlace<Guid>())
		{
			m_propertySourceGuid = *propertySourceGuid;
			propertySourceIdentifier = propertySourceCache.Find(*propertySourceGuid);
			if (propertySourceIdentifier.IsInvalid())
			{
				propertySourceIdentifier = propertySourceCache.FindOrRegister(
					Context::Utils::GetGuid(*propertySourceGuid, deserializer.GetParent(), deserializer.GetSceneRegistry())
				);
			}
		}
		else
		{
			isDynamic = deserializer.m_reader.ReadWithDefaultValue<bool>("dynamic", true);
			if (const Optional<Guid> guid = deserializer.m_reader.Read<Guid>("guid"))
			{
				propertySourceGuid = *guid;
			}
			else
			{
				propertySourceGuid = Guid::Generate();
			}
			m_propertySourceGuid = *propertySourceGuid;
			propertySourceIdentifier = propertySourceCache.Find(*propertySourceGuid);
			if (propertySourceIdentifier.IsInvalid())
			{
				propertySourceIdentifier = propertySourceCache.FindOrRegister(
					Context::Utils::GetGuid(*propertySourceGuid, deserializer.GetParent(), deserializer.GetSceneRegistry())
				);
			}
		}

		m_propertySourceIdentifier = propertySourceIdentifier;
		m_flags |= Flags::IsDynamic * isDynamic;

		if (!deserializer.GetParent().GetRootScene().IsTemplate())
		{
			propertySourceCache.AddOnChangedListener(
				propertySourceIdentifier,
				ngine::PropertySource::Cache::OnChangedListenerData{
					this,
					[&owner = deserializer.GetParent()](Data::PropertySource& propertySourceComponent)
					{
						propertySourceComponent.OnDataChanged(owner);
					}
				}
			);
		}

		if (isDynamic)
		{
			ngine::PropertySource::Dynamic* pDynamicPropertySource = new ngine::PropertySource::Dynamic(propertySourceIdentifier);
			deserializer.m_reader.SerializeInPlace(*pDynamicPropertySource, dataSourceCache);
		}
	}

	PropertySource::PropertySource(const PropertySource& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_propertySourceGuid(templateComponent.m_propertySourceGuid)
		, m_flags(templateComponent.m_flags)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

		ngine::DataSource::Identifier propertySourceIdentifier;
		if (m_flags.IsSet(Flags::IsDynamic))
		{
			propertySourceIdentifier = propertySourceCache.FindOrRegister(Guid::Generate());
		}
		else
		{
			propertySourceIdentifier = propertySourceCache.Find(m_propertySourceGuid);
		}
		if (propertySourceIdentifier.IsInvalid())
		{
			propertySourceIdentifier =
				propertySourceCache.FindOrRegister(Context::Utils::GetGuid(m_propertySourceGuid, cloner.GetParent(), cloner.GetSceneRegistry()));
		}
		Assert(
			m_flags.IsNotSet(Flags::IsDynamic) || propertySourceCache.FindGuid(propertySourceIdentifier) != m_propertySourceGuid,
			"Dynamic property source must use unique identifiers"
		);
		m_propertySourceIdentifier = propertySourceIdentifier;
		Assert(m_propertySourceIdentifier.IsValid());

		if (!cloner.GetParent().GetRootScene().IsTemplate())
		{
			propertySourceCache.AddOnChangedListener(
				m_propertySourceIdentifier,
				ngine::PropertySource::Cache::OnChangedListenerData{
					this,
					[&owner = cloner.GetParent()](Data::PropertySource& propertySourceComponent)
					{
						propertySourceComponent.OnDataChanged(owner);
					}
				}
			);
		}

		if (m_flags.IsSet(Flags::IsDynamic))
		{
			const Optional<ngine::PropertySource::Interface*> pTemplatePropertySource =
				propertySourceCache.Get(templateComponent.m_propertySourceIdentifier);
			Assert(pTemplatePropertySource.IsValid());
			if (LIKELY(pTemplatePropertySource.IsValid()))
			{
				ngine::PropertySource::Dynamic& dynamicTemplate = static_cast<ngine::PropertySource::Dynamic&>(*pTemplatePropertySource);
				[[maybe_unused]] ngine::PropertySource::Dynamic* pDynamicPropertySource =
					new ngine::PropertySource::Dynamic(propertySourceIdentifier, dynamicTemplate);
			}
		}
	}

	PropertySource::~PropertySource() = default;

	void PropertySource::OnCreated(Widget& owner)
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			owner.UpdateFromDataSource(owner.GetSceneRegistry());
		}
	}

	void PropertySource::OnParentCreated(Widget& owner)
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			owner.UpdateFromDataSource(owner.GetSceneRegistry());
		}
	}

	void PropertySource::OnDestroying(Widget&)
	{
		if (m_propertySourceIdentifier.IsValid())
		{
			ngine::PropertySource::Cache& propertySourceCache = System::Get<ngine::DataSource::Cache>().GetPropertySourceCache();
			propertySourceCache.RemoveOnChangedListener(m_propertySourceIdentifier, this);

			if (m_flags.IsSet(Flags::IsDynamic))
			{
				const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(m_propertySourceIdentifier);
				Assert(pPropertySource.IsValid());
				if (LIKELY(pPropertySource.IsValid()))
				{
					ngine::PropertySource::Dynamic* pDynamic = &static_cast<ngine::PropertySource::Dynamic&>(*pPropertySource);
					delete pDynamic;
				}
			}
		}
	}

	void PropertySource::OnDataChanged(Widget& owner)
	{
		owner.UpdateFromDataSource(owner.GetSceneRegistry());
	}

	void PropertySource::SetPropertySource(Widget& owner, ngine::PropertySource::Identifier newPropertySourceIdentifier)
	{
		ngine::PropertySource::Cache& propertySourceCache = System::Get<ngine::DataSource::Cache>().GetPropertySourceCache();

		if (!owner.GetRootScene().IsTemplate())
		{
			propertySourceCache.RemoveOnChangedListener(m_propertySourceIdentifier, this);

			propertySourceCache.AddOnChangedListener(
				newPropertySourceIdentifier,
				ngine::PropertySource::Cache::OnChangedListenerData{
					this,
					[&owner](Data::PropertySource& propertySourceComponent)
					{
						propertySourceComponent.OnDataChanged(owner);
					}
				}
			);
		}

		if (m_flags.IsSet(Flags::IsDynamic))
		{
			const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(m_propertySourceIdentifier);
			Assert(pPropertySource.IsValid());
			if (LIKELY(pPropertySource.IsValid()))
			{
				ngine::PropertySource::Dynamic* pDynamic = &static_cast<ngine::PropertySource::Dynamic&>(*pPropertySource);
				delete pDynamic;
			}
			m_flags.Clear(Flags::IsDynamic);
		}

		m_propertySourceIdentifier = newPropertySourceIdentifier;
		OnDataChanged(owner);
	}

	bool PropertySource::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		writer.Serialize("guid", m_propertySourceGuid);
		const bool isDynamic = m_flags.IsSet(Flags::IsDynamic);
		writer.SerializeWithDefaultValue("dynamic", isDynamic, true);
		if (isDynamic)
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

			const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(m_propertySourceIdentifier);
			Assert(pPropertySource.IsValid());

			if (LIKELY(pPropertySource.IsValid()))
			{
				ngine::PropertySource::Dynamic& dynamic = static_cast<ngine::PropertySource::Dynamic&>(*pPropertySource);
				dynamic.Serialize(writer, dataSourceCache);
			}
		}

		return true;
	}

	void PropertySource::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& owner)
	{
		if (pReader.IsValid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();

			ngine::DataSource::Identifier newPropertySourceIdentifier;
			bool isDynamic = false;
			if (Optional<Guid> propertySourceGuid = pReader->ReadInPlace<Guid>())
			{
				m_propertySourceGuid = *propertySourceGuid;
				newPropertySourceIdentifier = propertySourceCache.Find(*propertySourceGuid);
				if (newPropertySourceIdentifier.IsInvalid())
				{
					newPropertySourceIdentifier =
						propertySourceCache.FindOrRegister(Context::Utils::GetGuid(*propertySourceGuid, owner, sceneRegistry));
				}
			}
			else
			{
				propertySourceGuid = pReader->ReadWithDefaultValue<Guid>("guid", Guid::Generate());
				m_propertySourceGuid = *propertySourceGuid;
				newPropertySourceIdentifier = propertySourceCache.Find(*propertySourceGuid);
				if (newPropertySourceIdentifier.IsInvalid())
				{
					newPropertySourceIdentifier =
						propertySourceCache.FindOrRegister(Context::Utils::GetGuid(*propertySourceGuid, owner, sceneRegistry));
				}
				isDynamic = pReader->ReadWithDefaultValue<bool>("dynamic", true);
			}

			if (newPropertySourceIdentifier != m_propertySourceIdentifier)
			{
				propertySourceCache.RemoveOnChangedListener(m_propertySourceIdentifier, this);

				propertySourceCache.AddOnChangedListener(
					newPropertySourceIdentifier,
					ngine::PropertySource::Cache::OnChangedListenerData{
						this,
						[&owner](Data::PropertySource& propertySourceComponent)
						{
							propertySourceComponent.OnDataChanged(owner);
						}
					}
				);

				if (m_flags.IsSet(Flags::IsDynamic))
				{
					const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(m_propertySourceIdentifier);
					Assert(pPropertySource.IsValid());
					if (LIKELY(pPropertySource.IsValid()))
					{
						ngine::PropertySource::Dynamic* pDynamic = &static_cast<ngine::PropertySource::Dynamic&>(*pPropertySource);
						delete pDynamic;
					}
					m_flags.Clear(Flags::IsDynamic);
				}

				m_propertySourceIdentifier = newPropertySourceIdentifier;
				m_flags |= Flags::IsDynamic * isDynamic;

				if (isDynamic)
				{
					ngine::PropertySource::Dynamic* pDynamicPropertySource = new ngine::PropertySource::Dynamic(newPropertySourceIdentifier);
					pReader->SerializeInPlace(*pDynamicPropertySource, dataSourceCache);
				}

				OnDataChanged(owner);
			}
		}
	}

	[[maybe_unused]] const bool wasPropertySourceTypeRegistered = Reflection::Registry::RegisterType<Data::PropertySource>();
	[[maybe_unused]] const bool wasPropertySourceComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::PropertySource>>::Make());
}
