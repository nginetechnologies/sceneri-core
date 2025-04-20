#include "Data/SubscribedEvents.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Widget.inl>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/Layout.h>
#include <Widgets/Data/GridLayout.h>
#include <Widgets/Data/TextDrawable.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Context/Utils.h>
#include <Engine/Event/EventManager.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Scene/Scene2D.h>

#include <Renderer/Window/DocumentData.h>

#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Platform/OpenInFileBrowser.h>
#include <Common/Math/Hash.h>

namespace ngine::Widgets::Data
{
	SubscribedEvents::SubscribedEvents(const Deserializer& deserializer)
	{
		const Serialization::Reader eventsReader = *deserializer.m_reader.FindSerializer("events");

		Events::Manager& eventManager = System::Get<Events::Manager>();
		m_events.Reserve((uint16)eventsReader.GetMemberCount());
		for (const Serialization::Member<Serialization::Reader> eventMember : eventsReader.GetMemberView())
		{
			EventInfo eventInfo = deserializer.GetParent().DeserializeEventInfo(eventMember.key);
			EventMask eventMask{0};
			Events::Identifier eventIdentifier;
			if (deserializer.GetParent().GetRootScene().IsTemplate())
			{
				for (const Serialization::Reader eventTypeReader : eventMember.value.GetArrayView())
				{
					const EventTypeDefinition eventType = ParseEventType(*eventTypeReader.ReadInPlace<ConstStringView>());
					eventMask |= 1 << (uint8)eventType;
				}
			}
			else
			{
				eventIdentifier = ResolveEvent(deserializer.GetParent(), eventInfo).GetExpected<Events::Identifier>();

				for (const Serialization::Reader eventTypeReader : eventMember.value.GetArrayView())
				{
					const EventTypeDefinition eventType = ParseEventType(*eventTypeReader.ReadInPlace<ConstStringView>());
					eventMask |= 1 << (uint8)eventType;
					[[maybe_unused]] const bool wasSubscribed = TryRegisterEvent(deserializer.GetParent(), eventType, eventIdentifier, eventManager);
					Assert(wasSubscribed);
				}
			}
			m_events.EmplaceBack(Event{Move(eventInfo), eventIdentifier, eventMask});
		}
	}

	SubscribedEvents::SubscribedEvents(const SubscribedEvents& templateComponent, const Cloner& cloner)
	{
		Events::Manager& eventManager = System::Get<Events::Manager>();
		m_events.Reserve(templateComponent.m_events.GetSize());
		if (!cloner.GetParent().GetRootScene().IsTemplate())
		{
			for (const Event& templateEvent : templateComponent.m_events)
			{
				Events::Identifier eventIdentifier = ResolveEvent(cloner.GetParent(), templateEvent.info).GetExpected<Events::Identifier>();

				for (const EventMask eventIndex : Memory::GetSetBitsIterator(templateEvent.mask))
				{
					const EventTypeDefinition eventType = (EventTypeDefinition)eventIndex;
					[[maybe_unused]] const bool wasSubscribed = TryRegisterEvent(cloner.GetParent(), eventType, eventIdentifier, eventManager);
					Assert(wasSubscribed);
				}
				m_events.EmplaceBack(Event{templateEvent.info, eventIdentifier, templateEvent.mask});
			}
		}
		else
		{
			for (const Event& templateEvent : templateComponent.m_events)
			{
				m_events.EmplaceBack(Event{templateEvent.info, Events::Identifier{}, templateEvent.mask});
			}
		}
	}

	SubscribedEvents::~SubscribedEvents() = default;

	void SubscribedEvents::OnDestroying()
	{
		Events::Manager& eventManager = System::Get<Events::Manager>();
		for (const Event& event : m_events)
		{
			for (const EventMask eventIndex : Memory::GetSetBitsIterator(event.mask))
			{
				UNUSED(eventIndex);
				eventManager.UnsubscribeAll(event.identifier, *this);
			}
		}
	}

	bool SubscribedEvents::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		return writer.SerializeObjectWithCallback(
			"events",
			[this](Serialization::Writer eventWriter)
			{
				for (const Event& subscribedEvent : m_events)
				{
					String key = Widget::GetEventInfoKey(subscribedEvent.info);
					EventMask remainingMask{subscribedEvent.mask};
					eventWriter.SerializeArrayWithCallback(
						key,
						[&remainingMask](Serialization::Writer writer, [[maybe_unused]] const uint32 index)
						{
							const EventMask eventBit = Memory::GetFirstSetIndex(remainingMask);
							remainingMask &= (EventMask) ~(1 << eventBit);

							const EventTypeDefinition eventType = (EventTypeDefinition)eventBit;
							const ConstStringView eventName = GetEventTypeName(eventType);
							return eventName.HasElements() && writer.SerializeInPlace(eventName);
						},
						Memory::GetNumberOfSetBits(remainingMask)
					);
				}

				return true;
			}
		);
	}

	void SubscribedEvents::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& owner)
	{
		if (pReader.IsValid())
		{
			Events::Manager& eventManager = System::Get<Events::Manager>();
			for (const Event& event : m_events)
			{
				for (const EventMask eventIndex : Memory::GetSetBitsIterator(event.mask))
				{
					UNUSED(eventIndex);
					eventManager.UnsubscribeAll(event.identifier, *this);
				}
			}
			m_events.Clear();

			const Serialization::Reader eventsReader = *pReader->FindSerializer("events");

			m_events.Reserve((uint16)eventsReader.GetMemberCount());
			for (const Serialization::Member<Serialization::Reader> eventMember : eventsReader.GetMemberView())
			{
				EventInfo eventInfo = owner.DeserializeEventInfo(eventMember.key);
				EventMask eventMask{0};
				Events::Identifier eventIdentifier;
				if (!owner.GetRootScene().IsTemplate())
				{
					eventIdentifier = ResolveEvent(owner, eventInfo).GetExpected<Events::Identifier>();
				}
				for (const Serialization::Reader eventTypeReader : eventMember.value.GetArrayView())
				{
					const EventTypeDefinition eventType = ParseEventType(*eventTypeReader.ReadInPlace<ConstStringView>());
					eventMask |= 1 << (uint8)eventType;
					[[maybe_unused]] const bool wasSubscribed = TryRegisterEvent(owner, eventType, eventIdentifier, eventManager);
					Assert(wasSubscribed);
				}
				m_events.EmplaceBack(Event{Move(eventInfo), eventIdentifier, eventMask});
			}
		}
	}

	/* static */ Guid SubscribedEvents::GetEventInstanceGuid(Widget& owner, const EventInstanceGuidType type)
	{
		switch (type)
		{
			case EventInstanceGuidType::ThisWidgetParentAssetRootInstanceGuid:
				return owner.GetParent().GetRootAssetWidget().GetInstanceGuid();
			case EventInstanceGuidType::ThisWidgetAssetRootInstanceGuid:
				return owner.GetRootAssetWidget().GetInstanceGuid();
			case EventInstanceGuidType::ThisWidgetParentInstanceGuid:
				return owner.GetParent().GetInstanceGuid();
			case EventInstanceGuidType::ThisWidgetInstanceGuid:
				return owner.GetInstanceGuid();
		}
		ExpectUnreachable();
	}

	/* static */ SubscribedEvents::ResolvedEvent SubscribedEvents::ResolveEvent(Widget& owner, const EventInfo& eventInfo)
	{
		Events::Manager& eventManager = System::Get<Events::Manager>();
		return eventInfo.Visit(
			[&owner, &eventManager](const Guid eventGuid) -> ResolvedEvent
			{
				const Guid guid = Context::Utils::GetGuid(eventGuid, owner, owner.GetSceneRegistry());
				return eventManager.FindOrRegisterEvent(guid);
			},
			[&owner, &eventManager](const EventInstanceGuidType eventInstanceGuidType) -> ResolvedEvent
			{
				return eventManager.FindOrRegisterEvent(GetEventInstanceGuid(owner, eventInstanceGuidType));
			},
			[](const ngine::DataSource::PropertyIdentifier propertyIdentifier) -> ResolvedEvent
			{
				return propertyIdentifier;
			},
			[]() -> ResolvedEvent
			{
				ExpectUnreachable();
			}
		);
	}

	/* static */ SubscribedEvents::EventTypeDefinition SubscribedEvents::ParseEventType(const ConstStringView eventTypeName)
	{
		if (eventTypeName == "set_data_allowed_tags")
		{
			return EventTypeDefinition::SetDataAllowedTags;
		}
		else if (eventTypeName == "set_data_required_tags")
		{
			return EventTypeDefinition::SetDataRequiredTags;
		}
		else if (eventTypeName == "set_data_allowed_items")
		{
			return EventTypeDefinition::SetDataAllowedItems;
		}
		else if (eventTypeName == "clear_data_allowed_items")
		{
			return EventTypeDefinition::ClearDataAllowedItems;
		}
		else if (eventTypeName == "focus_layout_data")
		{
			return EventTypeDefinition::FocusLayoutData;
		}
		else if (eventTypeName == "hide")
		{
			return EventTypeDefinition::Hide;
		}
		else if (eventTypeName == "show")
		{
			return EventTypeDefinition::Show;
		}
		else if (eventTypeName == "toggle_visibility")
		{
			return EventTypeDefinition::ToggleVisiblity;
		}
		else if (eventTypeName == "set_ignored")
		{
			return EventTypeDefinition::SetIgnored;
		}
		else if (eventTypeName == "clear_ignored")
		{
			return EventTypeDefinition::ClearIgnored;
		}
		else if (eventTypeName == "toggle_ignored")
		{
			return EventTypeDefinition::ToggleIgnored;
		}
		else if (eventTypeName == "disable")
		{
			return EventTypeDefinition::Disable;
		}
		else if (eventTypeName == "enable")
		{
			return EventTypeDefinition::Enable;
		}
		else if (eventTypeName == "on")
		{
			return EventTypeDefinition::On;
		}
		else if (eventTypeName == "off")
		{
			return EventTypeDefinition::Off;
		}
		else if (eventTypeName == "toggle")
		{
			return EventTypeDefinition::Toggle;
		}
		else if (eventTypeName == "activate")
		{
			return EventTypeDefinition::Activate;
		}
		else if (eventTypeName == "deactivate")
		{
			return EventTypeDefinition::Deactivate;
		}
		else if (eventTypeName == "toggle_active")
		{
			return EventTypeDefinition::ToggleActive;
		}
		else if (eventTypeName == "open_context_menu")
		{
			return EventTypeDefinition::OpenContextMenu;
		}
		else if (eventTypeName == "open_assets")
		{
			return EventTypeDefinition::OpenAssets;
		}
		else if (eventTypeName == "edit_assets")
		{
			return EventTypeDefinition::EditAssets;
		}
		else if (eventTypeName == "open_asset_in_os")
		{
			return EventTypeDefinition::OpenAssetInOS;
		}
		else if (eventTypeName == "clone_assets_from_template")
		{
			return EventTypeDefinition::CloneAssetsFromTemplate;
		}
		else if (eventTypeName == "open_uris")
		{
			return EventTypeDefinition::OpenURIs;
		}
		else if (eventTypeName == "set_text")
		{
			return EventTypeDefinition::SetText;
		}
		else
		{
			Assert(false, "Widget specified invalid event name!");
			return EventTypeDefinition::Invalid;
		}
	}

	/* static */ ConstStringView SubscribedEvents::GetEventTypeName(const EventTypeDefinition eventType)
	{
		switch (eventType)
		{
			case EventTypeDefinition::SetDataAllowedTags:
				return "set_data_allowed_tags";
			case EventTypeDefinition::SetDataRequiredTags:
				return "set_data_required_tags";
			case EventTypeDefinition::SetDataAllowedItems:
				return "set_data_allowed_items";
			case EventTypeDefinition::ClearDataAllowedItems:
				return "clear_data_allowed_items";
			case EventTypeDefinition::FocusLayoutData:
				return "focus_layout_data";
			case EventTypeDefinition::Hide:
				return "hide";
			case EventTypeDefinition::Show:
				return "show";
			case EventTypeDefinition::ToggleVisiblity:
				return "toggle_visibility";
			case EventTypeDefinition::SetIgnored:
				return "set_ignored";
			case EventTypeDefinition::ClearIgnored:
				return "clear_ignored";
			case EventTypeDefinition::ToggleIgnored:
				return "toggle_ignored";
			case EventTypeDefinition::Disable:
				return "disable";
			case EventTypeDefinition::Enable:
				return "enable";
			case EventTypeDefinition::On:
				return "on";
			case EventTypeDefinition::Off:
				return "off";
			case EventTypeDefinition::Toggle:
				return "toggle";
			case EventTypeDefinition::Activate:
				return "activate";
			case EventTypeDefinition::Deactivate:
				return "deactivate";
			case EventTypeDefinition::ToggleActive:
				return "toggle_active";
			case EventTypeDefinition::OpenContextMenu:
				return "open_context_menu";
			case EventTypeDefinition::OpenAssets:
				return "open_assets";
			case EventTypeDefinition::EditAssets:
				return "edit_assets";
			case EventTypeDefinition::OpenAssetInOS:
				return "open_asset_in_os";
			case EventTypeDefinition::CloneAssetsFromTemplate:
				return "clone_assets_from_template";
			case EventTypeDefinition::OpenURIs:
				return "open_uris";
			case EventTypeDefinition::SetText:
				return "set_text";
			case EventTypeDefinition::Invalid:
				return "";
			case EventTypeDefinition::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	bool SubscribedEvents::TryRegisterEvent(
		Widget& owner, const EventTypeDefinition eventType, const Events::Identifier eventIdentifier, Events::Manager& eventManager
	)
	{
		switch (eventType)
		{
			case EventTypeDefinition::SetDataAllowedTags:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Tag::Mask& tags)
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
						{
							pDataSourceData->SetAllowedTags(owner, Tag::AllowedMask(tags));
						}
						else
						{
							pDataSourceData = owner.CreateDataComponent<Data::DataSource>(
								sceneRegistry,
								Data::DataSource::Initializer{Widgets::Data::Component::Initializer{owner, sceneRegistry}, ngine::DataSource::Identifier{}}
							);
							pDataSourceData->SetAllowedTags(owner, Tag::AllowedMask(tags));
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::SetDataRequiredTags:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Tag::Mask& tags)
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
						{
							pDataSourceData->SetRequiredTags(owner, Tag::RequiredMask(tags));
						}
						else
						{
							pDataSourceData = owner.CreateDataComponent<Data::DataSource>(
								sceneRegistry,
								Data::DataSource::Initializer{Widgets::Data::Component::Initializer{owner, sceneRegistry}, ngine::DataSource::Identifier{}}
							);
							pDataSourceData->SetRequiredTags(owner, Tag::RequiredMask(tags));
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::SetDataAllowedItems:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Asset::Mask& mask)
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
						{
							pDataSourceData->SetAllowedItems(owner, mask);
						}
						else
						{
							pDataSourceData = owner.CreateDataComponent<Data::DataSource>(
								sceneRegistry,
								Data::DataSource::Initializer{Widgets::Data::Component::Initializer{owner, sceneRegistry}, ngine::DataSource::Identifier{}}
							);
							pDataSourceData->SetAllowedItems(owner, mask);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::ClearDataAllowedItems:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
						{
							pDataSourceData->ClearAllowedItems(owner);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::FocusLayoutData:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const ngine::DataSource::GenericDataIndex dataIndex)
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
						{
							pFlexLayoutComponent->UpdateViewToShowVirtualItemAtIndex(owner, sceneRegistry, dataIndex);
						}
						else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
						{
							pGridLayoutComponent->UpdateViewToShowVirtualItemAtIndex(owner, sceneRegistry, dataIndex);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::Hide:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.Hide();
					}
				);
				return true;
			}
			case EventTypeDefinition::Show:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.MakeVisible();
					}
				);
				return true;
			}
			case EventTypeDefinition::ToggleVisiblity:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ToggleVisibility();
					}
				);
				return true;
			}
			case EventTypeDefinition::SetIgnored:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.Ignore(owner.GetSceneRegistry());
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::ClearIgnored:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.Unignore(owner.GetSceneRegistry());
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::ToggleIgnored:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ToggleIgnore(owner.GetSceneRegistry());
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Disable:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.DisableWithChildren();
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Enable:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.EnableWithChildren();
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::On:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ClearToggledOff();
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Off:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.SetToggledOff();
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Toggle:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ToggleOff();
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Activate:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.SetModifiers(Style::Modifier::Active);
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::Deactivate:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ClearModifiers(Style::Modifier::Active);
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::ToggleActive:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						owner.ToggleModifiers(Style::Modifier::Active);
						// For now force whole hierarchy to recalculate because of an issue with children sizing.
						owner.GetRootWidget().RecalculateHierarchy();
					}
				);
				return true;
			}
			case EventTypeDefinition::OpenContextMenu:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner]()
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						Optional<Widget*> pWidget = &owner;
						while (pWidget.IsValid() && !pWidget->HasContextMenu(sceneRegistry))
						{
							pWidget = pWidget->GetParentSafe();
						}

						if (pWidget.IsValid())
						{
							const Math::Rectanglei contentArea = owner.GetContentArea(sceneRegistry);
							const Math::Vector2i coordinate{0, contentArea.GetSize().y};
							[[maybe_unused]] const bool wasContextMenuOpened = pWidget->HandleSpawnContextMenu(coordinate, sceneRegistry);
							Assert(wasContextMenuOpened);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::OpenAssets:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Asset::Mask& assets)
					{
						InlineVector<Rendering::ToolWindow::DocumentData, 1> documents;
						documents.Reserve(assets.GetNumberOfSetBits());
						for (const Asset::Identifier::IndexType identifierIndex : assets.GetSetBitsIterator())
						{
							const Asset::Identifier identifier = Asset::Identifier::MakeFromValidIndex(identifierIndex);
							documents.EmplaceBack(identifier);
						}

						Threading::JobBatch jobBatch = owner.GetOwningWindow()->OpenDocuments(documents, Rendering::ToolWindow::OpenDocumentFlags{});
						if (jobBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::EditAssets:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Asset::Mask& assets)
					{
						InlineVector<Rendering::ToolWindow::DocumentData, 1> documents;
						documents.Reserve(assets.GetNumberOfSetBits());
						for (const Asset::Identifier::IndexType identifierIndex : assets.GetSetBitsIterator())
						{
							const Asset::Identifier identifier = Asset::Identifier::MakeFromValidIndex(identifierIndex);
							documents.EmplaceBack(identifier);
						}

						Threading::JobBatch jobBatch =
							owner.GetOwningWindow()->OpenDocuments(documents, Rendering::ToolWindow::OpenDocumentFlags::EnableEditing);
						if (jobBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::OpenAssetInOS:
#if PLATFORM_DESKTOP
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[](const ArrayView<const Guid> assetGuids)
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();

						for (const Guid assetGuid : assetGuids)
						{
							IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
							if (!assetPath.HasElements())
							{
								assetPath = assetManager.GetAssetLibrary().GetAssetPath(assetGuid);
							}

							Assert(assetPath.HasElements());
							if (assetPath.HasElements())
							{
								Platform::OpenInFileBrowser(assetPath);
							}
						}
					}
				);
				return true;
			}
#else
				return false;
#endif
			case EventTypeDefinition::CloneAssetsFromTemplate:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const Asset::Mask& assets)
					{
						InlineVector<Rendering::ToolWindow::DocumentData, 1> documents;
						documents.Reserve(assets.GetNumberOfSetBits());
						for (const Asset::Identifier::IndexType identifierIndex : assets.GetSetBitsIterator())
						{
							const Asset::Identifier identifier = Asset::Identifier::MakeFromValidIndex(identifierIndex);
							documents.EmplaceBack(identifier);
						}

						Threading::JobBatch jobBatch = owner.GetOwningWindow()->OpenDocuments(
							documents,
							Rendering::ToolWindow::OpenDocumentFlags::EnableEditing | Rendering::ToolWindow::OpenDocumentFlags::Clone
						);
						if (jobBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::OpenURIs:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[](const ArrayView<const IO::URI> uris)
					{
						for (const IO::URI& uri : uris)
						{
							uri.OpenWithAssociatedApplication();
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::SetText:
			{
				eventManager.SubscribeWithDuplicates(
					eventIdentifier,
					this,
					[&owner](const ArrayView<const UnicodeString> strings)
					{
						if (const Optional<Data::TextDrawable*> pTextDrawable = owner.FindDataComponentOfType<Data::TextDrawable>(owner.GetSceneRegistry()))
						{
							pTextDrawable->SetText(owner, strings[0]);
						}
					}
				);
				return true;
			}
			case EventTypeDefinition::Invalid:
			{
				Assert(false, "Widget specified invalid event name!");
				return false;
			}
			case EventTypeDefinition::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	[[maybe_unused]] const bool wasSubscribedEventsTypeRegistered = Reflection::Registry::RegisterType<SubscribedEvents>();
	[[maybe_unused]] const bool wasSubscribedEventsComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SubscribedEvents>>::Make());
}
