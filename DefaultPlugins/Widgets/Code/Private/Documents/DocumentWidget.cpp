#include <Widgets/Documents/DocumentWidget.h>
#include <Widgets/Documents/DocumentWindow.h>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/Style/CombinedEntry.h>

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Window/DocumentData.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Widgets::Document
{
	Widget::Widget(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
		, ngine::PropertySource::Interface(System::Get<ngine::DataSource::Cache>().GetPropertySourceCache().FindOrRegister(Guid::Generate()))
		, m_documentTitlePropertyIdentifier(System::Get<ngine::DataSource::Cache>().FindOrRegisterPropertyIdentifier("document_title"))
		, m_documentUriPropertyIdentifier(System::Get<ngine::DataSource::Cache>().FindOrRegisterPropertyIdentifier("document_uri"))
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(ngine::PropertySource::Interface::GetIdentifier(), *this);

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		DataComponentOwner::CreateDataComponent<Data::PropertySource>(
			sceneRegistry,
			Data::PropertySource::Initializer{
				Widgets::Data::Component::Initializer{*this, sceneRegistry},
				ngine::PropertySource::Interface::GetIdentifier()
			}
		);
	}

	Widget::Widget(const Deserializer& deserializer)
		: BaseType(deserializer)
		, ngine::PropertySource::Interface(System::Get<ngine::DataSource::Cache>().GetPropertySourceCache().FindOrRegister(Guid::Generate()))
		, m_documentTitlePropertyIdentifier(System::Get<ngine::DataSource::Cache>().FindOrRegisterPropertyIdentifier("document_title"))
		, m_documentUriPropertyIdentifier(System::Get<ngine::DataSource::Cache>().FindOrRegisterPropertyIdentifier("document_uri"))
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(ngine::PropertySource::Interface::GetIdentifier(), *this);

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		DataComponentOwner::CreateDataComponent<Data::PropertySource>(
			sceneRegistry,
			Data::PropertySource::Initializer{
				Widgets::Data::Component::Initializer{*this, sceneRegistry},
				ngine::PropertySource::Interface::GetIdentifier()
			}
		);
	}

	Widget::Widget(const Widget& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, ngine::PropertySource::Interface(System::Get<ngine::DataSource::Cache>().GetPropertySourceCache().FindOrRegister(Guid::Generate()))
		, m_documentTitlePropertyIdentifier(templateComponent.m_documentTitlePropertyIdentifier)
		, m_documentUriPropertyIdentifier(templateComponent.m_documentUriPropertyIdentifier)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(ngine::PropertySource::Interface::GetIdentifier(), *this);

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		DataComponentOwner::CreateDataComponent<Data::PropertySource>(
			sceneRegistry,
			Data::PropertySource::Initializer{
				Widgets::Data::Component::Initializer{*this, sceneRegistry},
				ngine::PropertySource::Interface::GetIdentifier()
			}
		);
	}

	void Widget::OnCreated()
	{
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers();
		const Style::CombinedEntry style = GetStyle();
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		{
			if (Optional<const Style::EntryValue*> pAttachedAsset = style.Find(Style::ValueTypeIdentifier::AttachedDocumentAsset, matchingModifiers))
			{
				if (const Optional<const Asset::Guid*> assetGuid = pAttachedAsset->Get<Asset::Guid>())
				{
					EnumFlags<OpenDocumentFlags> openDocumentFlags;
					const bool enableEditing = style.GetWithDefault(Widgets::Style::ValueTypeIdentifier::EnableEditing, false, matchingModifiers);
					openDocumentFlags |= OpenDocumentFlags::EnableEditing * enableEditing;
					[[maybe_unused]] Threading::JobBatch jobBatch =
						static_cast<Window&>(*GetOwningWindow()).OpenDocuments(Array{DocumentData{*assetGuid}}, openDocumentFlags, this);
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}
			}
		}
	}

	ngine::DataSource::PropertyValue Widget::GetDataProperty(const ngine::DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_documentTitlePropertyIdentifier)
		{
			const Asset::Reference assetReference = GetDocumentAssetReference();
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			const IO::Path::StringType assetName = assetManager.GetAssetName(assetReference.GetAssetGuid());
			if (assetName.HasElements())
			{
				return UnicodeString(assetName.GetView());
			}

			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			if (const Optional<const Reflection::TypeInterface*> pAssetTypeInterface = reflectionRegistry.FindTypeInterface(assetReference.GetTypeGuid()))
			{
				return UnicodeString{pAssetTypeInterface->GetName()};
			}

			return UnicodeString{GetTypeInterface()->GetName()};
		}
		else if (identifier == m_documentUriPropertyIdentifier)
		{
			return GetDocumentURI();
		}

		return {};
	}

	void Widget::SetAssetFromProperty(Asset::Picker asset)
	{
		Threading::JobBatch jobBatch =
			static_cast<Window&>(*GetOwningWindow()).OpenDocuments(Array{DocumentData{asset.GetAssetGuid()}}, OpenDocumentFlags{}, this);
		if (jobBatch.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(jobBatch);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadProject);
			}
		}
	}

	Threading::JobBatch Widget::SetDeserializedAssetFromProperty(
		const Asset::Picker asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		EnumFlags<OpenDocumentFlags> openDocumentFlags;
		openDocumentFlags |= OpenDocumentFlags::EnableEditing * typeReader.ReadWithDefaultValue<bool>("edit", false);
		return static_cast<Window&>(*GetOwningWindow()).OpenDocuments(Array{DocumentData{asset.GetAssetGuid()}}, openDocumentFlags, this);
	}

	[[nodiscard]] Asset::Picker Widget::GetAssetFromProperty() const
	{
		return {GetDocumentAssetReference(), Asset::Types{GetSupportedDocumentAssetTypeGuids()}};
	}

	void Widget::OnStyleChanged(
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedValues
	)
	{
		if (changedValues.IsSet((uint8)Widgets::Style::ValueTypeIdentifier::AttachedDocumentAsset))
		{
			if (Optional<const Style::EntryValue*> pAttachedAsset = style.Find(Style::ValueTypeIdentifier::AttachedDocumentAsset, matchingModifiers))
			{
				if (const Optional<const Asset::Guid*> assetGuid = pAttachedAsset->Get<Asset::Guid>())
				{
					EnumFlags<OpenDocumentFlags> openDocumentFlags;
					const bool enableEditing = style.GetWithDefault(Widgets::Style::ValueTypeIdentifier::EnableEditing, false, matchingModifiers);
					openDocumentFlags |= OpenDocumentFlags::EnableEditing * enableEditing;
					[[maybe_unused]] Threading::JobBatch jobBatch =
						static_cast<Window&>(*GetOwningWindow()).OpenDocuments(Array{DocumentData{*assetGuid}}, openDocumentFlags, this);
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}
			}
		}
	}
}
