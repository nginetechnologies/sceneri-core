#pragma once

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Common/System/Query.h>
#include <Common/Memory/CheckedCast.h>
#include <Common/TypeTraits/IsAbstract.h>
#include <Common/TypeTraits/IsFinal.h>

namespace ngine::Entity
{
	template<typename DataComponentType, typename... Args>
	inline Optional<DataComponentType*>
	DataComponentOwner::CreateDataComponent(ComponentTypeSceneData<DataComponentType>& sceneData, Args&&... args)
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		static_assert(!Reflection::GetType<DataComponentType>().IsAbstract(), "Cannot instantiate abstract component type!");
		const ComponentIdentifier identifier = GetIdentifier();
		return sceneData.CreateInstance(identifier, static_cast<typename DataComponentType::ParentType&>(*this), Forward<Args>(args)...);
	}

	template<typename DataComponentType, typename... Args>
	inline Optional<DataComponentType*> DataComponentOwner::CreateDataComponent(SceneRegistry& sceneRegistry, Args&&... args)
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		ComponentTypeSceneData<DataComponentType>& dataComponentSceneData = *sceneRegistry.GetOrCreateComponentTypeData<DataComponentType>();
		return CreateDataComponent<DataComponentType>(dataComponentSceneData, Forward<Args>(args)...);
	}

	template<typename DataComponentType>
	inline bool DataComponentOwner::RemoveDataComponentOfType(ComponentTypeSceneData<DataComponentType>& sceneData)
	{
		static_assert(!TypeTraits::IsAbstract<DataComponentType>, "Can't remove an abstract component!");
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		SceneRegistry& sceneRegistry = sceneData.GetSceneRegistry();
		const ComponentIdentifier identifier = GetIdentifier();
		if (sceneRegistry.OnDataComponentRemoved(identifier, sceneData.GetIdentifier()))
		{
			sceneData.OnBeforeRemoveInstance(sceneData.GetDataComponentUnsafe(identifier), *this);
			sceneData.RemoveInstance(sceneData.GetDataComponentUnsafe(identifier), *this);
			return true;
		}
		return false;
	}

	template<typename DataComponentType>
	inline bool DataComponentOwner::RemoveDataComponentOfType(const SceneRegistry& sceneRegistry)
	{
		static_assert(!TypeTraits::IsAbstract<DataComponentType>, "Can't remove an abstract component!");
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if (const Optional<ComponentTypeSceneData<DataComponentType>*> pDataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			return RemoveDataComponentOfType(*pDataComponentSceneData);
		}
		return false;
	}

	template<typename DataComponentType>
	inline bool DataComponentOwner::RemoveFirstDataComponentImplementingType(SceneRegistry& sceneRegistry)
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return RemoveDataComponentOfType<DataComponentType>(sceneRegistry);
		}
		else
		{
			const Guid typeGuid = Reflection::GetTypeGuid<DataComponentType>();

			for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(m_identifier))
			{
				const ComponentTypeIdentifier typeIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(index);
				const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneData = sceneRegistry.FindComponentTypeData(typeIdentifier);
				Assert(pComponentTypeSceneData.IsValid());
				if (LIKELY(pComponentTypeSceneData.IsValid()))
				{
					const ComponentTypeInterface& componentTypeInfo = pComponentTypeSceneData->GetTypeInterface();
					if (componentTypeInfo.GetTypeInterface().Implements(typeGuid))
					{
						if (sceneRegistry.OnDataComponentRemoved(m_identifier, ComponentTypeIdentifier::MakeFromValidIndex(index)))
						{
							const ComponentIdentifier identifier = GetIdentifier();
							pComponentTypeSceneData->OnBeforeRemoveInstance(pComponentTypeSceneData->GetDataComponentUnsafe(identifier), *this);
							pComponentTypeSceneData->RemoveInstance(pComponentTypeSceneData->GetDataComponentUnsafe(identifier), *this);
							return true;
						}
					}
				}
			}
			return false;
		}
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Optional<DataComponentType*>
	DataComponentOwner::FindDataComponentOfType(ComponentTypeSceneData<DataComponentType>& sceneData) const
	{
		static_assert(!TypeTraits::IsAbstract<DataComponentType>, "Can't get an abstract component!");
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		return sceneData.GetComponentImplementation(GetIdentifier());
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Optional<DataComponentType*> DataComponentOwner::FindDataComponentOfType(const SceneRegistry& sceneRegistry) const
	{
		static_assert(!TypeTraits::IsAbstract<DataComponentType>, "Can't get an abstract component!");
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			return FindDataComponentOfType<DataComponentType>(*dataComponentSceneData.Get());
		}

		return Invalid;
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Optional<DataComponentType*>
	DataComponentOwner::FindFirstDataComponentImplementingType(const SceneRegistry& sceneRegistry) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return FindDataComponentOfType<DataComponentType>(sceneRegistry);
		}
		else
		{
			if (Optional<Data::Component*> pComponent = FindFirstDataComponentImplementingType(sceneRegistry, Reflection::GetTypeGuid<DataComponentType>()))
			{
				return static_cast<DataComponentType&>(*pComponent);
			}
			return Invalid;
		}
	}

	template<typename DataComponentType>
	[[nodiscard]] inline bool DataComponentOwner::HasDataComponentOfType(const SceneRegistry& sceneRegistry) const
	{
		static_assert(!TypeTraits::IsAbstract<DataComponentType>, "Can't remove an abstract component!");
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		const ComponentTypeIdentifier dataComponentTypeIdentifier =
			System::Get<Entity::Manager>().GetRegistry().FindIdentifier(Reflection::GetTypeGuid<DataComponentType>());
		return sceneRegistry.HasDataComponentOfType(m_identifier, dataComponentTypeIdentifier);
	}

	template<typename DataComponentType>
	[[nodiscard]] inline bool DataComponentOwner::HasAnyDataComponentsImplementingType(const SceneRegistry& sceneRegistry) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return HasDataComponentOfType<DataComponentType>(sceneRegistry);
		}
		else
		{
			return HasAnyDataComponentsImplementingType(sceneRegistry, Reflection::GetTypeGuid<DataComponentType>());
		}
	}

	template<typename Callback>
	inline void DataComponentOwner::IterateDataComponents(const SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		const ComponentIdentifier identifier = GetIdentifier();

		for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(m_identifier))
		{
			const ComponentTypeIdentifier typeIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(index);
			const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneData = sceneRegistry.FindComponentTypeData(typeIdentifier);
			Assert(pComponentTypeSceneData.IsValid());
			if (LIKELY(pComponentTypeSceneData.IsValid()))
			{
				Optional<Component*> pComponent = pComponentTypeSceneData->GetDataComponent(identifier);
				if (LIKELY(pComponent.IsValid()))
				{
					ComponentTypeInterface& componentTypeInfo = pComponentTypeSceneData->GetTypeInterface();
					switch (callback(static_cast<Data::Component&>(*pComponent), componentTypeInfo, *pComponentTypeSceneData))
					{
						case Memory::CallbackResult::Break:
							return;
						case Memory::CallbackResult::Continue:
							break;
					}
				}
			}
		}
	}

	template<typename DataComponentType, typename Callback>
	inline void DataComponentOwner::IterateDataComponentsImplementingType(const SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>, "Type must be a data component!");
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			if (Optional<ComponentTypeSceneData<DataComponentType>*> pDataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
			{
				if (const Optional<DataComponentType*> pDataComponent = FindDataComponentOfType<DataComponentType>(*pDataComponentSceneData))
				{
					const ComponentTypeInterface& componentTypeInfo = pDataComponentSceneData->GetTypeInterface();
					callback(*pDataComponent, componentTypeInfo, *pDataComponentSceneData);
				}
			}
		}
		else
		{
			const ComponentIdentifier identifier = GetIdentifier();
			const Guid typeGuid = Reflection::GetTypeGuid<DataComponentType>();

			for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(m_identifier))
			{
				const Optional<ComponentTypeSceneDataInterface*> pDataComponentSceneData =
					sceneRegistry.FindComponentTypeData(ComponentTypeIdentifier::MakeFromValidIndex(index));
				Assert(pDataComponentSceneData.IsValid());
				if (LIKELY(pDataComponentSceneData.IsValid()))
				{
					Optional<Component*> pComponent = pDataComponentSceneData->GetDataComponent(identifier);
					if (LIKELY(pComponent.IsValid()))
					{
						const ComponentTypeInterface& componentTypeInfo = pDataComponentSceneData->GetTypeInterface();
						if (componentTypeInfo.GetTypeInterface().Implements(typeGuid))
						{
							switch (callback(static_cast<DataComponentType&>(*pComponent), componentTypeInfo, *pDataComponentSceneData))
							{
								case Memory::CallbackResult::Break:
									return;
								case Memory::CallbackResult::Continue:
									break;
							}
						}
					}
				}
			}
		}
	}

	template<typename Callback>
	inline void DataComponentOwner::IterateDataComponentsImplementingType(
		const SceneRegistry& sceneRegistry, const Guid typeGuid, Callback&& callback
	) const
	{
		const ComponentIdentifier identifier = GetIdentifier();

		for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(m_identifier))
		{
			const Optional<ComponentTypeSceneDataInterface*> pDataComponentSceneData =
				sceneRegistry.FindComponentTypeData(ComponentTypeIdentifier::MakeFromValidIndex(index));
			Assert(pDataComponentSceneData.IsValid());
			if (LIKELY(pDataComponentSceneData.IsValid()))
			{
				Optional<Component*> pComponent = pDataComponentSceneData->GetDataComponent(identifier);
				if (LIKELY(pComponent.IsValid()))
				{
					const ComponentTypeInterface& componentTypeInfo = pDataComponentSceneData->GetTypeInterface();
					Assert(componentTypeInfo.GetFlags().IsSet(ComponentTypeInterface::TypeFlags::IsDataComponent));
					if (componentTypeInfo.GetTypeInterface().Implements(typeGuid))
					{
						switch (callback(static_cast<Data::Component&>(*pComponent), componentTypeInfo, *pDataComponentSceneData))
						{
							case Memory::CallbackResult::Break:
								return;
							case Memory::CallbackResult::Continue:
								break;
						}
					}
				}
			}
		}
	}
}
