#pragma once

#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/ComponentTypeMask.h>
#include <Engine/Entity/ComponentMask.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Guid.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Threading/AtomicPtr.h>
#include <Common/Reflection/GetType.h>
#include <Common/EnumFlagOperators.h>
#include <Common/EnumFlags.h>

namespace ngine::Context::Data
{
	struct Component;
	struct Reference;
}

namespace ngine::Entity
{
	struct Manager;
	struct ComponentTypeInterface;
	struct ComponentTypeSceneDataInterface;

	template<typename ComponentType_>
	struct ComponentType;

	namespace Data
	{
		struct TypeIndex;
		struct Parent;
		struct PreferredIndex;
		struct InstanceGuid;
		struct Tags;
		struct Flags;

		struct WorldTransform;
		struct LocalTransform3D;
		struct BoundingBox;
		struct OctreeNode;

		struct WorldTransform2D;
		struct LocalTransform2D;
		struct QuadtreeNode;

		namespace RenderItem
		{
			struct StageMask;
			struct Identifier;
			struct TransformChangeTracker;

			struct StaticMeshIdentifier;
			struct MaterialInstanceIdentifier;
			struct VisibilityListener;
		}
	}

	struct SceneRegistry final
	{
		enum class Flags : uint8
		{
			CanModifyFrameGraph = 1 << 0,
			CanModifySceneFrameGraph = 1 << 0
		};

		SceneRegistry();
		~SceneRegistry();

		void Enable(Threading::StageBase& startFrameStage, Threading::StageBase& endFrameStage);
		void Disable(Threading::StageBase& startFrameStage, Threading::StageBase& endFrameStage);

		//! Queues a function to be called when the frame graph is safe to modify for this scene.
		//! Can execute immediately if it is currently safe!
		void ModifyFrameGraph(Function<void(), 24>&& callback);

		enum class CachedTypeSceneData : uint8
		{
			TypeIndex,
			Parent,
			InstanceGuid,
			Tags,
			Flags,
			PreferredIndex,

			RenderItemStageMask,
			RenderItemIdentifier,
			RenderItemTransformChangeTracker,

			StaticMeshIdentifier,
			MaterialInstanceIdentifier,
			VisibilityListener,

			WorldTransform,
			LocalTransform3D,
			BoundingBox,
			OctreeNode,

			WorldTransform2D,
			LocalTransform2D,
			QuadtreeNode,

			Context,
			ContextReference,

			Count,
			Invalid
		};
		template<typename Type>
		[[nodiscard]] PURE_STATICS ComponentTypeSceneData<Type>& GetCachedSceneData() const
		{
			constexpr CachedTypeSceneData cachedTypeSceneData = GetSceneDataType<Type>();
			static_assert(cachedTypeSceneData != CachedTypeSceneData::Invalid, "Type not cached!");
			return static_cast<ComponentTypeSceneData<Type>&>(*m_cachedTypeSceneData[cachedTypeSceneData].Get());
		}
		[[nodiscard]] ComponentTypeIdentifier FindComponentTypeIdentifier(const Guid typeGuid) const;

		Optional<ComponentTypeSceneDataInterface*>
		CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentTypeInterface& componentType);

		template<typename Type>
		inline ComponentTypeSceneData<Type>&
		CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentType<Type>& componentType)
		{
			Assert(typeIdentifier.IsValid());
			Assert(m_componentTypeSceneData[typeIdentifier] == nullptr);

			ComponentTypeSceneData<Type>* pComponentTypeSceneData =
				new ComponentTypeSceneData<Type>{typeIdentifier, componentType, m_manager, *this};
			ComponentTypeSceneDataInterface* pExpected{nullptr};
			const bool wasExchanged = m_componentTypeSceneData[typeIdentifier].CompareExchangeStrong(pExpected, pComponentTypeSceneData);
			Assert(wasExchanged);
			if (UNLIKELY(!wasExchanged))
			{
				delete pComponentTypeSceneData;
			}

			return *pComponentTypeSceneData;
		}

		template<typename Type>
		inline Optional<ComponentTypeSceneData<Type>*> CreateComponentTypeData(ComponentType<Type>& componentType)
		{
			const ComponentTypeIdentifier typeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>());
			if (LIKELY(typeIdentifier.IsValid()))
			{
				return CreateComponentTypeData<Type>(typeIdentifier, componentType);
			}
			return Invalid;
		}

		[[nodiscard]] Optional<ComponentTypeSceneDataInterface*> FindComponentTypeData(const ComponentTypeIdentifier typeIdentifier) const
		{
			const Optional<ComponentTypeSceneDataInterface*> pComponentSceneData = m_componentTypeSceneData[typeIdentifier].Load();
			return Optional<ComponentTypeSceneDataInterface*>(pComponentSceneData.Get(), pComponentSceneData.IsValid());
		}

		template<typename Type>
		[[nodiscard]] Optional<ComponentTypeSceneData<Type>*> FindComponentTypeData(const ComponentTypeIdentifier typeIdentifier) const
		{
			Assert(typeIdentifier == FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>()));
			return static_cast<ComponentTypeSceneData<Type>*>(FindComponentTypeData(typeIdentifier).Get());
		}

		template<typename Type>
		[[nodiscard]] Optional<ComponentTypeSceneData<Type>*> FindComponentTypeData() const
		{
			if constexpr (GetSceneDataType<Type>() != CachedTypeSceneData::Invalid)
			{
				return GetCachedSceneData<Type>();
			}
			else
			{
				const ComponentTypeIdentifier componentTypeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>());
				if (LIKELY(componentTypeIdentifier.IsValid()))
				{
					const Optional<ComponentTypeSceneDataInterface*> pComponentSceneData = m_componentTypeSceneData[componentTypeIdentifier].Load();
					return Optional<ComponentTypeSceneData<Type>*>(
						static_cast<ComponentTypeSceneData<Type>*>(pComponentSceneData.Get()),
						pComponentSceneData.IsValid()
					);
				}
				else
				{
					return Invalid;
				}
			}
		}

		[[nodiscard]] Optional<ComponentTypeSceneDataInterface*>
		GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentTypeInterface& componentType);

		template<typename Type>
		[[nodiscard]] ComponentTypeSceneData<Type>&
		GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier, ComponentType<Type>& componentType)
		{
			if constexpr (GetSceneDataType<Type>() != CachedTypeSceneData::Invalid)
			{
				return GetCachedSceneData<Type>();
			}
			else
			{
				Assert(typeIdentifier.IsValid());
				Optional<ComponentTypeSceneDataInterface*> pComponentSceneData = m_componentTypeSceneData[typeIdentifier].Load();
				if (pComponentSceneData.IsValid())
				{
					return static_cast<ComponentTypeSceneData<Type>&>(*pComponentSceneData);
				}

				Assert(m_componentTypeSceneData[typeIdentifier] == nullptr);

				ComponentTypeSceneData<Type>* pComponentTypeSceneData =
					new ComponentTypeSceneData<Type>{typeIdentifier, componentType, m_manager, *this};
				ComponentTypeSceneDataInterface* pExpected{nullptr};
				if (LIKELY(m_componentTypeSceneData[typeIdentifier].CompareExchangeStrong(pExpected, pComponentTypeSceneData)))
				{
					return *pComponentTypeSceneData;
				}
				else
				{
					delete pComponentTypeSceneData;
				}
				Assert(pExpected != nullptr);
				return static_cast<ComponentTypeSceneData<Type>&>(*pExpected);
			}
		}

		template<typename Type>
		[[nodiscard]] Optional<ComponentTypeSceneData<Type>*> GetOrCreateComponentTypeData(ComponentType<Type>& componentType)
		{
			const ComponentTypeIdentifier typeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>());
			if (LIKELY(typeIdentifier.IsValid()))
			{
				return GetOrCreateComponentTypeData<Type>(typeIdentifier, componentType);
			}
			return Invalid;
		}

		[[nodiscard]] Optional<ComponentTypeSceneDataInterface*> GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier);
		template<typename Type>
		[[nodiscard]] ComponentTypeSceneData<Type>& GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier);
		template<typename Type>
		[[nodiscard]] Optional<ComponentTypeSceneData<Type>*> GetOrCreateComponentTypeData();
		Optional<ComponentTypeSceneDataInterface*> CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier);
		template<typename Type>
		ComponentTypeSceneData<Type>& CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier);
		template<typename Type>
		Optional<ComponentTypeSceneData<Type>*> CreateComponentTypeData();

		ComponentIdentifier AcquireNewComponentIdentifier()
		{
			return m_componentIdentifiers.AcquireIdentifier();
		}

		[[nodiscard]] ComponentIdentifier::IndexType GetMaximumUsedElementCount() const
		{
			return m_componentIdentifiers.GetMaximumUsedElementCount();
		}

		[[nodiscard]] Threading::IntermediateStage& GetPhysicsSimulationStartStage()
		{
			return m_physicsSimulationStartStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetPhysicsStepStartStage()
		{
			return m_physicsStepStartStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetPhysicsStepFinishedStage()
		{
			return m_physicsStepFinishedStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetPhysicsSimulationFinishedStage()
		{
			return m_physicsSimulationFinishedStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetDynamicRenderUpdatesStartStage()
		{
			return m_dynamicUpdatesStartStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetDynamicRenderUpdatesFinishedStage()
		{
			return m_dynamicRenderUpdatesFinishedStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetDynamicLateUpdatesFinishedStage()
		{
			return m_dynamicLateUpdatesFinishedStage;
		}

		using DataComponentsAtomicMask = Threading::AtomicIdentifierMask<ComponentTypeIdentifier>;
		using DataComponentsBitIndexType = DataComponentsAtomicMask::BitIndexType;
		using DataComponentIterator = DataComponentsAtomicMask::SetBitsIterator;
		struct DataComponents
		{
			~DataComponents()
			{
				if (DataComponentsAtomicMask* pMask = m_pDataComponentsMask)
				{
					delete pMask;
				}
			}

			[[nodiscard]] DataComponentsAtomicMask& TryGet()
			{
				DataComponentsAtomicMask* pMask = m_pDataComponentsMask;
				if (pMask != nullptr)
				{
					return *pMask;
				}

				DataComponentsAtomicMask* pNewMask = new DataComponentsAtomicMask{};
				if (LIKELY(m_pDataComponentsMask.CompareExchangeStrong(pMask, pNewMask)))
				{
					return *pNewMask;
				}
				else
				{
					delete pNewMask;
					return *pMask;
				}
			}
			[[nodiscard]] Optional<DataComponentsAtomicMask*> Find() const
			{
				return m_pDataComponentsMask.Load();
			}

			bool Remove()
			{
				DataComponentsAtomicMask* pMask = m_pDataComponentsMask;
				if (pMask != nullptr)
				{
					if (LIKELY(m_pDataComponentsMask.CompareExchangeStrong(pMask, nullptr)))
					{
						delete pMask;
						return true;
					}
				}
				return false;
			}
		private:
			Threading::Atomic<DataComponentsAtomicMask*> m_pDataComponentsMask{nullptr};
		};

		//! Checks whether the specified component has any data components
		[[nodiscard]] bool HasDataComponents(const ComponentIdentifier componentIdentifier) const
		{
			const DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			return pMask.IsValid() && pMask->AreAnySet();
		}
		//! Checks whether the specified component has a data component of a specific type
		[[nodiscard]] bool
		HasDataComponentOfType(const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier) const
		{
			const DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			return pMask.IsValid() && pMask->IsSet(dataComponentTypeIdentifier);
		}
		//! Gets the number of data components on a given component
		[[nodiscard]] DataComponentsBitIndexType GetDataComponentCount(const ComponentIdentifier componentIdentifier) const
		{
			const DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			return pMask.IsValid() && pMask->GetNumberOfSetBits();
		}
		//! Gets a mask indicating all data components on the specified component
		[[nodiscard]] ComponentTypeMask GetDataComponentMask(const ComponentIdentifier componentIdentifier) const
		{
			const DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			if (const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find())
			{
				return *pMask;
			}
			else
			{
				return {};
			}
		}

		//! Called to reserve the creation of a data component
		//! If this returns false then the component is either still being created, or was created once before
		[[nodiscard]] bool
		ReserveDataComponent(const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			DataComponents& __restrict componentInfo = m_reservedDataComponents[componentIdentifier];
			DataComponentsAtomicMask& __restrict mask = componentInfo.TryGet();
			const bool wasReserved = mask.Set(dataComponentTypeIdentifier);
			Assert(!wasReserved || !HasDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier));
			return wasReserved;
		}
		//! Checks whether the specified component has reserved data component of a specific type
		//! If true, then the data component either exists or is being created
		[[nodiscard]] bool HasReservedDataComponentOfType(
			const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier
		) const
		{
			const DataComponents& __restrict componentInfo = m_reservedDataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			return pMask.IsValid() && pMask->IsSet(dataComponentTypeIdentifier);
		}
		//! Called after a data component finishes constructing
		[[nodiscard]] bool
		OnDataComponentCreated(const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			Assert(HasReservedDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier));
			DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			DataComponentsAtomicMask& __restrict mask = componentInfo.TryGet();
			return mask.Set(dataComponentTypeIdentifier);
		}

		[[nodiscard]] ComponentTypeMask StartBatchComponentRemoval(const ComponentIdentifier componentIdentifier)
		{
			DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			if (const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find())
			{
				const ComponentTypeMask dataComponentsMask = pMask->FetchClear();
				if constexpr (ENABLE_ASSERTS)
				{
					const Optional<DataComponentsAtomicMask*> pReservedMask = m_reservedDataComponents[componentIdentifier].Find();
					if (Ensure(pReservedMask.IsValid()))
					{
						[[maybe_unused]] const ComponentTypeMask reservedMask = *pReservedMask;
						Assert(reservedMask.AreAllSet(dataComponentsMask));
					}
				}
				return dataComponentsMask;
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] bool
		OnDataComponentRemoved(const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			if (Ensure(pMask.IsValid()))
			{
				if (pMask->Clear(dataComponentTypeIdentifier))
				{
					Assert(HasReservedDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier));
					return true;
				}
			}
			return false;
		}
		[[nodiscard]] bool
		OnDataComponentDestroyed(const ComponentIdentifier componentIdentifier, const ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			Assert(HasReservedDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier));
			DataComponents& __restrict reservedComponentInfo = m_reservedDataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pReservedMask = reservedComponentInfo.Find();
			if (Ensure(pReservedMask.IsValid()))
			{
				return pReservedMask->Clear(dataComponentTypeIdentifier);
			}
			return false;
		}

		[[nodiscard]] void OnComponentRemoved(const ComponentIdentifier componentIdentifier)
		{
			// Clear the flag indicating this component was reserved
			DataComponents& __restrict reservedComponentInfo = m_reservedDataComponents[componentIdentifier];
			if (reservedComponentInfo.Remove())
			{
				// The reserved component existed, remove the created flag
				DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
				componentInfo.Remove();
			}

			m_componentIdentifiers.ReturnIdentifier(componentIdentifier);
		}

		[[nodiscard]] DataComponentIterator GetDataComponentIterator(const ComponentIdentifier componentIdentifier) const
		{
			const DataComponents& __restrict componentInfo = m_dataComponents[componentIdentifier];
			const Optional<DataComponentsAtomicMask*> pMask = componentInfo.Find();
			return pMask.IsValid() ? pMask->GetSetBitsIterator() : DataComponentsAtomicMask::SetBitsIterator{};
		}

		[[nodiscard]] ComponentMask GetAllDataComponentsOfType(const Entity::ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			const FixedIdentifierArrayView<DataComponents, ComponentIdentifier> componentInfos = m_dataComponents.GetView();

			ComponentMask instances;
			for (const SceneRegistry::DataComponents& componentInfo : componentInfos)
			{
				if (const Optional<const SceneRegistry::DataComponentsAtomicMask*> pDataComponentsMask = componentInfo.Find())
				{
					if (pDataComponentsMask->IsSet(dataComponentTypeIdentifier))
					{
						const Entity::ComponentIdentifier ownerComponentIdentifier =
							Entity::ComponentIdentifier::MakeFromValidIndex(componentInfos.GetIteratorIndex(&componentInfo));
						instances.Set(ownerComponentIdentifier);
					}
				}
			}

			return instances;
		}

		[[nodiscard]] ComponentMask GetAllComponentsOwningDataComponentOfType(const Entity::ComponentTypeIdentifier dataComponentTypeIdentifier)
		{
			ComponentMask instances;
			const FixedIdentifierArrayView<DataComponents, ComponentIdentifier> componentInfos = m_dataComponents.GetView();
			for (const SceneRegistry::DataComponents& componentInfo : componentInfos)
			{
				if (const Optional<const SceneRegistry::DataComponentsAtomicMask*> pDataComponentsMask = componentInfo.Find())
				{
					if (pDataComponentsMask->IsSet(dataComponentTypeIdentifier))
					{
						const Entity::ComponentIdentifier ownerComponentIdentifier =
							Entity::ComponentIdentifier::MakeFromValidIndex(componentInfos.GetIteratorIndex(&componentInfo));
						instances.Set(ownerComponentIdentifier);
					}
				}
			}
			return instances;
		}

		void DestroyComponentTypes();
		void DestroyAllComponentInstances();

		void EnableSceneFrameGraphModification()
		{
			m_flags |= Flags::CanModifySceneFrameGraph;
		}
		void DisableSceneFrameGraphModification()
		{
			m_flags.Clear(Flags::CanModifySceneFrameGraph);
		}
	private:
		template<typename Type>
		[[nodiscard]] PURE_NOSTATICS static constexpr CachedTypeSceneData GetSceneDataType()
		{
			if constexpr (TypeTraits::IsSame<Type, Data::TypeIndex>)
			{
				return CachedTypeSceneData::TypeIndex;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::Parent>)
			{
				return CachedTypeSceneData::Parent;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::InstanceGuid>)
			{
				return CachedTypeSceneData::InstanceGuid;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::Tags>)
			{
				return CachedTypeSceneData::Tags;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::Flags>)
			{
				return CachedTypeSceneData::Flags;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::PreferredIndex>)
			{
				return CachedTypeSceneData::PreferredIndex;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::StageMask>)
			{
				return CachedTypeSceneData::RenderItemStageMask;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::Identifier>)
			{
				return CachedTypeSceneData::RenderItemIdentifier;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::TransformChangeTracker>)
			{
				return CachedTypeSceneData::RenderItemTransformChangeTracker;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::StaticMeshIdentifier>)
			{
				return CachedTypeSceneData::StaticMeshIdentifier;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::MaterialInstanceIdentifier>)
			{
				return CachedTypeSceneData::MaterialInstanceIdentifier;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::RenderItem::VisibilityListener>)
			{
				return CachedTypeSceneData::VisibilityListener;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::WorldTransform>)
			{
				return CachedTypeSceneData::WorldTransform;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::LocalTransform3D>)
			{
				return CachedTypeSceneData::LocalTransform3D;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::BoundingBox>)
			{
				return CachedTypeSceneData::BoundingBox;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::OctreeNode>)
			{
				return CachedTypeSceneData::OctreeNode;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::WorldTransform2D>)
			{
				return CachedTypeSceneData::WorldTransform2D;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::LocalTransform2D>)
			{
				return CachedTypeSceneData::LocalTransform2D;
			}
			else if constexpr (TypeTraits::IsSame<Type, Data::QuadtreeNode>)
			{
				return CachedTypeSceneData::QuadtreeNode;
			}
			else if constexpr (TypeTraits::IsSame<Type, Context::Data::Component>)
			{
				return CachedTypeSceneData::Context;
			}
			else if constexpr (TypeTraits::IsSame<Type, Context::Data::Reference>)
			{
				return CachedTypeSceneData::ContextReference;
			}
			else
			{
				return CachedTypeSceneData::Invalid;
			}
		}
	private:
		Manager& m_manager;
		TIdentifierArray<Threading::Atomic<ComponentTypeSceneDataInterface*>, ComponentTypeIdentifier> m_componentTypeSceneData{Memory::Zeroed};
		TSaltedIdentifierStorage<ComponentIdentifier> m_componentIdentifiers;
		//! Bitsets indicating data components that have been reserved for creation
		//! If this is set but m_dataComponents isn't, then the data component is still being constructed
		TIdentifierArray<DataComponents, Entity::ComponentIdentifier> m_reservedDataComponents;
		//! Bitsets indicating data components that have been created
		TIdentifierArray<DataComponents, Entity::ComponentIdentifier> m_dataComponents;
	protected:
		EnumFlags<Flags> m_flags;

		Optional<Threading::StageBase*> m_pPhysicsStage;
		Threading::IntermediateStage m_physicsSimulationStartStage{"Physics Simulation Start"};
		Threading::IntermediateStage m_physicsStepStartStage{"Physics Step Start"};
		Threading::IntermediateStage m_physicsStepFinishedStage{"Physics Step Finished"};
		Threading::IntermediateStage m_physicsSimulationFinishedStage{"Physics Simulation Finished"};
		Threading::IntermediateStage m_dynamicUpdatesStartStage{"Dynamic Updates Start"};
		Threading::IntermediateStage m_dynamicRenderUpdatesFinishedStage{"Dynamic Render Updates Finished"};
		Threading::IntermediateStage m_dynamicLateUpdatesFinishedStage{"Dynamic Late Updates Finished"};

		Array<Optional<ComponentTypeSceneDataInterface*>, (uint8)CachedTypeSceneData::Count, CachedTypeSceneData> m_cachedTypeSceneData;
	};

	ENUM_FLAG_OPERATORS(SceneRegistry::Flags);
}
