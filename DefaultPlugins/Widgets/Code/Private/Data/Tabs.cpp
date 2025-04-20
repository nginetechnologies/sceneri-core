#include <Widgets/Data/Tabs.h>

#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Widget.inl>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/Manager.h>
#include <Widgets/Documents/DocumentWidget.h>
#include "Pipelines/Pipelines.h"

#include <Common/System/Query.h>
#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Context/EventManager.inl>
#include <Engine/Context/Utils.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Devices/LogicalDevice.h>

#include <Common/Math/Floor.h>
#include <Common/Math/Color.h>
#include <Common/Math/Vector2/MultiplicativeInverse.h>
#include <Common/Memory/InlineDynamicBitset.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Common/Memory/AddressOf.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	Tabs::Tabs(
		Widget& owner,
		ngine::DataSource::Cache& dataSourceCache,
		const Guid dataSourceGuid,
		const Guid propertySourceGuid,
		const Guid deactivateEventGuid,
		const Guid becomeActiveEventGuid,
		const Guid becomeInactiveEventGuid,
		const bool applyContext
	)
		: ngine::DataSource::Interface(dataSourceCache.FindOrRegister(
				applyContext ? Context::Utils::GetGuid(dataSourceGuid, owner, owner.GetSceneRegistry()) : dataSourceGuid
			))
		, ngine::PropertySource::Interface(dataSourceCache.GetPropertySourceCache().FindOrRegister(
				applyContext ? Context::Utils::GetGuid(propertySourceGuid, owner, owner.GetSceneRegistry()) : propertySourceGuid
			))
		, m_owner(owner)
		, m_dataSourceGuid(dataSourceGuid)
		, m_propertySourceGuid(propertySourceGuid)
		, m_deactivateEventGuid(deactivateEventGuid)
		, m_becomeActiveEventGuid(becomeActiveEventGuid)
		, m_becomeInactiveEventGuid(becomeInactiveEventGuid)
		, m_tabContentWidgetPropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("tab_content_widget"))
		, m_tabActivePropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("tab_active"))
		, m_hasNoActiveTabPropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("has_no_active_tab"))
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			dataSourceCache.OnCreated(ngine::DataSource::Interface::m_identifier, *this);
			dataSourceCache.GetPropertySourceCache().OnCreated(ngine::PropertySource::Interface::m_identifier, *this);
		}
	}

	Tabs::Tabs(Initializer&& initializer)
		: Tabs(
				initializer.GetParent(),
				System::Get<ngine::DataSource::Cache>(),
				initializer.m_dataSourceGuid,
				initializer.m_propertySourceGuid,
				{},
				{},
				{},
				false
			)
	{
	}

	Tabs::Tabs(const Deserializer& deserializer)
		: Tabs(
				deserializer.GetParent(),
				System::Get<ngine::DataSource::Cache>(),
				*deserializer.m_reader.Read<Guid>("data_source_guid"),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("property_source_guid", Guid::Generate()),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("deactivate_signaled_event_guid", Guid{}),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("activate_event_guid", Guid{}),
				deserializer.m_reader.ReadWithDefaultValue<Guid>("deactivate_event_guid", Guid{}),
				true
			)
	{
		m_flags &= ~(Flags::RequiresActiveTab * deserializer.m_reader.ReadWithDefaultValue<bool>("allow_empty_active_tab", false));
		m_flags &= ~(Flags::AutoActivateTab * (!deserializer.m_reader.ReadWithDefaultValue<bool>("auto_activate_tab", true)));

		if (const Optional<Serialization::Reader> tabsReader = deserializer.m_reader.FindSerializer("tabs"))
		{
			m_tabAssetGuids.Reserve((uint16)tabsReader->GetArraySize());
			if (deserializer.GetParent().GetRootScene().IsTemplate())
			{
				for (const Serialization::Reader tabReader : tabsReader->GetArrayView())
				{
					m_tabAssetGuids.EmplaceBack(*tabReader.Read<Guid>("asset"));
				}
			}
			else
			{
				Threading::JobBatch tabsJobBatch;
				uint16 childIndex = 0;
				for (const Serialization::Reader tabReader : tabsReader->GetArrayView())
				{
					Guid assetGuid = *tabReader.Read<Guid>("asset");
					Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
						assetGuid,
						deserializer.GetSceneRegistry(),
						deserializer.GetParent(),
						[this, &sceneRegistry = deserializer.GetSceneRegistry()](const Optional<Widget*> pWidget)
						{
							if (pWidget != GetActiveTabWidget())
							{
								pWidget->Hide();
							}

							pWidget->DisableSaveToDisk(sceneRegistry);
							pWidget->DisableCloning(sceneRegistry);
						},
						childIndex
					);
					m_tabAssetGuids.EmplaceBack(assetGuid);
					tabsJobBatch.QueueAsNewFinishedStage(jobBatch);
					childIndex++;
				}
				System::Get<Threading::JobManager>().Queue(tabsJobBatch, Threading::JobPriority::UserInterfaceLoading);
			}
		}
	}

	Tabs::Tabs(const Tabs& templateComponent, const Cloner& cloner)
		: ngine::DataSource::Interface(System::Get<ngine::DataSource::Cache>().FindOrRegister(
				Context::Utils::GetGuid(templateComponent.m_dataSourceGuid, cloner.GetParent(), cloner.GetSceneRegistry())
			))
		, ngine::PropertySource::Interface(System::Get<ngine::DataSource::Cache>().GetPropertySourceCache().FindOrRegister(
				Context::Utils::GetGuid(templateComponent.m_propertySourceGuid, cloner.GetParent(), cloner.GetSceneRegistry())
			))
		, m_owner(cloner.GetParent())
		, m_dataSourceGuid(templateComponent.m_dataSourceGuid)
		, m_propertySourceGuid(templateComponent.m_propertySourceGuid)
		, m_deactivateEventGuid(templateComponent.m_deactivateEventGuid)
		, m_becomeActiveEventGuid(templateComponent.m_becomeActiveEventGuid)
		, m_becomeInactiveEventGuid(templateComponent.m_becomeInactiveEventGuid)
		, m_flags(templateComponent.m_flags)
		, m_tabContentWidgetPropertyIdentifier(templateComponent.m_tabContentWidgetPropertyIdentifier)
		, m_tabActivePropertyIdentifier(templateComponent.m_tabActivePropertyIdentifier)
		, m_hasNoActiveTabPropertyIdentifier(templateComponent.m_hasNoActiveTabPropertyIdentifier)
		, m_tabAssetGuids(templateComponent.m_tabAssetGuids)
	{
		Assert(!cloner.GetParent().GetRootScene().IsTemplate());
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache.OnCreated(ngine::DataSource::Interface::m_identifier, *this);
		dataSourceCache.GetPropertySourceCache().OnCreated(ngine::PropertySource::Interface::m_identifier, *this);

		if (m_tabAssetGuids.HasElements())
		{
			Threading::JobBatch tabsJobBatch;
			uint16 childIndex = 0;
			for (const Guid tabAssetGuid : m_tabAssetGuids)
			{
				Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
					tabAssetGuid,
					cloner.GetSceneRegistry(),
					cloner.GetParent(),
					[this, &sceneRegistry = cloner.GetSceneRegistry()](const Optional<Widget*> pWidget)
					{
						Assert(pWidget.IsValid());
						if (LIKELY(pWidget.IsValid()))
						{
							if (pWidget != GetActiveTabWidget())
							{
								pWidget->Hide();
							}

							pWidget->DisableSaveToDisk(sceneRegistry);
							pWidget->DisableCloning(sceneRegistry);
						}
					},
					childIndex
				);
				tabsJobBatch.QueueAsNewFinishedStage(jobBatch);
				childIndex++;
			}
			System::Get<Threading::JobManager>().Queue(tabsJobBatch, Threading::JobPriority::UserInterfaceLoading);
		}
	}

	Tabs::~Tabs()
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSourceCache
			.Deregister(ngine::DataSource::Interface::m_identifier, dataSourceCache.FindGuid(ngine::DataSource::Interface::m_identifier));
		dataSourceCache.GetPropertySourceCache().Deregister(
			ngine::PropertySource::Interface::m_identifier,
			dataSourceCache.GetPropertySourceCache().FindGuid(ngine::PropertySource::Interface::m_identifier)
		);
		/*dataSourceCache.DeregisterProperty(m_tabContentWidgetPropertyIdentifier, "tab_content_widget");
		dataSourceCache.DeregisterProperty(m_tabActivePropertyIdentifier, "tab_active");*/
	}

	void Tabs::OnCreated(Widget& owner)
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			Context::EventManager eventsManager(owner, owner.GetSceneRegistry());
			// Activate tab
			eventsManager.Subscribe(
				"301b0555-33ca-4e84-8755-53756846e4b3"_guid,
				this,
				[&owner](const Entity::ComponentSoftReferences& components)
				{
					Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
					Tabs& tabs = *owner.FindDataComponentOfType<Tabs>(sceneRegistry);

					Assert(components.GetInstanceCount() == 1);
					components.IterateComponents<Entity::Component2D>(
						sceneRegistry,
						[&owner, &tabs, &sceneRegistry](Entity::Component2D& component)
						{
							const uint16 tabIndex = tabs.GetTabIndex(owner, component.AsExpected<Widget>(sceneRegistry));
							if (tabIndex == InvalidTabIndex)
							{
								return;
							}

							if (tabIndex != tabs.GetActiveTabIndex() && tabIndex < owner.GetChildCount())
							{
								tabs.SetActiveTab(owner, tabIndex);
							}
							else
							{
								const Widget::ChildView children = owner.GetChildren();
								if (tabIndex < children.GetSize())
								{
									Widget& currentlyActiveWidget = children[tabIndex];
									if (currentlyActiveWidget.IsHidden())
									{
										currentlyActiveWidget.MakeVisible();
									}
								}
							}
						}
					);
				}
			);
			// Toggle tab
			eventsManager.Subscribe(
				"edd842e7-5952-4613-ad77-9d0d740d5095"_guid,
				this,
				[&owner](const Entity::ComponentSoftReferences& components)
				{
					Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
					Tabs& tabs = *owner.FindDataComponentOfType<Tabs>(sceneRegistry);

					Assert(components.GetInstanceCount() == 1);
					components.IterateComponents<Entity::Component2D>(
						sceneRegistry,
						[&owner, &tabs, &sceneRegistry](Entity::Component2D& component)
						{
							const uint16 tabIndex = tabs.GetTabIndex(owner, component.AsExpected<Widget>(sceneRegistry));
							if (tabIndex == InvalidTabIndex)
							{
								return;
							}

							if (tabIndex != tabs.GetActiveTabIndex())
							{
								tabs.SetActiveTab(owner, tabIndex);
							}
							else
							{
								tabs.SetActiveTab(owner, InvalidTabIndex);
							}
						}
					);
				}
			);
			// Activate tab by index
			eventsManager.Subscribe(
				"2A4B8F5C-FD7C-4C4D-B4E5-E7E0C98C23D3"_guid,
				this,
				[&owner](const ngine::DataSource::GenericDataMask& mask)
				{
					Tabs& tabs = *owner.FindDataComponentOfType<Tabs>(owner.GetSceneRegistry());
					Assert(mask.GetNumberOfSetBits() == 1);

					const uint16 tabIndex = (uint16)mask.GetFirstSetIndex();
					if (tabIndex == InvalidTabIndex)
					{
						return;
					}
					if (tabIndex != tabs.GetActiveTabIndex() && tabIndex < owner.GetChildCount())
					{
						tabs.SetActiveTab(owner, tabIndex);
					}
				}
			);
			// Close tab
			eventsManager.Subscribe(
				"34EA75FA-EC91-4A99-9AD4-512FF9367711"_guid,
				this,
				[&owner](const Entity::ComponentSoftReferences& components)
				{
					Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
					Tabs& tabs = *owner.FindDataComponentOfType<Tabs>(sceneRegistry);

					Assert(components.GetInstanceCount() == 1);
					components.IterateComponents<Entity::Component2D>(
						sceneRegistry,
						[&owner, &tabs, &sceneRegistry](Entity::Component2D& component)
						{
							const uint16 tabIndex = tabs.GetTabIndex(owner, component.AsExpected<Widget>(sceneRegistry));
							if (tabIndex == InvalidTabIndex)
							{
								return;
							}

							if (tabIndex < owner.GetChildCount())
							{
								tabs.CloseTab(owner, tabIndex);
							}
						}
					);
				}
			);
			// Deactivate current tab
			if (m_deactivateEventGuid.IsValid())
			{
				eventsManager.Subscribe(
					m_deactivateEventGuid,
					this,
					[&owner]()
					{
						Tabs& tabs = *owner.FindDataComponentOfType<Tabs>(owner.GetSceneRegistry());
						if (tabs.m_activeTabIndex != InvalidTabIndex)
						{
							tabs.SetActiveTab(owner, InvalidTabIndex);
						}
					}
				);
			}
		}
	}

	void Tabs::OnDestroying(Widget& owner)
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			Context::EventManager eventsManager(owner, owner.GetSceneRegistry());
			// Activate tab
			eventsManager.Unsubscribe("301b0555-33ca-4e84-8755-53756846e4b3"_guid, *this);
			// Toggle tab
			eventsManager.Unsubscribe("edd842e7-5952-4613-ad77-9d0d740d5095"_guid, *this);
			// Activate tab by index
			eventsManager.Unsubscribe("2A4B8F5C-FD7C-4C4D-B4E5-E7E0C98C23D3"_guid, *this);
			// Close tab
			eventsManager.Unsubscribe("34EA75FA-EC91-4A99-9AD4-512FF9367711"_guid, *this);
			// Deactivate current tab
			if (m_deactivateEventGuid.IsValid())
			{
				eventsManager.Unsubscribe(m_deactivateEventGuid, *this);
			}
		}
	}

	void Tabs::OnTabForward(Widget& owner)
	{
		CycleTab(owner, TabCycleMode::Next);
	}

	void Tabs::OnTabBack(Widget& owner)
	{
		CycleTab(owner, TabCycleMode::Previous);
	}

	void Tabs::OnChildAdded(Widget& ownerWidget, Widget&, uint32 childIndex, const Optional<uint16> preferredChildIndex)
	{
		if (preferredChildIndex.IsValid())
		{
			childIndex = *preferredChildIndex;
		}

		const uint16 childCount = (uint16)ownerWidget.GetChildCount();
		if (m_activeTabIndex >= childCount && childIndex == 0 && m_flags.IsSet(Flags::AutoActivateTab))
		{
			SetActiveTab(ownerWidget, (uint16)childIndex);
		}
		ngine::DataSource::Interface::OnDataChanged();
		ngine::PropertySource::Interface::OnDataChanged();
	}

	void Tabs::OnChildRemoved(Widget& owner, Widget&, const uint32 previousChildIndex)
	{
		const uint16 previousActiveTabIndex = m_activeTabIndex;
		m_activeTabIndex = InvalidTabIndex;
		if (previousChildIndex > previousActiveTabIndex)
		{
			m_activeTabIndex = previousActiveTabIndex - 1;
		}
		else if (previousActiveTabIndex == previousChildIndex)
		{
			if (previousChildIndex > 0)
			{
				SetActiveTab(owner, uint16(previousChildIndex - 1));
			}
			else
			{
				if (owner.GetChildCount() > 1)
				{
					SetActiveTab(owner, uint16(previousChildIndex));
				}
			}
		}

		ngine::DataSource::Interface::OnDataChanged();
		ngine::PropertySource::Interface::OnDataChanged();
	}

	Optional<Widget*> Tabs::GetActiveTabWidget() const
	{
		if (m_activeTabIndex != InvalidTabIndex)
		{
			return m_owner.GetChildren()[m_activeTabIndex];
		}
		else
		{
			return Invalid;
		}
	}

	uint16 Tabs::GetTabIndex(Widget& ownerWidget, Widget& childWidget)
	{
		uint16 tabIndex = InvalidTabIndex;
		const Widget::ChildView children = ownerWidget.GetChildren();
		for (const ReferenceWrapper<Widget>& child : children)
		{
			if (&child == &childWidget)
			{
				tabIndex = (uint16)children.GetIteratorIndex(Memory::GetAddressOf(child));
			}
		}
		return tabIndex;
	}

	void Tabs::SetActiveTab(Widget& ownerWidget, const uint16 index)
	{
		Assert(index != m_activeTabIndex);

		Entity::SceneRegistry& sceneRegistry = ownerWidget.GetSceneRegistry();

		{
			Widget::ChildView children = ownerWidget.GetChildren();
			if (m_activeTabIndex < children.GetSize())
			{
				Widget& currentlyActiveWidget = children[m_activeTabIndex];
				currentlyActiveWidget.Hide();

				if (const Optional<Widgets::Data::PropertySource*> pContentPropertySource = currentlyActiveWidget.FindDataComponentOfType<Widgets::Data::PropertySource>(sceneRegistry))
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
					ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

					propertySourceCache.RemoveOnChangedListener(pContentPropertySource->GetPropertySourceIdentifier(), this);
				}
			}
		}

		if (m_activeTabIndex == InvalidTabIndex && m_becomeActiveEventGuid.IsValid())
		{
			Context::EventManager eventsManager(ownerWidget, sceneRegistry);
			eventsManager.Notify(m_becomeActiveEventGuid);
		}

		m_activeTabIndex = index;

		if (index != InvalidTabIndex)
		{
			Widget& newActiveWidget = ownerWidget.GetChildren()[index];
			newActiveWidget.MakeVisible();
			newActiveWidget.GetRootWidget().RecalculateHierarchy();

			if (const Optional<Widgets::Data::PropertySource*> pContentPropertySource = newActiveWidget.FindDataComponentOfType<Widgets::Data::PropertySource>(sceneRegistry))
			{
				ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
				ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();

				propertySourceCache.AddOnChangedListener(
					pContentPropertySource->GetPropertySourceIdentifier(),
					ngine::PropertySource::Cache::OnChangedListenerData{
						*this,
						[](Tabs& tabs)
						{
							tabs.ngine::PropertySource::Interface::OnDataChanged();
						}
					}
				);
			}
		}
		else if (m_becomeInactiveEventGuid.IsValid())
		{
			Context::EventManager eventsManager(ownerWidget, sceneRegistry);
			eventsManager.Notify(m_becomeInactiveEventGuid);
		}

		ngine::DataSource::Interface::OnDataChanged();
		ngine::PropertySource::Interface::OnDataChanged();
	}

	void Tabs::CloseTab(Widget& ownerWidget, const uint16 index)
	{
		System::Get<Engine>().OnBeforeStartFrame.Add(
			*this,
			[this, &ownerWidget, index](Tabs&)
			{
				if (m_activeTabIndex > index)
				{
					m_activeTabIndex--;
				}

				Widget& widget = ownerWidget.GetChildren()[index];
				Entity::SceneRegistry& sceneRegistry = ownerWidget.GetSceneRegistry();
				widget.Disable(sceneRegistry);
				widget.Hide(sceneRegistry);

				if (const Optional<Document::Widget*> pDocumentWidget = widget.As<Document::Widget>(sceneRegistry))
				{
					pDocumentWidget->CloseDocument();
				}

				widget.Destroy(sceneRegistry);

				if (m_activeTabIndex == index && index > 0)
				{
					SetActiveTab(ownerWidget, index - 1);
				}
			}
		);
	}

	void Tabs::CycleTab(Widget& ownerWidget, const TabCycleMode cycleMode)
	{
		uint16 tabIndex = m_activeTabIndex;
		switch (cycleMode)
		{
			case TabCycleMode::Next:
			{
				tabIndex += 1;
				if (tabIndex >= ownerWidget.GetChildCount())
				{
					tabIndex = 0;
				}
			}
			break;

			case TabCycleMode::Previous:
			{
				if (tabIndex == 0)
				{
					tabIndex = uint16(ownerWidget.GetChildCount() - 1);
				}
				else
				{
					tabIndex -= 1;
				}
			}
			break;
		}

		if (tabIndex != m_activeTabIndex)
		{
			SetActiveTab(ownerWidget, tabIndex);
		}
	}

	PanResult Tabs::
		OnStartPan([[maybe_unused]] Widget& ownerWidget, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
	{
		/*const int32 tabHeight = (int32)GetOwningWindow()->GetPixelsFromReferenceHeight(tabHeightReference);
		if (coordinate.y <= tabHeight)
		{
		  // Handle switching of tabs
		  int32 nextTabPosition = 0;
		  for (uint16 tabIndex = 0, tabCount = m_tabs.GetSize(); tabIndex < tabCount; ++tabIndex)
		  {
		    const Math::Vector2ui fontSize = m_fontAtlas.CalculateSize(m_tabs[tabIndex].m_name);
		    nextTabPosition += fontSize.x + GetOwningWindow()->GetPixelsFromReferenceWidth(tabTitleOffsetX) +
		GetOwningWindow()->GetPixelsFromReferenceWidth(tabExtraWidthAtEnd);

		    if (coordinate.x <= nextTabPosition)
		    {
		      m_flags |= Flags::IsDraggingTab;
		      const bool isActiveTab = tabIndex == m_activeTabIndex;
		      if (!isActiveTab)
		      {
		        WidgetInfo& currentlyActiveWidget = *(m_tabs.begin() + m_activeTabIndex);
		        currentlyActiveWidget.m_contentWidget.Hide();

		        m_activeTabIndex = tabIndex;

		        WidgetInfo& newActiveWidget = *(m_tabs.begin() + m_activeTabIndex);
		        newActiveWidget.m_contentWidget.MakeVisible();

		        //SetWindowText(static_cast<HWND>(GetWindowHandle()), newActiveTab.GetTitle().GetData());

		        pointer.SetCursor(Rendering::CursorType::Hand);
		        return PanResult{ this };
		      }

		      break;
		    }
		  }
		}*/

		return PanResult{};
	}

	CursorResult Tabs::
		OnMovePan([[maybe_unused]] Widget& ownerWidget, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
	{
		/*if (m_flags.IsSet(Flags::IsDraggingTab))
		{
		  const int32 tabHeight = (int32)GetOwningWindow()->GetPixelsFromReferenceHeight(tabHeightReference);
		  if (coordinate.y <= tabHeight)
		  {
		    int32 tabPosition = 0;
		    for (uint16 tabIndex = 0, tabCount = m_tabs.GetSize(); tabIndex < tabCount; ++tabIndex)
		    {
		      const Math::Vector2ui fontSize = m_fontAtlas.CalculateSize(m_tabs[tabIndex].m_name);
		      tabPosition += fontSize.x + GetOwningWindow()->GetPixelsFromReferenceWidth(tabTitleOffsetX) +
		GetOwningWindow()->GetPixelsFromReferenceWidth(tabExtraWidthAtEnd);

		      if (coordinate.x <= tabPosition)
		      {
		        pointer.SetCursor(Rendering::CursorType::Hand);

		        // Move the tab to this index
		        if (m_activeTabIndex != tabIndex)
		        {
		          WidgetInfo activeWidget = Move(m_tabs[m_activeTabIndex]);
		          m_tabs.Remove(m_tabs.begin() + m_activeTabIndex);

		          const uint16 newTabIndex = tabIndex;
		          m_activeTabIndex = tabIndex;

		          m_tabs.Emplace(m_tabs.begin() + newTabIndex, Move(activeWidget));
		          return CursorResult{ this };
		        }

		        return CursorResult{ this };
		      }
		    }

		    if (coordinate.x >= tabPosition)
		    {
		      DetachGrabbedTab();
		    }
		  }
		  else
		  {
		    DetachGrabbedTab();
		  }
		}
		*/
		return CursorResult{};
	}

	CursorResult Tabs::OnEndPan(
		[[maybe_unused]] Widget& ownerWidget,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const LocalWidgetCoordinate coordinate,
		[[maybe_unused]] const Math::Vector2f velocity
	)
	{
		m_flags &= ~Flags::IsDraggingTab;
		return {};
	}

	CursorResult Tabs::OnCancelPan([[maybe_unused]] Widget& ownerWidget, const Input::DeviceIdentifier)
	{
		m_flags &= ~Flags::IsDraggingTab;
		return {};
	}

	void Tabs::DetachGrabbedTab()
	{
		/*Assert(m_flags.IsSet(Flags::IsDraggingTab));

		m_flags &= ~Flags::IsDraggingTab;

		System::Get<Threading::JobManager>().QueueCallback(
		  [this](Threading::JobRunnerThread&)
		  {
		    WidgetInfo& activeWidget = m_tabs[m_activeTabIndex];

		    Math::Vector2i windowPosition = GetOwningWindow()->GetPosition();
		    windowPosition += (Math::Vector2i)GetPosition();

		    ToolWindow& newWindow = ToolWindow::Create(ToolWindow::Initializer{
		      System::Get<Rendering::Renderer>(),
		      GetOwningWindow()->GetLogicalDevice(),
		      activeWidget.m_name,
		      Math::Rectanglei{windowPosition, GetContentArea().GetSize()}});
		    Widgets::Style::Entry dummyProperties;
		    Widgets::DockableLayoutWidget& dockableAreaWidget =
		      newWindow.EmplaceWidget<Widgets::DockableLayoutWidget, Math::Rectanglei, Widgets::Style::Entry&>(
		        newWindow.GetLocalClientArea(),
		        dummyProperties
		      );

		    dockableAreaWidget.AddColumn(0, (Math::Ratiof)0_percent, (Math::Ratiof)100_percent, dummyProperties);
		    dockableAreaWidget.AddRow(0, (Math::Ratiof)0_percent, (Math::Ratiof)100_percent);

		    activeWidget.m_contentWidget.ChangeOwningWindow(newWindow, nullptr);

		    if (m_tabs.GetSize() > 1)
		    {
		      TabWidget& tabWidget =
		        dockableAreaWidget.EmplaceWidgetInColumn<TabWidget>(0, 0, GetExternalStyle(), m_tabAreaWidget.GetExternalStyle(), true);
		      activeWidget.m_contentWidget.SetContentArea(tabWidget.GetTabContentArea());

		      // activeWidget.m_contentWidget.ChangeParent(tabWidget);
		      // tabWidget.m_tabs.EmplaceBack(WidgetInfo{ activeWidget.m_name, activeWidget.m_headerWidget, activeWidget.m_contentWidget });

		      m_tabs.Remove(m_tabs.begin() + m_activeTabIndex);

		      m_activeTabIndex = (uint16)Math::Max(m_activeTabIndex - 1, 0);

		      newWindow.GiveFocus();
		      newWindow.MakeVisible();
		    }
		    else
		    {
		      UniqueRef<Widget> thisWidget = GetOwningWindow()->DetachDockedWidget(*this);
		      m_owningWindow = newWindow;

		      SetContentArea(Math::Rectangleui{ { 0, 0 }, GetContentArea().GetSize() });
		      activeWidget.m_contentWidget.SetContentArea(GetTabContentArea());

		      dockableAreaWidget.EmplaceWidgetInColumn(Move(thisWidget), 0, 0);

		      newWindow.GiveFocus();
		      newWindow.MakeVisible();
		    }
		  },
		  Threading::JobPriority::UserRequestedAsyncMin
		);*/
	}

	/*TabWidget::DragTabResult TabWidget::UpdateDraggingTab(const Input::Mouse& mouse, const Math::Vector2ui relativeCoordinates, bool
	released)
	{
	  float tabPosition = 0.f;
	  for (uint16 tabIndex = 0; tabIndex < m_activeTabIndex; ++tabIndex)
	  {
	    tabPosition += tabWidthTest;
	  }

	  const bool isDetaching = (relativeCoordinates.x < tabPosition)
	    | (relativeCoordinates.x > tabPosition + tabWidthTest)
	    | (relativeCoordinates.y < 0)
	    | (relativeCoordinates.y > tabHeight);

	  if ((GetStyle() == WindowStyle::Child) & (m_tabs.GetSize() == 1) & !released & isDetaching)
	  {
	    // Detach the entire window
	    m_pParent->DetachWindow(*this);
	    m_pParent = nullptr;

	#if PLATFORM_WINDOWS
	    SetParent(static_cast<HWND>(GetWindowHandle()), nullptr);
	#endif

	    SetStyle(WindowStyle::NoFrame);

	#if PLATFORM_WINDOWS
	    ShowWindow(static_cast<HWND>(GetWindowHandle()), SW_SHOW);
	    SetActiveWindow(static_cast<HWND>(GetWindowHandle()));
	#endif

	    m_flags |= Flags::IsDraggingTab;
	    Assert(g_pEmplaceTabIntoWindowIndicator == nullptr);
	    g_pEmplaceTabIntoWindowIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) });

	    g_pDockLeftIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockRightIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockTopIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero,
	Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockBottomIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) });
	  }
	  else
	  {
	    if (released)
	    {
	#if PLATFORM_WINDOWS
	      POINT point = { static_cast<long>(relativeCoordinates.x), static_cast<long>(relativeCoordinates.y) };
	      ClientToScreen(static_cast<HWND>(GetWindowHandle()), &point);

	      HWND otherWindowHandle = WindowFromPoint(point);
	      if ((otherWindowHandle != nullptr) & (otherWindowHandle != GetWindowHandle()))
	      {
	        if (GetWindowThreadProcessId(otherWindowHandle, nullptr) == GetCurrentThreadId())
	        {
	          Window& otherWindow = *reinterpret_cast<Window*>(GetWindowLongPtr(otherWindowHandle, GWLP_USERDATA));
	          if (otherWindow.IsContentWindow())
	          {
	            Window& otherParentWindow = *reinterpret_cast<Window*>(GetWindowLongPtr(GetParent(otherWindowHandle), GWLP_USERDATA));
	            const DragTabResult result = OnDraggedWindowOntoAnother(otherParentWindow, mouse);
	            if (result == DragTabResult::DestroyedSelf)
	            {
	              return DragTabResult::DestroyedSelf;;
	            }
	          }
	          else
	          {
	            const DragTabResult result = OnDraggedWindowOntoAnother(otherWindow, mouse);
	            if (result == DragTabResult::DestroyedSelf)
	            {
	              return DragTabResult::DestroyedSelf;;
	            }
	          }
	        }
	      }
	#endif
	    }
	    else if(isDetaching)
	    {
	      if (m_tabs.GetSize() == 1)
	      {
	        if (!m_flags.IsSet(Flags::IsDraggingTab))
	        {
	          m_flags |= Flags::IsDraggingTab;

	          Assert(g_pEmplaceTabIntoWindowIndicator == nullptr);
	          g_pEmplaceTabIntoWindowIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockLeftIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockRightIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "",
	Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) });
	g_pDockTopIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockBottomIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) });
	        }
	      }
	      else
	      {
	        // Detach one tab, and create a new tab window for it
	        Window& tabContentWindow = *(m_tabs.begin() + m_activeTabIndex);
	        m_tabs.Erase(m_tabs.begin() + m_activeTabIndex);

	        m_activeTabIndex = Math::Select(m_activeTabIndex != 0, m_activeTabIndex - 1, 0);
	        Window& newActiveTabWindow = *(m_tabs.begin() + m_activeTabIndex);
	        newActiveTabWindow.MakeVisible();

	#if PLATFORM_WINDOWS
	        SetWindowText(static_cast<HWND>(GetWindowHandle()), newActiveTabWindow.GetTitle().GetData());
	#endif

	        m_flags &= ~Flags::ClickedOnTab;

	        InvalidateDraw();

	#if PLATFORM_WINDOWS
	        POINT point = { static_cast<long>(relativeCoordinates.x), static_cast<long>(relativeCoordinates.y) };
	        ClientToScreen(static_cast<HWND>(GetWindowHandle()), &point);

	        const Math::Vector2ui tabSize = tabContentWindow.GetSize() + Math::Vector2ui(0, static_cast<uint32>(tabHeight));

	        TabWidget& newTabWindow = TabWidget::Create(tabContentWindow, Math::Vector2i{ point.x, point.y }, tabSize);
	        newTabWindow.m_flags |= Flags::ClickedOnTab | Flags::IsDraggingTab;
	        Assert(g_pEmplaceTabIntoWindowIndicator == nullptr);
	        g_pEmplaceTabIntoWindowIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockLeftIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockRightIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "",
	Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) });
	g_pDockTopIndicator = &DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{
	static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight)) }); g_pDockBottomIndicator =
	&DockingIndicatorsWindow::Create(GetLogicalDevice(), "", Math::Zero, Math::Vector2ui{ static_cast<uint32>(Math::Floor(tabWidthTest)),
	static_cast<uint32>(Math::Floor(tabHeight)) });

	        newTabWindow.MakeVisible();
	#endif

	        return DragTabResult::None;
	      }
	    }
	    else if (m_flags.IsSet(Flags::IsDraggingTab))
	    {
	#if PLATFORM_WINDOWS
	      POINT point = { static_cast<long>(relativeCoordinates.x), static_cast<long>(relativeCoordinates.y) };
	      ClientToScreen(static_cast<HWND>(GetWindowHandle()), &point);

	      auto hideIndicators = []()
	      {
	        if (g_pEmplaceTabIntoWindowIndicator != nullptr)
	        {
	          g_pEmplaceTabIntoWindowIndicator->Hide();
	          g_pDockLeftIndicator->Hide();
	          g_pDockRightIndicator->Hide();
	          g_pDockTopIndicator->Hide();
	          g_pDockBottomIndicator->Hide();
	        }
	      };

	      HWND otherWindowHandle = WindowFromPoint(point);
	      if ((otherWindowHandle != nullptr) & (otherWindowHandle != GetWindowHandle()))
	      {
	        if (GetWindowThreadProcessId(otherWindowHandle, nullptr) == GetCurrentThreadId())
	        {
	          Window& otherWindow = *reinterpret_cast<Window*>(GetWindowLongPtr(otherWindowHandle, GWLP_USERDATA));
	          if (otherWindow.IsContentWindow())
	          {
	            Window& otherParentWindow = *reinterpret_cast<Window*>(GetWindowLongPtr(GetParent(otherWindowHandle), GWLP_USERDATA));
	            OnDraggingWindowOverAnother(otherParentWindow, mouse);
	          }
	          else
	          {
	            OnDraggingWindowOverAnother(otherWindow, mouse);
	          }
	        }
	        else
	        {
	          hideIndicators();
	        }
	      }
	      else
	      {
	        hideIndicators();
	      }
	#endif
	    }

	    if ((GetStyle() != WindowStyle::Child) & m_flags.IsSet(Flags::IsDraggingTab))
	    {
	#if PLATFORM_WINDOWS
	      RECT windowRect;
	      GetWindowRect(static_cast<HWND>(GetWindowHandle()), &windowRect);

	      windowRect.left += relativeCoordinates.x - static_cast<long>(Math::Floor(tabWidthTest * 0.5f));
	      windowRect.top += relativeCoordinates.y - static_cast<long>(Math::Floor(tabHeight * 0.5f));

	      SetWindowPos(static_cast<HWND>(GetWindowHandle()), nullptr, windowRect.left, windowRect.top, 0, 0, SWP_NOSIZE);
	#endif
	    }
	  }

	  return DragTabResult::None;
	}*/

	/*EnumFlags<ToolWindow::DockingOrientation> TabWidget::GetPossibleDockingOrientationsForWindow(const ToolWindow& window) const
	{
	  if (m_layoutColumns.IsEmpty())
	  {
	    return DockingOrientation::Above | DockingOrientation::Below | DockingOrientation::Left | DockingOrientation::Right;
	  }

	  return ToolWindow::GetPossibleDockingOrientationsForWindow(window);
	}*/

	/*void TabWidget::OnDraggingWindowOverAnother(Window&, const Input::Mouse&)
	{
	  // See if we can merge tabs
	  if (otherWindow.GetType() == Type::Tab)
	  {
	    const Math::TVector2<long> otherWindowRelativeMousePositionLong = mouse.GetRelativeCoordinates(otherWindow);
	    const Math::Vector2ui otherWindowRelativeMousePosition = { static_cast<uint32>(otherWindowRelativeMousePositionLong.x),
	static_cast<uint32>(otherWindowRelativeMousePositionLong.y) };

	    TabWidget& otherTab = static_cast<TabWidget&>(otherWindow);

	    if (otherWindowRelativeMousePosition.y <= tabHeight)
	    {
	      uint16 desiredTabIndex = 0;
	      float tabPosition = 0.f;
	      for (uint16 numExistingTabs = otherTab.m_tabs.GetSize(); desiredTabIndex < numExistingTabs; ++desiredTabIndex)
	      {
	        if ((otherWindowRelativeMousePosition.x >= tabPosition) & (otherWindowRelativeMousePosition.x <= tabPosition + tabWidthTest))
	        {
	          break;
	        }

	        tabPosition += tabWidthTest;
	      }

	      const Math::Vector2ui indicatorSize = { static_cast<uint32>(Math::Floor(tabWidthTest)), static_cast<uint32>(Math::Floor(tabHeight))
	};

	#if PLATFORM_WINDOWS
	      RECT windowRect;
	      GetClientRect(static_cast<HWND>(otherTab.GetWindowHandle()), &windowRect);

	      ClientToScreen(static_cast<HWND>(otherTab.GetWindowHandle()), reinterpret_cast<POINT*>(&windowRect.left));

	      windowRect.left += static_cast<uint32>(Math::Floor(tabPosition));

	      SetWindowPos(static_cast<HWND>(g_pEmplaceTabIntoWindowIndicator->GetWindowHandle()), nullptr, windowRect.left, windowRect.top,
	indicatorSize.x, indicatorSize.y, SWP_NOACTIVATE); #endif

	      g_pEmplaceTabIntoWindowIndicator->MakeVisible();
	      g_pDockLeftIndicator->Hide();
	      g_pDockRightIndicator->Hide();
	      g_pDockTopIndicator->Hide();
	      g_pDockBottomIndicator->Hide();
	      return;
	    }

	    // See if we can dock

	#if PLATFORM_WINDOWS
	    HWND parentHandle = GetParent(static_cast<HWND>(otherTab.GetWindowHandle()));
	    ToolWindow* pWindowToDockInto;
	    if (parentHandle != nullptr)
	    {
	      pWindowToDockInto = &static_cast<ToolWindow&>(*reinterpret_cast<Window*>(GetWindowLongPtr(parentHandle, GWLP_USERDATA)));
	    }
	    else
	    {
	      pWindowToDockInto = &otherTab;
	    }

	    const EnumFlags<DockingOrientation> dockingOrientations = pWindowToDockInto->GetPossibleDockingOrientationsForWindow(otherTab);

	    RECT windowRect;
	    GetWindowRect(static_cast<HWND>(otherTab.GetWindowHandle()), &windowRect);

	    windowRect.left += otherTab.GetSize().x / 2u;
	    windowRect.top += otherTab.GetSize().y / 2u;

	    const Math::Vector2i indicatorPosition = { windowRect.left - static_cast<int>(dockIndicatorSize.x / 2u), windowRect.top -
	static_cast<int>(dockIndicatorSize.y / 2) };

	    SetWindowPos(static_cast<HWND>(g_pEmplaceTabIntoWindowIndicator->GetWindowHandle()), nullptr, indicatorPosition.x,
	indicatorPosition.y, dockIndicatorSize.x, dockIndicatorSize.y, SWP_NOACTIVATE); g_pEmplaceTabIntoWindowIndicator->MakeVisible();

	    if (dockingOrientations.IsSet(DockingOrientation::Left))
	    {
	      SetWindowPos(static_cast<HWND>(g_pDockLeftIndicator->GetWindowHandle()), nullptr, indicatorPosition.x - dockIndicatorSize.x -
	dockIndicatorSpacing, indicatorPosition.y, dockIndicatorSize.x, dockIndicatorSize.y, SWP_NOACTIVATE); g_pDockLeftIndicator->MakeVisible();
	    }
	    else
	    {
	      g_pDockLeftIndicator->Hide();
	    }

	    if (dockingOrientations.IsSet(DockingOrientation::Right))
	    {
	      SetWindowPos(static_cast<HWND>(g_pDockRightIndicator->GetWindowHandle()), nullptr, indicatorPosition.x + dockIndicatorSize.x +
	dockIndicatorSpacing, indicatorPosition.y, dockIndicatorSize.x, dockIndicatorSize.y, SWP_NOACTIVATE);
	g_pDockRightIndicator->MakeVisible();
	    }
	    else
	    {
	      g_pDockRightIndicator->Hide();
	    }

	    if (dockingOrientations.IsSet(DockingOrientation::Above))
	    {
	      SetWindowPos(static_cast<HWND>(g_pDockTopIndicator->GetWindowHandle()), nullptr, indicatorPosition.x, indicatorPosition.y -
	dockIndicatorSize.y - dockIndicatorSpacing, dockIndicatorSize.x, dockIndicatorSize.y, SWP_NOACTIVATE); g_pDockTopIndicator->MakeVisible();
	    }
	    else
	    {
	      g_pDockTopIndicator->Hide();
	    }

	    if (dockingOrientations.IsSet(DockingOrientation::Below))
	    {
	      SetWindowPos(static_cast<HWND>(g_pDockBottomIndicator->GetWindowHandle()), nullptr, indicatorPosition.x, indicatorPosition.y +
	dockIndicatorSize.y + dockIndicatorSpacing, dockIndicatorSize.x, dockIndicatorSize.y, SWP_NOACTIVATE);
	g_pDockBottomIndicator->MakeVisible();
	    }
	    else
	    {
	      g_pDockBottomIndicator->Hide();
	    }
	#endif

	    return;
	  }

	  if (g_pEmplaceTabIntoWindowIndicator != nullptr)
	  {
	    g_pEmplaceTabIntoWindowIndicator->Hide();
	    g_pDockLeftIndicator->Hide();
	    g_pDockRightIndicator->Hide();
	    g_pDockTopIndicator->Hide();
	    g_pDockBottomIndicator->Hide();
	  }
	}*/

	/*void TabWidget::MergeTabWindow(TabWidget& otherTabWindow, uint16 desiredTabIndex)
	{
	  Window& activeTabContentWindow = *(m_tabs.begin() + m_activeTabIndex);
	  activeTabContentWindow.Hide();

	  m_activeTabIndex = desiredTabIndex;

	  const Math::Vector2ui contentLocation = Math::Vector2ui(0, static_cast<uint32>(tabHeight));
	  const Math::Vector2ui contentRenderSize = GetSize() - contentLocation;

	#if PLATFORM_WINDOWS
	  for (Rendering::Window& contentWindow : otherTabWindow.m_tabs)
	  {
	    SetParent(static_cast<HWND>(contentWindow.GetWindowHandle()), static_cast<HWND>(GetWindowHandle()));

	    SetWindowPos(static_cast<HWND>(contentWindow.GetWindowHandle()), nullptr, contentLocation.x, contentLocation.y, contentRenderSize.x,
	contentRenderSize.y, 0);
	  }
	#endif

	  m_tabs.MoveFrom(m_tabs.begin() + desiredTabIndex, otherTabWindow.m_tabs);

	  otherTabWindow.Close();

	  Window& newActiveTabContentWindow = *(m_tabs.begin() + m_activeTabIndex);
	  newActiveTabContentWindow.MakeVisible();

	#if PLATFORM_WINDOWS
	  SetWindowText(static_cast<HWND>(GetWindowHandle()), newActiveTabContentWindow.GetTitle().GetData());
	#endif
	}*/

	/*TabWidget::DragTabResult TabWidget::OnDraggedWindowOntoAnother(Window&, const Input::Mouse&)
	{
	  if (otherWindow.GetType() == Type::Tab)
	  {
	    TabWidget& otherTab = static_cast<TabWidget&>(otherWindow);

	    const Math::TVector2<long> otherWindowRelativeMousePositionLong = mouse.GetRelativeCoordinates(otherWindow);
	    const Math::Vector2ui otherWindowRelativeMousePosition = { static_cast<uint32>(otherWindowRelativeMousePositionLong.x),
	static_cast<uint32>(otherWindowRelativeMousePositionLong.y) };

	    // See if we can merge tabs
	    if (otherWindowRelativeMousePosition.y <= tabHeight)
	    {
	      uint16 desiredTabIndex = 0;
	      float tabPosition = 0.f;
	      for (uint16 numExistingTabs = otherTab.m_tabs.GetSize(); desiredTabIndex < numExistingTabs; ++desiredTabIndex)
	      {
	        if ((otherWindowRelativeMousePosition.x >= tabPosition) & (otherWindowRelativeMousePosition.x <= tabPosition + tabWidthTest))
	        {
	          break;
	        }

	        tabPosition += tabWidthTest;
	      }

	      otherTab.MergeTabWindow(*this, desiredTabIndex);
	      return DragTabResult::DestroyedSelf;
	    }

	#if PLATFORM_WINDOWS
	    // See if we can dock
	    HWND parentHandle = GetParent(static_cast<HWND>(otherTab.GetWindowHandle()));
	    ToolWindow* pWindowToDockInto;
	    if (parentHandle != nullptr)
	    {
	      pWindowToDockInto = &static_cast<ToolWindow&>(*reinterpret_cast<Window*>(GetWindowLongPtr(parentHandle, GWLP_USERDATA)));
	    }
	    else
	    {
	      pWindowToDockInto = &otherTab;
	    }

	    POINT mouseCoordinates = { otherWindowRelativeMousePositionLong.x, otherWindowRelativeMousePositionLong.y };
	    ClientToScreen(static_cast<HWND>(otherWindow.GetWindowHandle()), &mouseCoordinates);

	    auto isPointInsideWindow = [](const POINT mouseCoordinates, DockingIndicatorsWindow& window) -> bool
	    {
	      RECT windowRect;
	      GetWindowRect(static_cast<HWND>(window.GetWindowHandle()), &windowRect);
	      return (mouseCoordinates.x >= windowRect.left) & (mouseCoordinates.x <= windowRect.right) & (mouseCoordinates.y >= windowRect.top) &
	(mouseCoordinates.y <= windowRect.bottom);
	    };

	    if(isPointInsideWindow(mouseCoordinates, *g_pEmplaceTabIntoWindowIndicator))
	    {
	      otherTab.MergeTabWindow(*this, otherTab.m_tabs.GetSize());
	      return DragTabResult::DestroyedSelf;
	    }
	    else if (g_pDockLeftIndicator->IsVisible() & isPointInsideWindow(mouseCoordinates, *g_pDockLeftIndicator))
	    {
	      pWindowToDockInto->DockWindowNextTo(*this, otherTab, DockingOrientation::Left);
	    }
	    else if (g_pDockRightIndicator->IsVisible() & isPointInsideWindow(mouseCoordinates, *g_pDockRightIndicator))
	    {
	      pWindowToDockInto->DockWindowNextTo(*this, otherTab, DockingOrientation::Right);
	    }
	    else if (g_pDockTopIndicator->IsVisible() & isPointInsideWindow(mouseCoordinates, *g_pDockTopIndicator))
	    {
	      pWindowToDockInto->DockWindowNextTo(*this, otherTab, DockingOrientation::Above);
	    }
	    else if (g_pDockBottomIndicator->IsVisible() & isPointInsideWindow(mouseCoordinates, *g_pDockBottomIndicator))
	    {
	      pWindowToDockInto->DockWindowNextTo(*this, otherTab, DockingOrientation::Below);
	    }
	#endif
	  }

	  return DragTabResult::None;
	}*/

	void Tabs::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
	{
		cachedQueryOut.ClearAll();

		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		Entity::ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagComponentSceneData =
			sceneRegistry.GetOrCreateComponentTypeData<Entity::Data::Tags>(componentRegistry.FindIdentifier<Entity::Data::Tags>());

		if (query.m_allowedItems.IsValid())
		{
			cachedQueryOut = *query.m_allowedItems;
		}

		using TabMask = ngine::DataSource::GenericDataMask;
		using TabIdentifier = ngine::DataSource::GenericDataIdentifier;
		TabMask availableTabsMask;
		for (uint16 tabIndex = 0, tabCount = (uint16)m_owner.GetChildCount(); tabIndex < tabCount; ++tabIndex)
		{
			availableTabsMask.Set(TabIdentifier::MakeFromValidIndex(tabIndex));
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			TabMask allowedFilterMask;
			for (const TabIdentifier::IndexType tabIndex : availableTabsMask.GetSetBitsIterator(0, sceneRegistry.GetMaximumUsedElementCount()))
			{
				const Widget& tabWidget = m_owner.GetChildren()[tabIndex];
				if (Optional<Entity::Component*> tagComponent = tagComponentSceneData.GetDataComponent(tabWidget.GetIdentifier()))
				{
					const Entity::Data::Tags& componentTags = static_cast<const Entity::Data::Tags&>(*tagComponent);
					if (query.m_allowedFilterMask.AreAnySet(componentTags.GetMask()))
					{
						allowedFilterMask.Set(TabIdentifier::MakeFromValidIndex(tabIndex));
					}
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				cachedQueryOut |= allowedFilterMask;
			}
			else
			{
				cachedQueryOut &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			cachedQueryOut = availableTabsMask;
		}

		const bool hasDisallowed = query.m_disallowedFilterMask.AreAnySet();
		const bool hasRequired = query.m_requiredFilterMask.AreAnySet();
		if (hasRequired || hasDisallowed)
		{
			for (const TabIdentifier::IndexType tabIndex : availableTabsMask.GetSetBitsIterator(0, sceneRegistry.GetMaximumUsedElementCount()))
			{
				const Widget& tabWidget = m_owner.GetChildren()[tabIndex];
				if (Optional<Entity::Component*> tagComponent = tagComponentSceneData.GetDataComponent(tabWidget.GetIdentifier()))
				{
					const Entity::Data::Tags& componentTags = static_cast<const Entity::Data::Tags&>(*tagComponent);
					const Tag::Mask componentTagMask = componentTags.GetMask();
					if (hasDisallowed && componentTagMask.AreAnySet(query.m_disallowedFilterMask))
					{
						cachedQueryOut.Clear(TabIdentifier::MakeFromValidIndex(tabIndex));
					}
					if (hasRequired && !componentTagMask.AreAllSet(query.m_requiredFilterMask))
					{
						cachedQueryOut.Clear(TabIdentifier::MakeFromValidIndex(tabIndex));
					}
				}
			}
		}
	}

	ngine::DataSource::GenericDataIndex Tabs::GetDataCount() const
	{
		return (ngine::DataSource::GenericDataIndex)m_owner.GetChildCount();
	}

	void Tabs::IterateData(
		const CachedQuery& cachedQuery, IterationCallback&& callback, const Math::Range<ngine::DataSource::GenericDataIndex> offset
	) const
	{
		for (const uint16 index : cachedQuery.GetSetBitsIterator((uint16)offset.GetMinimum(), (uint16)offset.GetSize()))
		{
			ReferenceWrapper<Widget> widget{m_owner.GetChildren()[index]};
			callback(widget);
		}
	}

	void Tabs::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			ReferenceWrapper<Widget> widget{m_owner.GetChildren()[identifierIndex]};
			callback(widget);
		}
	}

	ngine::PropertySource::PropertyValue Tabs::GetDataProperty(const Data data, const ngine::DataSource::PropertyIdentifier identifier) const
	{
		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		const Widget& widget = data.GetExpected<ReferenceWrapper<Widget>>();

		if (identifier == m_tabContentWidgetPropertyIdentifier)
		{
			return Entity::ComponentSoftReference(widget, sceneRegistry);
		}
		else if (identifier == m_tabActivePropertyIdentifier)
		{
			return m_activeTabIndex < m_owner.GetChildCount() ? &widget == &m_owner.GetChildren()[m_activeTabIndex] : false;
		}

		// Always return properties from the content widget itself
		if (const Optional<Widgets::Data::PropertySource*> pContentPropertySource = widget.FindDataComponentOfType<Widgets::Data::PropertySource>(sceneRegistry))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
			if (const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(pContentPropertySource->GetPropertySourceIdentifier()))
			{
				pPropertySource->LockRead();
				ngine::PropertySource::PropertyValue value = pPropertySource->GetDataProperty(identifier);
				pPropertySource->UnlockRead();
				return Move(value);
			}
		}

		return {};
	}

	ngine::DataSource::PropertyValue Tabs::GetDataProperty(const PropertyIdentifier identifier) const
	{
		if (identifier == m_hasNoActiveTabPropertyIdentifier)
		{
			return m_activeTabIndex >= m_owner.GetChildCount();
		}

		const uint16 activeTabIndex = m_activeTabIndex;
		if (activeTabIndex < m_owner.GetChildCount())
		{
			Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
			Widget& activeContentWidget = m_owner.GetChildren()[activeTabIndex];
			if (const Optional<Widgets::Data::PropertySource*> pContentPropertySource = activeContentWidget.FindDataComponentOfType<Widgets::Data::PropertySource>(sceneRegistry))
			{
				ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
				ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
				if (const Optional<ngine::PropertySource::Interface*> pPropertySource = propertySourceCache.Get(pContentPropertySource->GetPropertySourceIdentifier()))
				{
					pPropertySource->LockRead();
					ngine::PropertySource::PropertyValue value = pPropertySource->GetDataProperty(identifier);
					pPropertySource->UnlockRead();
					return Move(value);
				}
			}
		}

		return {};
	}

	bool Tabs::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		writer.Serialize("data_source_guid", m_dataSourceGuid);
		writer.Serialize("property_source_guid", m_propertySourceGuid);
		writer.SerializeWithDefaultValue("allow_empty_active_tab", m_flags.IsNotSet(Flags::RequiresActiveTab), false);
		writer.SerializeWithDefaultValue("auto_activate_tab", m_flags.IsSet(Flags::AutoActivateTab), true);

		if (m_tabAssetGuids.HasElements())
		{
			writer.SerializeArrayWithCallback(
				"tabs",
				[tabAssetGuids = m_tabAssetGuids.GetView()](Serialization::Writer writer, const uint16 index)
				{
					writer.GetValue().SetObject();
					return writer.Serialize("asset", tabAssetGuids[index]);
				},
				m_tabAssetGuids.GetSize()
			);
		}
		return true;
	}

	void Tabs::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget&)
	{
		if (pReader.IsValid())
		{
		}
	}

	[[maybe_unused]] const bool wasTabsTypeRegistered = Reflection::Registry::RegisterType<Widgets::Data::Tabs>();
	[[maybe_unused]] const bool wasTabsComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widgets::Data::Tabs>>::Make());
}
