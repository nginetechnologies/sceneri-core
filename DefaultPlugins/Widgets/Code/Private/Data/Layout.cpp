#include "Data/Layout.h"
#include "Data/GridLayout.h"
#include "Data/TextDrawable.h"
#include "Data/DataSource.h"
#include "Data/ExternalStyle.h"
#include "Data/InlineStyle.h"
#include "Data/DynamicStyle.h"
#include "Data/Modifiers.h"

#include <Widgets/Manager.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/DynamicEntry.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Data/DataSourceEntry.h>

#include <Common/System/Query.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Common/Memory/AddressOf.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/Vector2/Floor.h>
#include <Common/Math/Primitives/Serialization/RectangleEdges.h>
#include <Common/Math/Mod.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/Log.h>

namespace ngine::Widgets::Data
{
	[[nodiscard]] Style::SizeAxisEdges GetEdgesFromStyle(
		const Style::CombinedEntry& entry,
		const Style::ValueTypeIdentifier valueTypeIdentifier,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers
	)
	{
		const Optional<const Style::EntryValue*> value = entry.Find(valueTypeIdentifier, matchingModifiers);
		if (value.IsValid())
		{
			if (const Optional<const Style::Size*> pSize = value->Get<Style::Size>())
			{
				const Style::Size size = *pSize;
				return {size.x, size.y, size.x, size.y};
			}
			else if (const Optional<const Style::SizeAxisEdges*> pEdges = value->Get<Style::SizeAxisEdges>())
			{
				return *pEdges;
			}
		}

		return Style::SizeAxisEdges{Math::Zero};
	}

	Math::Rectanglei CalculateContentAreaWithMargin(
		Rendering::ToolWindow& owningWindow, const Math::Rectanglei contentArea, const Style::SizeAxisEdges margins
	)
	{
		const Rendering::ScreenProperties screenProperties = owningWindow.GetCurrentScreenProperties();
		const int32 left = margins.m_left.Get(contentArea.GetSize().x, screenProperties);
		const int32 right = margins.m_right.Get(contentArea.GetSize().x, screenProperties);
		const int32 top = margins.m_top.Get(contentArea.GetSize().y, screenProperties);
		const int32 bottom = margins.m_bottom.Get(contentArea.GetSize().y, screenProperties);

		return {
			contentArea.GetPosition() + Math::Vector2i{left, top},
			Math::Max(contentArea.GetSize() - Math::Vector2i{right + left, bottom + top}, (Math::Vector2i)Math::Zero)
		};
	}

	Layout::Layout(Initializer&& initializer)
		: BaseType()
		, m_owner(initializer.GetParent())
		, m_margin(GetEdgesFromStyle(
				initializer.GetParent().GetStyle(),
				Style::ValueTypeIdentifier::Margin,
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
		, m_padding(GetEdgesFromStyle(
				initializer.GetParent().GetStyle(),
				Style::ValueTypeIdentifier::Padding,
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
		, m_childOffset(initializer.GetParent().GetStyle().GetSize(
				Style::ValueTypeIdentifier::ChildOffset,
				0_px,
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
		, m_childOrientation(initializer.GetParent().GetStyle().GetWithDefault<Orientation>(
				Style::ValueTypeIdentifier::Orientation,
				Orientation::Horizontal,
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
		, m_overflowType(initializer.GetParent().GetStyle().GetWithDefault<OverflowType>(
				Style::ValueTypeIdentifier::OverflowType,
				OverflowType(DefaultOverflowType),
				initializer.GetParent().GetStyle().GetMatchingModifiers(initializer.GetParent().GetActiveModifiers())
			))
	{
	}

	Layout::~Layout()
	{
	}

	void Layout::OnDataSourceChanged(Widget& owner)
	{
		ResetCachedDataSourceView();

		// Make sure we recreate the cached data
		if (m_pCachedDataQuery.IsValid())
		{
			m_pCachedDataQuery->ClearAll();
		}

		owner.RecalculateHierarchy();
	}

	bool Layout::HasDataSource(Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		return owner.HasDataComponentOfType<Data::DataSource>(sceneRegistry);
	}

	void Layout::OnDataSourcePropertiesChanged(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		ResetCachedDataSourceView();

		// Make sure we request the data again
		if (m_pCachedDataQuery.IsValid())
		{
			m_pCachedDataQuery->ClearAll();
		}

		if (HasDataSource(owner, sceneRegistry))
		{
			// m_scrollPosition = m_virtualScrollPosition = 0;
			owner.RecalculateHierarchy(sceneRegistry);
		}
	}

	bool Layout::OnStyleChanged(
		Widget& owner,
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		bool changedAny = false;
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Margin))
		{
			const Style::SizeAxisEdges newMargin = GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Margin, matchingModifiers);
			changedAny |= m_margin != newMargin;
			m_margin = newMargin;
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Padding))
		{
			const Style::SizeAxisEdges newPadding = GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Padding, matchingModifiers);
			changedAny |= newPadding != newPadding;
			m_padding = newPadding;
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::ChildOffset))
		{
			const Style::Size newChildOffset = style.GetSize(Style::ValueTypeIdentifier::ChildOffset, 0_px, matchingModifiers);
			changedAny |= m_childOffset != newChildOffset;
			m_childOffset = newChildOffset;
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Orientation))
		{
			const Orientation newChildOrientation =
				style.GetWithDefault<Orientation>(Style::ValueTypeIdentifier::Orientation, Orientation::Horizontal, matchingModifiers);
			changedAny |= m_childOrientation != newChildOrientation;
			m_childOrientation = newChildOrientation;
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::OverflowType))
		{
			const OverflowType overflowType =
				style.GetWithDefault<OverflowType>(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);
			changedAny |= m_overflowType != overflowType;
			m_overflowType = overflowType;
		}

		if (changedAny)
		{
			owner.RecalculateHierarchy(sceneRegistry);
		}
		return changedAny;
	}

	Math::Rectanglei Layout::GetAvailableChildContentArea(const Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		return CalculateContentAreaWithMargin(*owner.GetOwningWindow(), owner.GetContentArea(sceneRegistry), m_padding);
	}

	Math::Vector2i Layout::GetNextAutoChildPosition(const Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		const Style::SizeAxisEdges padding = m_padding;
		const Style::Size startOffsetStyle{padding.m_left, padding.m_top};
		const uint8 direction = (uint8)m_childOrientation;

		const Math::Rectanglei availableContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		Math::Vector2i result;
		if (owner.HasChildren())
		{
			result[direction] = owner.GetChildren().GetLastElement().GetContentArea(sceneRegistry).GetEndPosition()[direction] +
			                    m_childOffset.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties())[direction];
			result[!direction] = availableContentArea.GetPosition()[!direction];
		}
		else
		{
			result = availableContentArea.GetPosition();
			result[direction] += startOffsetStyle.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties())[direction];
		}
		return result;
	}

	[[nodiscard]] Math::Vector2i GetAlignedPosition(
		const Array<Alignment, 2> alignments,
		uint8 direction,
		Math::Vector2i position,
		const Math::Vector2i size,
		const Math::Rectanglei childContentArea,
		const bool isLastElement
	)
	{
		switch (alignments[direction])
		{
			case Alignment::Center:
			{
				if (isLastElement)
				{
					const int32 remainingSize = childContentArea.GetEndPosition()[direction] - position[direction];
					position[direction] += (remainingSize - size[direction]) / 2;
				}
			}
			break;
			case Alignment::End:
			{
				if (isLastElement)
				{
					position[direction] = childContentArea.GetEndPosition()[direction] - size[direction];
				}
			}
			break;
			default:
				break;
		}

		direction = !direction;
		switch (alignments[direction])
		{
			case Alignment::Center:
				position[direction] = position[direction] + (childContentArea.GetSize()[direction] - size[direction]) / 2;
				break;
			case Alignment::End:
				position[direction] = position[direction] + childContentArea.GetSize()[direction] - size[direction];
				break;
			default:
				break;
		}

		return position;
	}

	Math::Vector2i FlexLayout::GetInitialChildLocation(
		const Widget& owner,
		Entity::SceneRegistry& sceneRegistry,
		[[maybe_unused]] const Widget& childWidget,
		[[maybe_unused]] const Math::Vector2i size
	) const
	{
		return GetNextAutoChildPosition(owner, sceneRegistry);
	}

	void FlexLayout::PopulateItemWidgets(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry);
		    pDataSourceData.IsValid() && pDataSourceData->GetDataSourceAssetGuid().IsValid())
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

								const Optional<FlexLayout*> pFlexLayout = pOwner->FindDataComponentOfType<Data::FlexLayout>(sceneRegistry);
								if (pFlexLayout.IsInvalid())
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

								pFlexLayout->m_flags |= Flags::HasWidgetData;
								pFlexLayout->m_flags &= ~Flags::AwaitingWidgetData;
								pOwner->GetRootWidget().RecalculateHierarchy(sceneRegistry);
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

				return;
			}

			Rendering::ToolWindow& owningWindow = *owner.GetOwningWindow();

			// Start with spawning the first widget
			if (!owner.HasChildren())
			{
				Threading::JobBatch cachedWidgetLoadJobBatch = widgetCache.TryLoad(
					widgetTemplateIdentifier,
					WidgetCache::LoadListenerData{
						&widgetCache,
						[&owner, &sceneRegistry](WidgetCache& widgetCache, const TemplateIdentifier templateIdentifier) mutable
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
								(uint16)0u
							);
							if (pComponent.IsValid())
							{
								Widget& widget = static_cast<Entity::HierarchyComponentBase&>(*pComponent).AsExpected<Widget>(sceneRegistry);
#if PROFILE_BUILD
								String debugName;
								debugName.Format("{}[{}]", widget.GetDebugName(), 0);
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

			if (owner.HasChildren())
			{
				const Style::CombinedEntry style = owner.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = owner.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				const Style::Size preferredSize = style.GetWithDefault(Style::ValueTypeIdentifier::PreferredSize, Style::Size(), matchingModifiers);
				const uint8 direction = (uint8)GetChildOrientation();

				uint32 spawnedChildCount;
				{
					Widget& firstWidget = owner.GetChildren()[0];
					const Style::CombinedEntry firstWidgetStyle = firstWidget.GetStyle(sceneRegistry);
					const EnumFlags<Style::Modifier> firstWidgetActiveModifiers = firstWidget.GetActiveModifiers(sceneRegistry);
					const Style::CombinedEntry::MatchingModifiers firstWidgetMatchingModifiers =
						firstWidgetStyle.GetMatchingModifiers(firstWidgetActiveModifiers);

					Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
						*sceneRegistry.FindComponentTypeData<Data::ExternalStyle>();
					Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>(
					);
					Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
						*sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
					Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();
					Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
						*sceneRegistry.FindComponentTypeData<Data::TextDrawable>();

					const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
					const Math::Vector2i entrySize = firstWidget
					                                   .GetPreferredSize(
																							 sceneRegistry,
																							 firstWidgetStyle,
																							 firstWidgetMatchingModifiers,
																							 externalStyleSceneData,
																							 inlineStyleSceneData,
																							 dynamicStyleSceneData,
																							 modifiersSceneData,
																							 textDrawableSceneData
																						 )
					                                   .Get(availableChildContentArea.GetSize(), owningWindow.GetCurrentScreenProperties());
					const Math::Vector2i entryOffset =
						m_childOffset.Get(availableChildContentArea.GetSize(), owningWindow.GetCurrentScreenProperties());

					const float firstWidgetGrowth =
						firstWidgetStyle.GetWithDefault(Style::ValueTypeIdentifier::ChildGrowthFactor, 0.f, firstWidgetMatchingModifiers);
					const float firstWidgetShrinkage =
						firstWidgetStyle.GetWithDefault(Style::ValueTypeIdentifier::ChildShrinkFactor, 0.f, firstWidgetMatchingModifiers);

					const uint32 maxChildCount = GetFilteredDataCount(owner, sceneRegistry);
					if (preferredSize[direction].Is<Style::FitContentSizeType>() || firstWidgetGrowth > 0.f || firstWidgetShrinkage > 0.f)
					{
						spawnedChildCount = maxChildCount;
					}
					else
					{
						spawnedChildCount =
							(entrySize[direction] > 0
						     ? (uint16)Math::Ceil(
										 (float)availableChildContentArea.GetSize()[direction] / (float)(entrySize[direction] + entryOffset[direction])
									 )
						     : 0);
						spawnedChildCount++;
						spawnedChildCount = Math::Min(spawnedChildCount, maxChildCount);
					}
				}

				// Check for early out
				uint32 childCount = owner.GetChildCount();
				if (childCount == spawnedChildCount)
				{
					return;
				}
				else if (childCount > spawnedChildCount)
				{
					return; // This could happen when we switch from portrait to landscape or vice versa
				}

				for (uint16 index = 1, count = uint16(spawnedChildCount); index < count; ++index)
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

	bool Layout::UpdateVirtualItemVisibility(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		if (owner.IsVisible() && owner.IsEnabled(sceneRegistry))
		{
			return UpdateVirtualItemVisibilityInternal(owner, sceneRegistry);
		}
		return false;
	}

	bool FlexLayout::UpdateVirtualItemVisibilityInternal(Widget& owner, Entity::SceneRegistry& sceneRegistry)
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

					const Widget& firstChild = *getChild(0);
					const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
					const Math::Vector2i entrySize = firstChild.GetSize(sceneRegistry);
					const Math::Vector2i entryOffset =
						m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

					const uint8 direction = (uint8)m_childOrientation;
					const float itemSize = float(entrySize[direction] + entryOffset[direction]);

					const uint32 dataCount = GetFilteredDataCount(owner, sceneRegistry);
					const float totalContentSize = float(dataCount) * itemSize;
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

					const ngine::DataSource::GenericDataIndex dataIndexStart = (ngine::DataSource::GenericDataIndex
					)Math::Floor(m_virtualScrollPosition / itemSize);
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
								if (pItemWidget->GetActiveModifiers(sceneRegistry).IsSet(Widgets::Style::Modifier::ToggledOff))
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

						if (pDataSourceData->GetSortedPropertyIdentifier().IsValid())
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

	float FlexLayout::CalculateNewScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, float position)
	{
		const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
		const Math::Vector2i entrySize = owner.GetChildren()[0].GetSize(sceneRegistry);
		const Math::Vector2i entryOffset =
			m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

		const uint8 direction = (uint8)m_childOrientation;
		const float itemSize = float(entrySize[direction] + entryOffset[direction]);

		// This requires syncing widgetInfo in update hierarchy, so for now we disable the optimization
		// const float nextPosition = m_scrollPosition + (position - m_virtualScrollPosition);
		// if (nextPosition >= itemSize && position != m_maximumVirtualScrollPosition)
		//{
		//	owner.RotateChildren(owner.GetChildCount() - 1);
		//}
		// else if (nextPosition <= 0.f && nextPosition != 0.f)
		//{
		//	owner.RotateChildren(1);
		//}

		return Math::Mod(position, itemSize);
	}

	uint32 Layout::GetFilteredDataCount(Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(pDataSourceData->GetDataSourceIdentifier()))
			{
				pDataSource->LockRead();
				ngine::DataSource::GenericDataIndex dataCount;
				if (const Optional<const Tag::Query*> pTagQuery = pDataSourceData->GetTagQuery())
				{
					CacheOutOfDateDataSourceQuery(owner, sceneRegistry);
					dataCount = m_pCachedDataQuery->GetNumberOfSetBits() + 1;
				}
				else
				{
					dataCount = pDataSource->GetDataCount();
				}
				pDataSource->UnlockRead();
				return Math::Min(dataCount, pDataSourceData->GetMaximumDataCount());
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	void Layout::CacheOutOfDateDataSourceQuery(Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(pDataSourceData->GetDataSourceIdentifier()))
			{
				bool isOutOfDate = false;
				if (m_pCachedDataQuery.IsValid())
				{
					ngine::DataSource::CachedQuery& cachedQuery = *m_pCachedDataQuery;
					// Consider the cached data invalid if no bits are set
					isOutOfDate |= !cachedQuery.AreAnySet();
				}
				else
				{
					m_pCachedDataQuery.CreateInPlace();
					isOutOfDate = true;
				}

				if (isOutOfDate)
				{
					ngine::DataSource::CachedQuery& cachedQuery = *m_pCachedDataQuery;
					if (const Optional<const Tag::Query*> pTagQuery = pDataSourceData->GetTagQuery())
					{
						pDataSource->CacheQuery(*pTagQuery, cachedQuery);
					}
					else
					{
						pDataSource->CacheQuery(ngine::DataSource::Interface::Query{}, cachedQuery);
					}
				}

				if (const ngine::DataSource::PropertyIdentifier sortedPropertyIdentifier = pDataSourceData->GetSortedPropertyIdentifier();
				    sortedPropertyIdentifier.IsValid())
				{
					if (m_pCachedDataSortingQuery.IsInvalid())
					{
						m_pCachedDataSortingQuery.CreateInPlace();
						isOutOfDate = true;
					}

					if (isOutOfDate)
					{
						ngine::DataSource::SortedQueryIndices& sortedQuery = *m_pCachedDataSortingQuery;
						[[maybe_unused]] const bool wasSorted =
							pDataSource->SortQuery(*m_pCachedDataQuery, sortedPropertyIdentifier, pDataSourceData->GetSortingOrder(), sortedQuery);
						Assert(wasSorted, "Attempting to sort by unsupported property");
					}
				}
			}
		}
	}

	void Layout::SetVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, const float position)
	{
		const float newVirtualScrollPosition = Math::Clamp<float>(position, 0.f, GetMaximumVirtualScrollPosition(owner, sceneRegistry));
		const float previousVirtualScrollPosition = m_virtualScrollPosition;
		const bool changed = newVirtualScrollPosition != previousVirtualScrollPosition;
		if (changed)
		{
			if (HasDataSource(owner, sceneRegistry))
			{
				m_scrollPosition = CalculateNewScrollPosition(owner, sceneRegistry, newVirtualScrollPosition);
				m_virtualScrollPosition = newVirtualScrollPosition;
			}
			else
			{
				m_scrollPosition = newVirtualScrollPosition;
				m_virtualScrollPosition = newVirtualScrollPosition;
			}

			for (Widget& childWidget : owner.GetChildren())
			{
				if (const Optional<GridLayout*> pChildGridLayout = childWidget.FindDataComponentOfType<GridLayout>(sceneRegistry))
				{
					const Style::CombinedEntry childStyle = childWidget.GetStyle(sceneRegistry);
					const EnumFlags<Style::Modifier> childActiveModifiers = childWidget.GetActiveModifiers(sceneRegistry);
					const Style::CombinedEntry::MatchingModifiers childMatchingModifiers = childStyle.GetMatchingModifiers(childActiveModifiers);
					const Style::Size childPreferredSize =
						childStyle.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, childMatchingModifiers);

					if (childPreferredSize[(uint8)pChildGridLayout->m_childOrientation].Is<Widgets::Style::FitContentSizeType>())
					{
						pChildGridLayout
							->SetInlineVirtualScrollPosition(childWidget, sceneRegistry, previousVirtualScrollPosition, newVirtualScrollPosition);
					}
				}
			}

			owner.RecalculateHierarchy(sceneRegistry);

			const bool reachedEnd = newVirtualScrollPosition >= m_maximumScrollPosition;
			if (reachedEnd)
			{
				RequestMoreData(owner, sceneRegistry);
			}
		}
	}

	void Layout::RequestMoreData(Widget& owner, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::DataSource*> pDataSourceData = owner.FindDataComponentOfType<Data::DataSource>(sceneRegistry);
		    pDataSourceData.IsValid() && m_pCachedDataSortingQuery.IsValid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

			const uint32 maximumRowCount = 15; // TODO: Detect
			dataSourceCache.RequestMoreData(
				pDataSourceData->GetDataSourceIdentifier(),
				maximumRowCount,
				pDataSourceData->GetSortedPropertyIdentifier(),
				pDataSourceData->GetSortingOrder(),
				pDataSourceData->GetTagQuery(),
				*m_pCachedDataSortingQuery,
				m_lastDataIndexEnd
			);
		}
		else
		{
			for (Widget& childWidget : owner.GetChildren())
			{
				if (const Optional<FlexLayout*> pChildFlexLayout = childWidget.FindDataComponentOfType<FlexLayout>(sceneRegistry))
				{
					pChildFlexLayout->RequestMoreData(childWidget, sceneRegistry);
				}
				else if (const Optional<GridLayout*> pChildGridLayout = childWidget.FindDataComponentOfType<GridLayout>(sceneRegistry))
				{
					pChildGridLayout->RequestMoreData(childWidget, sceneRegistry);
				}
			}
		}
	}

	void Layout::SetInlineVirtualScrollPosition(
		Widget&, Entity::SceneRegistry&, [[maybe_unused]] const float previousParentPosition, [[maybe_unused]] const float newParentPosition
	)
	{
	}

	void Layout::UpdateViewToShowItemAtIndex(Widget& owner, Entity::SceneRegistry& sceneRegistry, const uint32 index)
	{
		const Math::Vector2i relativePosition = owner.GetChildren()[(uint16)index].GetPosition(sceneRegistry) -
		                                        GetAvailableChildContentArea(owner, sceneRegistry).GetPosition();
		const uint8 direction = (uint8)GetChildOrientation();
		const float newVirtualScrollPosition = (float)relativePosition[direction];
		Widget::ChildView visibleItems = owner.GetChildren();

		float maximumVisibleScrollPosition = GetMaximumVirtualScrollPosition(owner, sceneRegistry);
		Assert(visibleItems.HasElements());
		if (visibleItems.HasElements())
		{
			const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
			const Math::Vector2i entrySize = visibleItems[0].GetSize(sceneRegistry);
			const Math::Vector2i entryOffset =
				m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

			const float itemSize = float(entrySize[direction] + entryOffset[direction]);
			maximumVisibleScrollPosition = m_virtualScrollPosition + itemSize * float(visibleItems.GetSize());
		}

		if (newVirtualScrollPosition < m_virtualScrollPosition || newVirtualScrollPosition > maximumVisibleScrollPosition)
		{
			m_shouldResetPanVelocity = true;
			m_panVelocity = 0.f;
			m_requestedPanAcceleration = 0.f;
			SetVirtualScrollPosition(owner, sceneRegistry, newVirtualScrollPosition);
		}
	}

	float Layout::GetMaximumVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		if (HasDataSource(owner, sceneRegistry))
		{
			Widget::ChildView visibleItems = owner.GetChildren();
			if (visibleItems.HasElements())
			{
				const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
				const Math::Vector2i entrySize = visibleItems[0].GetSize(sceneRegistry);
				const Math::Vector2i entryOffset =
					m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

				const uint8 direction = (uint8)m_childOrientation;
				const float itemSize = float(entrySize[direction] + entryOffset[direction]);

				const uint32 dataCount = GetFilteredDataCount(owner, sceneRegistry);
				const float totalContentSize = float(dataCount) * itemSize;
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
				return 0.f;
			}
		}
		else
		{
			return m_maximumScrollPosition;
		}
	}

	void Layout::UpdateViewToShowVirtualItemAtIndex(Widget& owner, Entity::SceneRegistry& sceneRegistry, const uint32 index)
	{
		Widget::ChildView visibleItems = owner.GetChildren();
		Assert(visibleItems.HasElements());
		if (visibleItems.HasElements())
		{
			const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
			const Math::Vector2i entrySize = visibleItems[0].GetSize(sceneRegistry);
			const Math::Vector2i entryOffset =
				m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

			const uint8 direction = (uint8)m_childOrientation;
			const float itemSize = float(entrySize[direction] + entryOffset[direction]);

			const float newVirtualScrollPosition =
				Math::Clamp(float(index) * itemSize, 0.f, GetMaximumVirtualScrollPosition(owner, sceneRegistry));
			const float maximumVisibleScrollPosition = m_virtualScrollPosition + itemSize * float(visibleItems.GetSize());
			if (newVirtualScrollPosition < m_virtualScrollPosition || newVirtualScrollPosition > maximumVisibleScrollPosition)
			{
				m_shouldResetPanVelocity = true;
				m_panVelocity = 0.f;
				m_requestedPanAcceleration = 0.f;
				SetVirtualScrollPosition(owner, sceneRegistry, newVirtualScrollPosition);
			}
		}
	}

	CursorResult Layout::OnStartScroll(
		Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const Math::Vector2i coordinate, const Math::Vector2i delta
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();

				Math::Vector2f direction{Math::Zero};
				direction[childDirection] = 1.f;

				const float velocityInDirection = Math::Abs(Math::Vector2f{delta}.GetNormalizedSafe(Math::Zero).Dot(direction));
				constexpr float minimumVelocity = 0.5f;
				if (velocityInDirection < minimumVelocity)
				{
					return {};
				}

				float newScroll = float(-delta[childDirection]);

				// For now we only support line based scrolling on windows. This should move in the platform layer
				if constexpr (PLATFORM_WINDOWS)
				{
					const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
#if 0 // For now disable line scrolling because it can be confusing for different child sizes (inspector)
					const Math::Vector2i entryOffset =
						m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

					if (owner.HasChildren())
					{
						if (HasDataSource(owner))
						{
							// We can assume that entries in layouts with a datasource have the same size
							const Math::Vector2i entrySize = Math::Min(owner.GetChildren()[0].GetSize(), owner.GetSize());
							newScroll *= float(entrySize[childDirection] + entryOffset[childDirection]);
						}
						else
						{
							// Walk the individual sizes to scroll exact lines
							if (owner.HasChildren())
							{
								float childItemPos = 0.f;
								uint32 targetScroll = uint32(Math::Abs(newScroll));
								if (Math::Sign(newScroll) > 0.f)
								{
									for (Widget& childWidget : owner.GetChildren())
									{
										childItemPos += float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
										if (childItemPos > m_virtualScrollPosition && !--targetScroll)
										{
											break;
										}
									}
								}
								else
								{
									for (Widget& childWidget : owner.GetChildren())
									{
										childItemPos += float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
									}
									Widget::ChildView childView = owner.GetChildren();
									uint32 index = owner.GetChildCount();
									while (index--)
									{
										Widget& childWidget = childView[index];
										childItemPos -= float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
										if (childItemPos < m_virtualScrollPosition && !--targetScroll)
										{
											break;
										}
									}
								}
								newScroll = childItemPos - m_virtualScrollPosition;
							}
						}
					}
					else
					{
						newScroll *= (float(availableChildContentArea.GetSize()[childDirection]) * 0.01f);
					}
#else
					newScroll *= (float(availableChildContentArea.GetSize()[childDirection]) * 0.1f);
#endif
				}
				m_requestedPanAcceleration = 0;
				m_panVelocity = 0;
				m_shouldResetPanVelocity = true;

				SetVirtualScrollPosition(owner, sceneRegistry, m_virtualScrollPosition + newScroll);
				return CursorResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	CursorResult Layout::OnScroll(
		Widget& owner,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const Math::Vector2i coordinate,
		[[maybe_unused]] const Math::Vector2i delta
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();

				if (delta[childDirection] == 0)
				{
					return {};
				}

				float newScroll = float(-delta[childDirection]);

				// For now we only support line based scrolling on windows. This should move in the platform layer
				if constexpr (PLATFORM_WINDOWS)
				{
					const Math::Rectanglei availableChildContentArea = GetAvailableChildContentArea(owner, sceneRegistry);
#if 0 // For now disable line scrolling because it can be confusing for different child sizes (inspector)
					const Math::Vector2i entryOffset =
						m_childOffset.Get(availableChildContentArea.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties());

					if (owner.HasChildren())
					{
						if (HasDataSource(owner))
						{
							// We can assume that entries in layouts with a datasource have the same size
							const Math::Vector2i entrySize = Math::Min(owner.GetChildren()[0].GetSize(), owner.GetSize());
							newScroll *= float(entrySize[childDirection] + entryOffset[childDirection]);
						}
						else
						{
							// Walk the individual sizes to scroll exact lines
							if (owner.HasChildren())
							{
								float childItemPos = 0.f;
								uint32 targetScroll = uint32(Math::Abs(newScroll));
								if (Math::Sign(newScroll) > 0.f)
								{
									for (Widget& childWidget : owner.GetChildren())
									{
										childItemPos += float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
										if (childItemPos > m_virtualScrollPosition && !--targetScroll)
										{
											break;
										}
									}
								}
								else
								{
									for (Widget& childWidget : owner.GetChildren())
									{
										childItemPos += float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
									}
									Widget::ChildView childView = owner.GetChildren();
									uint32 index = owner.GetChildCount();
									while (index--)
									{
										Widget& childWidget = childView[index];
										childItemPos -= float(childWidget.GetSize()[childDirection] + entryOffset[childDirection]);
										if (childItemPos < m_virtualScrollPosition && !--targetScroll)
										{
											break;
										}
									}
								}
								newScroll = childItemPos - m_virtualScrollPosition;
							}
						}
					}
					else
					{
						newScroll *= (float(availableChildContentArea.GetSize()[childDirection]) * 0.01f);
					}
#else
					newScroll *= (float(availableChildContentArea.GetSize()[childDirection]) * 0.1f);
#endif
				}
				m_requestedPanAcceleration = 0;
				m_panVelocity = 0;
				m_shouldResetPanVelocity = true;

				SetVirtualScrollPosition(owner, sceneRegistry, m_virtualScrollPosition + newScroll);

				return CursorResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	CursorResult Layout::OnEndScroll(
		Widget& owner,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const Math::Vector2i coordinate,
		[[maybe_unused]] const Math::Vector2f velocity
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();

				Math::Vector2f direction{Math::Zero};
				direction[childDirection] = 1.f;

				const float velocityInDirection = Math::Abs(velocity.GetNormalizedSafe(Math::Zero).Dot(direction));
				constexpr float minimumVelocity = 0.5f;
				if (velocityInDirection < minimumVelocity)
				{
					return {};
				}

				m_shouldResetPanVelocity = true;
				m_requestedPanAcceleration = -velocity[childDirection];
				EnableUpdate(owner, sceneRegistry);

				return CursorResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	CursorResult Layout::OnCancelScroll(Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const Math::Vector2i coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				m_shouldResetPanVelocity = true;
				m_requestedPanAcceleration = 0.f;
				EnableUpdate(owner, sceneRegistry);

				return CursorResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	PanResult Layout::OnStartPan(
		[[maybe_unused]] Widget& owner,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const Math::Vector2i coordinate,
		[[maybe_unused]] const Math::Vector2f velocity,
		[[maybe_unused]] const Optional<uint16> touchRadiu,
		[[maybe_unused]] SetCursorCallback& pointer
	)
	{
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();
				Math::Vector2f direction{Math::Zero};
				direction[childDirection] = 1.f;

				const float velocityInDirection = Math::Abs(velocity.GetNormalized().Dot(direction));
				constexpr float minimumVelocity = 0.5f;
				if (velocityInDirection < minimumVelocity)
				{
					return {};
				}

				m_startPanScrollPosition = m_virtualScrollPosition;
				m_startPanCoordinate = (float)coordinate[childDirection];
				m_panVelocity = 0;
				m_shouldResetPanVelocity = true;
				return PanResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	CursorResult Layout::OnMovePan(
		[[maybe_unused]] Widget& owner,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const Math::Vector2i coordinate,
		[[maybe_unused]] const Math::Vector2i delta,
		[[maybe_unused]] const Math::Vector2f velocity,
		[[maybe_unused]] const Optional<uint16> touchRadius,
		[[maybe_unused]] SetCursorCallback& pointer
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();
				const float coordinateDelta = m_startPanCoordinate - (float)coordinate[childDirection];

				SetVirtualScrollPosition(owner, sceneRegistry, m_startPanScrollPosition + coordinateDelta);
				m_panVelocity = 0.f;
				m_requestedPanAcceleration = 0.f;
				m_shouldResetPanVelocity = true;

				return PanResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	void FlexLayout::EnableUpdate(Widget&, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<FlexLayout>& layoutSceneData = *sceneRegistry.FindComponentTypeData<FlexLayout>();
		if (!layoutSceneData.IsUpdateEnabled(*this))
		{
			layoutSceneData.EnableUpdate(*this);
		}
	}

	void FlexLayout::DisableUpdate(Widget&, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<FlexLayout>& layoutSceneData = *sceneRegistry.FindComponentTypeData<FlexLayout>();
		if (layoutSceneData.IsUpdateEnabled(*this))
		{
			layoutSceneData.DisableUpdate(*this);
		}
	}

	CursorResult Layout::OnEndPan(
		[[maybe_unused]] Widget& owner,
		const Input::DeviceIdentifier,
		[[maybe_unused]] const Math::Vector2i coordinate,
		[[maybe_unused]] const Math::Vector2f velocity
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		switch (m_overflowType)
		{
			case OverflowType::Scroll:
			case OverflowType::Auto:
			{
				const uint8 childDirection = (uint8)GetChildOrientation();
				Assert(childDirection == 0 || childDirection == 1);

				m_shouldResetPanVelocity = true;
				m_requestedPanAcceleration = -velocity[childDirection];
				EnableUpdate(owner, sceneRegistry);
				return PanResult{&owner};
			}
			default:
				break;
		}

		return {};
	}

	void Layout::Update()
	{
		FrameTime frameTime;
		if (m_stopwatch.IsRunning())
		{
			frameTime = FrameTime(m_stopwatch.GetElapsedTimeAndRestart());
		}
		else
		{
			m_stopwatch.Start();
			frameTime = FrameTime(16_milliseconds);
		}

		const float deltaTime = (float)frameTime;

		float panVelocity = m_panVelocity;
		bool expected = true;
		if (m_shouldResetPanVelocity.CompareExchangeStrong(expected, false))
		{
			panVelocity = 0.f;
		}

		panVelocity += m_requestedPanAcceleration;
		m_requestedPanAcceleration = 0.f;
		const float itemScrollDelta = panVelocity * deltaTime;

		Entity::SceneRegistry& sceneRegistry = m_owner.GetSceneRegistry();
		constexpr Widgets::ReferencePixel velocityLossSpeed = 500_px;
		const uint8 childDirection = (uint8)GetChildOrientation();
		const float adjustedVelocityLossSpeed = Style::Size{velocityLossSpeed, velocityLossSpeed}.GetPoints(
			(Math::Vector2f)m_owner.GetSize(sceneRegistry),
			m_owner.GetOwningWindow()->GetCurrentScreenProperties()
		)[childDirection];
		panVelocity = Math::SignNonZero(panVelocity) * Math::Max(Math::Abs(panVelocity) - adjustedVelocityLossSpeed * deltaTime, 0.f);

		SetVirtualScrollPosition(m_owner, sceneRegistry, m_virtualScrollPosition + itemScrollDelta);

		m_panVelocity = panVelocity;

		const bool reachedEnd = (m_virtualScrollPosition == 0.f) |
		                        (m_virtualScrollPosition >= GetMaximumVirtualScrollPosition(m_owner, sceneRegistry));
		if (itemScrollDelta == 0.f || reachedEnd)
		{
			m_stopwatch.Stop();
			DisableUpdate(m_owner, sceneRegistry);
			m_panVelocity = 0.f;
		}
	}

	void Layout::OnDestroying()
	{
		DisableUpdate(m_owner, m_owner.GetSceneRegistry());
	}

	[[maybe_unused]] const bool wasLayoutTypeRegistered = Reflection::Registry::RegisterType<Layout>();
	[[maybe_unused]] const bool wasLayoutComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Layout>>::Make());
	[[maybe_unused]] const bool wasFlexTypeRegistered = Reflection::Registry::RegisterType<FlexLayout>();
	[[maybe_unused]] const bool wasFlexComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FlexLayout>>::Make());
}
