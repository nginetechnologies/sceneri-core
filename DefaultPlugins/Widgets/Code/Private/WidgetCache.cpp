#include "WidgetCache.h"
#include "WidgetScene.h"
#include "RootWidget.h"
#include "Manager.h"

#include <Widgets/Data/Layout.h>
#include <Widgets/Data/GridLayout.h>
#include <Widgets/Data/Stylesheet.h>
#include <Widgets/Data/ExternalStyle.h>
#include <Widgets/Data/InlineStyle.h>
#include <Widgets/Data/DynamicStyle.h>
#include <Widgets/Data/Modifiers.h>
#include <Widgets/Data/TextDrawable.h>
#include <Widgets/Data/ImageDrawable.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/Data/Tabs.h>
#include <Widgets/Data/ModalTrigger.h>
#include <Widgets/Primitives/CircleDrawable.h>
#include <Widgets/Primitives/RectangleDrawable.h>
#include <Widgets/Primitives/RoundedRectangleDrawable.h>
#include <Widgets/Primitives/GridDrawable.h>
#include <Widgets/Primitives/LineDrawable.h>
#include <Widgets/Style/DynamicEntry.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Asset/AssetType.inl>
#include <Engine/Entity/Manager.h>
#include <Engine/Entity/Scene/SceneChildInstance.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/InstanceGuid.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Asset/Format/Guid.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/IO/Log.h>
#include <Common/Reflection/Registry.h>

namespace ngine::Widgets
{
	WidgetCache::WidgetCache()
	{
		Entity::SceneRegistry& sceneRegistry = System::Get<Entity::Manager>().GetComponentTemplateCache().GetTemplateSceneRegistry();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::FlexLayout>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::GridLayout>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Stylesheet>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::ExternalStyle>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::InlineStyle>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::DynamicStyle>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Modifiers>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::DataSource>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::PropertySource>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::TextDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::ImageDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Primitives::CircleDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Primitives::RectangleDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Primitives::RoundedRectangleDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Primitives::GridDrawable>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::Primitives::LineDrawable>();

		m_pTemplateScene = UniquePtr<Widgets::Scene>::Make(
			sceneRegistry,
			Invalid,
			Invalid,
			Guid::Generate(),
			Scene::Flags::IsDisabled | Scene::Flags::IsTemplate
		);

		RegisterAssetModifiedCallback(System::Get<Asset::Manager>());
	}

	WidgetCache::~WidgetCache() = default;

	Template::~Template() = default;

	TemplateIdentifier WidgetCache::FindOrRegister(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const TemplateIdentifier, const Asset::Guid) -> Template
			{
				return Template();
			}
		);
	}

	Threading::JobBatch
	WidgetCache::TryLoad(const TemplateIdentifier identifier, LoadListenerData&& newListenerData, const EnumFlags<LoadFlags> loadFlags)
	{
		LoadEvent* pSceneRequesters;

		{
			Threading::SharedLock readLock(m_widgetRequesterMutex);
			decltype(m_widgetRequesterMap)::iterator it = m_widgetRequesterMap.Find(identifier);
			if (it != m_widgetRequesterMap.end())
			{
				pSceneRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_widgetRequesterMutex);
				it = m_widgetRequesterMap.Find(identifier);
				if (it != m_widgetRequesterMap.end())
				{
					pSceneRequesters = it->second.Get();
				}
				else
				{
					pSceneRequesters = m_widgetRequesterMap.Emplace(TemplateIdentifier(identifier), UniquePtr<LoadEvent>::Make())->second.Get();
				}
			}
		}

		pSceneRequesters->Emplace(Forward<LoadListenerData>(newListenerData));

		Template& widgetTemplate = GetAssetData(identifier);
		if (widgetTemplate.m_pWidget.IsInvalid())
		{
			if (m_loadingWidgets.Set(identifier))
			{
				if (widgetTemplate.m_pWidget.IsInvalid())
				{
					const Asset::Guid assetGuid = GetAssetGuid(identifier);

					Threading::Job* pLoadAssetJob = System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
						assetGuid,
						Threading::JobPriority::LoadSceneTemplate,
						[assetGuid, this, identifier, pSceneRequesters](const ConstByteView data)
						{
							if (UNLIKELY(!data.HasElements()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Widget template load failed: Asset with guid {0} metadata could not be read", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Serialization::RootReader sceneSerializer = Serialization::GetReaderFromBuffer(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
							);
							if (UNLIKELY(!sceneSerializer.GetData().IsValid()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Widget template load failed: Asset with guid {0} metadata was invalid", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							const Serialization::Reader reader = sceneSerializer;
							const Guid widgetTypeGuid = reader.ReadWithDefaultValue<Guid>("typeGuid", Guid(Reflection::GetTypeGuid<Widget>()));

							Entity::Manager& entityManager = System::Get<Entity::Manager>();
							Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

							Entity::ComponentRegistry& registry = entityManager.GetRegistry();
							const Entity::ComponentTypeIdentifier typeIdentifier = registry.FindIdentifier(widgetTypeGuid);
							if (UNLIKELY(!typeIdentifier.IsValid()))
							{
								System::Get<Log>().Warning(
									SOURCE_LOCATION,
									"Widget template load failed: Asset with guid {0} component type could not be found",
									assetGuid
								);
								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							const Optional<Entity::ComponentTypeInterface*> pComponentTypeInfo = registry.Get(typeIdentifier);
							if (UNLIKELY(!pComponentTypeInfo.IsValid()))
							{
								System::Get<Log>().Warning(
									SOURCE_LOCATION,
									"Widget template load failed: Asset with guid {0} component type could not be found",
									assetGuid
								);
								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Entity::SceneRegistry& sceneRegistry = m_pTemplateScene->GetEntitySceneRegistry();
							Threading::JobBatch componentLoadJobBatchOut;
							Widget::Deserializer deserializer{
								Reflection::TypeDeserializer{reader, reflectionRegistry, componentLoadJobBatchOut},
								m_pTemplateScene->GetRootWidget(),
								sceneRegistry
							};
							Optional<Entity::Component*> pComponent =
								pComponentTypeInfo->DeserializeInstanceWithoutChildrenManualOnCreated(deserializer, sceneRegistry);
							if (UNLIKELY(!pComponent.IsValid()))
							{
								LogWarning("Widget template load failed: Asset with guid {0} failed to deserialize", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Widget& widget = static_cast<Widget&>(*pComponent);
							Template& widgetTemplate = GetAssetData(identifier);
							widgetTemplate.m_pWidget = &widget;

							Threading::JobBatch childJobBatch = widget.DeserializeDataComponentsAndChildren(deserializer.m_reader);
							deserializer.m_pJobBatch->QueueAsNewFinishedStage(childJobBatch);

							pComponentTypeInfo->OnComponentDeserialized(widget, deserializer.m_reader, *deserializer.m_pJobBatch);

							deserializer.m_pJobBatch->QueueAsNewFinishedStage(Threading::CreateCallback(
								[pComponentTypeInfo, &widget, &sceneRegistry](Threading::JobRunnerThread&)
								{
									pComponentTypeInfo->OnComponentCreated(widget, widget.GetParent(), sceneRegistry);
								},
								Threading::JobPriority::UserInterfaceLoading
							));

							// if (widget.IsScene())
							{
								using ApplySceneChildInstance = void (*)(
									Widget& component,
									Entity::ComponentTypeSceneData<Entity::SceneChildInstance>& sceneChildData,
									Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData
								);
								static ApplySceneChildInstance applySceneChildInstance =
									[](
										Widget& __restrict component,
										Entity::ComponentTypeSceneData<Entity::SceneChildInstance>& sceneChildData,
										Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData
									)
								{
									const Guid componentInstanceGuid = instanceGuidSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());

									if (const Optional<Entity::SceneChildInstance*> pSceneChildInstance = component.FindDataComponentOfType<Entity::SceneChildInstance>(sceneChildData))
									{
										pSceneChildInstance->m_parentTemplateInstanceGuid = componentInstanceGuid;
									}
									else
									{
										component.CreateDataComponent<Entity::SceneChildInstance>(sceneChildData, componentInstanceGuid, componentInstanceGuid);
									}

									for (Widget& child : component.GetChildren())
									{
										applySceneChildInstance(child, sceneChildData, instanceGuidSceneData);
									}
								};

								Entity::SceneRegistry& templateSceneRegistry = m_pTemplateScene->GetEntitySceneRegistry();
								Entity::ComponentTypeSceneData<Entity::SceneChildInstance>& sceneChildData =
									*templateSceneRegistry.GetOrCreateComponentTypeData<Entity::SceneChildInstance>();
								Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
									*templateSceneRegistry.GetOrCreateComponentTypeData<Entity::Data::InstanceGuid>();
								for (Widget& child : widget.GetChildren())
								{
									applySceneChildInstance(child, sceneChildData, instanceGuidSceneData);
								}
							}

							Vector<ByteType, size> readData(Memory::ConstructWithSize, Memory::Uninitialized, data.GetDataSize());
							ByteView{readData.GetView()}.CopyFrom(data);
							widgetTemplate.m_data = Move(readData);

							if (componentLoadJobBatchOut.IsValid())
							{
								componentLoadJobBatchOut.QueueAsNewFinishedStage(Threading::CreateCallback(
									[this, identifier, pSceneRequesters](Threading::JobRunnerThread&)
									{
										[[maybe_unused]] const bool wasSet = m_loadedWidgets.Set(identifier);
										Assert(wasSet);

										[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
										Assert(wasCleared);

										{
											Threading::SharedLock readLock(m_widgetRequesterMutex);
											const decltype(m_widgetRequesterMap)::const_iterator it = m_widgetRequesterMap.Find(identifier);
											if (it != m_widgetRequesterMap.end())
											{
												readLock.Unlock();
												(*pSceneRequesters)(identifier);
											}
										}
									},
									Threading::JobPriority::LoadScene
								));
								Threading::JobRunnerThread::GetCurrent()->Queue(componentLoadJobBatchOut);
							}
							else
							{
								[[maybe_unused]] const bool wasSet = m_loadedWidgets.Set(identifier);
								Assert(wasSet);

								[[maybe_unused]] const bool wasCleared = m_loadingWidgets.Clear(identifier);
								Assert(wasCleared);

								(*pSceneRequesters)(identifier);
							}
						}
					);

					if (LIKELY(pLoadAssetJob != nullptr))
					{
						Threading::JobBatch jobBatch{*pLoadAssetJob, Threading::CreateIntermediateStage("Finish Loading Scene Template Asset Stage")};
						return jobBatch;
					}
					else
					{
						return {};
					}
				}
			}
			else
			{
				return {};
			}
		}

		if (m_loadedWidgets.IsSet(identifier))
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent();
			    pThread.IsValid() && loadFlags.IsNotSet(LoadFlags::AlwaysDeferCallback))
			{
				// Scene template was already loaded, only notify the requester
				[[maybe_unused]] const bool wasExecuted = pSceneRequesters->Execute(newListenerData.m_identifier, identifier);
				Assert(wasExecuted);
			}
			else
			{
				return Threading::CreateCallback(
					[pSceneRequesters, listenerIdentifier = newListenerData.m_identifier, identifier](Threading::JobRunnerThread&)
					{
						// Scene template was already loaded, only notify the requester
						[[maybe_unused]] const bool wasExecuted = pSceneRequesters->Execute(listenerIdentifier, identifier);
						Assert(wasExecuted);
					},
					Threading::JobPriority::UserInterfaceLoading
				);
			}
		}
		return {};
	}

	bool WidgetCache::HasLoaded(const TemplateIdentifier identifier) const
	{
		return m_loadedWidgets.IsSet(identifier);
	}

	void WidgetCache::Reset()
	{
		Entity::SceneRegistry& templateSceneRegistry = System::Get<Entity::Manager>().GetComponentTemplateCache().GetTemplateSceneRegistry();

		m_loadingWidgets.Clear(m_loadedWidgets);
		for (const TemplateIdentifier::IndexType identifierIndex : m_loadedWidgets.GetSetBitsIterator())
		{
			const TemplateIdentifier identifier = TemplateIdentifier::MakeFromValidIndex(identifierIndex);
			Template& sceneTemplate = GetAssetData(identifier);
			if (sceneTemplate.m_pWidget.IsValid())
			{
				sceneTemplate.m_pWidget->Destroy(templateSceneRegistry);
			}
			sceneTemplate.m_pWidget = {};
			m_loadedWidgets.Clear(identifier);
			m_loadingWidgets.Clear(identifier);
		}
		for (const TemplateIdentifier::IndexType identifierIndex : m_loadingWidgets.GetSetBitsIterator())
		{
			const TemplateIdentifier identifier = TemplateIdentifier::MakeFromValidIndex(identifierIndex);
			Template& sceneTemplate = GetAssetData(identifier);
			if (sceneTemplate.m_pWidget.IsValid())
			{
				sceneTemplate.m_pWidget->Destroy(templateSceneRegistry);
			}
			sceneTemplate.m_pWidget = {};
			m_loadedWidgets.Clear(identifier);
			m_loadingWidgets.Clear(identifier);
		}
		m_pTemplateScene->ProcessFullDestroyedComponentsQueue();
	}

	void WidgetCache::Reset(const TemplateIdentifier identifier)
	{
		Entity::SceneRegistry& templateSceneRegistry = System::Get<Entity::Manager>().GetComponentTemplateCache().GetTemplateSceneRegistry();

		Template& sceneTemplate = GetAssetData(identifier);
		if (sceneTemplate.m_pWidget.IsValid())
		{
			sceneTemplate.m_pWidget->Destroy(templateSceneRegistry);
			m_pTemplateScene->ProcessFullDestroyedComponentsQueue();
		}
		sceneTemplate.m_pWidget = {};
		m_loadedWidgets.Clear(identifier);
		m_loadingWidgets.Clear(identifier);
	}

#if DEVELOPMENT_BUILD
	void WidgetCache::OnAssetModified(const Asset::Guid, const IdentifierType, [[maybe_unused]] const IO::PathView filePath)
	{
	}
#endif
}
