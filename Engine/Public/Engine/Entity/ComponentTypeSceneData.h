#pragma once

#include "ComponentTypeSceneDataInterface.h"

#include <Common/Memory/GetNumericSize.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Prefetch.h>
#include <Common/Memory/CheckedCast.h>
#include <Common/TypeTraits/EnableIf.h>
#include <Common/TypeTraits/HasMemberFunction.h>
#include <Common/TypeTraits/IsAbstract.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Time/FrameTime.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Platform/NoUniqueAddress.h>
#include <Common/Threading/Jobs/Job.h>

#include <Engine/Entity/Manager.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/HierarchyComponentBase.h>
#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/Data/TypeIndex.h>
#include <Engine/Entity/DataComponentOwner.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Threading/JobRunnerThread.h>

#if STAGE_DEPENDENCY_PROFILING
#include <Common/Memory/Containers/String.h>
#endif

namespace ngine
{
	struct FrameTime;
}

namespace ngine::Entity
{
	namespace Internal
	{
		struct DummyComponentStage
		{
#if STAGE_DEPENDENCY_PROFILING
			DummyComponentStage(SceneRegistry&, const String&&)
#else
			DummyComponentStage(SceneRegistry&)
#endif
			{
			}
		};
	}

	struct ComponentStage : public Threading::Job
	{
#if STAGE_DEPENDENCY_PROFILING
		ComponentStage(String&& name)
			: Threading::Job(Threading::JobPriority::ComponentUpdates)
			, m_name(Forward<String>(name))
		{
		}

		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return m_name;
		}
#else
		ComponentStage()
			: Threading::Job(Threading::JobPriority::ComponentUpdates)
		{
		}
#endif

		void AddSubsequentStage(ComponentStage& other, SceneRegistry& sceneRegistry);
	protected:
#if STAGE_DEPENDENCY_PROFILING
		String m_name;
#endif
	};

	namespace Internal
	{
		struct DataComponentSparseIdentifierStorage
		{
			void OnInstanceCreated(const Entity::ComponentIdentifier identifier)
			{
				m_maximumUsedElementCount.AssignMax(identifier.GetIndex());
			}

			void OnInstanceRemoved(const Entity::ComponentIdentifier identifier)
			{
				typename Entity::ComponentIdentifier::IndexType maximumUsedCount = m_maximumUsedElementCount;
				if (identifier.GetIndex() == maximumUsedCount)
				{
					while (!m_maximumUsedElementCount.CompareExchangeWeak(maximumUsedCount, maximumUsedCount - 1))
					{
						if (identifier.GetIndex() != maximumUsedCount)
						{
							break;
						}
					}
				}
			}

			void Reset()
			{
				m_maximumUsedElementCount = 0;
			}

			template<typename ElementType>
			[[nodiscard]] IdentifierArrayView<ElementType, Entity::ComponentIdentifier>
			GetValidElementView(IdentifierArrayView<ElementType, Entity::ComponentIdentifier> view) const
			{
				return view.GetSubViewUpTo(m_maximumUsedElementCount);
			}

			template<typename ElementType>
			[[nodiscard]] IdentifierArrayView<ElementType, Entity::ComponentIdentifier>
			GetValidElementView(FixedIdentifierArrayView<ElementType, Entity::ComponentIdentifier> view) const
			{
				return view.GetSubViewUpTo(m_maximumUsedElementCount);
			}
		protected:
			[[nodiscard]] typename Entity::ComponentIdentifier::IndexType GetMaximumUsedElementCount() const
			{
				return m_maximumUsedElementCount;
			}
		protected:
			typename Threading::Atomic<typename Entity::ComponentIdentifier::IndexType> m_maximumUsedElementCount = 0;
		};
	}

	template<typename Type>
	struct ComponentTypeSceneData final : public ComponentTypeSceneDataInterface
	{
		using InstanceIdentifier = typename Type::InstanceIdentifier;
		using ComponentType = ComponentType<Type>;
	protected:
		using StoredType = Type;

		inline static constexpr bool IsDataComponent = TypeTraits::IsBaseOf<Data::Component, Type>;
		inline static constexpr bool IsDataComponentOwner = TypeTraits::IsBaseOf<DataComponentOwner, Type>;
		inline static constexpr bool HasUniqueInstanceIdentifier = !IsDataComponent;

		using DenseIdentifier = InstanceIdentifier;
		using SparseIdentifier = TypeTraits::Select<IsDataComponent, Entity::ComponentIdentifier, InstanceIdentifier>;

		template<typename Type_, typename = void>
		struct GetSparseIdentifierStorageType
		{
			using ResultType = TSaltedIdentifierStorage<InstanceIdentifier>;
		};
		template<typename Type_>
		struct GetSparseIdentifierStorageType<Type_, EnableIf<TypeTraits::IsBaseOf<Data::Component, Type_>>>
		{
			using ResultType = Internal::DataComponentSparseIdentifierStorage;
		};

		using SparseIdentifierStorage = TIdentifierArray<typename DenseIdentifier::IndexType, SparseIdentifier>;
		using DenseStorageType = TIdentifierArray<StoredType, DenseIdentifier>;
		using FixedDenseStorageType = Memory::FixedAllocator<StoredType, DenseIdentifier::MaximumCount>;
		using SparseIdentifierStorageType = typename GetSparseIdentifierStorageType<Type>::ResultType;
		using DenseIdentifierStorageType = TSaltedIdentifierStorage<DenseIdentifier>;

		using ValidElementIterator = typename DenseIdentifierStorageType::template ValidElementIterator<typename DenseStorageType::View>;
	public:
		HasTypeMemberFunction(Type, OnCreated, void);
		HasTypeMemberFunctionNamed(HasDataComponentOnCreated, Type, OnCreated, void, typename Type::ParentType&);
		HasTypeMemberFunction(Type, OnConstructed, void);
		HasTypeMemberFunction(Type, Destroy, bool);
		HasTypeMemberFunction(Type, OnDestroying, void);
		HasTypeMemberFunctionNamed(HasDataComponentOnDestroying, Type, OnDestroying, void, typename Type::ParentType&);
		HasTypeMemberFunction(Type, OnDisable, void);
		HasTypeMemberFunction(Type, OnEnable, void);
		HasTypeMemberFunction(Type, OnDetachedFromTree, void, Optional<typename Type::ParentType*>);
		HasTypeMemberFunction(Type, OnAttachedToTree, void, Optional<typename Type::ParentType*>);
		HasTypeMemberFunctionNamed(HasDataComponentOnDisable, Type, OnDisable, void, typename Type::ParentType&);
		HasTypeMemberFunctionNamed(HasDataComponentOnEnable, Type, OnEnable, void, typename Type::ParentType&);
		HasTypeMemberFunctionNamed(HasDataComponentOnSimulationPaused, Type, OnSimulationPaused, void, typename Type::ParentType&);
		HasTypeMemberFunctionNamed(HasDataComponentOnSimulationResumed, Type, OnSimulationResumed, void, typename Type::ParentType&);
		HasTypeMemberFunction(Type, OnSimulationPaused, void);
		HasTypeMemberFunction(Type, OnSimulationResumed, void);
		HasTypeMemberFunction(Type, Update, void);
		HasTypeMemberFunction(Type, BeforePhysicsUpdate, void);
		HasTypeMemberFunction(Type, FixedPhysicsUpdate, void);
		HasTypeMemberFunction(Type, AfterPhysicsUpdate, void);
		HasTypeMemberFunction(Type, GetInstanceGuid, Guid);
		HasTypeMemberFunction(Type, CanClone, bool);

		inline static constexpr bool IsAbstract = TypeTraits::IsAbstract<Type> || Reflection::GetType<Type>().IsAbstract();
		inline static constexpr bool ShouldEnableUpdate = HasUpdate && !IsAbstract;
		inline static constexpr bool ShouldEnableBeforePhysicsUpdate = HasBeforePhysicsUpdate && !IsAbstract;
		inline static constexpr bool ShouldEnableFixedPhysicsUpdate = HasFixedPhysicsUpdate && !IsAbstract;
		inline static constexpr bool ShouldEnableAfterPhysicsUpdate = HasAfterPhysicsUpdate && !IsAbstract;

		ComponentTypeSceneData(
			const ComponentTypeIdentifier identifier, ComponentType& componentType, Manager& entityManager, SceneRegistry& sceneRegistry
		)
			: ComponentTypeSceneDataInterface(identifier, componentType, entityManager, sceneRegistry)
#if STAGE_DEPENDENCY_PROFILING
			, m_mainUpdateComponents(sceneRegistry, String::Merge(String{componentType.GetTypeInterface().GetName()}, ConstStringView{" Update"}))
			, m_beforePhysicsUpdateComponents(
					sceneRegistry, String::Merge(String{componentType.GetTypeInterface().GetName()}, ConstStringView{" Before Physics Update"})
				)
			, m_fixedPhysicsUpdateComponents(
					sceneRegistry, String::Merge(String{componentType.GetTypeInterface().GetName()}, ConstStringView{" Fixed Physics Update"})
				)
			, m_afterPhysicsUpdateComponents(
					sceneRegistry, String::Merge(String{componentType.GetTypeInterface().GetName()}, ConstStringView{" After Physics Update"})
				)
#else
			, m_mainUpdateComponents(sceneRegistry)
			, m_beforePhysicsUpdateComponents(sceneRegistry)
			, m_fixedPhysicsUpdateComponents(sceneRegistry)
			, m_afterPhysicsUpdateComponents(sceneRegistry)
#endif
		{
		}
		virtual ~ComponentTypeSceneData()
		{
			if constexpr (ShouldEnableUpdate)
			{
				m_mainUpdateComponents.DeregisterStage(m_sceneRegistry);
			}
			if constexpr (ShouldEnableBeforePhysicsUpdate)
			{
				m_beforePhysicsUpdateComponents.DeregisterStage(m_sceneRegistry);
			}
			if constexpr (ShouldEnableFixedPhysicsUpdate)
			{
				m_fixedPhysicsUpdateComponents.DeregisterStage(m_sceneRegistry);
			}
			if constexpr (ShouldEnableAfterPhysicsUpdate)
			{
				m_afterPhysicsUpdateComponents.DeregisterStage(m_sceneRegistry);
			}

			DestroyAllInstances();
		}

		[[nodiscard]] ComponentType& GetType() const
		{
			return static_cast<ComponentType&>(m_componentType);
		}

		template<typename _Type = Type, typename... Args>
		inline EnableIf<!TypeTraits::IsBaseOf<Data::Component, _Type>, Optional<Type*>> CreateInstance(Args&&... args)
		{
			static_assert(!Reflection::GetType<_Type>().IsAbstract(), "Can't instantiate abstract type");

			Optional<Type*> pComponent = CreateInstanceManualOnCreated(Forward<Args>(args)...);

			if constexpr (HasOnCreated)
			{
				if (LIKELY(pComponent.IsValid()))
				{
					GetType().OnComponentCreated(*pComponent, Invalid, m_sceneRegistry);
				}
			}
			return pComponent;
		}

		template<typename _Type = Type, typename... Args>
		inline EnableIf<TypeTraits::IsBaseOf<Data::Component, _Type>, Optional<Type*>>
		CreateInstance(const SparseIdentifier sparseIdentifier, [[maybe_unused]] Optional<typename Type::ParentType*> pParent, Args&&... args)
		{
			static_assert(!Reflection::GetType<_Type>().IsAbstract(), "Can't instantiate abstract type");

			const ComponentTypeIdentifier dataComponentIdentifier = GetIdentifier();
			if (m_sceneRegistry.ReserveDataComponent(sparseIdentifier, dataComponentIdentifier))
			{
				Optional<Type*> pComponent = CreateInstanceManualOnCreated(sparseIdentifier, Forward<Args>(args)...);
				if (LIKELY(pComponent.IsValid()))
				{
					GetType().OnComponentCreated(*pComponent, pParent, m_sceneRegistry);
				}
				else
				{
					[[maybe_unused]] const bool wasRemoved = m_sceneRegistry.OnDataComponentDestroyed(sparseIdentifier, dataComponentIdentifier);
					Assert(wasRemoved);
				}

				return pComponent;
			}
			else
			{
				return Invalid;
			}
		}

		template<typename _Type = Type>
		EnableIf<!TypeTraits::IsBaseOf<Data::Component, _Type>, void>
		RemoveInstanceWithSparseIdentifier(const SparseIdentifier sparseComponentIdentifier)
		{
			const DenseIdentifier denseIdentifier =
				m_denseIdentifierStorage.GetActiveIdentifier(DenseIdentifier::MakeFromIndex(m_sparseStorage[sparseComponentIdentifier]));
			RemoveInstanceWithDenseIdentifier(denseIdentifier);
		}

		template<typename _Type = Type>
		EnableIf<!TypeTraits::IsBaseOf<Data::Component, _Type>, void> RemoveInstancesWithSparseIdentifier(
			const IdentifierMask<SparseIdentifier>& sparseComponentIdentifiers, const typename SparseIdentifier::IndexType maximumUsedElementCount
		)
		{
			for (const typename SparseIdentifier::IndexType sparseIdentifierIndex :
			     sparseComponentIdentifiers.GetSetBitsIterator(0, maximumUsedElementCount))
			{
				const SparseIdentifier sparseIdentifier = SparseIdentifier::MakeFromValidIndex(sparseIdentifierIndex);

				// TODO: Batch this better
				const DenseIdentifier denseIdentifier =
					m_denseIdentifierStorage.GetActiveIdentifier(DenseIdentifier::MakeFromIndex(m_sparseStorage[sparseIdentifier]));
				RemoveInstanceWithDenseIdentifier(denseIdentifier);
			}
		}

		[[nodiscard]] bool RemoveInstanceGuidInternal(Guid instanceGuid)
		{
			Threading::UniqueLock lock(m_instanceIdentifierLookup.m_mutex);
			const auto it = m_instanceIdentifierLookup.m_map.Find(instanceGuid);
			Assert(it != m_instanceIdentifierLookup.m_map.end());
			if (LIKELY(it != m_instanceIdentifierLookup.m_map.end()))
			{
				m_instanceIdentifierLookup.m_map.Remove(it);
				return true;
			}
			return false;
		}

		template<typename _Type = Type>
		EnableIf<!TypeTraits::IsBaseOf<Data::Component, _Type>, void>
		RemoveInstanceWithDenseIdentifier(const DenseIdentifier denseComponentIdentifier)
		{
			if constexpr (HasGetInstanceGuid)
			{
				StoredType& storedValue = m_denseComponentStorage.GetView()[denseComponentIdentifier.GetFirstValidIndex()];
				[[maybe_unused]] const bool wasRemoved = RemoveInstanceGuidInternal(storedValue.GetInstanceGuid());
			}

			RemoveInstanceWithDenseIdentifierInternal(denseComponentIdentifier);
		}

		void RemoveInstanceWithDenseIdentifierInternal(const DenseIdentifier denseComponentIdentifier)
		{
			StoredType& storedValue = m_denseComponentStorage.GetView()[denseComponentIdentifier.GetFirstValidIndex()];
			storedValue.~StoredType();

			if constexpr (ShouldEnableUpdate)
			{
				Assert(!m_mainUpdateComponents.IsRegistered(denseComponentIdentifier));
			}
			if constexpr (ShouldEnableBeforePhysicsUpdate)
			{
				Assert(!m_beforePhysicsUpdateComponents.IsRegistered(denseComponentIdentifier));
			}
			if constexpr (ShouldEnableFixedPhysicsUpdate)
			{
				Assert(!m_fixedPhysicsUpdateComponents.IsRegistered(denseComponentIdentifier));
			}
			if constexpr (ShouldEnableAfterPhysicsUpdate)
			{
				Assert(!m_afterPhysicsUpdateComponents.IsRegistered(denseComponentIdentifier));
			}

			const typename SparseIdentifier::IndexType sparseComponentIndex = m_sparseIdentifierLookupTable[denseComponentIdentifier];
			const SparseIdentifier sparseIdentifier = SparseIdentifier::MakeFromValidIndex(sparseComponentIndex);
			m_sparseStorage[sparseIdentifier] = 0;
			m_sparseIdentifierLookupTable[denseComponentIdentifier] = {};

			if constexpr (HasUniqueInstanceIdentifier)
			{
				m_sparseIdentifierStorage.ReturnIdentifier(sparseIdentifier);
			}
			else
			{
				m_sparseIdentifierStorage.OnInstanceRemoved(sparseIdentifier);
			}
			m_denseIdentifierStorage.ReturnIdentifier(denseComponentIdentifier);
		}

		virtual void OnBeforeRemoveInstance(Component& component, [[maybe_unused]] const Optional<DataComponentOwner*> parent) override final
		{
			[[maybe_unused]] Type& componentWithFinalType = static_cast<Type&>(component);

			if constexpr (IsDataComponent && HasDataComponentOnDestroying)
			{
				componentWithFinalType.OnDestroying(static_cast<typename Type::ParentType&>(*parent));
			}
			else if constexpr (HasOnDestroying)
			{
				componentWithFinalType.OnDestroying();
			}
		}

		virtual void RemoveInstance(Component& component, [[maybe_unused]] const Optional<DataComponentOwner*> parent) override final
		{
			Type& componentWithFinalType = static_cast<Type&>(component);

			[[maybe_unused]] Guid instanceGuid;
			if constexpr (HasGetInstanceGuid)
			{
				instanceGuid = componentWithFinalType.GetInstanceGuid();
			}

			[[maybe_unused]] SparseIdentifier sparseIdentifier;
			if constexpr (IsDataComponent)
			{
				sparseIdentifier = GetSparseComponentIdentifier(componentWithFinalType);
				Assert(!m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, m_identifier));
				Assert(m_sceneRegistry.HasReservedDataComponentOfType(sparseIdentifier, m_identifier));
			}

			if constexpr (IsDataComponentOwner)
			{
				DataComponentOwner& dataComponentOwner = static_cast<DataComponentOwner&>(component);
				const ComponentIdentifier identifier = dataComponentOwner.GetIdentifier();

				for (const ComponentTypeIdentifier::IndexType typeIdentifierIndex : m_sceneRegistry.GetDataComponentIterator(identifier))
				{
					const ComponentTypeIdentifier dataComponentTypeIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(typeIdentifierIndex);
					ComponentTypeSceneDataInterface& dataComponentTypeSceneData = *m_sceneRegistry.FindComponentTypeData(dataComponentTypeIdentifier);
					Component& dataComponent = dataComponentTypeSceneData.GetDataComponentUnsafe(identifier);
					dataComponentTypeSceneData.OnBeforeRemoveInstance(dataComponent, dataComponentOwner);
				}

				const ComponentTypeMask dataComponents = m_sceneRegistry.StartBatchComponentRemoval(identifier);
				for (const ComponentTypeIdentifier::IndexType typeIdentifierIndex : dataComponents.GetSetBitsReverseIterator())
				{
					const ComponentTypeIdentifier dataComponentTypeIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(typeIdentifierIndex);
					ComponentTypeSceneDataInterface& dataComponentTypeSceneData = *m_sceneRegistry.FindComponentTypeData(dataComponentTypeIdentifier);
					Component& dataComponent = dataComponentTypeSceneData.GetDataComponentUnsafe(identifier);
					dataComponentTypeSceneData.RemoveInstance(dataComponent, dataComponentOwner);
				}
			}

			StoredType& storedValue = reinterpret_cast<StoredType&>(componentWithFinalType);

			Assert(m_denseComponentStorage.GetView().IsWithinBounds(&storedValue));
			const typename DenseIdentifier::IndexType denseComponentIndex =
				static_cast<typename DenseIdentifier::IndexType>(&storedValue - m_denseComponentStorage.GetView().GetData());

			if constexpr (HasGetInstanceGuid)
			{
				[[maybe_unused]] const bool wasRemoved = RemoveInstanceGuidInternal(instanceGuid);
				Assert(wasRemoved, "Failed to remove instance guid lookup");
			}

			RemoveInstanceWithDenseIdentifierInternal(
				m_denseIdentifierStorage.GetActiveIdentifier(DenseIdentifier::MakeFromValidIndex(denseComponentIndex))
			);

			if constexpr (IsDataComponent)
			{
				[[maybe_unused]] const bool wasRemoved =
					m_sceneRegistry.OnDataComponentDestroyed(sparseIdentifier, m_componentType.GetIdentifier());
				Assert(wasRemoved);
			}
		}

		virtual void DisableInstance(Component& component, const Optional<DataComponentOwner*> parent) override final
		{
			if constexpr (IsDataComponentOwner)
			{
				DataComponentOwner& dataComponentOwner = static_cast<DataComponentOwner&>(component);
				if (m_sceneRegistry.HasDataComponents(dataComponentOwner.GetIdentifier()))
				{
					dataComponentOwner.IterateDataComponents(
						m_sceneRegistry,
						[pComponent = &static_cast<HierarchyComponentBase&>(component)](
							Entity::Data::Component& dataComponent,
							const Optional<const Entity::ComponentTypeInterface*>,
							ComponentTypeSceneDataInterface& typeSceneDataInterface
						)
						{
							typeSceneDataInterface.DisableInstance(dataComponent, pComponent);
							return Memory::CallbackResult::Continue;
						}
					);
				}
			}

			[[maybe_unused]] Type& componentWithFinalType = static_cast<Type&>(component);
			if constexpr (IsDataComponent && HasDataComponentOnDisable)
			{
				componentWithFinalType.OnDisable(static_cast<typename Type::ParentType&>(*parent));
			}
			else if constexpr (HasOnDisable)
			{
				componentWithFinalType.OnDisable();
			}
		}

		virtual void EnableInstance(Component& component, const Optional<DataComponentOwner*> parent) override final
		{
			if constexpr (IsDataComponentOwner)
			{
				DataComponentOwner& dataComponentOwner = static_cast<DataComponentOwner&>(component);
				if (m_sceneRegistry.HasDataComponents(dataComponentOwner.GetIdentifier()))
				{
					dataComponentOwner.IterateDataComponents(
						m_sceneRegistry,
						[pComponent = &static_cast<HierarchyComponentBase&>(component)](
							Entity::Data::Component& dataComponent,
							const Optional<const Entity::ComponentTypeInterface*>,
							ComponentTypeSceneDataInterface& typeSceneDataInterface
						)
						{
							typeSceneDataInterface.EnableInstance(dataComponent, pComponent);
							return Memory::CallbackResult::Continue;
						}
					);
				}
			}

			[[maybe_unused]] Type& componentWithFinalType = static_cast<Type&>(component);
			if constexpr (IsDataComponent && HasDataComponentOnEnable)
			{
				componentWithFinalType.OnEnable(static_cast<typename Type::ParentType&>(*parent));
			}
			else if constexpr (HasOnEnable)
			{
				componentWithFinalType.OnEnable();
			}
		}

		virtual void DetachInstanceFromTree(Component& component, const Optional<DataComponentOwner*> parent) override final
		{
			[[maybe_unused]] Type& componentWithFinalType = static_cast<Type&>(component);
			if constexpr (HasOnDetachedFromTree)
			{
				componentWithFinalType.OnDetachedFromTree(static_cast<typename Type::ParentType*>(parent.Get()));
			}
		}

		virtual void AttachInstanceToTree(Component& component, const Optional<DataComponentOwner*> parent) override final
		{
			[[maybe_unused]] Type& componentWithFinalType = static_cast<Type&>(component);
			if constexpr (HasOnAttachedToTree)
			{
				componentWithFinalType.OnAttachedToTree(static_cast<typename Type::ParentType*>(parent.Get()));
			}
		}

		virtual void DestroyAllInstances() override final
		{
			if constexpr (HasDestroy)
			{
				for (const typename DenseIdentifier::IndexType denseComponentIdentifierIndex :
				     m_sparseIdentifierStorage.GetValidElementView(m_sparseStorage.GetView()))
				{
					const DenseIdentifier denseComponentIdentifier = DenseIdentifier::MakeFromIndex(denseComponentIdentifierIndex);
					if (denseComponentIdentifier.IsValid())
					{
						StoredType& storedValue = m_denseComponentStorage.GetView()[denseComponentIdentifier.GetFirstValidIndex()];
						storedValue.Destroy();
					}
				}
			}
			else
			{
				for (const typename DenseIdentifier::IndexType denseComponentIdentifierIndex :
				     m_sparseIdentifierStorage.GetValidElementView(m_sparseStorage.GetView()))
				{
					const DenseIdentifier denseComponentIdentifier = DenseIdentifier::MakeFromIndex(denseComponentIdentifierIndex);
					if (denseComponentIdentifier.IsValid())
					{
						StoredType& storedValue = m_denseComponentStorage.GetView()[denseComponentIdentifier.GetFirstValidIndex()];
						storedValue.~StoredType();

						if constexpr (IsDataComponentOwner)
						{
							m_sceneRegistry.OnComponentRemoved(storedValue.DataComponentOwner::GetIdentifier());
						}
					}
				}

				m_sparseIdentifierStorage.Reset();
				m_denseIdentifierStorage.Reset();
				m_sparseStorage.GetView().ZeroInitialize();
				m_sparseIdentifierLookupTable.GetView().ZeroInitialize();

				if constexpr (HasGetInstanceGuid)
				{
					Threading::UniqueLock lock(m_instanceIdentifierLookup.m_mutex);
					m_instanceIdentifierLookup.m_map.Clear();
				}

				if constexpr (HasUpdate)
				{
					m_mainUpdateComponents.Clear();
				}

				if constexpr (HasBeforePhysicsUpdate)
				{
					m_beforePhysicsUpdateComponents.Clear();
				}

				if constexpr (HasFixedPhysicsUpdate)
				{
					m_fixedPhysicsUpdateComponents.Clear();
				}

				if constexpr (HasAfterPhysicsUpdate)
				{
					m_afterPhysicsUpdateComponents.Clear();
				}
			}
		}

		virtual void PauseInstanceSimulation(Component& component, const Optional<DataComponentOwner*> pParent) override final
		{
			if constexpr (HasDataComponentOnSimulationPaused)
			{
				static_cast<Type&>(component).OnSimulationPaused(static_cast<typename Type::ParentType&>(*pParent));
			}
			else if constexpr (HasOnSimulationPaused)
			{
				static_cast<Type&>(component).OnSimulationPaused();
			}
		}
		virtual void ResumeInstanceSimulation(Component& component, const Optional<DataComponentOwner*> pParent) override final
		{
			if constexpr (HasDataComponentOnSimulationPaused)
			{
				static_cast<Type&>(component).OnSimulationResumed(static_cast<typename Type::ParentType&>(*pParent));
			}
			else if constexpr (HasOnSimulationResumed)
			{
				static_cast<Type&>(component).OnSimulationResumed();
			}
		}

		static_assert(
			GenericComponentInstanceIdentifier::MaximumCount >= DenseIdentifier::MaximumCount,
			"Generic component instance type must be able to contain any component identifier!"
		);
		static_assert(
			GenericComponentInstanceIdentifier::MaximumIndexReuseCount >= DenseIdentifier::MaximumIndexReuseCount,
			"Generic component instance type must be able to contain any component identifier!"
		);

		[[nodiscard]] DenseIdentifier GetDenseComponentIdentifier(const Type& component) const
		{
			const StoredType& storedValue = reinterpret_cast<const StoredType&>(component);
			Assert(m_denseComponentStorage.GetView().IsWithinBounds(&storedValue));
			if (LIKELY(m_denseComponentStorage.GetView().IsWithinBounds(&storedValue)))
			{
				const typename DenseIdentifier::IndexType denseComponentIndex =
					static_cast<typename DenseIdentifier::IndexType>(&storedValue - m_denseComponentStorage.GetView().GetData());
				return m_denseIdentifierStorage.GetActiveIdentifier(DenseIdentifier::MakeFromValidIndex(denseComponentIndex));
			}
			return {};
		}

		[[nodiscard]] SparseIdentifier GetSparseComponentIdentifier(const DenseIdentifier denseComponentIdentifier) const
		{
			const typename SparseIdentifier::IndexType sparseComponentIndex = m_sparseIdentifierLookupTable[denseComponentIdentifier];
			return SparseIdentifier::MakeFromValidIndex(sparseComponentIndex);
		}
		[[nodiscard]] SparseIdentifier GetSparseComponentIdentifier(const GenericComponentInstanceIdentifier identifier) const
		{
			const DenseIdentifier denseIdentifier{
				Memory::CheckedCast<typename DenseIdentifier::IndexType>(identifier.GetIndex()),
				Memory::CheckedCast<typename DenseIdentifier::IndexReuseType>(identifier.GetIndexUseCount())
			};
			return GetSparseComponentIdentifier(denseIdentifier);
		}
		[[nodiscard]] SparseIdentifier GetSparseComponentIdentifier(const Type& component) const
		{
			return GetSparseComponentIdentifier(GetDenseComponentIdentifier(component));
		}

		[[nodiscard]] virtual GenericComponentInstanceIdentifier GetComponentInstanceIdentifier(const Component& component) const override final
		{
			const Type& componentWithFinalType = static_cast<const Type&>(component);
			const DenseIdentifier denseIdentifier = GetDenseComponentIdentifier(componentWithFinalType);
			return {denseIdentifier.GetIndex(), denseIdentifier.GetIndexUseCount()};
		}

		[[nodiscard]] virtual GenericComponentInstanceIdentifier FindComponentInstanceIdentifier([[maybe_unused]] const Guid instanceGuid
		) const override final
		{
			if constexpr (HasGetInstanceGuid)
			{
				Threading::SharedLock lock(m_instanceIdentifierLookup.m_mutex);
				auto it = m_instanceIdentifierLookup.m_map.Find(instanceGuid);
				if (it != m_instanceIdentifierLookup.m_map.end())
				{
					const DenseIdentifier denseIdentifier = it->second;
					return {denseIdentifier.GetIndex(), denseIdentifier.GetIndexUseCount()};
				}
			}
			return {};
		}

		[[nodiscard]] virtual GenericComponentInstanceIdentifier
		FindDataComponentInstanceIdentifier(const ComponentIdentifier componentIdentifier) const override final
		{
			if constexpr (IsDataComponent)
			{
				const typename DenseIdentifier::IndexType denseStorageIndex = m_sparseStorage[componentIdentifier];
				const DenseIdentifier denseIdentifier = DenseIdentifier::MakeFromIndex(denseStorageIndex);
				return GenericComponentInstanceIdentifier{
					Memory::CheckedCast<typename GenericComponentInstanceIdentifier::IndexType>(denseIdentifier.GetIndex()),
					Memory::CheckedCast<typename GenericComponentInstanceIdentifier::IndexReuseType>(denseIdentifier.GetIndexUseCount())
				};
			}
			else
			{
				ExpectUnreachable();
			}
		}

		[[nodiscard]] virtual Guid FindComponentInstanceGuid(const GenericComponentInstanceIdentifier identifier) const override final
		{
			if constexpr (HasGetInstanceGuid)
			{
				const DenseIdentifier denseIdentifier{
					Memory::CheckedCast<typename DenseIdentifier::IndexType>(identifier.GetIndex()),
					Memory::CheckedCast<typename DenseIdentifier::IndexReuseType>(identifier.GetIndexUseCount())
				};
				const StoredType& component = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];
				return component.GetInstanceGuid();
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] virtual Optional<Component*> FindComponent(const GenericComponentInstanceIdentifier identifier) override final
		{
			if (identifier.IsValid() && identifier.GetIndex() <= DenseIdentifier::MaximumCount && identifier.GetIndexUseCount() <= DenseIdentifier::MaximumIndexReuseCount)
			{
				const DenseIdentifier denseIdentifier{
					Memory::CheckedCast<typename DenseIdentifier::IndexType>(identifier.GetIndex()),
					Memory::CheckedCast<typename DenseIdentifier::IndexReuseType>(identifier.GetIndexUseCount())
				};
				StoredType& component = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];

				if constexpr (IsDataComponent)
				{
					const SparseIdentifier sparseIdentifier = GetSparseComponentIdentifier(denseIdentifier);
					return Optional<Component*>{component, m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, m_identifier)};
				}
				else
				{
					return Optional<Component*>{&component, m_denseIdentifierStorage.IsIdentifierActive(denseIdentifier)};
				}
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] virtual void* GetComponentAddress(const GenericComponentInstanceIdentifier identifier) override final
		{
			const DenseIdentifier denseIdentifier{
				Memory::CheckedCast<typename DenseIdentifier::IndexType>(identifier.GetIndex()),
				Memory::CheckedCast<typename DenseIdentifier::IndexReuseType>(identifier.GetIndexUseCount())
			};
			StoredType& component = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];

			if constexpr (IsDataComponent)
			{
				const SparseIdentifier sparseIdentifier = GetSparseComponentIdentifier(denseIdentifier);
				if (m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, m_identifier))
				{
					return &component;
				}
			}
			else
			{
				if (m_denseIdentifierStorage.IsIdentifierActive(denseIdentifier))
				{
					return &component;
				}
			}
			return nullptr;
		}

		virtual Optional<Component*> GetDataComponent(const ComponentIdentifier componentIdentifier) override final
		{
			if constexpr (IsDataComponent)
			{
				return GetComponentImplementation(componentIdentifier);
			}
			else
			{
				ExpectUnreachable();
			}
		}

		virtual Component& GetDataComponentUnsafe(const ComponentIdentifier componentIdentifier) override final
		{
			if constexpr (IsDataComponent)
			{
				const typename DenseIdentifier::IndexType denseStorageIndex = m_sparseStorage[componentIdentifier];
				const DenseIdentifier denseIdentifier = DenseIdentifier::MakeFromIndex(denseStorageIndex);
				return m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];
			}
			else
			{
				ExpectUnreachable();
			}
		}

		[[nodiscard]] bool IsComponentActive(const Type& component) const
		{
			const DenseIdentifier denseIdentifier = GetDenseComponentIdentifier(component);
			const SparseIdentifier sparseIdentifier = GetSparseComponentIdentifier(denseIdentifier);
			if constexpr (IsDataComponent)
			{
				return m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, m_identifier);
			}
			else
			{
				return m_denseIdentifierStorage.IsIdentifierActive(denseIdentifier) &
				       m_sparseIdentifierStorage.IsIdentifierActive(sparseIdentifier);
			}
		}

		virtual Optional<Entity::ComponentStage*> GetUpdateStage() override final
		{
			if constexpr (ShouldEnableUpdate)
			{
				return &m_mainUpdateComponents;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::ComponentStage*> GetBeforePhysicsUpdateStage() override final
		{
			if constexpr (ShouldEnableBeforePhysicsUpdate)
			{
				return &m_beforePhysicsUpdateComponents;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::ComponentStage*> GetFixedPhysicsUpdateStage() override final
		{
			if constexpr (ShouldEnableFixedPhysicsUpdate)
			{
				return &m_fixedPhysicsUpdateComponents;
			}
			else
			{
				return Invalid;
			}
		}

		virtual Optional<Entity::ComponentStage*> GetAfterPhysicsUpdateStage() override final
		{
			if constexpr (ShouldEnableAfterPhysicsUpdate)
			{
				return &m_afterPhysicsUpdateComponents;
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] Optional<Type*> GetComponentImplementation(const SparseIdentifier sparseIdentifier)
		{
			const typename DenseIdentifier::IndexType denseStorageIndex = m_sparseStorage[sparseIdentifier];
			const DenseIdentifier denseIdentifier = DenseIdentifier::MakeFromIndex(denseStorageIndex);
			if (denseIdentifier.IsValid())
			{
				StoredType& component = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];

				if constexpr (IsDataComponent)
				{
					return Optional<Type*>{component, m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, m_identifier)};
				}
				else
				{
					if constexpr (HasUniqueInstanceIdentifier)
					{
						if (!m_sparseIdentifierStorage.IsIdentifierActive(sparseIdentifier))
						{
							return Invalid;
						}
					}

					return Optional<Type*>{component, m_denseIdentifierStorage.IsIdentifierActive(denseIdentifier)};
				}
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] Type& GetComponentImplementationUnchecked(const SparseIdentifier sparseIdentifier)
		{
			const typename DenseIdentifier::IndexType denseStorageIndex = m_sparseStorage[sparseIdentifier];
			const DenseIdentifier denseIdentifier = DenseIdentifier::MakeFromIndex(denseStorageIndex);
			StoredType& component = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndexUnchecked()];
			return component;
		}

		template<bool CanUpdate = ShouldEnableUpdate, typename = EnableIf<CanUpdate>>
		bool EnableUpdate(Type& component)
		{
			return m_mainUpdateComponents.RegisterComponent(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableUpdate, typename = EnableIf<CanUpdate>>
		[[nodiscard]] bool IsUpdateEnabled(Type& component) const
		{
			return m_mainUpdateComponents.IsRegistered(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableUpdate, typename = EnableIf<CanUpdate>>
		bool DisableUpdate(Type& component)
		{
			return m_mainUpdateComponents.DeregisterComponent(GetDenseComponentIdentifier(component));
		}

		template<bool CanUpdate = ShouldEnableBeforePhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool EnableBeforePhysicsUpdate(Type& component)
		{
			return m_beforePhysicsUpdateComponents.RegisterComponent(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableBeforePhysicsUpdate, typename = EnableIf<CanUpdate>>
		[[nodiscard]] bool IsBeforePhysicsUpdateEnabled(Type& component) const
		{
			return m_beforePhysicsUpdateComponents.IsRegistered(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableBeforePhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool DisableBeforePhysicsUpdate(Type& component)
		{
			return m_beforePhysicsUpdateComponents.DeregisterComponent(GetDenseComponentIdentifier(component));
		}

		template<bool CanUpdate = ShouldEnableFixedPhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool EnableFixedPhysicsUpdate(Type& component)
		{
			return m_fixedPhysicsUpdateComponents.RegisterComponent(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableFixedPhysicsUpdate, typename = EnableIf<CanUpdate>>
		[[nodiscard]] bool IsFixedPhysicsUpdateEnabled(Type& component) const
		{
			return m_fixedPhysicsUpdateComponents.IsRegistered(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableFixedPhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool DisableFixedPhysicsUpdate(Type& component)
		{
			return m_fixedPhysicsUpdateComponents.DeregisterComponent(GetDenseComponentIdentifier(component));
		}

		template<bool CanUpdate = ShouldEnableAfterPhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool EnableAfterPhysicsUpdate(Type& component)
		{
			return m_afterPhysicsUpdateComponents.RegisterComponent(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableAfterPhysicsUpdate, typename = EnableIf<CanUpdate>>
		[[nodiscard]] bool IsAfterPhysicsUpdateEnabled(Type& component) const
		{
			return m_afterPhysicsUpdateComponents.IsRegistered(GetDenseComponentIdentifier(component));
		}
		template<bool CanUpdate = ShouldEnableAfterPhysicsUpdate, typename = EnableIf<CanUpdate>>
		bool DisableAfterPhysicsUpdate(Type& component)
		{
			return m_afterPhysicsUpdateComponents.DeregisterComponent(GetDenseComponentIdentifier(component));
		}

		[[nodiscard]] typename DenseStorageType::DynamicView GetAllocatedView() LIFETIME_BOUND
		{
			const typename DenseStorageType::View storageView = m_denseComponentStorage.GetView();
			return m_denseIdentifierStorage.GetValidElementView(storageView);
		}
		[[nodiscard]] typename DenseStorageType::ConstDynamicView GetAllocatedView() const LIFETIME_BOUND
		{
			const typename DenseStorageType::ConstView storageView = m_denseComponentStorage.GetView();
			return m_denseIdentifierStorage.GetValidElementView(storageView);
		}

		virtual void OnInstanceGuidChanged(const Guid previousGuid, const Guid newGuid) override final
		{
			if constexpr (HasGetInstanceGuid)
			{
				Threading::UniqueLock lock(m_instanceIdentifierLookup.m_mutex);
				auto it = m_instanceIdentifierLookup.m_map.Find(previousGuid);
				Assert(it != m_instanceIdentifierLookup.m_map.end());
				if (LIKELY(it != m_instanceIdentifierLookup.m_map.end()))
				{
					const DenseIdentifier denseIdentifier = it->second;
					m_instanceIdentifierLookup.m_map.Remove(it);
					Assert(!m_instanceIdentifierLookup.m_map.Contains(newGuid), "Instance guids must not be duplicated!");
					m_instanceIdentifierLookup.m_map.EmplaceOrAssign(Guid(newGuid), DenseIdentifier(denseIdentifier));
				}
			}
		}
	protected:
		template<typename _Type = Type, typename... Args>
		inline EnableIf<!TypeTraits::IsBaseOf<Data::Component, _Type> && !Reflection::GetType<_Type>().IsAbstract(), Optional<Type*>>
		CreateInstanceManualOnCreated(Args&&... args)
		{
			const SparseIdentifier sparseIdentifier = m_sparseIdentifierStorage.AcquireIdentifier();
			Assert(sparseIdentifier.IsValid());
			const DenseIdentifier denseIdentifier = m_denseIdentifierStorage.AcquireIdentifier();
			Assert(denseIdentifier.IsValid());

			if (LIKELY(sparseIdentifier.IsValid() && denseIdentifier.IsValid()))
			{
				return CreateInstanceInternal(sparseIdentifier, denseIdentifier, Forward<Args>(args)...);
			}
			else if (sparseIdentifier.IsValid())
			{
				m_sparseIdentifierStorage.ReturnIdentifier(sparseIdentifier);
			}
			else if (denseIdentifier.IsValid())
			{
				m_denseIdentifierStorage.ReturnIdentifier(denseIdentifier);
			}
			return Invalid;
		}

		template<typename _Type = Type, typename... Args>
		inline EnableIf<TypeTraits::IsBaseOf<Data::Component, _Type> && !Reflection::GetType<_Type>().IsAbstract(), Optional<Type*>>
		CreateInstanceManualOnCreated(const SparseIdentifier sparseIdentifier, Args&&... args)
		{
			const DenseIdentifier denseIdentifier = m_denseIdentifierStorage.AcquireIdentifier();
			Assert(denseIdentifier.IsValid());
			if (LIKELY(denseIdentifier.IsValid()))
			{
				return CreateInstanceInternal(sparseIdentifier, denseIdentifier, Forward<Args>(args)...);
			}
			return Invalid;
		}

		template<typename DynamicInitializerType, typename DesiredDynamicInitializerType = Reflection::CustomDynamicTypeInitializer<Type>>
		[[nodiscard]] static DesiredDynamicInitializerType GetDynamicInitializer(AnyView initializer)
		{
			if (const Optional<DynamicInitializerType*> pInitializer = initializer.Get<DynamicInitializerType>())
			{
				static_assert(TypeTraits::HasConstructor<DesiredDynamicInitializerType, DynamicInitializerType&&>);
				return DesiredDynamicInitializerType{Move(*pInitializer)};
			}
			else if constexpr (Reflection::HasBaseType<DynamicInitializerType> && TypeTraits::HasConstructor<DesiredDynamicInitializerType, typename DynamicInitializerType::BaseType&&>)
			{
				return GetDynamicInitializer<typename DynamicInitializerType::BaseType, DesiredDynamicInitializerType>(initializer);
			}
			else
			{
				ExpectUnreachable();
			}
		}

		virtual Optional<Component*> CreateInstanceDynamic(AnyView initializer) override
		{
			if constexpr (Reflection::GetType<Type>().GetFlags().AreNoneSet(
											Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::IsAbstract
										))
			{
				using DynamicInitializerType = Reflection::CustomDynamicTypeInitializer<Type>;

				using Initializer = Reflection::CustomTypeInitializer<Type>;
				DynamicInitializerType dynamicInitializer = GetDynamicInitializer<DynamicInitializerType, DynamicInitializerType>(initializer);

				Optional<Type*> pComponent;

				if constexpr (TypeTraits::HasConstructor<Type, DynamicInitializerType&&>)
				{
					if constexpr (IsDataComponent)
					{
						[[maybe_unused]] typename Type::ParentType& parent = static_cast<typename Type::ParentType&>(dynamicInitializer.GetParent());
						if (m_sceneRegistry.ReserveDataComponent(parent.GetIdentifier(), GetIdentifier()))
						{
							pComponent = CreateInstanceManualOnCreated(parent.GetIdentifier(), Move(dynamicInitializer));

							if (UNLIKELY(!pComponent.IsInvalid()))
							{
								[[maybe_unused]] const bool wasRemoved = m_sceneRegistry.OnDataComponentDestroyed(parent.GetIdentifier(), GetIdentifier());
								Assert(wasRemoved);
							}
						}
					}
					else
					{
						pComponent = CreateInstanceManualOnCreated(Move(dynamicInitializer));
					}
				}
				else if constexpr (TypeTraits::HasConstructor<Initializer, DynamicInitializerType&&>)
				{
					if constexpr (IsDataComponent)
					{
						[[maybe_unused]] typename Type::ParentType& parent = static_cast<typename Type::ParentType&>(dynamicInitializer.GetParent());
						if (m_sceneRegistry.ReserveDataComponent(parent.GetIdentifier(), GetIdentifier()))
						{
							pComponent = CreateInstanceManualOnCreated(parent.GetIdentifier(), Initializer(Move(dynamicInitializer)));

							if (UNLIKELY(!pComponent.IsInvalid()))
							{
								[[maybe_unused]] const bool wasRemoved = m_sceneRegistry.OnDataComponentDestroyed(parent.GetIdentifier(), GetIdentifier());
								Assert(wasRemoved);
							}
						}
					}
					else
					{
						pComponent = CreateInstanceManualOnCreated(Initializer(Move(dynamicInitializer)));
					}
				}
				else if constexpr (TypeTraits::HasConstructor<Initializer, const DynamicInitializerType&>)
				{
					if constexpr (IsDataComponent)
					{
						[[maybe_unused]] typename Type::ParentType& parent = static_cast<typename Type::ParentType&>(dynamicInitializer.GetParent());
						if (m_sceneRegistry.ReserveDataComponent(parent.GetIdentifier(), GetIdentifier()))
						{
							pComponent = CreateInstanceManualOnCreated(parent.GetIdentifier(), Initializer(dynamicInitializer));

							if (UNLIKELY(!pComponent.IsInvalid()))
							{
								[[maybe_unused]] const bool wasRemoved = m_sceneRegistry.OnDataComponentDestroyed(parent.GetIdentifier(), GetIdentifier());
								Assert(wasRemoved);
							}
						}
					}
					else
					{
						pComponent = CreateInstanceManualOnCreated(Initializer(dynamicInitializer));
					}
				}
				else
				{
					static_unreachable();
				}

				if (LIKELY(pComponent.IsValid()))
				{
					GetType().OnComponentCreated(*pComponent, dynamicInitializer.GetParent(), m_sceneRegistry);
				}

				return pComponent;
			}
			else
			{
				Assert(false, "Attempting to create instance of abstract type");
				return nullptr;
			}
		}

		template<typename _Type = Type, typename... Args>
		inline EnableIf<!Reflection::GetType<_Type>().IsAbstract(), Type&>
		CreateInstanceInternal(const SparseIdentifier sparseIdentifier, const DenseIdentifier denseIdentifier, Args&&... args)
		{
			static_assert(!TypeTraits::IsAbstract<Type>, "Attempting to create an instance of an abstract component type!");
			static_assert(TypeTraits::HasConstructor<Type, Args...>, "Component constructor not found!");

			if constexpr (IsDataComponent)
			{
				Assert(
					m_sceneRegistry.HasReservedDataComponentOfType(sparseIdentifier, GetIdentifier()),
					"Must reserve data component before constructing"
				);
				Assert(
					!m_sceneRegistry.HasDataComponentOfType(sparseIdentifier, GetIdentifier()),
					"Must reserve data component before constructing"
				);
			}

			Assert(sparseIdentifier.IsValid());
			Assert(denseIdentifier.IsValid());
			if constexpr (HasUniqueInstanceIdentifier)
			{
				Assert(m_sparseIdentifierStorage.IsIdentifierActive(sparseIdentifier));
			}
			else
			{
				m_sparseIdentifierStorage.OnInstanceCreated(sparseIdentifier);
			}
			Assert(m_denseIdentifierStorage.IsIdentifierActive(denseIdentifier));

			m_sparseStorage[sparseIdentifier] = denseIdentifier.GetIndex();
			m_sparseIdentifierLookupTable[denseIdentifier] = sparseIdentifier.GetFirstValidIndex();

			Type* __restrict pComponent;
			{
				StoredType& storedValue = m_denseComponentStorage.GetView()[denseIdentifier.GetFirstValidIndex()];

				pComponent = &storedValue;
				new (pComponent) Type(Forward<Args&&>(args)...);
			}

			if constexpr (!IsDataComponent)
			{
				CreateTypeIndexDataComponent(*pComponent);
			}

			if constexpr (HasGetInstanceGuid)
			{
				Threading::UniqueLock lock(m_instanceIdentifierLookup.m_mutex);
				Assert(!m_instanceIdentifierLookup.m_map.Contains(pComponent->GetInstanceGuid()), "Instance guids must not be duplicated!");
				m_instanceIdentifierLookup.m_map.EmplaceOrAssign(Guid(pComponent->GetInstanceGuid()), DenseIdentifier(denseIdentifier));
			}

			if constexpr (IsDataComponent)
			{
				[[maybe_unused]] const bool wasMarkedCreated = m_sceneRegistry.OnDataComponentCreated(sparseIdentifier, GetIdentifier());
				Assert(wasMarkedCreated);
			}

			if constexpr (HasOnConstructed)
			{
				pComponent->OnConstructed();
			}

			return *pComponent;
		}

		void CreateTypeIndexDataComponent(Type& component);

		friend ComponentType;
	protected:
		DenseIdentifierStorageType m_denseIdentifierStorage;
		SparseIdentifierStorageType m_sparseIdentifierStorage;

		FixedDenseStorageType m_denseComponentStorage;

		SparseIdentifierStorage m_sparseStorage;
		TIdentifierArray<typename SparseIdentifier::IndexType, DenseIdentifier> m_sparseIdentifierLookupTable;

		struct Dummy
		{
		};

		using ComponentInstanceIdentifierGuidMap = UnorderedMap<Guid, DenseIdentifier, Guid::Hash>;
		struct InstanceIdentifierLookup
		{
			mutable Threading::SharedMutex m_mutex;
			ComponentInstanceIdentifierGuidMap m_map;
		};
		NO_UNIQUE_ADDRESS TypeTraits::Select<HasGetInstanceGuid, InstanceIdentifierLookup, Dummy> m_instanceIdentifierLookup;

		struct ComponentStage : public Entity::ComponentStage
		{
			using BaseType = Entity::ComponentStage;
			using BaseType::BaseType;

			[[nodiscard]] bool RegisterComponent(const DenseIdentifier denseComponentIdentifier)
			{
				return m_updatedComponents.Set(denseComponentIdentifier);
			}

			bool DeregisterComponent(const DenseIdentifier denseComponentIdentifier)
			{
				return m_updatedComponents.Clear(denseComponentIdentifier);
			}

			[[nodiscard]] bool IsRegistered(const DenseIdentifier denseComponentIdentifier) const
			{
				return m_updatedComponents.IsSet(denseComponentIdentifier);
			}

			[[nodiscard]] bool HasElements() const
			{
				return m_updatedComponents.AreAnySet();
			}

			virtual void OnDependenciesResolved(Threading::JobRunnerThread& thread) override final
			{
				if (m_updatedComponents.AreAnySet())
				{
					Queue(thread);
				}
				else
				{
					SignalExecutionFinished(thread);
				}
			}

			void Clear()
			{
				m_updatedComponents.Clear();
			}
		protected:
			Threading::AtomicIdentifierMask<DenseIdentifier> m_updatedComponents;
		};

		struct MainUpdateComponents final : public ComponentStage
		{
#if STAGE_DEPENDENCY_PROFILING
			MainUpdateComponents(SceneRegistry& sceneRegistry, String&& name)
				: ComponentStage{Forward<String>(name)}
			{
				RegisterStage(sceneRegistry);
			}
#else
			MainUpdateComponents(SceneRegistry& sceneRegistry)
			{
				RegisterStage(sceneRegistry);
			}
#endif

			void RegisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::StageBase::AddSubsequentStage(sceneRegistry.GetDynamicRenderUpdatesFinishedStage());
							sceneRegistry.GetDynamicRenderUpdatesStartStage().AddSubsequentStage(*this);
						}
					);
				}
			}

			void DeregisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							sceneRegistry.GetDynamicRenderUpdatesStartStage().RemoveSubsequentStage(*this, thread, Threading::StageBase::RemovalFlags{});
							Threading::Job::RemoveSubsequentStage(
								sceneRegistry.GetDynamicRenderUpdatesFinishedStage(),
								thread,
								Threading::StageBase::RemovalFlags{}
							);
						}
					);
				}
			}

			virtual Threading::Job::Result OnExecute(Threading::JobRunnerThread&) override final
			{
				if constexpr (ShouldEnableUpdate)
				{
					ComponentTypeSceneData& typeSceneData = GetTypeSceneData();
					// TODO: Prefetching logic
					const typename FixedDenseStorageType::View denseStorage = typeSceneData.m_denseComponentStorage.GetView();
					const typename DenseIdentifier::IndexType maximumUsedDenseIndex =
						typeSceneData.m_denseIdentifierStorage.GetMaximumUsedElementCount();
					for (const typename DenseIdentifier::IndexType denseComponentIndex :
					     ComponentStage::m_updatedComponents.GetSetBitsIterator(0, maximumUsedDenseIndex))
					{
						StoredType& component = denseStorage[denseComponentIndex];
						component.Update();
					}
				}
				return Threading::Job::Result::Finished;
			}

			PURE_LOCALS_AND_POINTERS ComponentTypeSceneData& GetTypeSceneData()
			{
				return Memory::GetOwnerFromMember(*this, &ComponentTypeSceneData::m_mainUpdateComponents);
			}
			PURE_LOCALS_AND_POINTERS const ComponentTypeSceneData& GetTypeSceneData() const
			{
				return Memory::GetConstOwnerFromMember(*this, &ComponentTypeSceneData::m_mainUpdateComponents);
			}
		};
		struct BeforePhysicsUpdateComponents final : public ComponentStage
		{
#if STAGE_DEPENDENCY_PROFILING
			BeforePhysicsUpdateComponents(SceneRegistry& sceneRegistry, String&& name)
				: ComponentStage{Forward<String>(name)}
			{
				RegisterStage(sceneRegistry);
			}
#else
			BeforePhysicsUpdateComponents(SceneRegistry& sceneRegistry)
			{
				RegisterStage(sceneRegistry);
			}
#endif

			void RegisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableBeforePhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::StageBase::AddSubsequentStage(sceneRegistry.GetDynamicRenderUpdatesFinishedStage());
							sceneRegistry.GetDynamicRenderUpdatesStartStage().AddSubsequentStage(*this);
							Threading::StageBase::AddSubsequentStage(sceneRegistry.GetPhysicsSimulationStartStage());
						}
					);
				}
			}

			void DeregisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableBeforePhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							sceneRegistry.GetDynamicRenderUpdatesStartStage().RemoveSubsequentStage(*this, thread, Threading::StageBase::RemovalFlags{});
							Threading::Job::RemoveSubsequentStage(
								sceneRegistry.GetDynamicRenderUpdatesFinishedStage(),
								thread,
								Threading::StageBase::RemovalFlags{}
							);
							Threading::StageBase::RemoveSubsequentStage(
								sceneRegistry.GetPhysicsSimulationStartStage(),
								thread,
								Threading::StageBase::RemovalFlags{}
							);
						}
					);
				}
			}

			virtual Threading::Job::Result OnExecute(Threading::JobRunnerThread&) override final
			{
				if constexpr (ShouldEnableBeforePhysicsUpdate)
				{
					ComponentTypeSceneData& typeSceneData = GetTypeSceneData();
					// TODO: Prefetching logic
					const typename FixedDenseStorageType::View denseStorage = typeSceneData.m_denseComponentStorage.GetView();
					const typename DenseIdentifier::IndexType maximumUsedDenseIndex =
						typeSceneData.m_denseIdentifierStorage.GetMaximumUsedElementCount();
					for (const typename DenseIdentifier::IndexType denseComponentIndex :
					     ComponentStage::m_updatedComponents.GetSetBitsIterator(0, maximumUsedDenseIndex))
					{
						StoredType& component = denseStorage[denseComponentIndex];
						component.BeforePhysicsUpdate();
					}
				}
				return Threading::Job::Result::Finished;
			}

			PURE_LOCALS_AND_POINTERS ComponentTypeSceneData& GetTypeSceneData()
			{
				return Memory::GetOwnerFromMember(*this, &ComponentTypeSceneData::m_beforePhysicsUpdateComponents);
			}
			PURE_LOCALS_AND_POINTERS const ComponentTypeSceneData& GetTypeSceneData() const
			{
				return Memory::GetConstOwnerFromMember(*this, &ComponentTypeSceneData::m_beforePhysicsUpdateComponents);
			}
		};
		struct FixedPhysicsUpdateComponents final : public ComponentStage
		{
#if STAGE_DEPENDENCY_PROFILING
			FixedPhysicsUpdateComponents(SceneRegistry& sceneRegistry, String&& name)
				: ComponentStage{Forward<String>(name)}
			{
				RegisterStage(sceneRegistry);
			}
#else
			FixedPhysicsUpdateComponents(SceneRegistry& sceneRegistry)
			{
				RegisterStage(sceneRegistry);
			}
#endif

			void RegisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableFixedPhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							sceneRegistry.GetPhysicsStepStartStage().AddSubsequentStage(*this);
							Threading::StageBase::AddSubsequentStage(sceneRegistry.GetPhysicsStepFinishedStage());
						}
					);
				}
			}

			void DeregisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableFixedPhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							sceneRegistry.GetPhysicsStepStartStage().RemoveSubsequentStage(*this, thread, Threading::StageBase::RemovalFlags{});
							Threading::Job::RemoveSubsequentStage(
								sceneRegistry.GetPhysicsStepFinishedStage(),
								thread,
								Threading::StageBase::RemovalFlags{}
							);
						}
					);
				}
			}

			virtual Threading::Job::Result OnExecute(Threading::JobRunnerThread&) override final
			{
				if constexpr (ShouldEnableFixedPhysicsUpdate)
				{
					ComponentTypeSceneData& typeSceneData = GetTypeSceneData();
					// TODO: Prefetching logic
					const typename FixedDenseStorageType::View denseStorage = typeSceneData.m_denseComponentStorage.GetView();
					const typename DenseIdentifier::IndexType maximumUsedDenseIndex =
						typeSceneData.m_denseIdentifierStorage.GetMaximumUsedElementCount();
					for (const typename DenseIdentifier::IndexType denseComponentIndex :
					     ComponentStage::m_updatedComponents.GetSetBitsIterator(0, maximumUsedDenseIndex))
					{
						StoredType& component = denseStorage[denseComponentIndex];
						component.FixedPhysicsUpdate();
					}
				}
				return Threading::Job::Result::Finished;
			}

			PURE_LOCALS_AND_POINTERS ComponentTypeSceneData& GetTypeSceneData()
			{
				return Memory::GetOwnerFromMember(*this, &ComponentTypeSceneData::m_fixedPhysicsUpdateComponents);
			}
			PURE_LOCALS_AND_POINTERS const ComponentTypeSceneData& GetTypeSceneData() const
			{
				return Memory::GetConstOwnerFromMember(*this, &ComponentTypeSceneData::m_fixedPhysicsUpdateComponents);
			}
		};
		struct AfterPhysicsUpdateComponents final : public ComponentStage
		{
#if STAGE_DEPENDENCY_PROFILING
			AfterPhysicsUpdateComponents(SceneRegistry& sceneRegistry, String&& name)
				: ComponentStage{Forward<String>(name)}
			{
				RegisterStage(sceneRegistry);
			}
#else
			AfterPhysicsUpdateComponents(SceneRegistry& sceneRegistry)
			{
				RegisterStage(sceneRegistry);
			}
#endif

			void RegisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableAfterPhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::StageBase::AddSubsequentStage(sceneRegistry.GetDynamicRenderUpdatesFinishedStage());
							sceneRegistry.GetDynamicRenderUpdatesStartStage().AddSubsequentStage(*this);
							sceneRegistry.GetPhysicsSimulationFinishedStage().AddSubsequentStage(*this);
						}
					);
				}
			}

			void DeregisterStage(SceneRegistry& sceneRegistry)
			{
				if constexpr (ShouldEnableAfterPhysicsUpdate)
				{
					sceneRegistry.ModifyFrameGraph(
						[this, &sceneRegistry]()
						{
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							sceneRegistry.GetDynamicRenderUpdatesStartStage().RemoveSubsequentStage(*this, thread, Threading::StageBase::RemovalFlags{});
							sceneRegistry.GetPhysicsSimulationFinishedStage().RemoveSubsequentStage(*this, thread, Threading::StageBase::RemovalFlags{});
							Threading::Job::RemoveSubsequentStage(
								sceneRegistry.GetDynamicRenderUpdatesFinishedStage(),
								thread,
								Threading::StageBase::RemovalFlags{}
							);
						}
					);
				}
			}

			virtual Threading::Job::Result OnExecute(Threading::JobRunnerThread&) override final
			{
				if constexpr (ShouldEnableAfterPhysicsUpdate)
				{
					ComponentTypeSceneData& typeSceneData = GetTypeSceneData();
					// TODO: Prefetching logic
					const typename FixedDenseStorageType::View denseStorage = typeSceneData.m_denseComponentStorage.GetView();
					const typename DenseIdentifier::IndexType maximumUsedDenseIndex =
						typeSceneData.m_denseIdentifierStorage.GetMaximumUsedElementCount();
					for (const typename DenseIdentifier::IndexType denseComponentIndex :
					     ComponentStage::m_updatedComponents.GetSetBitsIterator(0, maximumUsedDenseIndex))
					{
						StoredType& component = denseStorage[denseComponentIndex];
						component.AfterPhysicsUpdate();
					}
				}
				return Threading::Job::Result::Finished;
			}

			PURE_LOCALS_AND_POINTERS ComponentTypeSceneData& GetTypeSceneData()
			{
				return Memory::GetOwnerFromMember(*this, &ComponentTypeSceneData::m_afterPhysicsUpdateComponents);
			}
			PURE_LOCALS_AND_POINTERS const ComponentTypeSceneData& GetTypeSceneData() const
			{
				return Memory::GetConstOwnerFromMember(*this, &ComponentTypeSceneData::m_afterPhysicsUpdateComponents);
			}
		};

		NO_UNIQUE_ADDRESS TypeTraits::Select<ShouldEnableUpdate, MainUpdateComponents, Internal::DummyComponentStage> m_mainUpdateComponents;
		NO_UNIQUE_ADDRESS TypeTraits::Select<ShouldEnableBeforePhysicsUpdate, BeforePhysicsUpdateComponents, Internal::DummyComponentStage>
			m_beforePhysicsUpdateComponents;
		NO_UNIQUE_ADDRESS TypeTraits::Select<ShouldEnableFixedPhysicsUpdate, FixedPhysicsUpdateComponents, Internal::DummyComponentStage>
			m_fixedPhysicsUpdateComponents;
		NO_UNIQUE_ADDRESS TypeTraits::Select<ShouldEnableAfterPhysicsUpdate, AfterPhysicsUpdateComponents, Internal::DummyComponentStage>
			m_afterPhysicsUpdateComponents;
	};

	template<typename Type>
	inline void ComponentTypeSceneData<Type>::CreateTypeIndexDataComponent(Type& __restrict component)
	{
		if constexpr (!IsDataComponent)
		{
			ComponentTypeSceneData<Data::TypeIndex>& typeIndexSceneData = m_sceneRegistry.GetCachedSceneData<Data::TypeIndex>();
			typeIndexSceneData.CreateInstance(component.DataComponentOwner::GetIdentifier(), component, m_identifier);
		}
	}

	template<typename Type>
	[[nodiscard]] inline ComponentTypeSceneData<Type>&
	SceneRegistry::GetOrCreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier)
	{
		return GetOrCreateComponentTypeData<Type>(
			typeIdentifier,
			static_cast<ComponentType<Type>&>(*m_manager.GetRegistry().Get(typeIdentifier))
		);
	}

	template<typename Type>
	[[nodiscard]] inline Optional<ComponentTypeSceneData<Type>*> SceneRegistry::GetOrCreateComponentTypeData()
	{
		const ComponentTypeIdentifier typeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>());
		return GetOrCreateComponentTypeData<Type>(
			typeIdentifier,
			static_cast<ComponentType<Type>&>(*m_manager.GetRegistry().Get(typeIdentifier))
		);
	}

	template<typename Type>
	inline ComponentTypeSceneData<Type>& SceneRegistry::CreateComponentTypeData(const ComponentTypeIdentifier typeIdentifier)
	{
		return CreateComponentTypeData<Type>(typeIdentifier, static_cast<ComponentType<Type>&>(*m_manager.GetRegistry().Get(typeIdentifier)));
	}

	template<typename Type>
	inline Optional<ComponentTypeSceneData<Type>*> SceneRegistry::CreateComponentTypeData()
	{
		const ComponentTypeIdentifier typeIdentifier = FindComponentTypeIdentifier(Reflection::GetTypeGuid<Type>());
		return CreateComponentTypeData<Type>(typeIdentifier, static_cast<ComponentType<Type>&>(*m_manager.GetRegistry().Get(typeIdentifier)));
	}
}
