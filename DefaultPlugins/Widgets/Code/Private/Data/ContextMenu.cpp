#include "Data/ContextMenu.h"
#include "Data/ContextMenuEntries.h"

#include <Widgets/Widget.inl>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/SizeCorners.h>
#include <Widgets/LoadResourcesResult.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/Data/EventData.h>
#include <Widgets/Data/Layout.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Event/EventManager.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/DynamicDataSource.h>
#include <Engine/DataSource/DynamicPropertySource.h>
#include <Engine/Context/Utils.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	struct ContextMenuLayout final : public Widgets::Data::Component
	{
		using BaseType = Widgets::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 4>;
		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ContextMenuLayout>
	{
		static constexpr auto Type = Reflection::Reflect<Widgets::Data::ContextMenuLayout>(
			"0d254707-6aca-4f04-b074-e57426b998e8"_guid,
			MAKE_UNICODE_LITERAL("Context Menu Layout"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation
		);
	};
}

namespace ngine::Widgets::Data
{
	[[maybe_unused]] const bool wasContextMenulayoutTypeRegistered = Reflection::Registry::RegisterType<Widgets::Data::ContextMenuLayout>();
	[[maybe_unused]] const bool wasContextMenulayoutComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widgets::Data::ContextMenuLayout>>::Make());

	ContextMenu::ContextMenu(Initializer&& initializer)
		: ModalTrigger(Forward<ModalTrigger::Initializer>(initializer))
		, m_pDataSource(initializer.m_pDataSource)
		, m_pPropertySource(initializer.m_pPropertySource)
		, m_entryAsset(initializer.m_entryAsset)
	{
	}

	void ContextMenu::OnSpawnedModal(Widget& owner, Widget& modalWidget)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		if (m_pDataSource.IsValid())
		{
			modalWidget.RemoveDataComponentOfType<DataSource>(sceneRegistry);

			const Entity::DataComponentResult<ContextMenuLayout> contextMenuLayout =
				modalWidget.FindFirstDataComponentOfTypeInChildrenRecursive<ContextMenuLayout>(sceneRegistry);
			Widget& dataSourceWidget = contextMenuLayout.IsValid() ? *contextMenuLayout.m_pDataComponentOwner : modalWidget;
			[[maybe_unused]] const Optional<DataSource*> pDataSource = dataSourceWidget.CreateDataComponent<DataSource>(
				sceneRegistry,
				DataSource::Initializer{
					Component::Initializer{dataSourceWidget, sceneRegistry},
					*new ngine::DataSource::Dynamic(dataSourceCache.Register(Guid::Generate()), *m_pDataSource),
					m_entryAsset
				}
			);
			Assert(pDataSource.IsValid());
		}

		if (m_pPropertySource.IsValid())
		{
			modalWidget.RemoveDataComponentOfType<PropertySource>(sceneRegistry);
			[[maybe_unused]] const Optional<PropertySource*> pPropertySource = modalWidget.CreateDataComponent<PropertySource>(
				sceneRegistry,
				PropertySource::Initializer{
					Component::Initializer{modalWidget, sceneRegistry},
					*new ngine::PropertySource::Dynamic(dataSourceCache.GetPropertySourceCache().Register(Guid::Generate()), *m_pPropertySource)
				}
			);
			Assert(pPropertySource.IsValid());
		}

		modalWidget.RecalculateHierarchy();
	}

	void ContextMenu::OnModalClosed(Widget& owner, [[maybe_unused]] Widget& modalWidget)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		[[maybe_unused]] const bool wasRemoved = owner.RemoveDataComponentOfType<ContextMenu>(sceneRegistry);
		Assert(wasRemoved);
	}

	ContextMenuEntries::ContextMenuEntries(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		deserializer.GetParent().EnableInput();

		m_entries.Reserve((uint16)deserializer.m_reader.GetArraySize());
		for (const Serialization::Reader eventTypeReader : deserializer.m_reader.GetArrayView())
		{
			m_entries.EmplaceBack(Entry{
				Data::EventData::DeserializeEvent(deserializer.GetParent(), *eventTypeReader.FindSerializer("event")),
				eventTypeReader.ReadWithDefaultValue<UnicodeString>("title", UnicodeString{}),
				eventTypeReader.ReadWithDefaultValue<Asset::Guid>("icon", {})
			});
		}
	}

	ContextMenuEntries::ContextMenuEntries(const ContextMenuEntries& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_entries(Memory::Reserve, templateComponent.m_entries.GetSize())
	{
		cloner.GetParent().EnableInput();

		for (uint16 entryIndex = 0, entryCount = templateComponent.m_entries.GetSize(); entryIndex < entryCount; ++entryIndex)
		{
			m_entries.EmplaceBack(templateComponent.m_entries[entryIndex], cloner.GetParent());
		}
	}

	void ContextMenuEntries::UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties)
	{
		for (Entry& entry : m_entries)
		{
			entry.UpdateFromDataSource(owner, dataSourceProperties);
		}
	}

	[[maybe_unused]] const bool wasContextMenuTypeRegistered = Reflection::Registry::RegisterType<Widgets::Data::ContextMenu>();
	[[maybe_unused]] const bool wasContextMenuComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widgets::Data::ContextMenu>>::Make());

	[[maybe_unused]] const bool wasContextMenuEntriesTypeRegistered = Reflection::Registry::RegisterType<ContextMenuEntries>();
	[[maybe_unused]] const bool wasContextMenuEntriesComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ContextMenuEntries>>::Make());
}
