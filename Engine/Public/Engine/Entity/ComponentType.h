#pragma once

#include "ComponentTypeInterface.h"
#include "Data/Component.h"
#include "ComponentIdentifier.h"
#include "ComponentTypeSceneData.h"

#include <Engine/Entity/ComponentTypeSceneDataInterface.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/Data/Component.inl>

#include <Common/Reflection/Property.h>
#include <Common/Reflection/Serialization/Property.h>
#include <Common/TypeTraits/EnableIf.h>
#include <Common/TypeTraits/ConstMemberVariablePointer.h>
#include <Common/TypeTraits/MemberType.h>
#include <Common/TypeTraits/MemberOwnerType.h>
#include <Common/TypeTraits/HasMemberVariable.h>
#include <Common/TypeTraits/Void.h>
#include <Common/TypeTraits/HasMemberFunction.h>
#include <Common/Guid.h>
#include <Common/Memory/Any.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Common/Reflection/Serialization/Type.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Writer.h>

namespace ngine::Entity
{
	template<typename ComponentType_>
	struct ComponentType final : public ComponentTypeInterface
	{
		using Type = ComponentType_;
		using RootType = typename Type::RootType;
		using ParentType = typename Type::ParentType;
		using ChildType = typename Type::ChildType;
		using SceneData = ComponentTypeSceneData<Type>;

		HasTypeMemberFunction(Type, GetInstanceGuid, Guid);
		HasTypeMemberFunction(Type, DeserializeDataComponentsAndChildren, Threading::JobBatch, Serialization::Reader);
		HasTypeMemberFunction(Type, SerializeDataComponentsAndChildren, bool, Serialization::Writer);
		HasTypeMemberFunction(Type, OnDeserialized, void, Serialization::Reader, Threading::JobBatch&);
		HasTypeMemberFunctionNamed(HasDeserializerGetParent, typename Type::Deserializer, GetParent, void);
		HasTypeMemberFunction(Type, OnParentCreated, void, ParentType&);
		HasTypeMemberFunction(Type, CanClone, bool);

		ComponentType()
		{
			if constexpr (TypeTraits::IsBaseOf<Data::Component, Type>)
			{
				m_flags |= ComponentTypeInterface::TypeFlags::IsDataComponent;
			}
		}
		virtual ~ComponentType() = default;

		virtual const Reflection::TypeInterface& GetTypeInterface() const override
		{
			constexpr auto& reflectedType = Reflection::GetType<Type>();
			return reflectedType;
		}

		[[nodiscard]] static constexpr Optional<const ComponentTypeExtension*> GetComponentTypeExtension()
		{
			constexpr auto& reflectedType = Reflection::GetType<Type>();
#if COMPILER_MSVC
			if constexpr (reflectedType.HasExtension<ComponentTypeExtension>)
#else
			if constexpr (reflectedType.template HasExtension<ComponentTypeExtension>)
#endif
			{
				return reflectedType.template GetExtension<ComponentTypeExtension>();
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<const ComponentTypeExtension*> GetTypeExtension() const override
		{
			return GetComponentTypeExtension();
		}

		[[nodiscard]] static constexpr Optional<const IndicatorTypeExtension*> GetIndicatorComponentTypeExtension()
		{
			constexpr auto& reflectedType = Reflection::GetType<Type>();
#if COMPILER_MSVC
			if constexpr (reflectedType.HasExtension<IndicatorTypeExtension>)
#else
			if constexpr (reflectedType.template HasExtension<IndicatorTypeExtension>)
#endif
			{
				return reflectedType.template GetExtension<IndicatorTypeExtension>();
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<const IndicatorTypeExtension*> GetIndicatorTypeExtension() const override
		{
			return GetIndicatorComponentTypeExtension();
		}

		virtual Optional<Entity::Component*> DeserializeInstanceWithoutChildrenManualOnCreated(
			const Reflection::TypeDeserializer& deserializer_, SceneRegistry& sceneRegistry
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				using Deserializer = Reflection::CustomTypeDeserializer<Type>;
				static constexpr bool hasDeserializerConstructor = TypeTraits::HasConstructor<Type, const Deserializer&>;

				const Deserializer& deserializer = static_cast<const Deserializer&>(deserializer_);

				if constexpr (TypeTraits::IsBaseOf<Data::Component, Type>)
				{
					static constexpr bool hasDefaultConstructor = TypeTraits::IsDefaultConstructible<Type>;

					SceneData& sceneData = *sceneRegistry.template GetOrCreateComponentTypeData<Type>(*this);

					typename Type::ParentType& parent = static_cast<typename Type::ParentType&>(deserializer.GetParent());
					const ComponentIdentifier componentIdentifier = parent.GetIdentifier();

					Optional<Type*> pComponent;
					if constexpr (hasDeserializerConstructor || hasDefaultConstructor)
					{
						if (sceneRegistry.ReserveDataComponent(componentIdentifier, sceneData.GetIdentifier()))
						{
							if constexpr (hasDeserializerConstructor)
							{
								pComponent = sceneData.CreateInstanceManualOnCreated(componentIdentifier, deserializer);
							}
							else if constexpr (hasDefaultConstructor)
							{
								pComponent = sceneData.CreateInstanceManualOnCreated(componentIdentifier);
							}
							else
							{
								static_assert(
									Reflection::GetType<Type>().GetFlags().IsSet(Reflection::TypeFlags::DisableDynamicDeserialization),
									"Missing valid component deserialization constructor"
								);
							}

							if (UNLIKELY(!pComponent.IsValid()))
							{
								[[maybe_unused]] const bool wasRemoved =
									sceneRegistry.OnDataComponentDestroyed(componentIdentifier, sceneData.GetIdentifier());
								Assert(wasRemoved);
							}
						}
						else
						{
							pComponent = sceneData.GetComponentImplementation(componentIdentifier);
						}
					}
					else
					{
						pComponent = sceneData.GetComponentImplementation(componentIdentifier);
					}

					if (pComponent.IsValid())
					{
						if constexpr (Reflection::IsReflected<Type>)
						{
							Reflection::GetType<Type>().SerializeTypePropertiesInline(
								deserializer.m_reader,
								deserializer.m_reader,
								*pComponent,
								parent,
								*deserializer.m_pJobBatch
							);
						}
					}

					return pComponent;
				}
				else
				{
					if constexpr (hasDeserializerConstructor)
					{
						SceneData& sceneData = *sceneRegistry.GetOrCreateComponentTypeData<Type>(*this);

						Optional<Type*> pInstance;
						if constexpr (hasDeserializerConstructor)
						{
							pInstance = sceneData.CreateInstanceManualOnCreated(deserializer);
						}
						else
						{
							static_assert(
								Reflection::GetType<Type>().GetFlags().IsSet(Reflection::TypeFlags::DisableDynamicDeserialization),
								"Missing valid component deserialization constructor"
							);
						}

						if (LIKELY(pInstance.IsValid()))
						{
							if constexpr (Reflection::IsReflected<Type>)
							{
								Optional<typename Type::ParentType*> pParent;
								if constexpr (HasDeserializerGetParent)
								{
									pParent = deserializer.GetParent();
								}
								Reflection::GetType<Type>().SerializeType(deserializer.m_reader, *pInstance, pParent, *deserializer.m_pJobBatch);
							}
						}
						return pInstance;
					}
					else
					{
						return Invalid;
					}
				}
			}
			else
			{
				Assert(false, "Attempting to deserialize abstract component type");
				return nullptr;
			}
		}

		virtual Optional<Entity::Component*>
		DeserializeInstanceWithoutChildren(const Reflection::TypeDeserializer& deserializer_, SceneRegistry& sceneRegistry) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pComponent = DeserializeInstanceWithoutChildrenManualOnCreated(deserializer_, sceneRegistry);
				if (LIKELY(pComponent.IsValid()))
				{
					using Deserializer = Reflection::CustomTypeDeserializer<Type>;
					[[maybe_unused]] const Deserializer& deserializer = static_cast<const Deserializer&>(deserializer_);

					if constexpr (HasOnDeserialized)
					{
						static_cast<Type&>(*pComponent).OnDeserialized(deserializer.m_reader, *deserializer.m_pJobBatch);
					}

					OnComponentCreated(*pComponent, deserializer.GetParent(), sceneRegistry);
				}

				return pComponent;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*>
		DeserializeInstanceWithChildren(const Reflection::TypeDeserializer& deserializer_, SceneRegistry& sceneRegistry) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pComponent = DeserializeInstanceWithoutChildrenManualOnCreated(deserializer_, sceneRegistry);
				if (LIKELY(pComponent.IsValid()))
				{
					using Deserializer = Reflection::CustomTypeDeserializer<Type>;
					[[maybe_unused]] const Deserializer& deserializer = static_cast<const Deserializer&>(deserializer_);

					if constexpr (TypeTraits::IsBaseOf<HierarchyComponentBase, Type>)
					{
						if constexpr (HasDeserializeDataComponentsAndChildren)
						{
							Threading::JobBatch childJobBatch =
								static_cast<Type&>(*pComponent).DeserializeDataComponentsAndChildren(deserializer.m_reader);
							deserializer.m_pJobBatch->QueueAsNewFinishedStage(childJobBatch);
						}
					}

					if constexpr (HasOnDeserialized)
					{
						static_cast<Type&>(*pComponent).OnDeserialized(deserializer.m_reader, *deserializer.m_pJobBatch);
					}

					OnComponentCreated(*pComponent, deserializer.GetParent(), sceneRegistry);
				}
				return pComponent;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateManualOnCreated(
			[[maybe_unused]] const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			[[maybe_unused]] const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				const Type& templateComponentImplementation = static_cast<const Type&>(templateComponent);
				if constexpr (HasCanClone)
				{
					if (!templateComponentImplementation.CanClone())
					{
						return Invalid;
					}
				}

				typename Type::ParentType& parentRootType = static_cast<typename Type::ParentType&>(parent);

				using Cloner = Reflection::CustomTypeCloner<Type>;
				static constexpr bool hasCloneConstructor = TypeTraits::HasConstructor<Type, const Type&, const Cloner&>;

				if constexpr (Reflection::GetType<Type>().GetFlags().IsSet(Reflection::TypeFlags::DisableDynamicCloning))
				{
					return Invalid;
				}
				else if constexpr (TypeTraits::IsBaseOf<Data::Component, Type>)
				{
					SceneData& sceneData = *sceneRegistry.GetOrCreateComponentTypeData<Type>(*this);

					DataComponentOwner& dataComponentOwner = static_cast<DataComponentOwner&>(parent);
					const ComponentIdentifier componentIdentifier = dataComponentOwner.GetIdentifier();

					Optional<Type*> pComponent;
					if (sceneRegistry.ReserveDataComponent(componentIdentifier, sceneData.GetIdentifier()))
					{
						if constexpr (hasCloneConstructor)
						{
							pComponent = sceneData.CreateInstanceManualOnCreated(
								componentIdentifier,
								templateComponentImplementation,
								Cloner{jobBatchOut, parentRootType, templateParentComponent, sceneRegistry, templateSceneRegistry}
							);
						}
						else if constexpr (TypeTraits::IsCopyConstructible<Type>)
						{
							pComponent = sceneData.CreateInstanceManualOnCreated(componentIdentifier, templateComponentImplementation);
						}
						else
						{
							static_unreachable("Type did not provide a cloning constructor!");
						}

						if (UNLIKELY(!pComponent.IsValid()))
						{
							[[maybe_unused]] const bool wasRemoved =
								sceneRegistry.OnDataComponentDestroyed(componentIdentifier, sceneData.GetIdentifier());
							Assert(wasRemoved);
						}
					}
					else
					{
						pComponent = sceneData.GetComponentImplementation(componentIdentifier);
						if constexpr (TypeTraits::IsCopyAssignable<Type>)
						{
							*pComponent = templateComponentImplementation;
						}
						else if constexpr (hasCloneConstructor && TypeTraits::IsMoveAssignable<Type>)
						{
							*pComponent = Type(templateComponentImplementation, Cloner{jobBatchOut, parentRootType});
						}
					}

					return pComponent;
				}
				else
				{
					SceneData& sceneData = *sceneRegistry.GetOrCreateComponentTypeData<Type>(*this);

					Optional<Type*> pInstance;
					if constexpr (hasCloneConstructor)
					{
						pInstance = sceneData.CreateInstanceManualOnCreated(
							templateComponentImplementation,
							Cloner{jobBatchOut, parentRootType, sceneRegistry, templateSceneRegistry, instanceGuid, preferredChildIndex}
						);
					}
					else
					{
						static_unreachable("Type did not provide a cloning constructor!");
					}

					if (LIKELY(pInstance.IsValid()))
					{
						templateComponentImplementation.IterateDataComponents(
							templateSceneRegistry,
							[&sceneRegistry,
						   &templateSceneRegistry,
						   &targetComponent = *pInstance,
						   &templateComponentImplementation,
						   &jobBatchOut](Entity::Data::Component& templateDataComponent, ComponentTypeInterface& componentTypeInfo, ComponentTypeSceneDataInterface&)
							{
								Entity::ComponentValue<Entity::Data::Component> componentValue;
								Threading::JobBatch dataComponentBatch;
								componentValue.CloneFromTemplate(
									sceneRegistry,
									templateSceneRegistry,
									componentTypeInfo,
									templateDataComponent,
									templateComponentImplementation,
									targetComponent,
									dataComponentBatch
								);
								if (dataComponentBatch.IsValid())
								{
									jobBatchOut.QueueAfterStartStage(dataComponentBatch);
								}
								return Memory::CallbackResult::Continue;
							}
						);
					}

					return pInstance;
				}
			}
			else
			{
				Assert(false, "Attempting to clone abstract type");
				return nullptr;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateWithoutChildren(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pResult = CloneFromTemplateManualOnCreated(
					instanceGuid,
					templateComponent,
					templateParentComponent,
					parent,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if (LIKELY(pResult.IsValid()))
				{
					OnComponentCreated(*pResult, parent, sceneRegistry);
				}
				return pResult;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateWithChildrenManualOnCreated(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pComponent = CloneFromTemplateManualOnCreated(
					instanceGuid,
					templateComponent,
					templateParentComponent,
					parent,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if constexpr (TypeTraits::IsBaseOf<HierarchyComponentBase, Type>)
				{
					if (LIKELY(pComponent.IsValid()))
					{
						using CloneChild = void (*)(
							const HierarchyComponentBase& templateComponent,
							const HierarchyComponentBase& templateParentComponent,
							HierarchyComponentBase& parent,
							Entity::ComponentRegistry& componentRegistry,
							SceneRegistry& sceneRegistry,
							const SceneRegistry& templateSceneRegistry,
							Threading::JobBatch& jobBatchOut
						);
						static CloneChild cloneChild = [](
																						 const HierarchyComponentBase& templateComponent,
																						 const HierarchyComponentBase& templateParentComponent,
																						 HierarchyComponentBase& parent,
																						 Entity::ComponentRegistry& componentRegistry,
																						 SceneRegistry& sceneRegistry,
																						 const SceneRegistry& templateSceneRegistry,
																						 Threading::JobBatch& jobBatchOut
																					 )
						{
							ComponentTypeInterface& typeInterface =
								*componentRegistry.Get(componentRegistry.FindIdentifier(templateComponent.GetTypeGuid(templateSceneRegistry)));
							Optional<Entity::Component*> pComponent = typeInterface.CloneFromTemplateManualOnCreated(
								Guid::Generate(),
								templateComponent,
								templateParentComponent,
								parent,
								sceneRegistry,
								templateSceneRegistry,
								jobBatchOut,
								Invalid
							);
							if (LIKELY(pComponent.IsValid()))
							{
								for (HierarchyComponentBase& templateChild : templateComponent.GetChildren())
								{
									cloneChild(
										templateChild,
										templateComponent,
										static_cast<HierarchyComponentBase&>(*pComponent),
										componentRegistry,
										sceneRegistry,
										templateSceneRegistry,
										jobBatchOut
									);
								}

								typeInterface.OnComponentCreated(*pComponent, parent, sceneRegistry);
							}
						};

						const HierarchyComponentBase& templateComponentType = static_cast<const HierarchyComponentBase&>(templateComponent);
						for (HierarchyComponentBase& templateChild : templateComponentType.GetChildren())
						{
							cloneChild(
								templateChild,
								templateComponentType,
								static_cast<HierarchyComponentBase&>(*pComponent),
								componentRegistry,
								sceneRegistry,
								templateSceneRegistry,
								jobBatchOut
							);
						}
					}
				}
				return pComponent;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateWithChildren(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			const Optional<Entity::Component*> pComponent = CloneFromTemplateWithChildrenManualOnCreated(
				instanceGuid,
				templateComponent,
				templateParentComponent,
				parent,
				componentRegistry,
				sceneRegistry,
				templateSceneRegistry,
				jobBatchOut,
				preferredChildIndex
			);
			if (LIKELY(pComponent.IsValid()))
			{
				OnComponentCreated(*pComponent, parent, sceneRegistry);
			}

			return pComponent;
		}

		virtual void OnComponentCreated(
			Entity::Component& component, [[maybe_unused]] const Optional<DataComponentOwner*> pParent, SceneRegistry& sceneRegistry
		) const override final
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				if constexpr (SceneData::HasDataComponentOnCreated)
				{
					static_cast<Type&>(component).OnCreated(static_cast<typename Type::ParentType&>(*pParent));
				}
				else if constexpr (SceneData::HasOnCreated)
				{
					static_cast<Type&>(component).OnCreated();
				}

				if constexpr (TypeTraits::IsBaseOf<DataComponentOwner, Type>)
				{
					// Notify data components that its parent finished creating
					static_cast<Type&>(component).IterateDataComponents(
						sceneRegistry,
						[&component = static_cast<DataComponentOwner&>(component
					   )](Entity::Data::Component& dataComponent, ComponentTypeInterface& componentType, ComponentTypeSceneDataInterface&)
						{
							componentType.OnParentComponentCreated(dataComponent, component);
							return Memory::CallbackResult::Continue;
						}
					);
				}
			}
			else
			{
				Assert(false, "Should not be called for abstract types");
			}
		}

		virtual void OnParentComponentCreated(Entity::Component& component, DataComponentOwner& parent) const override final
		{
			if constexpr (HasOnParentCreated)
			{
				static_cast<Type&>(component).OnParentCreated(static_cast<ParentType&>(parent));
			}
		}

		virtual void
		OnComponentDeserialized(Entity::Component& component, const Serialization::Reader reader, Threading::JobBatch& jobBatch) const override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				if constexpr (HasOnDeserialized)
				{
					static_cast<Type&>(component).OnDeserialized(reader, jobBatch);
				}
			}
			else
			{
				Assert(false, "Should not be called for abstract types");
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithoutChildrenManualOnCreated(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				const Guid instanceGuid = reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate());

				Optional<Entity::Component*> pResult = CloneFromTemplateManualOnCreated(
					instanceGuid,
					templateComponent,
					templateParentComponent,
					parent,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if (LIKELY(pResult.IsValid()))
				{
					Type& component = static_cast<Type&>(*pResult);

					Threading::JobBatch localJobBatch;
					Reflection::GetType<Type>().SerializeType(reader, component, static_cast<ParentType&>(parent), localJobBatch);
					jobBatchOut.QueueAsNewFinishedStage(localJobBatch);

					if constexpr (TypeTraits::IsBaseOf<DataComponentOwner, Type>)
					{
						if (const Optional<Serialization::Reader> dataComponentsReader = reader.FindSerializer("data_components"))
						{
							Threading::JobBatch dataComponentBatch;
							for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
							{
								[[maybe_unused]] Optional<ComponentValue<Data::Component>> componentValue =
									dataComponentReader.ReadInPlace<ComponentValue<Data::Component>>(component, sceneRegistry, dataComponentBatch);
							}
							if (dataComponentBatch.IsValid())
							{
								jobBatchOut.QueueAsNewFinishedStage(dataComponentBatch);
							}
						}
					}
				}
				return pResult;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithoutChildren(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pResult = CloneFromTemplateAndSerializeWithoutChildrenManualOnCreated(
					templateComponent,
					templateParentComponent,
					parent,
					reader,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if (LIKELY(pResult.IsValid()))
				{
					OnComponentCreated(*pResult, parent, sceneRegistry);
				}
				return pResult;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithChildrenManualOnCreated(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				const Guid instanceGuid = reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate());

				Optional<Entity::Component*> pResult = CloneFromTemplateWithChildrenManualOnCreated(
					instanceGuid,
					templateComponent,
					templateParentComponent,
					parent,
					componentRegistry,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if (LIKELY(pResult.IsValid()))
				{
					Type& component = static_cast<Type&>(*pResult);

					Threading::JobBatch localJobBatch;
					Reflection::GetType<Type>().SerializeType(reader, component, static_cast<ParentType&>(parent), localJobBatch);
					jobBatchOut.QueueAsNewFinishedStage(localJobBatch);

					if constexpr (TypeTraits::IsBaseOf<DataComponentOwner, Type>)
					{
						if (const Optional<Serialization::Reader> dataComponentsReader = reader.FindSerializer("data_components"))
						{
							Threading::JobBatch dataComponentBatch;
							for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
							{
								[[maybe_unused]] Optional<ComponentValue<Data::Component>> componentValue =
									dataComponentReader.ReadInPlace<ComponentValue<Data::Component>>(component, sceneRegistry, dataComponentBatch);
							}
							if (dataComponentBatch.IsValid())
							{
								jobBatchOut.QueueAsNewFinishedStage(dataComponentBatch);
							}
						}
					}
				}
				return pResult;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithChildren(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				Optional<Entity::Component*> pResult = CloneFromTemplateAndSerializeWithChildrenManualOnCreated(
					templateComponent,
					templateParentComponent,
					parent,
					reader,
					componentRegistry,
					sceneRegistry,
					templateSceneRegistry,
					jobBatchOut,
					preferredChildIndex
				);
				if (LIKELY(pResult.IsValid()))
				{
					OnComponentCreated(*pResult, parent, sceneRegistry);
				}
				return pResult;
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] virtual Threading::JobBatch SerializeInstanceWithChildren(
			const Serialization::Reader reader, Entity::Component& component, const Optional<Entity::Component*> parentComponent
		) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				if constexpr (Reflection::IsReflected<Type>)
				{
					Threading::JobBatch jobBatch;
					Reflection::GetType<Type>()
						.SerializeType(reader, static_cast<Type&>(component), static_cast<ParentType*>(parentComponent.Get()), jobBatch);

					if constexpr (TypeTraits::IsBaseOf<HierarchyComponentBase, Type>)
					{
						if constexpr (HasDeserializeDataComponentsAndChildren)
						{
							Threading::JobBatch childJobBatch = static_cast<Type&>(component).DeserializeDataComponentsAndChildren(reader);
							jobBatch.QueueAsNewFinishedStage(childJobBatch);
						}
					}

					if constexpr (HasOnDeserialized)
					{
						static_cast<Type&>(component).OnDeserialized(reader, jobBatch);
					}

					return jobBatch;
				}
				else if constexpr (TypeTraits::IsBaseOf<HierarchyComponentBase, Type> && HasDeserializeDataComponentsAndChildren)
				{
					return static_cast<Type&>(component).DeserializeDataComponentsAndChildren(reader);
				}
				else
				{
					return {};
				}
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] virtual Threading::JobBatch SerializeInstanceWithoutChildren(
			const Serialization::Reader reader, Entity::Component& component, const Optional<Entity::Component*> parentComponent
		) override
		{
			if constexpr (Reflection::IsReflected<Type> && !Reflection::GetType<Type>().IsAbstract())
			{
				Threading::JobBatch jobBatch;
				Reflection::GetType<Type>()
					.SerializeType(reader, static_cast<Type&>(component), static_cast<ParentType*>(parentComponent.Get()), jobBatch);
				return jobBatch;
			}
			else
			{
				return {};
			}
		}

		virtual bool SerializeInstanceWithoutChildren(
			Serialization::Writer writer,
			const Entity::Component& component,
			[[maybe_unused]] const Optional<const Entity::Component*> parentComponent
		) const override final
		{
			if constexpr (Reflection::IsReflected<Type> && !Reflection::GetType<Type>().IsAbstract())
			{
				constexpr auto& reflectedType = Reflection::GetType<Type>();
				if (reflectedType.GetFlags().IsSet(Reflection::TypeFlags::DisableWriteToDisk) & writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::ToDisk))
				{
					return false;
				}
				else
				{
					if constexpr (Reflection::HasShouldSerialize<Type>)
					{
						if (!static_cast<const Type&>(component).ShouldSerialize(writer))
						{
							return false;
						}
					}

					if constexpr (TypeTraits::IsBaseOf<Data::Component, Type>)
					{
						writer.Serialize("typeGuid", Reflection::GetTypeGuid<Type>());
						reflectedType
							.SerializeTypePropertiesInline(writer, static_cast<const Type&>(component), static_cast<const ParentType&>(*parentComponent));
					}
					else
					{

						if (writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
						{
							const Entity::ComponentTypeSceneData<Type>& componentType =
								static_cast<const Entity::ComponentTypeSceneData<Type>&>(*static_cast<const Type&>(component).GetTypeSceneData());
							writer.Serialize("typeIdentifier", componentType.GetIdentifier().GetValue());
						}
						else
						{
							writer.Serialize("typeGuid", Reflection::GetTypeGuid<Type>());
						}

						[[maybe_unused]] const EnumFlags<Serialization::ContextFlags> contextFlags = writer.GetData().GetContextFlags();

						if (contextFlags.IsNotSet(Serialization::ContextFlags::Duplication))
						{
							if constexpr (HasGetInstanceGuid)
							{
								writer.Serialize("instanceGuid", static_cast<const Type&>(component).GetInstanceGuid());
							}

							if (contextFlags.IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
							{
								const Entity::ComponentTypeSceneData<Type>& componentType =
									static_cast<const Entity::ComponentTypeSceneData<Type>&>(*static_cast<const Type&>(component).GetTypeSceneData());

								using InstanceIdentifier = typename Type::RootType::InstanceIdentifier;

								const GenericComponentInstanceIdentifier genericInstanceIdentifier = componentType.GetComponentInstanceIdentifier(component
								);
								const InstanceIdentifier instanceIdentifier = {
									Memory::CheckedCast<typename InstanceIdentifier::IndexType>(genericInstanceIdentifier.GetIndex()),
									Memory::CheckedCast<typename InstanceIdentifier::IndexReuseType>(genericInstanceIdentifier.GetIndexUseCount())
								};

								writer.Serialize("instanceIdentifier", instanceIdentifier.GetValue());
							}
						}

						if (!reflectedType
						       .Serialize(writer, static_cast<const Type&>(component), static_cast<const ParentType*>(parentComponent.Get())))
						{
							return false;
						}
					}

					return true;
				}
			}
			else
			{
				return true;
			}
		}

		virtual bool SerializeInstanceWithChildren(
			Serialization::Writer writer, const Entity::Component& component, const Optional<const Entity::Component*> parentComponent
		) const override final
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				if (!SerializeInstanceWithoutChildren(writer, component, parentComponent))
				{
					return false;
				}

				if constexpr (TypeTraits::IsBaseOf<HierarchyComponentBase, Type>)
				{
					if constexpr (HasSerializeDataComponentsAndChildren)
					{
						static_cast<const Type&>(component).SerializeDataComponentsAndChildren(writer);
					}
				}

				return true;
			}
			else
			{
				return false;
			}
		}

		[[nodiscard]] virtual UniquePtr<ComponentTypeSceneDataInterface>
		CreateSceneData(const ComponentTypeIdentifier typeIdentifier, Manager& entityManager, SceneRegistry& sceneRegistry) override
		{
			if constexpr (!Reflection::GetType<Type>().IsAbstract())
			{
				return UniquePtr<SceneData>::Make(typeIdentifier, *this, entityManager, sceneRegistry);
			}
			else
			{
				return {};
			}
		}
	};
}
