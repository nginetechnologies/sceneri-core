#include "Data/EventData.h"

#include <Widgets/Widget.inl>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/SizeCorners.h>
#include <Widgets/LoadResourcesResult.h>
#include <Widgets/Data/SubscribedEvents.h>

#include <Engine/Asset/Serialization/Mask.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Event/EventManager.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyMask.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Context/Utils.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Widgets
{
	EventData::EventData(const EventData& other, Widget& owner)
		: m_info(other.m_info)
		, m_arguments(Memory::Reserve, other.m_arguments.GetSize())
	{
		if (!owner.GetRootScene().IsTemplate())
		{
			Data::SubscribedEvents::ResolvedEvent resolvedEvent = Data::SubscribedEvents::ResolveEvent(owner, other.m_info);
			resolvedEvent.Visit(
				[this](const Events::Identifier eventIdentifier)
				{
					m_identifier = eventIdentifier;
				},
				[this](const ngine::DataSource::PropertyIdentifier propertyIdentifier)
				{
					m_dynamicPropertyIdentifier = propertyIdentifier;
				},
				[]()
				{
					ExpectUnreachable();
				}
			);
		}

		for (const Widgets::EventData::ArgumentValue& __restrict templateEventArgumentValue : other.m_arguments)
		{
			templateEventArgumentValue.Visit(
				[&arguments = m_arguments](const UniquePtr<Tags>& pTags)
				{
					arguments.EmplaceBack(UniquePtr<Tags>::Make(*pTags));
				},
				[&arguments = m_arguments](const UniquePtr<Assets>& pAssets)
				{
					arguments.EmplaceBack(UniquePtr<Assets>::Make(*pAssets));
				},
				[&arguments = m_arguments](const UniquePtr<Identifiers>& pIdentifiers)
				{
					arguments.EmplaceBack(UniquePtr<Identifiers>::Make(*pIdentifiers));
				},
				[&arguments = m_arguments](const UniquePtr<Components>& pComponents)
				{
					arguments.EmplaceBack(UniquePtr<Components>::Make(*pComponents));
				},
				[&arguments = m_arguments](const URIs& uris)
				{
					arguments.EmplaceBack(uris);
				},
				[&arguments = m_arguments](const Strings& strings)
				{
					arguments.EmplaceBack(strings);
				},
				[&arguments = m_arguments](const Guids& guids)
				{
					arguments.EmplaceBack(guids);
				},
				[&arguments = m_arguments](const ThisWidget&)
				{
					arguments.EmplaceBack(ThisWidget{});
				},
				[&arguments = m_arguments](const ParentWidget&)
				{
					arguments.EmplaceBack(ParentWidget{});
				},
				[&arguments = m_arguments](const AssetRootWidget&)
				{
					arguments.EmplaceBack(AssetRootWidget{});
				},
				[]()
				{
					ExpectUnreachable();
				}
			);
		}
	}

	void EventData::NotifyAll(Widget& owner) const
	{
		Assert(m_identifier.IsValid());
		Array<Events::DynamicArgument, Events::MaxiumArgumentCount> arguments{};
		Array<Variant<Tag::Mask, Asset::Mask, ngine::DataSource::GenericDataMask, Entity::ComponentSoftReferences>, Events::MaxiumArgumentCount>
			argumentsStorage;
		for (const ArgumentValue& __restrict argumentValue : m_arguments)
		{
			const uint8 argumentIndex = m_arguments.GetIteratorIndex(&argumentValue);

			argumentValue.Visit(
				[&arguments, &argumentsStorage, argumentIndex](const UniquePtr<Tags>& pTags)
				{
					const Tags& tags = *pTags;
					argumentsStorage[argumentIndex] = Tag::Mask();
					Tag::Mask& mask = argumentsStorage[argumentIndex].GetExpected<Tag::Mask>();
					for (const Tag::Identifier::IndexType tagIndex : tags.tags)
					{
						mask.Set(Tag::Identifier::MakeFromValidIndex(tagIndex));
					}
					for (const Tag::Identifier::IndexType tagIndex : tags.dynamicTags)
					{
						mask.Set(Tag::Identifier::MakeFromValidIndex(tagIndex));
					}

					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Tag::Mask>(mask));
				},
				[&arguments, &argumentsStorage, argumentIndex](const UniquePtr<Assets>& pAssets)
				{
					const Assets& assets = *pAssets;
					argumentsStorage[argumentIndex] = Asset::Mask();
					Asset::Mask& mask = argumentsStorage[argumentIndex].GetExpected<Asset::Mask>();
					for (const Asset::Identifier::IndexType assetIndex : assets.assets)
					{
						mask.Set(Asset::Identifier::MakeFromValidIndex(assetIndex));
					}
					for (const Asset::Identifier::IndexType assetIndex : assets.dynamicAssets)
					{
						mask.Set(Asset::Identifier::MakeFromValidIndex(assetIndex));
					}

					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Asset::Mask>(mask));
				},
				[&arguments, &argumentsStorage, argumentIndex](const UniquePtr<Identifiers>& pIdentifiers)
				{
					const Identifiers& identifiers = *pIdentifiers;
					argumentsStorage[argumentIndex] = ngine::DataSource::GenericDataMask();
					ngine::DataSource::GenericDataMask& mask = argumentsStorage[argumentIndex].GetExpected<ngine::DataSource::GenericDataMask>();
					for (const ngine::DataSource::GenericDataIdentifier::IndexType assetIndex : identifiers.data)
					{
						mask.Set(ngine::DataSource::GenericDataIdentifier::MakeFromValidIndex(assetIndex));
					}
					for (const ngine::DataSource::GenericDataIdentifier::IndexType assetIndex : identifiers.dynamicData)
					{
						mask.Set(ngine::DataSource::GenericDataIdentifier::MakeFromValidIndex(assetIndex));
					}

					arguments[argumentIndex] =
						Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const ngine::DataSource::GenericDataMask>(mask));
				},
				[&arguments, &argumentsStorage, argumentIndex](const UniquePtr<Components>& pComponents)
				{
					const Components& components = *pComponents;
					Entity::ComponentSoftReferences softReferences = components.componentSoftReferences;
					softReferences.Add(components.dynamicComponentSoftReferences);

					argumentsStorage[argumentIndex] = Move(softReferences);
					const Entity::ComponentSoftReferences& softReferencesReference =
						argumentsStorage[argumentIndex].GetExpected<Entity::ComponentSoftReferences>();

					arguments[argumentIndex] =
						Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Entity::ComponentSoftReferences>(softReferencesReference));
				},
				[&arguments, argumentIndex](const URIs& uris)
				{
					const ArrayView<const IO::URI> urisView = uris.m_uris;
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(urisView);
				},
				[&arguments, argumentIndex](const Strings& strings)
				{
					const ArrayView<const UnicodeString> stringsView = strings.m_strings;
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(stringsView);
				},
				[&arguments, argumentIndex](const Guids& guids)
				{
					const ArrayView<const Guid> guidsView = guids.m_guids;
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(guidsView);
				},
				[&arguments, argumentIndex, &owner](const ThisWidget&)
				{
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(&owner);
				},
				[&arguments, argumentIndex, &owner](const ParentWidget&)
				{
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(owner.GetParentSafe().Get());
				},
				[&arguments, argumentIndex, &owner](const AssetRootWidget&)
				{
					arguments[argumentIndex] = Scripting::VM::DynamicInvoke::LoadArgument(&owner.GetRootAssetWidget());
				},
				[]()
				{
					Assert(false, "Encountered invalid event");
				}
			);
		}

		System::Get<Events::Manager>().NotifyAll(m_identifier, arguments[0], arguments[1], arguments[2], arguments[3], arguments[4]);
	}

	void EventData::UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties)
	{
		Assert(!owner.GetRootScene().IsTemplate());

		if (m_dynamicPropertyIdentifier.IsValid())
		{
			const ngine::DataSource::PropertyValue dataProperty = dataSourceProperties.GetDataProperty(m_dynamicPropertyIdentifier);
			if (dataProperty.HasValue())
			{
				if (const Optional<const Guid*> eventGuid = dataProperty.Get<Guid>())
				{
					Events::Manager& eventManager = System::Get<Events::Manager>();
					const Guid localGuid = Context::Utils::GetGuid(*eventGuid, owner, owner.GetSceneRegistry());
					m_identifier = eventManager.FindOrRegisterEvent(localGuid);
				}
				else
				{
					m_identifier = {};
				}
			}
			else
			{
				m_identifier = {};
			}
		}

		for (Widgets::EventData::ArgumentValue& __restrict eventArgumentValue : m_arguments)
		{
			eventArgumentValue.Visit(
				[&dataSourceProperties](const UniquePtr<Tags>& pTags)
				{
					Widgets::EventData::Tags& eventTags = *pTags;
					eventTags.dynamicTags.Clear();
					if (!eventTags.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					for (const ngine::DataSource::Identifier::IndexType propertyIndex : eventTags.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::Identifier propertyIdentifier = ngine::DataSource::Identifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const Tag::Identifier*> tagIdentifier = dataPropertyValue.Get<Tag::Identifier>())
							{
								if (tagIdentifier->IsValid())
								{
									eventTags.dynamicTags.EmplaceBackUnique(tagIdentifier->GetFirstValidIndex());
								}
							}
							else if (const Optional<const Tag::Mask*> tagMask = dataPropertyValue.Get<Tag::Mask>())
							{
								for (Tag::Identifier::IndexType tagIndex : tagMask->GetSetBitsIterator())
								{
									eventTags.dynamicTags.EmplaceBackUnique(Tag::Identifier::IndexType{tagIndex});
								}
							}
						}
					}
				},
				[&dataSourceProperties](const UniquePtr<Assets>& pAssets)
				{
					Widgets::EventData::Assets& eventAssets = *pAssets;
					eventAssets.dynamicAssets.Clear();
					if (!eventAssets.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					for (const ngine::DataSource::Identifier::IndexType propertyIndex : eventAssets.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::Identifier propertyIdentifier = ngine::DataSource::Identifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const Asset::Identifier*> assetIdentifier = dataPropertyValue.Get<Asset::Identifier>())
							{
								if (assetIdentifier->IsValid())
								{
									eventAssets.dynamicAssets.EmplaceBackUnique(assetIdentifier->GetFirstValidIndex());
								}
							}
							else if (const Optional<const Asset::Mask*> assetMask = dataPropertyValue.Get<Asset::Mask>())
							{
								for (Asset::Identifier::IndexType assetIndex : assetMask->GetSetBitsIterator())
								{
									eventAssets.dynamicAssets.EmplaceBackUnique(Asset::Identifier::IndexType{assetIndex});
								}
							}
						}
					}
				},
				[&dataSourceProperties](const UniquePtr<Identifiers>& pIdentifiers)
				{
					Widgets::EventData::Identifiers& eventIdentifiers = *pIdentifiers;
					eventIdentifiers.dynamicData.Clear();
					if (!eventIdentifiers.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					for (const ngine::DataSource::GenericDataIdentifier::IndexType propertyIndex :
				       eventIdentifiers.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::PropertyIdentifier propertyIdentifier =
							ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const ngine::DataSource::GenericDataIdentifier*> dataIdentifier = dataPropertyValue.Get<ngine::DataSource::GenericDataIdentifier>())
							{
								if (dataIdentifier->IsValid())
								{
									eventIdentifiers.dynamicData.EmplaceBackUnique(dataIdentifier->GetFirstValidIndex());
								}
							}
						}
					}
				},
				[&dataSourceProperties](const UniquePtr<Components>& pComponents)
				{
					Widgets::EventData::Components& eventComponents = *pComponents;
					eventComponents.dynamicComponentSoftReferences.ClearAll();
					if (!eventComponents.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					for (const ngine::DataSource::Identifier::IndexType propertyIndex : eventComponents.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::Identifier propertyIdentifier = ngine::DataSource::Identifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const Entity::ComponentSoftReference*> componentReference3D =
						        dataPropertyValue.Get<Entity::ComponentSoftReference>();
						      componentReference3D.IsValid() && componentReference3D->IsPotentiallyValid())
							{
								eventComponents.dynamicComponentSoftReferences.Add(*componentReference3D);
							}
							else if (const Optional<const Entity::ComponentSoftReference*> componentReference2D =
						             dataPropertyValue.Get<Entity::ComponentSoftReference>();
						           componentReference2D.IsValid() && componentReference2D->IsPotentiallyValid())
							{
								eventComponents.dynamicComponentSoftReferences.Add(*componentReference2D);
							}
							else if (const Optional<const Entity::ComponentSoftReferences*> componentSoftReferences = dataPropertyValue.Get<Entity::ComponentSoftReferences>())
							{
								eventComponents.dynamicComponentSoftReferences.Add(*componentSoftReferences);
							}
						}
					}
				},
				[&dataSourceProperties](URIs& eventURIs)
				{
					if (!eventURIs.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					eventURIs.m_uris.Clear();
					for (const ngine::DataSource::Identifier::IndexType propertyIndex : eventURIs.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::Identifier propertyIdentifier = ngine::DataSource::Identifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const UnicodeString*> pURIString = dataPropertyValue.Get<UnicodeString>())
							{
								eventURIs.m_uris.EmplaceBack(IO::URI(*pURIString));
							}
						}
					}
				},
				[](const Strings&)
				{
				},
				[&dataSourceProperties](Guids& eventGuids)
				{
					if (!eventGuids.dynamicPropertyMask.AreAnySet())
					{
						return;
					}

					eventGuids.m_guids.Clear();
					for (const ngine::DataSource::Identifier::IndexType propertyIndex : eventGuids.dynamicPropertyMask.GetSetBitsIterator())
					{
						const ngine::DataSource::Identifier propertyIdentifier = ngine::DataSource::Identifier::MakeFromValidIndex(propertyIndex);
						const ngine::DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
						if (dataPropertyValue.HasValue())
						{
							if (const Optional<const Guid*> pGuid = dataPropertyValue.Get<Guid>())
							{
								eventGuids.m_guids.EmplaceBack(*pGuid);
							}
							else if (const Optional<const Asset::Guid*> pAssetGuid = dataPropertyValue.Get<Asset::Guid>())
							{
								eventGuids.m_guids.EmplaceBack(*pAssetGuid);
							}
						}
					}
				},
				[](const ThisWidget&)
				{
				},
				[](const ParentWidget&)
				{
				},
				[](const AssetRootWidget&)
				{
				},
				[]()
				{
					ExpectUnreachable();
				}
			);

			if (const Optional<Widgets::EventData::Guids*> pEventGuids = eventArgumentValue.Get<Widgets::EventData::Guids>())
			{
			}
		}
	}
}

namespace ngine::Widgets::Data
{
	EventData::EventData(const EventData& templateComponent, const Cloner& cloner)
	{
		for (uint8 eventTypeIndex = 0; eventTypeIndex < (uint8)EventType::Count; ++eventTypeIndex)
		{
			const uint16 eventCount = templateComponent.m_events[eventTypeIndex].GetSize();
			m_events[eventTypeIndex].Reserve(eventCount);
			for (uint16 eventIndex = 0; eventIndex < eventCount; ++eventIndex)
			{
				m_events[eventTypeIndex].EmplaceBack(templateComponent.m_events[eventTypeIndex][eventIndex], cloner.GetParent());
			}
		}
	}

	EventData::EventData(const Deserializer& deserializer)
	{
		const Serialization::Reader notifyEventsReader = *deserializer.m_reader.FindSerializer("notify_events");

		constexpr Array<ConstStringView, (uint8)EventType::Count> eventNames{"tap", "double_tap", "long_press"};
		for (Iterator<const ConstStringView> it = eventNames.begin(), end = eventNames.end(); it != end; ++it)
		{
			if (const Optional<Serialization::Reader> eventReader = notifyEventsReader.FindSerializer(*it))
			{
				const EventType eventType = static_cast<EventType>(eventNames.GetIteratorIndex(it));
				if (eventReader->IsArray())
				{
					m_events[(uint8)eventType].Reserve((uint16)eventReader->GetArraySize());
					for (const Serialization::Reader eventTypeReader : eventReader->GetArrayView())
					{
						m_events[(uint8)eventType].EmplaceBack(DeserializeEvent(deserializer.GetParent(), eventTypeReader));
					}
				}
				else
				{
					m_events[(uint8)eventType].EmplaceBack(DeserializeEvent(deserializer.GetParent(), *eventReader));
				}
				deserializer.GetParent().EnableInput();
			}
		}
	}

	EventData::~EventData() = default;

	bool EventData::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		return writer.SerializeObjectWithCallback(
			"notify_events",
			[this](Serialization::Writer eventWriter)
			{
				for (const EventContainer& eventContainer : m_events)
				{
					if (eventContainer.HasElements())
					{
						const EventType eventType = (EventType)m_events.GetIteratorIndex(&eventContainer);
						constexpr Array<ConstStringView, (uint8)EventType::Count> eventNames{"tap", "double_tap", "long_press"};
						const ConstStringView eventName = eventNames[(uint8)eventType];

						eventWriter.SerializeArrayWithCallback(
							eventName,
							[eventContainer = eventContainer.GetView()](Serialization::Writer writer, const uint16 index)
							{
								const Widgets::EventData& eventData = eventContainer[index];
								String key = Widget::GetEventInfoKey(eventData.m_info);
								writer.GetValue().SetObject();
								writer.Serialize("guid", key);

								if (eventData.m_arguments.HasElements())
								{
									writer.SerializeObjectWithCallback(
										"arguments",
										[arguments = eventData.m_arguments.GetView()](Serialization::Writer argumentsWriter) mutable
										{
											while (arguments.HasElements())
											{
												const Widgets::EventData::ArgumentValue& argumentValue = arguments[0];
												++arguments;

												argumentValue.Visit(
													[argumentsWriter](const UniquePtr<Widgets::EventData::Tags>& pTags) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
														const Widgets::EventData::Tags& tags = *pTags;
														argumentsWriter.SerializeArrayWithCallback(
															"tags",
															[&dataSourceCache, &tagRegistry, tags = tags.tags.GetView(), dynamicPropertyMask = tags.dynamicPropertyMask](
																Serialization::Writer writer,
																[[maybe_unused]] const uint32 index
															) mutable
															{
																if (tags.HasElements())
																{
																	const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tags[0]);
																	tags++;
																	return writer.SerializeInPlace(tagRegistry.GetAssetGuid(tagIdentifier));
																}
																else
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
															},
															tags.tags.GetSize() + tags.dynamicTags.GetSize()
														);
													},
													[argumentsWriter](const UniquePtr<Widgets::EventData::Assets>& pAssets) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														Asset::Manager& assetManager = System::Get<Asset::Manager>();
														const Widgets::EventData::Assets& assets = *pAssets;
														argumentsWriter.SerializeArrayWithCallback(
															"assets",
															[&dataSourceCache,
										           &assetManager,
										           assets = assets.assets.GetView(),
										           dynamicPropertyMask =
										             assets.dynamicPropertyMask](Serialization::Writer writer, [[maybe_unused]] const uint32 index) mutable
															{
																if (assets.HasElements())
																{
																	const Asset::Identifier tagIdentifier = Asset::Identifier::MakeFromValidIndex(assets[0]);
																	assets++;
																	return writer.SerializeInPlace(assetManager.GetAssetGuid(tagIdentifier));
																}
																else
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
															},
															assets.assets.GetSize() + assets.dynamicPropertyMask.GetNumberOfSetBits()
														);
													},
													[argumentsWriter](const UniquePtr<Widgets::EventData::Identifiers>& pIdentifiers) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														const Widgets::EventData::Identifiers& identifiers = *pIdentifiers;
														argumentsWriter.SerializeArrayWithCallback(
															"identifiers",
															[&dataSourceCache,
										           identifiers = identifiers.data.GetView(),
										           dynamicPropertyMask =
										             identifiers.dynamicPropertyMask](Serialization::Writer writer, [[maybe_unused]] const uint32 index) mutable
															{
																if (identifiers.HasElements())
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(identifiers[0]);
																	identifiers++;
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
																else
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
															},
															identifiers.data.GetSize() + identifiers.dynamicPropertyMask.GetNumberOfSetBits()
														);
													},
													[argumentsWriter](const UniquePtr<Widgets::EventData::Components>& pComponents) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														const Widgets::EventData::Components& components = *pComponents;
														argumentsWriter.SerializeArrayWithCallback(
															"components",
															[&dataSourceCache,
										           dynamicPropertyMask =
										             components.dynamicPropertyMask](Serialization::Writer writer, [[maybe_unused]] const uint32 index) mutable
															{
																const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																	ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																dynamicPropertyMask.Clear(propertyIdentifier);

																String format;
																format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																return writer.SerializeInPlace(format);
															},
															components.dynamicPropertyMask.GetNumberOfSetBits()
														);
													},
													[argumentsWriter](const Widgets::EventData::URIs& uris) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														argumentsWriter.SerializeArrayWithCallback(
															"uris",
															[uris = uris.m_uris.GetView(),
										           dynamicPropertyMask = uris.dynamicPropertyMask,
										           &dataSourceCache](Serialization::Writer writer, const uint32 index) mutable
															{
																if (index < uris.GetSize())
																{
																	return writer.SerializeInPlace(uris[index]);
																}
																else
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
															},
															uris.m_uris.GetSize() + uris.dynamicPropertyMask.GetNumberOfSetBits()
														);
													},
													[argumentsWriter](const Widgets::EventData::Strings& strings) mutable
													{
														argumentsWriter.SerializeArrayWithCallback(
															"strings",
															[strings = strings.m_strings.GetView()](Serialization::Writer writer, const uint32 index) mutable
															{
																return writer.SerializeInPlace(strings[index]);
															},
															strings.m_strings.GetSize()
														);
													},
													[argumentsWriter](const Widgets::EventData::Guids& guids) mutable
													{
														ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
														argumentsWriter.SerializeArrayWithCallback(
															"guids",
															[guids = guids.m_guids.GetView(),
										           dynamicPropertyMask = guids.dynamicPropertyMask,
										           &dataSourceCache](Serialization::Writer writer, const uint32 index) mutable
															{
																if (index < guids.GetSize())
																{
																	return writer.SerializeInPlace(guids[index]);
																}
																else
																{
																	const ngine::DataSource::PropertyIdentifier propertyIdentifier =
																		ngine::DataSource::PropertyIdentifier::MakeFromValidIndex(dynamicPropertyMask.GetFirstSetIndex());
																	dynamicPropertyMask.Clear(propertyIdentifier);

																	String format;
																	format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
																	return writer.SerializeInPlace(format);
																}
															},
															guids.m_guids.GetSize() + guids.dynamicPropertyMask.GetNumberOfSetBits()
														);
													},
													[argumentsWriter](const Widgets::EventData::ThisWidget&) mutable
													{
														argumentsWriter.Serialize("widget", ConstStringView{"this"});
													},
													[argumentsWriter](const Widgets::EventData::ParentWidget&) mutable
													{
														argumentsWriter.Serialize("widget", ConstStringView{"parent"});
													},
													[argumentsWriter](const Widgets::EventData::AssetRootWidget&) mutable
													{
														argumentsWriter.Serialize("widget", ConstStringView{"root"});
													},
													[]()
													{
														ExpectUnreachable();
													}
												);
											}

											return true;
										}
									);
								}

								return true;
							},
							eventContainer.GetSize()
						);
					}
				}
				return true;
			}
		);
	}

	void EventData::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& owner)
	{
		if (pReader.IsValid())
		{
			const Serialization::Reader notifyEventsReader = *pReader->FindSerializer("notify_events");

			constexpr Array<ConstStringView, (uint8)EventType::Count> eventNames{"tap", "double_tap", "long_press"};
			for (Iterator<const ConstStringView> it = eventNames.begin(), end = eventNames.end(); it != end; ++it)
			{
				if (const Optional<Serialization::Reader> eventReader = notifyEventsReader.FindSerializer(*it))
				{
					const EventType eventType = static_cast<EventType>(eventNames.GetIteratorIndex(it));
					m_events[(uint8)eventType].Clear();
					if (eventReader->IsArray())
					{
						m_events[(uint8)eventType].Reserve((uint16)eventReader->GetArraySize());
						for (const Serialization::Reader eventTypeReader : eventReader->GetArrayView())
						{
							m_events[(uint8)eventType].EmplaceBack(DeserializeEvent(owner, eventTypeReader));
						}
					}
					else
					{
						m_events[(uint8)eventType].EmplaceBack(DeserializeEvent(owner, *eventReader));
					}
					owner.EnableInput();
				}
			}
		}
	}

	/* static */ Widgets::EventData EventData::DeserializeEvent(Widget& owner, Serialization::Reader eventReader)
	{
		SubscribedEvents::ResolvedEvent resolvedEvent;
		EventInfo eventInfo;
		if (const Optional<Serialization::Reader> guidReader = eventReader.FindSerializer("guid"))
		{
			eventInfo = owner.DeserializeEventInfo(*guidReader);
			if (!owner.GetRootScene().IsTemplate())
			{
				resolvedEvent = SubscribedEvents::ResolveEvent(owner, eventInfo);
			}
		}
		else
		{
			Assert(false, "Notify event has no valid guid!");
			return {};
		}

		Widgets::EventData::ArgumentContainer arguments;
		if (const Optional<Serialization::Reader> argumentsReader = eventReader.FindSerializer("arguments"))
		{
			for (const Serialization::Member<Serialization::Reader> argumentMember : argumentsReader->GetMemberView())
			{
				const ConstStringView argumentName = argumentMember.key;
				const Serialization::Reader argumentReader = argumentMember.value;
				if (argumentName == "tags")
				{
					Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					Tag::Mask tagMask = argumentReader.ReadInPlaceWithDefaultValue<Tag::Mask>({}, tagRegistry);
					InlineVector<Tag::Identifier::IndexType, 1> tags;
					tags.Reserve(tagMask.GetNumberOfSetBits());
					for (const Tag::Identifier::IndexType tagIndex : tagMask.GetSetBitsIterator())
					{
						tags.EmplaceBack(tagIndex);
					}

					arguments.EmplaceBack(UniquePtr<Widgets::EventData::Tags>::Make(Widgets::EventData::Tags{
						Move(tags),
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache),
					}));
				}
				else if (argumentName == "assets")
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					Asset::Mask assetMask = argumentReader.ReadInPlaceWithDefaultValue<Asset::Mask>({}, assetManager);
					InlineVector<Asset::Identifier::IndexType, 1> assets;
					assets.Reserve(assetMask.GetNumberOfSetBits());
					for (const Asset::Identifier::IndexType assetIndex : assetMask.GetSetBitsIterator())
					{
						assets.EmplaceBack(assetIndex);
					}

					arguments.EmplaceBack(UniquePtr<Widgets::EventData::Assets>::Make(Widgets::EventData::Assets{
						Move(assets),
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache),
					}));
				}
				else if (argumentName == "identifiers")
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					ngine::DataSource::PropertyMask propertyMask =
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache);
					InlineVector<ngine::DataSource::PropertyIdentifier::IndexType, 1> properties;
					properties.Reserve(propertyMask.GetNumberOfSetBits());
					for (const ngine::DataSource::PropertyIdentifier::IndexType propertyIndex : propertyMask.GetSetBitsIterator())
					{
						properties.EmplaceBack(propertyIndex);
					}

					arguments.EmplaceBack(UniquePtr<Widgets::EventData::Identifiers>::Make(Widgets::EventData::Identifiers{
						Move(properties),
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache)
					}));
				}
				else if (argumentName == "components")
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					arguments.EmplaceBack(UniquePtr<Widgets::EventData::Components>::Make(Widgets::EventData::Components{
						// argumentReader.ReadInPlaceWithDefaultValue<Entity::ComponentSoftReferences>(assetManager),
						{}, // TODO: Support reading components from disk?
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache),
					}));
				}
				else if (argumentName == "uris")
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					arguments.EmplaceBack(Widgets::EventData::URIs{
						argumentReader.ReadInPlaceWithDefaultValue<InlineVector<IO::URI, 1>>({}),
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache)
					});
				}
				else if (argumentName == "strings")
				{
					arguments.EmplaceBack(Widgets::EventData::Strings{
						argumentReader.ReadInPlaceWithDefaultValue<InlineVector<UnicodeString, 1>>({}),
					});
				}
				else if (argumentName == "guids")
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

					arguments.EmplaceBack(Widgets::EventData::Guids{
						argumentReader.ReadInPlaceWithDefaultValue<InlineVector<Guid, 1>>({}),
						argumentReader.ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyMask>({}, dataSourceCache)
					});
				}
				else if (argumentName == "widget")
				{
					const ConstStringView widgetType = argumentReader.ReadInPlaceWithDefaultValue<ConstStringView>({});
					if (widgetType == "this")
					{
						arguments.EmplaceBack(Widgets::EventData::ThisWidget{});
					}
					else if (widgetType == "parent")
					{
						arguments.EmplaceBack(Widgets::EventData::ParentWidget{});
					}
					else if (widgetType == "root")
					{
						arguments.EmplaceBack(Widgets::EventData::AssetRootWidget{});
					}
				}

				Assert(!arguments.ReachedCapacity());
				if (UNLIKELY_ERROR(arguments.ReachedCapacity()))
				{
					break;
				}
			}
		}

		Events::Identifier eventIdentifier;
		ngine::DataSource::PropertyIdentifier dynamicEventPropertyIdentifier;
		resolvedEvent.Visit(
			[&resolvedEventIdentifier = eventIdentifier](const Events::Identifier eventIdentifier)
			{
				resolvedEventIdentifier = eventIdentifier;
			},
			[&dynamicEventPropertyIdentifier](const ngine::DataSource::PropertyIdentifier propertyIdentifier)
			{
				dynamicEventPropertyIdentifier = propertyIdentifier;
			},
			[]()
			{
			}
		);
		return Widgets::EventData{eventIdentifier, dynamicEventPropertyIdentifier, eventInfo, Move(arguments)};
	}

	void EventData::UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties)
	{
		for (EventContainer& eventContainer : m_events)
		{
			for (Widgets::EventData& eventData : eventContainer)
			{
				eventData.UpdateFromDataSource(owner, dataSourceProperties);
			}
		}
	}

	void EventData::NotifyAll(Widget& owner, const EventType eventType)
	{
		for (const Widgets::EventData& eventData : m_events[(uint8)eventType])
		{
			if (eventData.m_identifier.IsValid())
			{
				eventData.NotifyAll(owner);
			}
		}
	}

	[[maybe_unused]] const bool wasEventDataTypeRegistered = Reflection::Registry::RegisterType<EventData>();
	[[maybe_unused]] const bool wasEventDataComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<EventData>>::Make());
}
