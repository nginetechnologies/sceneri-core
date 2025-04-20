#include <Widgets/Data/GridLayout.h>
#include <Widgets/Data/DataSource.h>

#include <Widgets/Widget.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Manager.h>
#include <Widgets/Data/DataSourceEntry.h>

#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Manager.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Common/Time/Duration.h>
#include <Common/Math/Mod.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Asset/Format/Guid.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/Log.h>
#include <Common/System/Query.h>

namespace ngine::Widgets::Data
{
	GridLayout::GridLayout(Initializer&& initializer)
		: Layout(Forward<Initializer>(initializer))
		, m_childSize(initializer.GetParent().GetStyle().GetSize(
				Style::ValueTypeIdentifier::ChildEntrySize,
				Style::Size{100_percent, 100_percent},
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
	{
		UpdateGridSize(initializer.GetParent(), initializer.GetSceneRegistry());
	}

	bool GridLayout::OnStyleChanged(
		Widget& owner,
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		bool changedAny = Layout::OnStyleChanged(owner, sceneRegistry, style, matchingModifiers, changedStyleValues);
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::ChildEntrySize))
		{
			const Style::Size newChildSize =
				style.GetSize(Style::ValueTypeIdentifier::ChildEntrySize, Style::Size{100_percent, 100_percent}, matchingModifiers);
			changedAny |= m_childSize != newChildSize;
			m_childSize = newChildSize;
		}
		return changedAny;
	}

	float GridLayout::GetMaximumVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		if (HasDataSource(owner, sceneRegistry))
		{
			Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
			const Math::Vector2i entrySize =
				m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
			const Math::Vector2i entryOffset =
				m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

			const uint8 direction = (uint8)m_childOrientation;
			const float itemSize = float(entrySize[direction] + entryOffset[direction]);
			const uint32 directionItemCount = m_childOrientation == Orientation::Horizontal ? m_maximumVisibleRowCount
			                                                                                : m_maximumVisibleColumnCount;

			const Style::CombinedEntry style = owner.GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = owner.GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			const Style::Size preferredSize =
				style.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, matchingModifiers);

			if (preferredSize[(uint8)m_childOrientation].Is<Widgets::Style::FitContentSizeType>())
			{
				availableChildContentArea = owner.GetMaskedContentArea(sceneRegistry, owner.GetOwningWindow()).Mask(availableChildContentArea);
			}

			const uint32 dataCount = GetFilteredDataCount(owner, sceneRegistry);
			const float totalContentSize = Math::Ceil(float(dataCount) / float(directionItemCount)) * itemSize;
			const float availableContentSize = float(availableChildContentArea.GetSize()[direction]);

			const float times = Math::Floor(totalContentSize / availableContentSize);
			if (times >= 1.f) // The content fills at least one virtual view, calculate how many and add remainder
			{
				return Math::Max(times - 1.f, 0.0f) * availableContentSize + Math::Mod(totalContentSize, availableContentSize);
			}
			else
			{
				return 0.f;
			}
		}
		else
		{
			return m_maximumScrollPosition;
		}
	}

	bool GridLayout::UpdateVirtualItemVisibilityInternal(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(pDataSourceData->GetDataSourceIdentifier()))
			{
				Optional<ngine::DataSource::State*> pDataSourceState;
				if (const ngine::DataSource::StateIdentifier dataSourceStateIdentifier = pDataSourceData->GetDataSourceStateIdentifier())
				{
					pDataSourceState = dataSourceCache.GetState(dataSourceStateIdentifier);
				}

				const uint16 childCount = owner.GetChildCount();
				if (childCount > 0)
				{
					uint16 nextChildIndex = 0;
					const auto getChild = [&owner](const uint16 childIndex) -> Optional<Widget*>
					{
						const Widget::ChildView children = owner.GetChildren();
						return childIndex < children.GetSize() ? (Optional<Widget*>)children[childIndex] : Invalid;
					};

					Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
					const Math::Vector2i entrySize =
						m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
					const Math::Vector2i entryOffset =
						m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

					const uint8 direction = (uint8)m_childOrientation;
					const float itemSize = float(entrySize[direction] + entryOffset[direction]);
					const uint32 directionItemCount = m_childOrientation == Orientation::Horizontal ? m_maximumVisibleRowCount
					                                                                                : m_maximumVisibleColumnCount;

					const Style::CombinedEntry style = owner.GetStyle(sceneRegistry);
					const EnumFlags<Style::Modifier> activeModifiers = owner.GetActiveModifiers(sceneRegistry);
					const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
					const Style::Size preferredSize =
						style.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, matchingModifiers);

					if (preferredSize[(uint8)m_childOrientation].Is<Widgets::Style::FitContentSizeType>())
					{
						availableChildContentArea = owner.GetMaskedContentArea(sceneRegistry, owner.GetOwningWindow()).Mask(availableChildContentArea);
					}

					const ngine::DataSource::GenericDataIndex dataCount = (ngine::DataSource::GenericDataIndex
					)GetFilteredDataCount(owner, sceneRegistry);
					const float totalContentSize = Math::Ceil(float(dataCount) / float(directionItemCount)) * itemSize;
					const float availableContentSize = float(availableChildContentArea.GetSize()[direction]);

					const float times = Math::Floor(totalContentSize / availableContentSize);
					if (times >= 1.f) // The content fills at least one virtual view, calculate how many and add remainder
					{
						if (m_overflowType != OverflowType::Hidden)
						{
							owner.EnableInput();
						}
					}
					else
					{
						m_maximumScrollPosition = 0;
					}

					const ngine::DataSource::GenericDataIndex dataIndexStart = ngine::DataSource::GenericDataIndex(
						(ngine::DataSource::GenericDataIndex)Math::Floor(m_virtualScrollPosition / itemSize) * directionItemCount
					);
					const ngine::DataSource::GenericDataIndex dataIndexEnd = ngine::DataSource::GenericDataIndex(
						Math::Max(uint32(1), Math::Min((ngine::DataSource::GenericDataIndex)(dataIndexStart + childCount), dataCount)) - 1u
					);
					const bool dataIndicesChanged = (dataIndexStart != m_lastDataIndexStart) || (dataIndexEnd != m_lastDataIndexEnd);
					if (dataIndicesChanged)
					{
						uint16 counter = 0;
						for (ngine::DataSource::GenericDataIndex i = dataIndexStart; i <= dataIndexEnd; i++)
						{
							if (const Optional<Widget*> pItemWidget = getChild(counter))
							{
								if (pItemWidget->GetActiveModifiers().IsSet(Widgets::Style::Modifier::ToggledOff))
								{
									pItemWidget->ClearToggledOff();
									m_lastActiveIndex = ngine::DataSource::GenericDataIndex(m_lastDataIndexStart + counter);
								}

								Optional<Data::DataSourceEntry*> pEntry = pItemWidget->FindDataComponentOfType<Data::DataSourceEntry>(sceneRegistry);
								if (pEntry.IsValid())
								{
									pEntry->UpdateDataIndex(i);
								}
							}

							++counter;
						}

						if (dataIndexStart <= m_lastActiveIndex && m_lastActiveIndex <= dataIndexEnd)
						{
							if (const Optional<Widget*> pItemWidget = getChild(m_lastActiveIndex - dataIndexStart))
							{
								pItemWidget->SetToggledOff();
							}
						}

						m_lastDataIndexStart = dataIndexStart;
						m_lastDataIndexEnd = dataIndexEnd;

						auto callback = [getChild, &nextChildIndex, &sceneRegistry, &dataSource = *pDataSource, pDataSourceState](
															const ngine::DataSource::Interface::Data data
														) mutable
						{
							if (const Optional<Widget*> pItemWidget = getChild(nextChildIndex))
							{
								pItemWidget->Unignore(sceneRegistry);
								pItemWidget->UpdateFromDataSource(sceneRegistry, Widget::DataSourceEntryInfo{dataSource, data, pDataSourceState});

								nextChildIndex++;
							}
						};

						pDataSource->LockRead();

						CacheOutOfDateDataSourceQuery(owner, sceneRegistry);

						if (const ngine::DataSource::PropertyIdentifier sortedPropertyIdentifier = pDataSourceData->GetSortedPropertyIdentifier();
						    sortedPropertyIdentifier.IsValid())
						{
							pDataSource->IterateData(
								*m_pCachedDataSortingQuery,
								Move(callback),
								Math::Range<ngine::DataSource::GenericDataIndex>::MakeStartToEnd(dataIndexStart, dataIndexEnd)
							);
						}
						else
						{
							pDataSource->IterateData(
								*m_pCachedDataQuery,
								Move(callback),
								Math::Range<ngine::DataSource::GenericDataIndex>::MakeStartToEnd(dataIndexStart, dataIndexEnd)
							);
						}
						pDataSource->UnlockRead();

						for (; nextChildIndex < childCount; ++nextChildIndex)
						{
							if (const Optional<Widget*> pUnusedItemWidget = getChild(nextChildIndex))
							{
								pUnusedItemWidget->Ignore(sceneRegistry);
							}
						}

						return true;
					}
				}
			}
		}
		return false;
	}

	float GridLayout::CalculateNewScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, float position)
	{
		const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		const Math::Vector2i entrySize =
			m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
		const Math::Vector2i entryOffset =
			m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

		const uint8 direction = (uint8)m_childOrientation;
		const float itemSize = float(entrySize[direction] + entryOffset[direction]);

		// This requires syncing widgetInfo in update hierarchy, so for now we disable the optimization
		// const uint32 directionItemCount = m_childOrientation == Orientation::Horizontal ? m_maximumVisibleRowCount
		//                                                                                : m_maximumVisibleColumnCount;
		// const float nextPosition = m_scrollPosition + (position - m_virtualScrollPosition);
		// if (nextPosition >= itemSize && position != m_maximumVirtualScrollPosition)
		//{
		//	owner.RotateChildren(owner.GetChildCount() - directionItemCount);
		//}
		// else if (nextPosition <= 0.f && nextPosition != 0.f)
		//{
		//	owner.RotateChildren(directionItemCount);
		//}

		return Math::Mod(position, itemSize);
	}

	void GridLayout::SetInlineVirtualScrollPosition(
		Widget& owner, Entity::SceneRegistry& sceneRegistry, const float previousParentPosition, const float newParentPosition
	)
	{
		const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		const Math::Vector2i childEntrySize =
			m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
		const Math::Vector2i childEntryOffset =
			m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

		const uint8 direction = (uint8)m_childOrientation;
		const float offsetFromParent = previousParentPosition + (float)owner.GetRelativeLocation()[direction];
		const float localScroll =
			Math::Min(Math::Max(newParentPosition, offsetFromParent) - offsetFromParent, GetMaximumVirtualScrollPosition(owner, sceneRegistry));

		const float childSize = float(childEntrySize[direction] + childEntryOffset[direction]);
		const float times = Math::Floor(localScroll / childSize);
		const float previousTimes = Math::Floor(m_virtualScrollPosition / childSize);

		if (times != previousTimes)
		{
			const uint16 directionItemCount = m_childOrientation == Orientation::Horizontal ? m_maximumVisibleRowCount
			                                                                                : m_maximumVisibleColumnCount;

			if (times > previousTimes && localScroll != GetMaximumVirtualScrollPosition(owner, sceneRegistry))
			{
				owner.RotateChildren(owner.GetChildCount() - directionItemCount);
			}
			else if (localScroll != 0.f)
			{
				owner.RotateChildren(directionItemCount);
			}
		}

		m_scrollPosition = -childSize * times;
		m_virtualScrollPosition = localScroll;
	}

	void GridLayout::UpdateGridSize(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		const Math::Vector2i entrySize =
			m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

		const Style::CombinedEntry style = owner.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = owner.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		const Style::Size preferredSize = style.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, matchingModifiers);

		if (preferredSize[(uint8)m_childOrientation].Is<Widgets::Style::FitContentSizeType>())
		{
			availableChildContentArea = owner.GetMaskedContentArea(sceneRegistry, owner.GetOwningWindow()).Mask(availableChildContentArea);
		}

		//	const Math::Vector2i entryOffset = m_childOffset.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
		const Math::Vector2i itemSize = entrySize; // + entryOffset;

		const uint16 newMaximumVisibleColumnCount = (uint16)Math::Floor((float)availableChildContentArea.GetSize().x / (float)itemSize.x);

		uint16 newMaximumVisibleRowCount = (uint16)entrySize.y > 0
		                                     ? (uint16)Math::Ceil((float)availableChildContentArea.GetSize().y / (float)(itemSize.y))
		                                     : 0;
		++newMaximumVisibleRowCount;

		if (newMaximumVisibleColumnCount != m_maximumVisibleColumnCount || newMaximumVisibleRowCount != m_maximumVisibleRowCount)
		{
			m_maximumVisibleColumnCount = newMaximumVisibleColumnCount;
			m_maximumVisibleRowCount = newMaximumVisibleRowCount;

			ResetCachedDataSourceView();
		}
	}

	void GridLayout::PopulateItemWidgets(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		UpdateGridSize(owner, sceneRegistry);

		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(pDataSourceData->GetDataSourceIdentifier()))
			{
				const Guid dataSourceAssetGuid = pDataSourceData->GetDataSourceAssetGuid();
				if (dataSourceAssetGuid.IsInvalid())
				{
					return;
				}

				Manager& widgetManager = *System::FindPlugin<Manager>();
				WidgetCache& widgetCache = widgetManager.GetWidgetCache();
				const TemplateIdentifier widgetTemplateIdentifier = widgetCache.FindOrRegister(dataSourceAssetGuid);

				if (!m_flags.IsSet(Flags::HasWidgetData))
				{
					if (!m_flags.IsSet(Flags::AwaitingWidgetData))
					{
						m_flags |= Flags::AwaitingWidgetData;

						Entity::ComponentSoftReference widgetReference{owner, sceneRegistry};

						Threading::JobBatch cachedWidgetLoadJobBatch = widgetCache.TryLoad(
							widgetTemplateIdentifier,
							WidgetCache::LoadListenerData{
								&widgetCache,
								[&sceneRegistry, widgetReference](WidgetCache& widgetCache, const TemplateIdentifier templateIdentifier) mutable
								{
									const Optional<Widget*> pOwner = widgetReference.Find<Widget>(sceneRegistry);
									if (pOwner.IsInvalid())
									{
										return EventCallbackResult::Remove;
									}

									const Optional<GridLayout*> pGridLayout = pOwner->FindDataComponentOfType<Data::GridLayout>(sceneRegistry);
									if (pGridLayout.IsInvalid())
									{
										return EventCallbackResult::Remove;
									}

									const bool isValid = widgetCache.GetAssetData(templateIdentifier).m_pWidget.IsValid();
									Assert(isValid);
									if (UNLIKELY(!isValid))
									{
										LogWarning("Layout widget asset data was empty when loading asset!");
										return EventCallbackResult::Remove;
									}

									pGridLayout->m_flags |= Flags::HasWidgetData;
									pGridLayout->m_flags &= ~Flags::AwaitingWidgetData;
									pOwner->GetRootWidget().RecalculateHierarchy();
									return EventCallbackResult::Remove;
								}
							}
						);
						if (cachedWidgetLoadJobBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(cachedWidgetLoadJobBatch);
						}
					}

					return;
				}

				const uint16 maximumVisibleItemCount = (uint16
				)Math::Min(uint32(m_maximumVisibleRowCount * m_maximumVisibleColumnCount), pDataSourceData->GetMaximumDataCount());
				uint16 childCount = (uint16)owner.GetChildCount();

				if (childCount >= maximumVisibleItemCount)
				{
					/*while(childCount > maximumVisibleItemCount)
					{
					  owner.GetChildren()[--childCount].Destroy();
					}*/
					return;
				}

				for (uint16 index = childCount, count = maximumVisibleItemCount; index < count; ++index)
				{
					Threading::JobBatch cachedWidgetLoadJobBatch = widgetCache.TryLoad(
						widgetTemplateIdentifier,
						WidgetCache::LoadListenerData{
							&widgetCache,
							[&owner, &sceneRegistry, index](WidgetCache& widgetCache, const TemplateIdentifier templateIdentifier) mutable
							{
								const Widgets::Template& widgetTemplate = widgetCache.GetAssetData(templateIdentifier);
								Threading::JobBatch jobBatch;
								Entity::Manager& entityManager = System::Get<Entity::Manager>();
								Optional<Entity::Component*> pComponent = widgetTemplate.m_pWidget->GetTypeInfo()->CloneFromTemplateWithChildren(
									Guid::Generate(),
									*widgetTemplate.m_pWidget,
									widgetTemplate.m_pWidget->GetParent(),
									owner,
									entityManager.GetRegistry(),
									sceneRegistry,
									entityManager.GetComponentTemplateCache().GetTemplateSceneRegistry(),
									jobBatch,
									index
								);
								if (pComponent.IsValid())
								{
									Widget& widget = static_cast<Entity::HierarchyComponentBase&>(*pComponent).AsExpected<Widget>(sceneRegistry);
#if PROFILE_BUILD
									String debugName;
									debugName.Format("{}[{}]", widget.GetDebugName(), index);
									widget.SetDebugName(Move(debugName));
#endif

									widget.DisableSaveToDisk(sceneRegistry);
									widget.CreateDataComponent<Data::DataSourceEntry>(
										sceneRegistry,
										Data::DataSourceEntry::Initializer{Widgets::Data::Component::Initializer{widget, sceneRegistry}}
									);
								}
								if (jobBatch.IsValid())
								{
									Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
								}
								return EventCallbackResult::Remove;
							}
						},
						WidgetCache::LoadFlags::Default & ~WidgetCache::LoadFlags::AlwaysDeferCallback
					);
					if (cachedWidgetLoadJobBatch.IsValid())
					{
						Threading::JobRunnerThread::GetCurrent()->Queue(cachedWidgetLoadJobBatch);
					}
				}
			}
		}
	}

	Math::Vector2i GridLayout::GetInitialChildLocation(
		[[maybe_unused]] const Widget& owner,
		Entity::SceneRegistry& sceneRegistry,
		const Widget& childWidget,
		[[maybe_unused]] const Math::Vector2i size
	) const
	{
		uint32 childIndex;
		{
			const Widget::ChildView children = owner.GetChildren();
			auto it = children.Find(childWidget);
			if (it.IsValid())
			{
				childIndex = children.GetIteratorIndex(it);
			}
			else
			{
				childIndex = children.GetSize();
			}
		}

		const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		const Math::Vector2i entrySize =
			m_childSize.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());
		const Math::Vector2i entryOffset = m_childOffset.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

		const int columnIndex = childIndex % m_maximumVisibleColumnCount;
		const int rowIndex = (int)Math::Floor((float)childIndex / (float)m_maximumVisibleColumnCount);
		return availableChildContentArea.GetPosition() + (entrySize + entryOffset) * Math::Vector2i{columnIndex, rowIndex};
	}

	void GridLayout::EnableUpdate(Widget&, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<GridLayout>& layoutSceneData = *sceneRegistry.FindComponentTypeData<GridLayout>();
		if (!layoutSceneData.IsUpdateEnabled(*this))
		{
			layoutSceneData.EnableUpdate(*this);
		}
	}

	void GridLayout::DisableUpdate(Widget&, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<GridLayout>& layoutSceneData = *sceneRegistry.FindComponentTypeData<GridLayout>();
		if (layoutSceneData.IsUpdateEnabled(*this))
		{
			layoutSceneData.DisableUpdate(*this);
		}
	}

	[[maybe_unused]] const bool wasGridLayoutTypeRegistered = Reflection::Registry::RegisterType<GridLayout>();
	[[maybe_unused]] const bool wasGridLayoutComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<GridLayout>>::Make());
}
