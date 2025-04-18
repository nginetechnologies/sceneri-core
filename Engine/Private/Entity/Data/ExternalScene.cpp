#include "Entity/Data/ExternalScene.h"
#include "Entity/Scene/SceneChildInstance.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/IsEquivalentTo.h>

namespace ngine::Entity::Data
{
	ExternalScene::ExternalScene(const Entity::ComponentTemplateIdentifier componentTemplateIdentifier)
		: m_componentTemplateIdentifier(componentTemplateIdentifier)
	{
	}

	ExternalScene::ExternalScene(const Deserializer&)
	{
	}

	ExternalScene::ExternalScene(const ExternalScene& templateComponent, const Cloner&)
		: m_componentTemplateIdentifier(templateComponent.m_componentTemplateIdentifier)
	{
	}

	Asset::Guid ExternalScene::GetAssetGuid() const
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		return m_componentTemplateIdentifier.IsValid() ? sceneTemplateCache.GetAssetGuid(m_componentTemplateIdentifier) : Asset::Guid{};
	}

	struct TargetComponentInfo
	{
		Serialization::Reader m_reader;
		EnumFlags<SceneChildInstance::Flags> m_sceneChildInstanceFlags;
	};

	using TargetComponentInfoMap = UnorderedMap<Guid, TargetComponentInfo, Guid::Hash>;

	/* static */ void
	PopulateTargetComponentMap(const Serialization::Reader targetComponentReader, TargetComponentInfoMap& targetComponentInfoMap)
	{
		if (const Optional<Serialization::Reader> dataComponentsReader = targetComponentReader.FindSerializer("data_components"))
		{
			for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
			{
				if (*dataComponentReader.Read<Guid>("typeGuid") == Reflection::GetTypeGuid<Entity::SceneChildInstance>())
				{
					Threading::JobBatch jobBatch;
					SceneChildInstance sceneChildInstance;
					Reflection::GetType<SceneChildInstance>()
						.SerializeTypePropertiesInline(dataComponentReader, dataComponentReader, sceneChildInstance, Invalid, jobBatch);
					Assert(jobBatch.IsInvalid());

					if (sceneChildInstance.m_flags.IsNotSet(SceneChildInstance::Flags::NoTemplateOverrides))
					{
						targetComponentInfoMap.EmplaceOrAssign(
							Guid(sceneChildInstance.m_sourceTemplateInstanceGuid),
							TargetComponentInfo{targetComponentReader, sceneChildInstance.m_flags}
						);
					}
					break;
				}
			}
		}

		if (const Optional<Serialization::Reader> childrenReader = targetComponentReader.FindSerializer("children"))
		{
			// TODO: Reserve ahead of time
			targetComponentInfoMap.Reserve(targetComponentInfoMap.GetSize() + (uint32)childrenReader->GetArraySize());

			for (const Serialization::Reader childReader : childrenReader->GetArrayView())
			{
				PopulateTargetComponentMap(childReader, targetComponentInfoMap);
			}
		}
	}

	/* static */ Threading::JobBatch SerializeDataComponentsAndNonTemplateChildren(
		const Serialization::Reader targetComponentReader,
		Entity::HierarchyComponentBase& targetParentComponent,
		const TargetComponentInfoMap& targetComponentInfoMap
	)
	{
		Threading::JobBatch jobBatch;
		if (const Optional<Serialization::Reader> childrenReader = targetComponentReader.FindSerializer("children"))
		{
			Entity::SceneRegistry& sceneRegistry = targetParentComponent.GetSceneRegistry();
			for (const Serialization::Reader childReader : childrenReader->GetArrayView())
			{
				const auto isTemplateInstance = [](const Serialization::Reader childReader)
				{
					if (const Optional<Serialization::Reader> dataComponentsReader = childReader.FindSerializer("data_components"))
					{
						bool isTemplateInstance = false;
						for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
						{
							const bool isInstanceComponent = *dataComponentReader.Read<Guid>("typeGuid") ==
							                                 Reflection::GetTypeGuid<Entity::SceneChildInstance>();
							isTemplateInstance = isInstanceComponent;
							if (isInstanceComponent)
							{
								break;
							}
						}
						return isTemplateInstance;
					}
					return false;
				};

				if (!isTemplateInstance(childReader))
				{
					const typename ComponentTypeIdentifier::IndexType typeIdentifierValue =
						childReader.ReadWithDefaultValue<typename ComponentTypeIdentifier::IndexType>(
							"typeIdentifier",
							typename ComponentTypeIdentifier::IndexType(ComponentTypeIdentifier::Invalid)
						);
					ComponentTypeIdentifier typeIdentifier = ComponentTypeIdentifier::MakeFromValue(typeIdentifierValue);
					Entity::ComponentRegistry& registry = System::Get<Entity::Manager>().GetRegistry();
					if (typeIdentifier.IsInvalid())
					{
						typeIdentifier = registry.FindIdentifier(childReader.ReadWithDefaultValue<Guid>("typeGuid", Guid{}));
					}

					Assert(typeIdentifier.IsValid());
					if (typeIdentifier.IsValid())
					{
						const Optional<ComponentTypeInterface*> pComponentTypeInfo = registry.Get(typeIdentifier);
						Assert(pComponentTypeInfo.IsValid());
						if (LIKELY(pComponentTypeInfo.IsValid()))
						{
							Threading::JobBatch childJobBatch;
							Optional<Entity::Component*> pComponent = pComponentTypeInfo->DeserializeInstanceWithoutChildren(
								Entity::HierarchyComponentBase::Deserializer{
									Reflection::TypeDeserializer{childReader, System::Get<Reflection::Registry>(), childJobBatch},
									sceneRegistry,
									targetParentComponent
								},
								sceneRegistry
							);

							Assert(pComponent.IsValid());
							if (LIKELY(pComponent.IsValid()))
							{
								Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pComponent);
								if (const Optional<Serialization::Reader> dataComponentsReader = childReader.FindSerializer("data_components"))
								{
									Threading::JobBatch dataComponentBatch;
									for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
									{
										[[maybe_unused]] Optional<ComponentValue<Data::Component>> componentValue =
											dataComponentReader
												.ReadInPlace<ComponentValue<Data::Component>>(component, component.GetSceneRegistry(), dataComponentBatch);
									}
									Assert(dataComponentBatch.IsInvalid());
								}

								Threading::JobBatch childrenJobBatch =
									SerializeDataComponentsAndNonTemplateChildren(childReader, component, targetComponentInfoMap);
								childJobBatch.QueueAsNewFinishedStage(childrenJobBatch);

								jobBatch.QueueAfterStartStage(childJobBatch);
							}
						}
					}
				}
			}
		}
		return jobBatch;
	}

	/* static */ void CloneTemplateComponentChildren(
		const Entity::HierarchyComponentBase& templateParentComponent,
		Entity::HierarchyComponentBase& targetParentComponent,
		Entity::HierarchyComponentBase& rootParentComponent,
		TargetComponentInfoMap& __restrict targetComponentInfoMap,
		Entity::ComponentTypeSceneData<SceneChildInstance>& sceneChildInstanceData,
		Entity::ComponentTypeSceneData<ExternalScene>& externalSceneInstanceData,
		Entity::SceneRegistry& sceneRegistry,
		const Entity::SceneRegistry& templateSceneRegistry,
		Threading::JobBatch& jobBatch
	)
	{
		// Copy over children
		for (const Entity::HierarchyComponentBase& templateChildComponent : templateParentComponent.GetChildren())
		{
			auto targetComponentInfoIt = targetComponentInfoMap.Find(templateChildComponent.GetInstanceGuid(templateSceneRegistry));
			if (targetComponentInfoIt == targetComponentInfoMap.end())
			{
				if (const Optional<SceneChildInstance*> pSceneChildInstance = templateChildComponent.FindDataComponentOfType<SceneChildInstance>(sceneChildInstanceData))
				{
					targetComponentInfoIt = targetComponentInfoMap.Find(pSceneChildInstance->m_parentTemplateInstanceGuid);
					if (targetComponentInfoIt == targetComponentInfoMap.end())
					{
						targetComponentInfoIt = targetComponentInfoMap.Find(pSceneChildInstance->m_sourceTemplateInstanceGuid);
					}
				}
			}

			if (targetComponentInfoIt != targetComponentInfoMap.end())
			{
				if (targetComponentInfoIt->second.m_sceneChildInstanceFlags.IsSet(SceneChildInstance::Flags::Removed))
				{
					targetComponentInfoMap.Remove(targetComponentInfoIt);
					continue;
				}
			}

			Threading::JobBatch childJobBatch;
			Optional<Entity::Component*> pComponent;
			if (targetComponentInfoIt != targetComponentInfoMap.end())
			{
				pComponent = templateChildComponent.GetTypeInfo()->CloneFromTemplateAndSerializeWithoutChildrenManualOnCreated(
					templateChildComponent,
					templateParentComponent,
					targetParentComponent,
					targetComponentInfoIt->second.m_reader,
					sceneRegistry,
					templateSceneRegistry,
					childJobBatch
				);

				if (LIKELY(pComponent.IsValid()))
				{
					Entity::HierarchyComponentBase& targetChildComponent = static_cast<Entity::HierarchyComponentBase&>(*pComponent);
					if (const Optional<SceneChildInstance*> pSceneChildInstance = targetChildComponent.FindDataComponentOfType<SceneChildInstance>(sceneRegistry))
					{
						pSceneChildInstance->m_parentTemplateInstanceGuid = templateChildComponent.GetInstanceGuid(templateSceneRegistry);
					}
					else
					{
						Assert(false, "Scene child instance should have been deserialized from disk!");
						targetChildComponent.CreateDataComponent<SceneChildInstance>(
							sceneChildInstanceData,
							templateChildComponent.GetInstanceGuid(templateSceneRegistry),
							templateChildComponent.GetInstanceGuid(templateSceneRegistry)
						);
					}
				}
			}
			else
			{
				pComponent = templateChildComponent.GetTypeInfo()->CloneFromTemplateManualOnCreated(
					Guid::Generate(),
					templateChildComponent,
					templateParentComponent,
					targetParentComponent,
					sceneRegistry,
					templateSceneRegistry,
					childJobBatch
				);

				if (LIKELY(pComponent.IsValid()))
				{
					Entity::HierarchyComponentBase& targetChildComponent = static_cast<Entity::HierarchyComponentBase&>(*pComponent);
					if (const Optional<SceneChildInstance*> pSceneChildInstance = targetChildComponent.FindDataComponentOfType<SceneChildInstance>(sceneRegistry))
					{
						pSceneChildInstance->m_parentTemplateInstanceGuid = templateChildComponent.GetInstanceGuid(templateSceneRegistry);
					}
					else
					{
						targetChildComponent.CreateDataComponent<SceneChildInstance>(
							sceneChildInstanceData,
							templateChildComponent.GetInstanceGuid(templateSceneRegistry),
							templateChildComponent.GetInstanceGuid(templateSceneRegistry)
						);
					}
				}
			}
			if (UNLIKELY(!pComponent.IsValid()))
			{
				continue;
			}

			Entity::HierarchyComponentBase& targetChildComponent = static_cast<Entity::HierarchyComponentBase&>(*pComponent);
			if (targetComponentInfoIt != targetComponentInfoMap.end())
			{
				const Serialization::Reader targetComponentReader = targetComponentInfoIt->second.m_reader;

				Threading::JobBatch childrenJobBatch =
					SerializeDataComponentsAndNonTemplateChildren(targetComponentReader, targetChildComponent, targetComponentInfoMap);
				childJobBatch.QueueAsNewFinishedStage(childrenJobBatch);
			}

			if (templateChildComponent.IsScene(templateSceneRegistry))
			{
				if constexpr (ENABLE_ASSERTS)
				{
					[[maybe_unused]] const Optional<Data::ExternalScene*> pExternalScene =
						externalSceneInstanceData.GetComponentImplementation(targetChildComponent.GetIdentifier());
					Assert(pExternalScene.IsValid());
				}

				if (const Optional<SceneChildInstance*> pSceneChildInstance = targetChildComponent.FindDataComponentOfType<SceneChildInstance>(sceneChildInstanceData))
				{
					auto it = targetComponentInfoMap.Find(pSceneChildInstance->m_sourceTemplateInstanceGuid);
					if (it != targetComponentInfoMap.end())
					{
						Serialization::Reader reader(it->second.m_reader);
						if (const Optional<ComponentTypeInterface*> pComponentTypeInfo = targetChildComponent.GetTypeInfo())
						{
							Threading::JobBatch componentJobBatch =
								pComponentTypeInfo->SerializeInstanceWithChildren(reader, targetChildComponent, targetChildComponent.GetParent());
							jobBatch.QueueAsNewFinishedStage(componentJobBatch);
						}
					}
					else
					{
						CloneTemplateComponentChildren(
							templateChildComponent,
							targetChildComponent,
							rootParentComponent,
							targetComponentInfoMap,
							sceneChildInstanceData,
							externalSceneInstanceData,
							sceneRegistry,
							templateSceneRegistry,
							childJobBatch
						);
					}
				}
				else
				{
					CloneTemplateComponentChildren(
						templateChildComponent,
						targetChildComponent,
						rootParentComponent,
						targetComponentInfoMap,
						sceneChildInstanceData,
						externalSceneInstanceData,
						sceneRegistry,
						templateSceneRegistry,
						childJobBatch
					);
				}
			}
			else
			{
				CloneTemplateComponentChildren(
					templateChildComponent,
					targetChildComponent,
					rootParentComponent,
					targetComponentInfoMap,
					sceneChildInstanceData,
					externalSceneInstanceData,
					sceneRegistry,
					templateSceneRegistry,
					childJobBatch
				);
			}

			targetChildComponent.GetTypeInfo()->OnComponentCreated(targetChildComponent, targetParentComponent, sceneRegistry);

			jobBatch.QueueAfterStartStage(childJobBatch);
		}
	}

	void Data::ExternalScene::OnMasterSceneLoaded(
		Entity::HierarchyComponentBase& component,
		const Optional<const Entity::HierarchyComponentBase*> templateSceneComponent,
		Serialization::Data&& serializedData,
		Threading::StageBase& finishedStage
	)
	{
		// TODO: Scene Master Instance Data Component should know where all the child instances are
		// And immediately mirror changes onto those.
		// We should support the mirroring through reflection properties.

		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		Entity::ComponentTypeSceneData<SceneChildInstance>& sceneChildInstanceData =
			*sceneRegistry.GetOrCreateComponentTypeData<SceneChildInstance>();

		Serialization::Reader serializer(serializedData.GetDocument(), serializedData);

		TargetComponentInfoMap targetComponentInfoMap;
		PopulateTargetComponentMap(serializer, targetComponentInfoMap);

		if (templateSceneComponent.IsValid())
		{
			Threading::JobBatch jobBatch;

			const Entity::SceneRegistry& templateSceneRegistry = templateSceneComponent->GetSceneRegistry();

			// Copy template root data components
			templateSceneComponent->IterateDataComponents(
				templateSceneRegistry,
				[&component,
			   &sceneRegistry,
			   &templateSceneRegistry,
			   &templateSceneComponent = *templateSceneComponent,
			   &jobBatch](Entity::Data::Component& templateDataComponent, const Optional<ComponentTypeInterface*> pComponentTypeInfo, ComponentTypeSceneDataInterface&)
				{
					Entity::ComponentValue<Entity::Data::Component> componentValue;
					Threading::JobBatch dataComponentBatch;
					componentValue.CloneFromTemplate(
						sceneRegistry,
						templateSceneRegistry,
						*pComponentTypeInfo,
						templateDataComponent,
						templateSceneComponent,
						component,
						dataComponentBatch
					);
					if (dataComponentBatch.IsValid())
					{
						jobBatch.QueueAfterStartStage(dataComponentBatch);
					}
					return Memory::CallbackResult::Continue;
				}
			);

			if (const Optional<Serialization::Reader> dataComponentsReader = serializer.FindSerializer("data_components"))
			{
				Threading::JobBatch dataComponentBatch;
				for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
				{
					[[maybe_unused]] Optional<ComponentValue<Data::Component>> componentValue =
						dataComponentReader.ReadInPlace<ComponentValue<Data::Component>>(component, sceneRegistry, dataComponentBatch);
				}
				Assert(dataComponentBatch.IsInvalid());
			}

			CloneTemplateComponentChildren(
				*templateSceneComponent,
				component,
				component,
				targetComponentInfoMap,
				sceneChildInstanceData,
				*sceneRegistry.FindComponentTypeData<ExternalScene>(),
				sceneRegistry,
				templateSceneRegistry,
				jobBatch
			);

			const bool isReadingSceneDefinition = serializer.ReadWithDefaultValue<Guid>("guid", Guid()) == GetAssetGuid();
			if (!isReadingSceneDefinition)
			{
				Threading::JobBatch childJobBatch = SerializeDataComponentsAndNonTemplateChildren(serializer, component, targetComponentInfoMap);
				jobBatch.QueueAsNewFinishedStage(childJobBatch);
			}

			// TODO: Restructure all component construction so the parent is always attached later
			// This way we can create all instances of a type in one batch for performance.

			if (jobBatch.IsValid())
			{
				jobBatch.GetFinishedStage().AddSubsequentStage(finishedStage);
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
			else
			{
				finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
			}
		}
		else
		{
			finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
		}
	}

	void ExternalScene::SpawnAssetChildren(
		Entity::HierarchyComponentBase& component, const Optional<const Entity::HierarchyComponentBase*> templateSceneComponent
	)
	{
		Assert(templateSceneComponent.IsValid());
		if (LIKELY(templateSceneComponent.IsValid()))
		{
			Threading::JobBatch jobBatch;

			Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
			const Entity::SceneRegistry& templateSceneRegistry = templateSceneComponent->GetSceneRegistry();

			// Copy template root data components
			templateSceneComponent->IterateDataComponents(
				templateSceneRegistry,
				[&component,
			   &sceneRegistry,
			   &templateSceneRegistry,
			   &templateSceneComponent = *templateSceneComponent,
			   &jobBatch](Entity::Data::Component& templateDataComponent, const Optional<ComponentTypeInterface*> pComponentTypeInfo, ComponentTypeSceneDataInterface&)
				{
					Entity::ComponentValue<Entity::Data::Component> componentValue;
					Threading::JobBatch dataComponentBatch;
					componentValue.CloneFromTemplate(
						sceneRegistry,
						templateSceneRegistry,
						*pComponentTypeInfo,
						templateDataComponent,
						templateSceneComponent,
						component,
						dataComponentBatch
					);
					if (dataComponentBatch.IsValid())
					{
						jobBatch.QueueAfterStartStage(dataComponentBatch);
					}
					return Memory::CallbackResult::Continue;
				}
			);

			// Copy over children
			for (const Entity::HierarchyComponentBase& templateChildComponent : templateSceneComponent->GetChildren())
			{
				Threading::JobBatch childJobBatch;
				templateChildComponent.GetTypeInfo()->CloneFromTemplateWithChildren(
					Guid::Generate(),
					templateChildComponent,
					*templateSceneComponent,
					component,
					System::Get<Entity::Manager>().GetRegistry(),
					sceneRegistry,
					templateSceneRegistry,
					childJobBatch
				);
				if (childJobBatch.IsValid())
				{
					jobBatch.QueueAfterStartStage(childJobBatch);
				}
			}

			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
		}
	}

	void RemoveDuplicateMembers(Serialization::Value& value, const Serialization::Value& referenceValue);

	void RemoveDuplicateArrayElements(Serialization::Value& value, const Serialization::Value& referenceValue)
	{
		Assert(value.GetType() == referenceValue.GetType());
		Assert(value.GetType() == rapidjson::kArrayType);

		Serialization::Value::ConstValueIterator referenceIt = referenceValue.Begin();
		const Serialization::Value::ConstValueIterator referenceEnd = referenceValue.End();

		// First start by removing all duplicate sub-elements in the array
		for (Serialization::Value::ValueIterator it = value.Begin(); (it != value.End()) & (referenceIt != referenceEnd); ++referenceIt, ++it)
		{
			switch (it->GetType())
			{
				case rapidjson::kObjectType:
					RemoveDuplicateMembers(*it, *referenceIt);
					break;
				case rapidjson::kArrayType:
					RemoveDuplicateArrayElements(*it, *referenceIt);
					break;
				default:
					break;
			}
		}

		referenceIt = referenceValue.Begin();

		// Now clear the array if all elements are identical to reference
		rapidjson::SizeType erasableElementCount = 0;
		for (Serialization::Value::ValueIterator it = value.Begin(); (it != value.End()) & (referenceIt != referenceEnd); ++referenceIt, ++it)
		{
			const bool isEqualToReference = it->GetType() == referenceIt->GetType() && *it == *referenceIt;
			const bool isEmpty = (it->IsObject() && it->MemberCount() == 0) || (it->IsArray() && it->Size() == 0);

			if (isEqualToReference || isEmpty)
			{
				erasableElementCount++;
			}
		}
		if (erasableElementCount == value.Size())
		{
			value.Clear();
		}
	}

	void RemoveDuplicateMembers(Serialization::Value& value, const Serialization::Value& referenceValue)
	{
		Assert(value.GetType() == referenceValue.GetType());
		Assert(value.GetType() == rapidjson::kObjectType);

		for (Serialization::Value::MemberIterator memberIt = value.MemberBegin(); memberIt != value.MemberEnd();)
		{
			Serialization::Value::ConstMemberIterator referenceIt = referenceValue.FindMember(memberIt->name);
			if (referenceIt == referenceValue.MemberEnd())
			{
				memberIt++;
				continue;
			}

			if (memberIt->value == referenceIt->value)
			{
				// Never remove identifying values
				if (memberIt->name == "typeGuid" || memberIt->name == "typeIdentifier" || memberIt->name == "data_components")
				{
					memberIt++;
					continue;
				}

				memberIt = value.EraseMember(memberIt);
			}
			else
			{
				switch (memberIt->value.GetType())
				{
					case rapidjson::kObjectType:
					{
						RemoveDuplicateMembers(memberIt->value, referenceIt->value);
						switch (memberIt->value.MemberCount())
						{
							case 0:
								memberIt = value.EraseMember(memberIt);
								break;
							case 1:
							{
								const Serialization::Value::ConstMemberIterator firstMemberIt = memberIt->value.GetObject().MemberBegin();
								if (firstMemberIt->name.IsString() && (ConstStringView(firstMemberIt->name.GetString(), firstMemberIt->name.GetStringLength()) == "typeGuid" || ConstStringView(firstMemberIt->name.GetString(), firstMemberIt->name.GetStringLength()) == "typeIdentifier"))
								{
									memberIt = value.EraseMember(memberIt);
								}
								else
								{
									memberIt++;
								}
							}
							break;
							default:
								memberIt++;
						}
					}
					break;
					case rapidjson::kArrayType:
					{
						// Don't touch data components
						if (memberIt->name == "data_components")
						{
							return;
						}

						RemoveDuplicateArrayElements(memberIt->value, referenceIt->value);
						if (memberIt->value.Size() == 0)
						{
							memberIt = value.EraseMember(memberIt);
						}
						else
						{
							memberIt++;
						}
					}
					break;
					case rapidjson::kNumberType:
					{
						if (memberIt->value.IsDouble())
						{
							Assert(Math::IsEquivalentTo(memberIt->value.GetFloat(), referenceIt->value.GetFloat(), 0.0005f));
							constexpr double epsilon = Math::NumericLimits<float>::Epsilon;
							if (Math::IsEquivalentTo(memberIt->value.GetDouble(), referenceIt->value.GetDouble(), epsilon))
							{
								memberIt = value.EraseMember(memberIt);
								break;
							}
						}
						memberIt++;
					}
					break;
					default:
						memberIt++;
						break;
				}
			}
		}
	}

	/* static */ bool RemoveDuplicateComponentMembers(Serialization::Writer componentWriter, const Serialization::Writer templateWriter)
	{
		// Do a normal pass removing duplicated members
		RemoveDuplicateMembers(componentWriter.GetValue(), templateWriter.GetValue());

		// Now step through the hierarchy and remove components by detecting the templated component by instance guid
		static auto getComponentTemplateInstanceGuid = [](const Serialization::Reader componentReader)
		{
			Guid componentTemplateInstanceGuid;
			if (const Optional<Serialization::Reader> dataComponentsReader = componentReader.FindSerializer("data_components"))
			{
				for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
				{
					if (*dataComponentReader.Read<Guid>("typeGuid") == Reflection::GetTypeGuid<Entity::SceneChildInstance>())
					{
						Threading::JobBatch jobBatch;
						SceneChildInstance sceneChildInstance;
						Reflection::GetType<SceneChildInstance>()
							.SerializeTypePropertiesInline(dataComponentReader, dataComponentReader, sceneChildInstance, Invalid, jobBatch);
						Assert(jobBatch.IsInvalid());

						componentTemplateInstanceGuid = sceneChildInstance.m_sourceTemplateInstanceGuid;
						break;
					}
				}
			}
			return componentTemplateInstanceGuid;
		};

		const Serialization::Reader templateComponentReader{templateWriter.GetValue(), templateWriter.GetData()};
		const Serialization::Reader componentReader{componentWriter.GetValue(), componentWriter.GetData()};

		Optional<Serialization::Reader> dataComponentsReader = componentReader.FindSerializer("data_components");
		const Optional<Serialization::Reader> templateDataComponentsReader = templateComponentReader.FindSerializer("data_components");
		if (dataComponentsReader.IsValid())
		{
			Serialization::Array& dataComponents =
				Serialization::Array::GetFromReference(const_cast<Serialization::TValue&>(dataComponentsReader->GetValue()));

			dataComponents.RemoveAllOccurrencesPredicate(
				[&data = dataComponentsReader->GetData(), templateDataComponentsReader](Serialization::Value& dataComponentValue)
				{
					const Serialization::Reader dataComponentReader(dataComponentValue, data);
					Assert(dataComponentValue.HasMember("typeGuid"));
					const Guid typeGuid = *dataComponentReader.Read<Guid>("typeGuid");
					if (typeGuid == Reflection::GetTypeGuid<SceneChildInstance>())
					{
						// Never remove scene child instance data
						return false;
					}

					bool isIdenticalToTemplate = false;
					if (templateDataComponentsReader.IsValid())
					{
						for (const Serialization::Reader templateDataComponentReader : templateDataComponentsReader->GetArrayView())
						{
							const Guid templateTypeGuid = *templateDataComponentReader.Read<Guid>("typeGuid");
							if (templateTypeGuid == typeGuid)
							{
								isIdenticalToTemplate = dataComponentValue == templateDataComponentReader.GetValue().GetValue();
								break;
							}
						}
					}

					bool shouldRemove = isIdenticalToTemplate;
					if (Optional<const Reflection::TypeInterface*> pTypeInterface = System::Get<Reflection::Registry>().FindTypeInterface(typeGuid))
					{
						// TODO: This is a workaround for not having a user added flag
					  // For now we assume that if it is not hidden from UI it should also be serialized when it has no properties
					  // This could be for example a data component that the user added
						const bool notUserAdded = pTypeInterface->GetFlags().IsSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation);
						shouldRemove |= notUserAdded && dataComponentValue.MemberCount() == 1;
					}
					return shouldRemove;
				}
			);

			if (dataComponents.IsEmpty())
			{
				componentWriter.GetValue().RemoveMember("data_components");
				dataComponentsReader = Invalid;
			}
		}

		// Go through children
		const Optional<Serialization::Reader> childrenReader = componentReader.FindSerializer("children");
		const Optional<Serialization::Reader> templateComponentChildrenReader = templateComponentReader.FindSerializer("children");

		if (childrenReader.IsValid())
		{
			if (templateComponentChildrenReader.IsValid())
			{
				Serialization::Array& children =
					Serialization::Array::GetFromReference(const_cast<Serialization::TValue&>(childrenReader->GetValue()));
				children.RemoveAllOccurrencesPredicate(
					[&data = childrenReader->GetData(),
				   templateComponentChildrenReader = *templateComponentChildrenReader](const Serialization::Value& child)
					{
						Serialization::Reader childReader{child, data};
						const Guid childTemplateInstanceGuid = getComponentTemplateInstanceGuid(childReader);
						bool shouldRemove = false;
						if (childTemplateInstanceGuid.IsValid())
						{
							for (const Serialization::Reader templateChildReader : templateComponentChildrenReader.GetArrayView())
							{
								const Guid templateChildInstanceGuid = templateChildReader.ReadWithDefaultValue("instanceGuid", Guid{});
								if (templateChildInstanceGuid == childTemplateInstanceGuid)
								{
									shouldRemove = RemoveDuplicateComponentMembers(
										Serialization::Writer{
											const_cast<Serialization::TValue&>(childReader.GetValue()),
											const_cast<Serialization::Data&>(childReader.GetData())
										},
										Serialization::Writer{
											const_cast<Serialization::TValue&>(templateChildReader.GetValue()),
											const_cast<Serialization::Data&>(templateChildReader.GetData())
										}
									);
									break;
								}
							}
						}

						return shouldRemove;
					}
				);
			}
		}
		else
		{
			Assert(
				componentReader.GetValue().GetValue().HasMember("typeGuid") || componentReader.GetValue().GetValue().HasMember("typeIdentifier")
			);
			Assert(
				componentReader.GetValue().GetValue().HasMember("instanceGuid") ||
				componentReader.GetValue().GetValue().HasMember("instanceIdentifier")
			);
			const bool hasTemplateInstanceGuid = getComponentTemplateInstanceGuid(componentReader).IsValid();
			const uint8 defaultMemberCount = 2 + hasTemplateInstanceGuid;

			// Detect this component being equivalent to the template
			// In that case we return true to indicate it can be removed.
			// Note that the instanceGuid will be different.
			auto hasSameTypeIdentifier = [](const Serialization::Reader componentReader, const Serialization::Reader templateComponentReader)
			{
				if (const Optional<Guid> componentGuid = componentReader.Read<Guid>("typeGuid"))
				{
					const Optional<Guid> templateComponentGuid = templateComponentReader.Read<Guid>("typeGuid");
					if (templateComponentGuid.IsValid())
					{
						return *componentGuid == *templateComponentGuid;
					}
				}
				else if (const Optional<typename ComponentTypeIdentifier::InternalType> componentTypeIdentifier = componentReader.Read<typename ComponentTypeIdentifier::InternalType>("typeIdentifier"))
				{
					const Optional<typename ComponentTypeIdentifier::InternalType> templateComponentTypeIdentifier =
						templateComponentReader.Read<typename ComponentTypeIdentifier::InternalType>("typeIdentifier");
					if (templateComponentTypeIdentifier.IsValid())
					{
						return *componentTypeIdentifier == *templateComponentTypeIdentifier;
					}
				}
				return false;
			};

			const bool hasSameDataComponents = dataComponentsReader.IsInvalid() || dataComponentsReader->GetValue().GetValue().Size() == 1;
			const bool hasSameMembers = componentReader.GetValue().GetValue().MemberCount() <= defaultMemberCount;
			return hasSameMembers & hasSameTypeIdentifier(componentReader, templateComponentReader) & hasSameDataComponents;
		}

		return false;
	}

	/* static */ bool FilterDifferencesFromTemplate(Serialization::Writer writer, const Entity::HierarchyComponentBase& templateComponent)
	{
		Serialization::Data templateComponentData(rapidjson::Type::kObjectType, writer.GetData().GetContextFlags());
		Serialization::Writer templateComponentWriter(templateComponentData);
		if (const Optional<const ComponentTypeInterface*> pTemplateComponentTypeInfo = templateComponent.GetTypeInfo())
		{
			pTemplateComponentTypeInfo
				->SerializeInstanceWithoutChildren(templateComponentWriter, templateComponent, templateComponent.GetParent());
			templateComponent.SerializeDataComponents(templateComponentWriter);

			if (RemoveDuplicateComponentMembers(writer, templateComponentWriter))
			{
				return false;
			}
		}
		return true;
	}

	using TemplateComponents = UnorderedMap<Guid, ReferenceWrapper<const Entity::HierarchyComponentBase>, Guid::Hash>;

	/* static */ uint32 GetTemplateComponentChildCount(const Entity::HierarchyComponentBase& templateComponent)
	{
		uint32 count{1};
		for (const Entity::HierarchyComponentBase& childTemplateComponent : templateComponent.GetChildren())
		{
			count += GetTemplateComponentChildCount(childTemplateComponent);
		}
		return count;
	}

	/* static */ void PopulateTemplateComponents(
		const Entity::HierarchyComponentBase& templateComponent,
		TemplateComponents& templateComponents,
		const Entity::SceneRegistry& templateSceneRegistry
	)
	{
		for (const Entity::HierarchyComponentBase& childTemplateComponent : templateComponent.GetChildren())
		{
			templateComponents.Emplace(childTemplateComponent.GetInstanceGuid(templateSceneRegistry), childTemplateComponent);

			PopulateTemplateComponents(childTemplateComponent, templateComponents, templateSceneRegistry);
		}
	}

	[[nodiscard]] TemplateComponents::iterator FindTemplateComponentIterator(
		TemplateComponents& templateComponents, const Optional<const SceneChildInstance*> pSceneChildInstanceComponent
	)
	{
		if (pSceneChildInstanceComponent.IsValid())
		{
			auto it = templateComponents.Find(pSceneChildInstanceComponent->m_parentTemplateInstanceGuid);
			if (it == templateComponents.end())
			{
				it = templateComponents.Find(pSceneChildInstanceComponent->m_sourceTemplateInstanceGuid);
			}
			return it;
		}
		return templateComponents.end();
	}

	[[nodiscard]] Optional<const Entity::HierarchyComponentBase*>
	FindTemplateComponent(TemplateComponents& templateComponents, const Optional<const SceneChildInstance*> pSceneChildInstanceComponent)
	{
		if (const TemplateComponents::iterator it = FindTemplateComponentIterator(templateComponents, pSceneChildInstanceComponent);
		    it != templateComponents.end())
		{
			return *it->second;
		}
		return Invalid;
	}

	[[nodiscard]] Optional<const Entity::HierarchyComponentBase*> FindTemplateComponent(
		const Entity::HierarchyComponentBase& component,
		TemplateComponents& templateComponents,
		Entity::ComponentTypeSceneData<SceneChildInstance>& sceneChildInstanceData
	)
	{
		return FindTemplateComponent(templateComponents, component.FindDataComponentOfType<SceneChildInstance>(sceneChildInstanceData));
	}

	/* static */ bool FilterSceneChildInstanceProperties(
		Serialization::Writer writer,
		const Optional<const Entity::HierarchyComponentBase*> pTemplateComponent,
		TemplateComponents& templateComponents,
		const Optional<const SceneChildInstance*> pSceneChildInstanceComponent
	)
	{
		if (pTemplateComponent.IsValid())
		{
			const TemplateComponents::iterator it = FindTemplateComponentIterator(templateComponents, pSceneChildInstanceComponent);
			if (Ensure(it != templateComponents.end()))
			{
				Assert(pTemplateComponent == &*it->second);
				const bool shouldKeep = FilterDifferencesFromTemplate(writer, it->second);
				templateComponents.Remove(it);
				return shouldKeep;
			}
		}
		return true;
	}

	/* static */ bool SerializeComponent(
		Serialization::Writer writer,
		Entity::SceneRegistry& sceneRegistry,
		const Entity::SceneRegistry& templateSceneRegistry,
		const Entity::HierarchyComponentBase& component,
		TemplateComponents& templateComponents,
		Entity::ComponentTypeSceneData<SceneChildInstance>& sceneChildInstanceData
	)
	{
		if (!component.ShouldSerialize(writer))
		{
			return false;
		}

		if (const Optional<const ComponentTypeInterface*> pComponentTypeInfo = component.GetTypeInfo(sceneRegistry))
		{
			pComponentTypeInfo->SerializeInstanceWithoutChildren(writer, component, component.GetParent());
		}

		const Optional<SceneChildInstance*> pSceneChildInstance = component.FindDataComponentOfType<SceneChildInstance>(sceneChildInstanceData);
		const Optional<const Entity::HierarchyComponentBase*> pTemplateComponent =
			FindTemplateComponent(templateComponents, pSceneChildInstance);

		const Entity::ComponentTypeMask dataComponentsMask = sceneRegistry.GetDataComponentMask(component.GetIdentifier());
		Serialization::Array dataComponents(Memory::Reserve, dataComponentsMask.GetNumberOfSetBits(), writer.GetDocument());

		// Detect removed data components
		if (pTemplateComponent.IsValid())
		{
			const Entity::ComponentTypeMask templateDataComponentsMask =
				templateSceneRegistry.GetDataComponentMask(pTemplateComponent->GetIdentifier());
			const Entity::ComponentTypeMask removedTemplateDataComponentsMask = templateDataComponentsMask & ~dataComponentsMask;
			for (const Entity::ComponentTypeIdentifier::IndexType removedComponentTypeIdentifierIndex :
			     removedTemplateDataComponentsMask.GetSetBitsIterator())
			{
				const Entity::ComponentTypeIdentifier removedComponentTypeIdentifier =
					Entity::ComponentTypeIdentifier::MakeFromValidIndex(removedComponentTypeIdentifierIndex);
				const Optional<const Entity::ComponentTypeSceneDataInterface*> pTemplateComponentTypeSceneData =
					templateSceneRegistry.FindComponentTypeData(removedComponentTypeIdentifier);
				Assert(pTemplateComponentTypeSceneData.IsValid());
				if (LIKELY(pTemplateComponentTypeSceneData.IsValid()))
				{
					Serialization::Object serializedValue;
					Serialization::Writer dataComponentWriter(serializedValue, writer.GetData());

					dataComponentWriter.Serialize("typeGuid", pTemplateComponentTypeSceneData->GetTypeInterface().GetTypeInterface().GetGuid());
					dataComponentWriter.Serialize("removed", true);

					dataComponents.PushBack(Move(serializedValue), writer.GetDocument());
				}
			}
		}

		if (dataComponentsMask.AreAnySet())
		{
			const Entity::ComponentIdentifier identifier = component.GetIdentifier();

			for (const Entity::ComponentTypeIdentifier::IndexType componentTypeIdentifierIndex : dataComponentsMask.GetSetBitsIterator())
			{
				const Entity::ComponentTypeIdentifier componentTypeIdentifier =
					Entity::ComponentTypeIdentifier::MakeFromValidIndex(componentTypeIdentifierIndex);

				const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneData =
					sceneRegistry.FindComponentTypeData(componentTypeIdentifier);
				Assert(pComponentTypeSceneData.IsValid());
				if (LIKELY(pComponentTypeSceneData.IsValid()))
				{
					Optional<Entity::Component*> pComponent = pComponentTypeSceneData->GetDataComponent(identifier);
					if (LIKELY(pComponent.IsValid()))
					{
						Entity::ComponentTypeInterface& typeInterface = pComponentTypeSceneData->GetTypeInterface();

						Serialization::Object serializedValue;
						Serialization::Writer dataComponentWriter(serializedValue, writer.GetData());

						ComponentValue<Data::Component> dataComponent = static_cast<Data::Component&>(*pComponent);
						if (dataComponentWriter.SerializeInPlace<
									ComponentValue<Data::Component>,
									const ComponentTypeInterface&,
									const Optional<const HierarchyComponentBase*>>(dataComponent, typeInterface, component))
						{
							dataComponents.PushBack(Move(serializedValue), writer.GetDocument());
						}
					}
				}
			}

			if (dataComponents.HasElements())
			{
				Serialization::Value& dataComponentsValue = dataComponents;
				writer.GetAsObject().RemoveMember("data_components");
				writer.AddMember("data_components", Move(dataComponentsValue));
			}
		}

		Serialization::Value childrenValue(rapidjson::Type::kArrayType);
		for (const Entity::HierarchyComponentBase& childComponent : component.GetChildren())
		{
			Serialization::Value childValue(rapidjson::Type::kObjectType);
			Serialization::Writer childWriter(childValue, writer.GetData());
			if (!SerializeComponent(
						childWriter,
						sceneRegistry,
						templateSceneRegistry,
						childComponent,
						templateComponents,
						sceneChildInstanceData
					))
			{
				continue;
			}

			if (!childValue.IsNull() && !childValue.ObjectEmpty())
			{
				childrenValue.PushBack(Move(childValue), writer.GetDocument().GetAllocator());
			}
		}
		if (!childrenValue.Empty())
		{
			writer.AddMember("children", Move(childrenValue));
		}

		return FilterSceneChildInstanceProperties(writer, pTemplateComponent, templateComponents, pSceneChildInstance);
	}

	bool ExternalScene::SerializeTemplateComponents(
		Serialization::Writer writer, const Entity::HierarchyComponentBase& component, const Entity::HierarchyComponentBase& templateComponent
	)
	{
		TemplateComponents templateComponents{Memory::Reserve, GetTemplateComponentChildCount(templateComponent)};
		const Entity::SceneRegistry& templateSceneRegistry = templateComponent.GetSceneRegistry();
		PopulateTemplateComponents(templateComponent, templateComponents, templateSceneRegistry);

		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		Entity::ComponentTypeSceneData<SceneChildInstance>& sceneChildInstanceData =
			*sceneRegistry.GetOrCreateComponentTypeData<SceneChildInstance>();
		[[maybe_unused]] const bool wasWritten =
			SerializeComponent(writer, sceneRegistry, templateSceneRegistry, component, templateComponents, sceneChildInstanceData);
		Assert(wasWritten);
		if (LIKELY(&component != &templateComponent))
		{
			[[maybe_unused]] const bool shouldKeepEntry = FilterDifferencesFromTemplate(writer, templateComponent);
			// TODO: Once we have a smart saving system this should be re-enabled
			// Assert(shouldKeepEntry);
		}

		if (!templateComponents.IsEmpty())
		{
			auto findOrCreateChildren = [](Serialization::Object& parent, Serialization::Document& document) -> Serialization::Array&
			{
				if (Optional<Serialization::TValue*> memberObject = parent.FindMember("children"))
				{
					return Serialization::Array::GetFromReference<Serialization::TValue>(*memberObject);
				}

				return Serialization::Array::GetFromReference<Serialization::TValue>(parent.AddMember("children", Serialization::Array(), document)
				);
			};

			const Entity::ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
			const Entity::ComponentTypeIdentifier sceneChildInstanceTypeIdentifier =
				componentRegistry.FindIdentifier(Reflection::GetTypeGuid<SceneChildInstance>());
			const Entity::ComponentTypeInterface& sceneChildInstanceTypeInfo = *componentRegistry.Get(sceneChildInstanceTypeIdentifier);

			Serialization::Array& children = findOrCreateChildren(writer.GetAsObject(), writer.GetDocument());
			for (auto removedTemplateComponent = templateComponents.begin(), end = templateComponents.end(); removedTemplateComponent != end;
			     ++removedTemplateComponent)
			{
				Serialization::Object& dummyComponentObject =
					Serialization::Object::GetFromReference<Serialization::Value>(children.PushBack(Serialization::Object(), writer.GetDocument()));
				Serialization::Writer dummyComponentWriter(dummyComponentObject, writer.GetData());

				if (writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
				{
					dummyComponentWriter
						.Serialize("typeIdentifier", removedTemplateComponent->second->GetTypeIdentifier(templateSceneRegistry).GetValue());
				}
				else
				{
					dummyComponentWriter.Serialize("typeGuid", removedTemplateComponent->second->GetTypeGuid(templateSceneRegistry));
				}

				Serialization::Array dataComponents;

				Serialization::Object dataComponentObject;
				Serialization::Writer dataComponentWriter(dataComponentObject, writer.GetData());
				dataComponentWriter.Serialize("typeGuid", Reflection::GetTypeGuid<Entity::SceneChildInstance>());

				SceneChildInstance sceneChildInstance(removedTemplateComponent->second->GetInstanceGuid(templateSceneRegistry), {});
				sceneChildInstance.m_flags |= SceneChildInstance::Flags::Removed;

				ComponentValue<Data::Component> dataComponent = sceneChildInstance;
				dataComponentWriter.SerializeInPlace<
					ComponentValue<Data::Component>,
					const Entity::ComponentTypeInterface&,
					const Optional<const HierarchyComponentBase*>>(
					dataComponent,
					sceneChildInstanceTypeInfo,
					Optional<const HierarchyComponentBase*>(&*removedTemplateComponent->second)
				);

				dataComponents.PushBack(Move(dataComponentObject), writer.GetDocument());
				dummyComponentWriter.GetAsObject().RemoveMember("data_components");
				dummyComponentWriter.AddMember("data_components", Move(dataComponents.GetValue()));
			}
		}

		return true;
	}

	[[maybe_unused]] const bool wasExternalSceneRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ExternalScene>>::Make());
	[[maybe_unused]] const bool wasExternalSceneTypeRegistered = Reflection::Registry::RegisterType<ExternalScene>();
}
