#pragma once

#include "DataComponentOwner.h"
#include "DataComponentResult.h"

#include "Data/HierarchyComponent.h"

#include <Engine/Entity/ComponentFlags.h>
#include <Engine/Tag/TagIdentifier.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Reflection/CoreTypes.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/EnumFlags.h>

namespace ngine
{
	struct SceneBase;
	struct Scene2D;
	struct Scene3D;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Entity
{
	struct ComponentTypeInterface;
	struct ComponentTypeSceneDataInterface;
	struct SceneRegistry;
	struct RootSceneComponent;
	struct RootSceneComponent2D;

	struct HierarchyComponentBase : public DataComponentOwner
	{
		using BaseType = DataComponentOwner;
		using RootType = HierarchyComponentBase;
		using ParentType = HierarchyComponentBase;
		using ChildType = HierarchyComponentBase;

		using InstanceIdentifier = TIdentifier<uint32, 12>;

		using Flags = ComponentFlags;
		using BaseType::BaseType;
		HierarchyComponentBase(
			const ComponentIdentifier identifier,
			const Optional<ParentType*> pParent,
			Entity::SceneRegistry& sceneRegistry,
			const EnumFlags<Flags> flags = {},
			const Guid instanceGuid = Guid::Generate()
		);
		HierarchyComponentBase(
			const Optional<ParentType*> pParent,
			Entity::SceneRegistry& sceneRegistry,
			const EnumFlags<Flags> flags = {},
			const Guid instanceGuid = Guid::Generate()
		);
		virtual ~HierarchyComponentBase() = default;

		using ChildIndex = uint16;

		struct Initializer : public Reflection::TypeInitializer
		{
			using BaseType = Reflection::TypeInitializer;

			Initializer(
				const Optional<ParentType*> pParent,
				SceneRegistry& sceneRegistry,
				const EnumFlags<Flags> flags = {},
				const Guid instanceGuid = Guid::Generate()
			)
				: m_sceneRegistry(sceneRegistry)
				, m_pParent(pParent)
				, m_flags(flags)
				, m_instanceGuid(instanceGuid)
			{
			}
			Initializer(
				BaseType&& baseInitializer,
				const Optional<ParentType*> pParent,
				SceneRegistry& sceneRegistry,
				const EnumFlags<Flags> flags = {},
				const Guid instanceGuid = Guid::Generate()
			)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_sceneRegistry(sceneRegistry)
				, m_pParent(pParent)
				, m_flags(flags)
				, m_instanceGuid(instanceGuid)
			{
			}

			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
			[[nodiscard]] Optional<ParentType*> GetParent() const
			{
				return m_pParent;
			}
			[[nodiscard]] EnumFlags<Flags> GetFlags() const
			{
				return m_flags;
			}
			[[nodiscard]] Guid GetInstanceGuid() const
			{
				return m_instanceGuid;
			}
		protected:
			SceneRegistry& m_sceneRegistry;
			Optional<ParentType*> m_pParent;
			EnumFlags<Flags> m_flags;
			Guid m_instanceGuid = Guid::Generate();
		};
		using DynamicInitializer = Initializer;
		struct Deserializer : public Reflection::TypeDeserializer
		{
			using BaseType = Reflection::TypeDeserializer;

			Deserializer(
				TypeDeserializer&& deserializer,
				SceneRegistry& sceneRegistry,
				const Optional<ParentType*> pParent,
				const EnumFlags<Flags> flags = {}
			)
				: TypeDeserializer{Forward<TypeDeserializer>(deserializer)}
				, m_sceneRegistry(sceneRegistry)
				, m_pParent(pParent)
				, m_flags(flags)
			{
			}

			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
			[[nodiscard]] Optional<ParentType*> GetParent() const
			{
				return m_pParent;
			}
			[[nodiscard]] EnumFlags<Flags> GetFlags() const
			{
				return m_flags;
			}
		protected:
			SceneRegistry& m_sceneRegistry;
			Optional<ParentType*> m_pParent;
			EnumFlags<Flags> m_flags;
		};
		struct Cloner : public Reflection::TypeCloner
		{
			Cloner(
				Threading::JobBatch& jobBatch,
				const Optional<ParentType*> pParent,
				SceneRegistry& sceneRegistry,
				const SceneRegistry& templateSceneRegistry,
				const Guid instanceGuid = Guid::Generate(),
				const Optional<ChildIndex> preferredChildIndex = Invalid
			)
				: TypeCloner{jobBatch}
				, m_sceneRegistry(sceneRegistry)
				, m_templateSceneRegistry(templateSceneRegistry)
				, m_pParent(pParent)
				, m_instanceGuid(instanceGuid)
				, m_preferredChildIndex(preferredChildIndex)
			{
			}

			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
			[[nodiscard]] const SceneRegistry& GetTemplateSceneRegistry() const
			{
				return m_templateSceneRegistry;
			}
			[[nodiscard]] Optional<ParentType*> GetParent() const
			{
				return m_pParent;
			}
			[[nodiscard]] Guid GetInstanceGuid() const
			{
				return m_instanceGuid;
			}
			[[nodiscard]] Optional<ChildIndex> GetPreferredChildIndex() const
			{
				return m_preferredChildIndex;
			}
		protected:
			SceneRegistry& m_sceneRegistry;
			const SceneRegistry& m_templateSceneRegistry;
			Optional<ParentType*> m_pParent;
			Guid m_instanceGuid;
			Optional<ChildIndex> m_preferredChildIndex;
		};
		HierarchyComponentBase(Initializer&& initializer);
		HierarchyComponentBase(const Deserializer& deserializer);
		HierarchyComponentBase(const HierarchyComponentBase& templateComponent, const Cloner& cloner);

		void OnConstructed();
		bool Destroy(SceneRegistry& sceneRegistry);

		void Disable();
		void Disable(Entity::SceneRegistry& sceneRegistry);
		void Enable();
		void Enable(Entity::SceneRegistry& sceneRegistry);
		void DisableWithChildren(Entity::SceneRegistry& sceneRegistry);
		void DisableWithChildren();
		void EnableWithChildren();
		void EnableWithChildren(Entity::SceneRegistry& sceneRegistry);

		void DetachFromOctree();
		void DetachFromOctree(Entity::SceneRegistry& sceneRegistry);
		void AttachToOctree();
		void AttachToOctree(Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] PURE_STATICS Guid GetInstanceGuid() const;
		[[nodiscard]] PURE_STATICS Guid GetInstanceGuid(const Entity::SceneRegistry&) const;

		[[nodiscard]] virtual PURE_STATICS Entity::SceneRegistry& GetSceneRegistry() const = 0;

		[[nodiscard]] PURE_STATICS Optional<ComponentTypeSceneDataInterface*> GetTypeSceneData(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Optional<ComponentTypeSceneDataInterface*> GetTypeSceneData() const
		{
			return GetTypeSceneData(GetSceneRegistry());
		}

		[[nodiscard]] PURE_STATICS ComponentTypeIdentifier GetTypeIdentifier(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS ComponentTypeIdentifier GetTypeIdentifier() const
		{
			return GetTypeIdentifier(GetSceneRegistry());
		}
		[[nodiscard]] PURE_STATICS Guid GetTypeGuid(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Guid GetTypeGuid() const
		{
			return GetTypeGuid(GetSceneRegistry());
		}
		[[nodiscard]] PURE_STATICS Optional<ComponentTypeInterface*> GetTypeInfo(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Optional<ComponentTypeInterface*> GetTypeInfo() const
		{
			return GetTypeInfo(GetSceneRegistry());
		}
		[[nodiscard]] PURE_STATICS Optional<const Reflection::TypeInterface*> GetTypeInterface(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Optional<const Reflection::TypeInterface*> GetTypeInterface() const
		{
			return GetTypeInterface(GetSceneRegistry());
		}

		template<typename Type>
		[[nodiscard]] PURE_STATICS bool Is(const SceneRegistry& sceneRegistry) const
		{
			if constexpr (TypeTraits::IsAbstract<Type>)
			{
				return false;
			}

			return GetTypeGuid(sceneRegistry) == Reflection::GetTypeGuid<Type>();
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS bool Is() const
		{
			return Is<Type>(GetSceneRegistry());
		}
		[[nodiscard]] PURE_STATICS bool Is(const Guid typeGuid, const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS bool Implements(const Guid typeGuid, const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS bool IsOrImplements(const Guid typeGuid, const SceneRegistry& sceneRegistry) const;

		template<typename Type>
		[[nodiscard]] PURE_STATICS bool Implements(const SceneRegistry& sceneRegistry) const
		{
			return Implements(Reflection::GetTypeGuid<Type>(), sceneRegistry);
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS bool Implements() const
		{
			return Implements<Type>(GetSceneRegistry());
		}

		template<typename Type>
		[[nodiscard]] PURE_STATICS bool IsOrImplements(const SceneRegistry& sceneRegistry) const
		{
			if constexpr (TypeTraits::IsAbstract<Type>)
			{
				return Implements<Type>(sceneRegistry);
			}
			else
			{
				return IsOrImplements(Reflection::GetTypeGuid<Type>(), sceneRegistry);
			}
		}

		//! Converts from one component type to another, given that they are compatible.
		//! This function is rather slow, prefer AsExactType if you know the exact type you are looking for.
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<Type*> As(const SceneRegistry& sceneRegistry)
		{
			return Optional<Type*>(&static_cast<Type&>(*this), IsOrImplements<Type>(sceneRegistry));
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<Type*> As()
		{
			return As<Type>(GetSceneRegistry());
		}

		//! Converts from one component type to another, given that they are compatible.
		//! This function is rather slow, prefer AsExactType if you know the exact type you are looking for.
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<const Type*> As(const SceneRegistry& sceneRegistry) const
		{
			return Optional<const Type*>(&static_cast<const Type&>(*this), IsOrImplements<Type>(sceneRegistry));
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<const Type*> As() const
		{
			return As<Type>(GetSceneRegistry());
		}

		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<Type*> AsExactType(const SceneRegistry& sceneRegistry)
		{
			return Optional<Type*>(&static_cast<Type&>(*this), Is<Type>(sceneRegistry));
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<Type*> AsExactType()
		{
			return AsExactType<Type>(GetSceneRegistry());
		}

		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<const Type*> AsExactType(const SceneRegistry& sceneRegistry) const
		{
			return Optional<const Type*>(&static_cast<const Type&>(*this), Is<Type>(sceneRegistry));
		}
		template<typename Type>
		[[nodiscard]] PURE_STATICS Optional<const Type*> AsExactType() const
		{
			return AsExactType<Type>(GetSceneRegistry());
		}

		template<typename Type>
		[[nodiscard]] Type& AsExpected()
		{
			Assert(IsOrImplements<Type>(GetSceneRegistry()));
			return static_cast<Type&>(*this);
		}
		template<typename Type>
		[[nodiscard]] Type& AsExpected([[maybe_unused]] const SceneRegistry& sceneRegistry)
		{
			Assert(IsOrImplements<Type>(sceneRegistry));
			return static_cast<Type&>(*this);
		}

		template<typename Type>
		[[nodiscard]] const Type& AsExpected() const
		{
			Assert(IsOrImplements<Type>(GetSceneRegistry()));
			return static_cast<const Type&>(*this);
		}
		template<typename Type>
		[[nodiscard]] const Type& AsExpected([[maybe_unused]] const SceneRegistry& sceneRegistry) const
		{
			Assert(IsOrImplements<Type>(sceneRegistry));
			return static_cast<const Type&>(*this);
		}

		[[nodiscard]] bool IsParentOf(const ChildType& child) const;
		[[nodiscard]] bool IsParentOfRecursive(const ChildType& child) const;
		[[nodiscard]] bool IsChildOf(const ParentType& parent) const;
		[[nodiscard]] bool IsChildOfRecursive(const ParentType& parent) const;

		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindFirstParentOfType(SceneRegistry& sceneRegistry) const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindFirstParentOfType() const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindFirstParentImplementingType(SceneRegistry& sceneRegistry) const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindFirstParentImplementingType() const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindLastParentOfType(SceneRegistry& sceneRegistry) const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindLastParentOfType() const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindLastParentImplementingType(SceneRegistry& sceneRegistry) const;
		template<typename ParentComponentType>
		Optional<ParentComponentType*> FindLastParentImplementingType() const;

		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildOfType(SceneRegistry& sceneRegistry) const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildOfType() const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildImplementingType(SceneRegistry& sceneRegistry) const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildImplementingType() const;

		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildOfTypeRecursive(SceneRegistry& sceneRegistry) const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildOfTypeRecursive() const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildImplementingTypeRecursive(SceneRegistry& sceneRegistry) const;
		template<typename ChildComponentType>
		Optional<ChildComponentType*> FindFirstChildImplementingTypeRecursive() const;

		template<typename ChildComponentType, typename Callback>
		void IterateChildrenOfType(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenOfType(Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenImplementingType(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenImplementingType(Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenOfTypeRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenOfTypeRecursive(Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenImplementingTypeRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateChildrenImplementingTypeRecursive(Callback&& callback) const;

		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInChildrenRecursive(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInSelfAndChildrenRecursive(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInChildrenRecursive(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInSelfAndChildrenRecursive(SceneRegistry& sceneRegistry
		) const;

		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInParents(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInParents(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentOfTypeInChildren(SceneRegistry& sceneRegistry) const;
		template<typename DataComponentType>
		DataComponentResult<DataComponentType> FindFirstDataComponentImplementingTypeInChildren(SceneRegistry& sceneRegistry) const;

		template<typename ChildComponentType, typename Callback>
		void IterateDataComponentsOfTypeInChildrenRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename ChildComponentType, typename Callback>
		void IterateDataComponentsImplementingTypeInChildrenRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;

		template<typename DataComponentType, typename Callback>
		void IterateDataComponentsOfTypeInParentsRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename DataComponentType, typename Callback>
		void IterateDataComponentsImplementingTypeInParentsRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const;

		[[nodiscard]] bool HasParent() const
		{
			return m_pParent != nullptr;
		}

		[[nodiscard]] ParentType& GetParent() const
		{
			return *m_pParent;
		}
		[[nodiscard]] Optional<ParentType*> GetParentSafe() const
		{
			return m_pParent;
		}
		[[nodiscard]] ComponentIdentifier GetParentIdentifier(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] ComponentIdentifier GetParentIdentifier() const
		{
			return GetParentIdentifier(GetSceneRegistry());
		}
		[[nodiscard]] RootType& GetRootParent() const
		{
			ParentType* pComponent = const_cast<HierarchyComponentBase*>(this);
			while (pComponent != nullptr && pComponent->m_pParent != nullptr)
			{
				pComponent = pComponent->m_pParent;
			}
			return static_cast<RootType&>(*pComponent);
		}

		//! Gets this widget's index within its parent's children container
		[[nodiscard]] ChildIndex GetParentChildIndex() const;

		//! Gets the scene component that this component directly belongs to
		//! For example, a player spawned as a scene will have the player scene component returned, and not the top level root scene.
		//! Note that if we are a scene component, we return the next parent and not ourselves.
		[[nodiscard]] PURE_STATICS Optional<HierarchyComponentBase*> GetParentSceneComponent() const;

		using ChildContainer = Vector<ReferenceWrapper<ChildType>, ChildIndex>;

		struct ChildView : public ArrayView<const ReferenceWrapper<ChildType>, ChildIndex>
		{
			using BaseType = ArrayView<const ReferenceWrapper<ChildType>, ChildIndex>;
			ChildView() = default;
			ChildView(
				Threading::SharedLock<Threading::SharedMutex>&& lock, const typename BaseType::iterator begin, const typename BaseType::iterator end
			)
				: BaseType(begin, end)
				, m_lock(Forward<Threading::SharedLock<Threading::SharedMutex>>(lock))
			{
				if (UNLIKELY_ERROR(!m_lock.IsLocked()))
				{
					BaseType::operator=({});
				}
			}
			ChildView(ChildView&& other) = default;
			ChildView(const ChildView&) = delete;
			ChildView& operator=(ChildView&&) = default;
			ChildView& operator=(const ChildView&) = delete;

			struct IteratorType : public ArrayView<const ReferenceWrapper<ChildType>, ChildIndex>::IteratorType
			{
				using BaseType = ArrayView<const ReferenceWrapper<ChildType>, ChildIndex>::IteratorType;
				using BaseType::BaseType;
				using BaseType::operator=;

				[[nodiscard]] constexpr operator ChildType&() const
				{
					return **BaseType::Get();
				}
				[[nodiscard]] constexpr ChildType& operator*() const
				{
					return **BaseType::Get();
				}
			};

			[[nodiscard]] PURE_STATICS constexpr IteratorType begin() const noexcept
			{
				return IteratorType(BaseType::begin());
			}
			[[nodiscard]] PURE_STATICS constexpr IteratorType end() const noexcept
			{
				return IteratorType(BaseType::end());
			}
		private:
			Threading::SharedLock<Threading::SharedMutex> m_lock;
		};
		[[nodiscard]] ChildView GetChildren() const LIFETIME_BOUND
		{
			Threading::SharedLock<Threading::SharedMutex> lock(m_childMutex);
			return {Move(lock), m_children.begin(), m_children.end()};
		}
		[[nodiscard]] bool HasChildren() const
		{
			Threading::SharedLock lock(m_childMutex);
			return m_children.HasElements();
		}
		[[nodiscard]] ChildIndex GetChildCount() const
		{
			Threading::SharedLock lock(m_childMutex);
			return m_children.GetSize();
		}
		[[nodiscard]] ChildIndex GetNextAvailableChildIndex() const
		{
			Threading::SharedLock lock(m_childMutex);
			return m_children.GetNextAvailableIndex();
		}

		[[nodiscard]] bool IsScene(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] bool IsScene() const;
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsMeshScene(const SceneRegistry& sceneRegistry) const
		{
			return GetFlags(sceneRegistry).IsSet(ComponentFlags::IsMeshScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsMeshScene() const
		{
			return GetFlags().IsSet(ComponentFlags::IsMeshScene);
		}
		[[nodiscard]] bool IsRenderItem(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] bool IsRenderItem() const;
		[[nodiscard]] bool IsStaticMesh(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] bool IsStaticMesh() const;
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsRootSceneComponent(const SceneRegistry& sceneRegistry) const
		{
			return GetFlags(sceneRegistry).IsSet(ComponentFlags::IsRootScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsRootSceneComponent() const
		{
			return GetFlags().IsSet(ComponentFlags::IsRootScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool Is2D() const
		{
			return GetFlags().IsSet(ComponentFlags::Is2D);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool Is3D() const
		{
			return GetFlags().IsSet(ComponentFlags::Is3D);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsLight() const
		{
			return GetFlags().IsSet(ComponentFlags::IsLight);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool ShouldSaveToDisk() const
		{
			return GetFlags().IsSet(ComponentFlags::SaveToDisk);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsDestroying(Entity::SceneRegistry& sceneRegistry) const
		{
			return GetFlags(sceneRegistry).IsSet(ComponentFlags::IsDestroying);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsDestroying() const
		{
			return GetFlags().IsSet(ComponentFlags::IsDestroying);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsDisabled() const
		{
			return GetFlags().AreAnySet(ComponentFlags::IsDisabledFromAnySource);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsEnabled() const
		{
			return !IsDisabled();
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsEnabled(Entity::SceneRegistry& sceneRegistry) const
		{
			return GetFlags(sceneRegistry).AreNoneSet(ComponentFlags::IsDisabledFromAnySource);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsSimulationPaused() const
		{
			return GetFlags().IsSet(ComponentFlags::IsSimulationPaused);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsSimulationActive() const
		{
			return !GetFlags().IsSet(ComponentFlags::IsSimulationPaused);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsReferenced() const
		{
			return GetFlags().IsSet(ComponentFlags::IsReferenced);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsRegisteredInTree() const
		{
			return !GetFlags().AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource | ComponentFlags::IsRootScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsDetachedFromTree() const
		{
			return GetFlags().AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource | ComponentFlags::IsRootScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsDetachedFromTree(Entity::SceneRegistry& sceneRegistry) const
		{
			return GetFlags(sceneRegistry).AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource | ComponentFlags::IsRootScene);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool CanClone() const
		{
			return GetFlags().IsNotSet(Flags::DisableCloning);
		}
		void SetIsReferenced(bool isReferenced, Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] HierarchyComponentBase& GetRootSceneComponent() const;
		[[nodiscard]] HierarchyComponentBase& GetRootSceneComponent(Entity::SceneRegistry& sceneRegistry) const;

		void EnableSaveToDisk();
		void EnableSaveToDisk(Entity::SceneRegistry& sceneRegistry);
		void DisableSaveToDisk();
		void DisableSaveToDisk(Entity::SceneRegistry& sceneRegistry);
		void DisableCloning();
		void DisableCloning(Entity::SceneRegistry& sceneRegistry);
		void EnableCloning();
		void EnableCloning(Entity::SceneRegistry& sceneRegistry);

		void SetIsMeshScene(Entity::SceneRegistry& sceneRegistry);
		void ClearIsMeshScene(Entity::SceneRegistry& sceneRegistry);
		void ToggleIsMeshScene(Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] EnumFlags<ComponentFlags> GetFlags(const Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] EnumFlags<ComponentFlags> GetFlags() const;

		void PauseSimulation(Entity::SceneRegistry& sceneRegistry);
		void ResumeSimulation(Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] bool ShouldSerialize(Serialization::Writer serializer) const;
		bool Serialize(Serialization::Reader serializer, Threading::JobBatch& jobBatchOut);
		bool Serialize(Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;
		bool SerializeDataComponents(Serialization::Writer serializer) const;
		[[nodiscard]] Threading::JobBatch DeserializeDataComponentsAndChildren(const Serialization::Reader serializer);
		bool SerializeChildren(Serialization::Writer serializer) const;
		bool SerializeDataComponentsAndChildren(Serialization::Writer serializer) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> serializer);
		bool SerializeCustomData(Serialization::Writer serializer) const;

		void AttachTo(HierarchyComponentBase& newParent, Entity::SceneRegistry& sceneRegistry);
		void AttachToNewParent(HierarchyComponentBase& newParent, Entity::SceneRegistry& sceneRegistry);
		void AttachToNewParent(HierarchyComponentBase& newParent, const ChildIndex index, Entity::SceneRegistry& sceneRegistry);

		void AddTag(const Tag::Identifier tag);
		void AddTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry);
		void RemoveTag(const Tag::Identifier tag);
		void RemoveTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry);
		[[nodiscard]] bool HasTag(const Tag::Identifier tag) const;
		[[nodiscard]] bool HasTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry) const;
	protected:
		friend Scene2D;
		friend Scene3D;
		friend RootSceneComponent2D;
		friend RootSceneComponent;
		friend ComponentTypeSceneData<HierarchyComponentBase>;
		friend Reflection::ReflectedType<HierarchyComponentBase>;

		void SetParent(ParentType& parent, const Entity::SceneRegistry& sceneRegistry);
		void ClearParent(const Entity::SceneRegistry& sceneRegistry);

		void ReserveAdditionalChildren(const ChildIndex count)
		{
			Threading::UniqueLock lock(m_childMutex);
			m_children.Reserve(m_children.GetSize() + count);
		}

		// Rotates children by n elements to the right
		void RotateChildren(ChildIndex n);
		[[nodiscard]] ChildContainer StealChildren()
		{
			Threading::UniqueLock lock(m_childMutex);
			return Move(m_children);
		}

		void AddChild(ChildType& newChild, Entity::SceneRegistry& sceneRegistry)
		{
			Assert(newChild.GetParentSafe() == nullptr || newChild.GetParentSafe() == this);
			{
				Threading::UniqueLock lock(m_childMutex);
				Assert(!m_children.Contains(newChild));
				m_children.EmplaceBack(newChild);
			}
			newChild.SetParent(*this, sceneRegistry);
		}
		void AddChild(ChildType& newChild, ChildIndex index, Entity::SceneRegistry& sceneRegistry)
		{
			Assert(newChild.GetParentSafe() == nullptr || newChild.GetParentSafe() == this);
			{
				Threading::UniqueLock lock(m_childMutex);
				Assert(!m_children.Contains(newChild));
				index = Math::Min(index, m_children.GetNextAvailableIndex());
				m_children.Emplace(m_children.begin() + index, Memory::Uninitialized, newChild);
			}
			newChild.SetParent(*this, sceneRegistry);
		}
		void RemoveChild(ChildType& existingChild)
		{
			Assert(existingChild.GetParentSafe() == this);
			{
				Threading::UniqueLock lock(m_childMutex);
				[[maybe_unused]] const bool removed = m_children.RemoveFirstOccurrence(existingChild);
				Assert(removed);
			}
		}
		void RemoveChildAndClearParent(ChildType& existingChild, Entity::SceneRegistry& sceneRegistry)
		{
			RemoveChild(existingChild);
			existingChild.ClearParent(sceneRegistry);
		}

		void AttachChild(HierarchyComponentBase& newChildComponent, Entity::SceneRegistry& sceneRegistry);
		void AttachChild(HierarchyComponentBase& newChildComponent, const ChildIndex index, Entity::SceneRegistry& sceneRegistry);

		virtual void OnAttachedToNewParent()
		{
		}

		virtual void OnBeforeDetachFromParent()
		{
		}

		virtual void OnChildAttached(
			[[maybe_unused]] HierarchyComponentBase& newChildComponent,
			[[maybe_unused]] const ChildIndex index,
			[[maybe_unused]] const Optional<ChildIndex> preferredChildIndex
		)
		{
		}

		virtual void OnChildDetached([[maybe_unused]] HierarchyComponentBase& childComponent)
		{
		}

		void EnableInternal(SceneRegistry& sceneRegistry);
		void DisableInternal(SceneRegistry& sceneRegistry);
		void DestroyInternal(SceneRegistry& sceneRegistry);

		[[nodiscard]] EnumFlags<Flags> DeserializeCustomDataInternal(const Optional<Serialization::Reader> serializer);
	private:
		Optional<ChildType*> m_pParent;
		mutable Threading::SharedMutex m_childMutex;
		ChildContainer m_children;
	};

	namespace Data
	{
		[[nodiscard]] inline Entity::HierarchyComponentBase& HierarchyComponent::DynamicInitializer::GetParent() const
		{
			return static_cast<Entity::HierarchyComponentBase&>(BaseType::GetParent());
		}
		[[nodiscard]] inline Entity::HierarchyComponentBase& HierarchyComponent::Cloner::GetParent() const
		{
			return static_cast<Entity::HierarchyComponentBase&>(BaseType::GetParent());
		}
		[[nodiscard]] inline Entity::HierarchyComponentBase& HierarchyComponent::Deserializer::GetParent() const
		{
			return static_cast<Entity::HierarchyComponentBase&>(BaseType::GetParent());
		}
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::HierarchyComponentBase>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::HierarchyComponentBase>(
			"{907E48C2-B4D4-4072-A94A-094F9E13C405}"_guid,
			MAKE_UNICODE_LITERAL("Hierarchy Component"),
			TypeFlags::IsAbstract,
			Tags{},
			Properties{},
			Functions{
				Function{
					"c29a8c9a-ddfc-4b1f-bfec-cf8fbae68928"_guid,
					MAKE_UNICODE_LITERAL("Enable"),
					(void(Entity::HierarchyComponentBase::*)()) & Entity::HierarchyComponentBase::Enable,
					FunctionFlags::VisibleToUI,
					ReturnType{}
				},
				Function{
					"01c40a50-917a-48e0-9503-8f7a5e9c8c33"_guid,
					MAKE_UNICODE_LITERAL("Disable"),
					(void(Entity::HierarchyComponentBase::*)()) & Entity::HierarchyComponentBase::Disable,
					FunctionFlags::VisibleToUI,
					ReturnType{}
				},
				Function{
					"50dbab68-1805-4be7-b521-2f244371aee4"_guid,
					MAKE_UNICODE_LITERAL("Enable with Children"),
					(void(Entity::HierarchyComponentBase::*)()) & Entity::HierarchyComponentBase::EnableWithChildren,
					FunctionFlags::VisibleToUI,
					ReturnType{}
				},
				Function{
					"9b6c7c40-1641-4ffe-a6bb-629afd90e0a3"_guid,
					MAKE_UNICODE_LITERAL("Disable with Children"),
					(void(Entity::HierarchyComponentBase::*)()) & Entity::HierarchyComponentBase::DisableWithChildren,
					FunctionFlags::VisibleToUI,
					ReturnType{}
				},
				Function{
					"f0ca0e47-79b7-49eb-a115-71e123f827bf"_guid,
					MAKE_UNICODE_LITERAL("Is Enabled"),
					(bool(Entity::HierarchyComponentBase::*)() const) & Entity::HierarchyComponentBase::IsEnabled,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"a72fb5e9-c075-43e6-9078-b7ffb3f09999"_guid, MAKE_UNICODE_LITERAL("Is Enabled")}
				},
				Function{
					"57c70714-3d55-4eeb-ab59-fe1a432cf25a"_guid,
					MAKE_UNICODE_LITERAL("Is Disabled"),
					(bool(Entity::HierarchyComponentBase::*)() const) & Entity::HierarchyComponentBase::IsDisabled,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"0690f524-8123-4d64-9378-7c44d645c347"_guid, MAKE_UNICODE_LITERAL("Is Disabled")}
				},
				Function{
					"fe7d4916-ed15-40dc-a566-ca5773e5ee8f"_guid,
					MAKE_UNICODE_LITERAL("Add Tag"),
					(void(Entity::HierarchyComponentBase::*)(Tag::Identifier)) & Entity::HierarchyComponentBase::AddTag,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"4361db83-9f23-4e62-b2c8-9442bf4c2dea"_guid, MAKE_UNICODE_LITERAL("Tag")},
				},
				Function{
					"5e0850f1-6c89-4158-8342-62219d753647"_guid,
					MAKE_UNICODE_LITERAL("Remove Tag"),
					(void(Entity::HierarchyComponentBase::*)(Tag::Identifier)) & Entity::HierarchyComponentBase::RemoveTag,
					FunctionFlags::VisibleToUI,
					Argument{},
					Argument{"1f272245-6b7d-4581-bcd3-f2c0452712cf"_guid, MAKE_UNICODE_LITERAL("Tag")},
				},
				Function{
					"97e01ed6-9d86-49cd-8b37-02b15c026ca2"_guid,
					MAKE_UNICODE_LITERAL("Has Tag"),
					(bool(Entity::HierarchyComponentBase::*)(Tag::Identifier) const) & Entity::HierarchyComponentBase::HasTag,
					FunctionFlags::VisibleToUI | FunctionFlags::IsPure,
					Argument{"08585945-db2a-4915-8fd5-34e559e40462"_guid, MAKE_UNICODE_LITERAL("Result")},
					Argument{"3f5d7914-10b8-4b2e-88c5-5be58f8e09d5"_guid, MAKE_UNICODE_LITERAL("Tag")}
				}
			}
		);
	};
}
