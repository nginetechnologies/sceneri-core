#pragma once

#include "HierarchyComponentBase.h"

#include <Common/TypeTraits/IsAbstract.h>
#include <Common/Reflection/Registry.h>
#include <Common/System/Query.h>
#include <Engine/Entity/Manager.h>

#include <Engine/Entity/ComponentTypeSceneData.h>

namespace ngine::Entity
{
	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindFirstParentOfType(SceneRegistry& sceneRegistry) const
	{
		static_assert(
			!TypeTraits::IsAbstract<ParentComponentType>,
			"Can't get an abstract component! Consider using FindFirstParentImplementingType()"
		);
		static_assert(TypeTraits::IsBaseOf<ParentType, ParentComponentType>);
		Optional<HierarchyComponentBase*> pParent = GetParentSafe();
		if (pParent.IsInvalid())
		{
			return Invalid;
		}

		while (pParent.IsValid())
		{
			if (pParent->template Is<ParentComponentType>(sceneRegistry))
			{
				return static_cast<ParentComponentType&>(*pParent);
			}
			pParent = pParent->GetParentSafe();
		}

		if (pParent.IsValid() && pParent->template Is<ParentComponentType>(sceneRegistry))
		{
			return static_cast<ParentComponentType&>(*pParent);
		}
		return Invalid;
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindFirstParentOfType() const
	{
		return FindFirstParentOfType<ParentComponentType>(GetSceneRegistry());
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindLastParentOfType(SceneRegistry& sceneRegistry) const
	{
		static_assert(
			!TypeTraits::IsAbstract<ParentComponentType>,
			"Can't get an abstract component! Consider using FindFirstParentImplementingType()"
		);
		static_assert(TypeTraits::IsBaseOf<ParentType, ParentComponentType>);
		Optional<HierarchyComponentBase*> pParent = GetParentSafe();
		if (pParent.IsInvalid())
		{
			return Invalid;
		}

		Optional<ParentComponentType*> pMatch;
		while (pParent.IsValid())
		{
			if (pParent->template Is<ParentComponentType>(sceneRegistry))
			{
				pMatch = static_cast<ParentComponentType&>(*pParent);
			}
			pParent = pParent->GetParentSafe();
		}

		if (pParent.IsValid() && pParent->template Is<ParentComponentType>(sceneRegistry))
		{
			pMatch = static_cast<ParentComponentType&>(*pParent);
		}
		return pMatch;
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindLastParentOfType() const
	{
		return FindLastParentOfType<ParentComponentType>(GetSceneRegistry());
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildOfType(SceneRegistry& sceneRegistry) const
	{
		static_assert(
			!TypeTraits::IsAbstract<ChildComponentType>,
			"Can't get an abstract component! Consider using FindFirstChildImplementingType()"
		);
		static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
		for (HierarchyComponentBase& childComponent : GetChildren())
		{
			if (childComponent.template Is<ChildComponentType>(sceneRegistry))
			{
				return static_cast<ChildComponentType&>(childComponent);
			}
		}

		return Invalid;
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildOfType() const
	{
		return FindFirstChildOfType<ChildComponentType>(GetSceneRegistry());
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildOfTypeRecursive(SceneRegistry& sceneRegistry) const
	{
		static_assert(
			!TypeTraits::IsAbstract<ChildComponentType>,
			"Can't get an abstract component! Consider using FindFirstChildInHierarchyImplementingType()"
		);
		static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
		using TraverseChildren = Optional<ChildComponentType*> (*)(SceneRegistry& sceneRegistry, HierarchyComponentBase& component);
		static TraverseChildren traverseChildren =
			[](SceneRegistry& sceneRegistry, HierarchyComponentBase& component) -> Optional<ChildComponentType*>
		{
			if (component.template Is<ChildComponentType>(sceneRegistry))
			{
				return static_cast<ChildComponentType&>(component);
			}
			else
			{
				for (HierarchyComponentBase& childComponent : component.GetChildren())
				{
					Optional<ChildComponentType*> pFoundComponent = traverseChildren(sceneRegistry, childComponent);
					if (pFoundComponent.IsValid())
					{
						return pFoundComponent;
					}
				}
			}

			return Invalid;
		};

		for (HierarchyComponentBase& childComponent : GetChildren())
		{
			Optional<ChildComponentType*> pFoundComponent = traverseChildren(sceneRegistry, childComponent);
			if (pFoundComponent.IsValid())
			{
				return pFoundComponent;
			}
		}

		return Invalid;
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildOfTypeRecursive() const
	{
		return FindFirstChildOfTypeRecursive<ChildComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType> HierarchyComponentBase::FindFirstDataComponentOfTypeInChildren(SceneRegistry& sceneRegistry
	) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				if (const Optional<DataComponentType*> pDataComponent = childComponent.FindDataComponentOfType<DataComponentType>(*dataComponentSceneData))
				{
					return DataComponentResult<DataComponentType>{
						pDataComponent,
						childComponent.AsExpected<typename DataComponentType::ParentType>(sceneRegistry)
					};
				}
			}
		}

		return {};
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentImplementingTypeInChildren(SceneRegistry& sceneRegistry) const
	{
		for (HierarchyComponentBase& childComponent : GetChildren())
		{
			if (Optional<DataComponentType*> pDataComponent = childComponent.template FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry))
			{
				return DataComponentResult<DataComponentType>{pDataComponent, childComponent.AsExpected<typename DataComponentType::ParentType>(sceneRegistry)};
			}
		}
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentOfTypeInChildrenRecursive(SceneRegistry& sceneRegistry) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			using TraverseChildren = DataComponentResult<DataComponentType> (*)(
				HierarchyComponentBase& component,
				SceneRegistry& sceneRegistry,
				ComponentTypeSceneData<DataComponentType>& dataComponentSceneData
			);
			static TraverseChildren traverseChildren =
				[](HierarchyComponentBase& component, SceneRegistry& sceneRegistry, ComponentTypeSceneData<DataComponentType>& dataComponentSceneData)
				-> DataComponentResult<DataComponentType>
			{
				if (Optional<DataComponentType*> pDataComponent = component.template FindDataComponentOfType<DataComponentType>(dataComponentSceneData))
				{
					return DataComponentResult<DataComponentType>{pDataComponent, component.AsExpected<typename DataComponentType::ParentType>(sceneRegistry)};
				}
				else
				{
					for (HierarchyComponentBase& childComponent : component.GetChildren())
					{
						if (DataComponentResult<DataComponentType> result = traverseChildren(childComponent, sceneRegistry, dataComponentSceneData))
						{
							return result;
						}
					}
				}

				return {};
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				if (DataComponentResult<DataComponentType> result = traverseChildren(childComponent, sceneRegistry, *dataComponentSceneData))
				{
					return result;
				}
			}
		}

		return {};
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentOfTypeInSelfAndChildrenRecursive(SceneRegistry& sceneRegistry) const
	{
		if (Optional<DataComponentType*> pDataComponent = DataComponentOwner::FindDataComponentOfType<DataComponentType>(sceneRegistry))
		{
			return DataComponentResult<DataComponentType>{
				pDataComponent,
				const_cast<HierarchyComponentBase&>(*this).AsExpected<typename DataComponentType::ParentType>(sceneRegistry)
			};
		}
		else
		{
			return (DataComponentResult<DataComponentType>)FindFirstDataComponentOfTypeInChildrenRecursive<DataComponentType>(sceneRegistry);
		}
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType> HierarchyComponentBase::FindFirstDataComponentOfTypeInParents(SceneRegistry& sceneRegistry
	) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return {};
			}

			while (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindDataComponentOfType<DataComponentType>(*dataComponentSceneData))
				{
					return DataComponentResult<DataComponentType>{pDataComponent, pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)};
				}
				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindDataComponentOfType<DataComponentType>(*dataComponentSceneData))
				{
					return DataComponentResult<DataComponentType>{pDataComponent, pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)};
				}
			}
		}

		return {};
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenOfType(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		static_assert(!TypeTraits::IsAbstract<ChildComponentType>, "Type is abstract! Consider using IterateChildrenImplementingType()");
		static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
		for (HierarchyComponentBase& childComponent : GetChildren())
		{
			if (childComponent.template Is<ChildComponentType>(sceneRegistry))
			{
				switch (callback(static_cast<ChildComponentType&>(childComponent)))
				{
					case Memory::CallbackResult::Break:
						return;
					case Memory::CallbackResult::Continue:
						break;
				}
			}
		}
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenOfType(Callback&& callback) const
	{
		IterateChildrenOfType<ChildComponentType, Callback>(GetSceneRegistry(), Forward<Callback>(callback));
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenOfTypeRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		static_assert(!TypeTraits::IsAbstract<ChildComponentType>, "Type is abstract! Consider using IterateChildrenImplementingType()");
		static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
		using TraverseChildren =
			Memory::CallbackResult (*)(SceneRegistry& sceneRegistry, HierarchyComponentBase& component, Callback& callback);
		static TraverseChildren traverseChildren =
			[](SceneRegistry& sceneRegistry, HierarchyComponentBase& component, Callback& callback) mutable
		{
			if (component.template Is<ChildComponentType>(sceneRegistry))
			{
				switch (callback(static_cast<ChildComponentType&>(component)))
				{
					case Memory::CallbackResult::Break:
						return Memory::CallbackResult::Break;
					case Memory::CallbackResult::Continue:
						break;
				}
			}

			for (HierarchyComponentBase& childComponent : component.GetChildren())
			{
				switch (traverseChildren(sceneRegistry, childComponent, callback))
				{
					case Memory::CallbackResult::Break:
						return Memory::CallbackResult::Break;
					case Memory::CallbackResult::Continue:
						break;
				}
			}

			return Memory::CallbackResult::Continue;
		};

		for (HierarchyComponentBase& childComponent : GetChildren())
		{
			switch (traverseChildren(sceneRegistry, childComponent, callback))
			{
				case Memory::CallbackResult::Break:
					return;
				case Memory::CallbackResult::Continue:
					break;
			}
		}
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenOfTypeRecursive(Callback&& callback) const
	{
		IterateChildrenOfTypeRecursive<ChildComponentType, Callback>(GetSceneRegistry(), Forward<Callback>(callback));
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindFirstParentImplementingType(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<ParentComponentType>)
		{
			return FindFirstParentOfType<ParentComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ParentType, ParentComponentType>);
			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return Invalid;
			}

			while (pParent.IsValid())
			{
				if (pParent->template Implements<ParentComponentType>(sceneRegistry))
				{
					return static_cast<ParentComponentType&>(*pParent);
				}
				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid() && pParent->template Implements<ParentComponentType>(sceneRegistry))
			{
				return static_cast<ParentComponentType&>(*pParent);
			}

			return Invalid;
		}
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindFirstParentImplementingType() const
	{
		return FindFirstParentImplementingType<ParentComponentType>(GetSceneRegistry());
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindLastParentImplementingType(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<ParentComponentType>)
		{
			return FindLastParentOfType<ParentComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ParentType, ParentComponentType>);
			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return Invalid;
			}

			Optional<ParentComponentType*> pMatch;
			while (pParent.IsValid())
			{
				if (pParent->template Implements<ParentComponentType>(sceneRegistry))
				{
					pMatch = static_cast<ParentComponentType&>(*pParent);
				}
				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid() && pParent->template Implements<ParentComponentType>(sceneRegistry))
			{
				pMatch = static_cast<ParentComponentType&>(*pParent);
			}

			return pMatch;
		}
	}

	template<typename ParentComponentType>
	inline Optional<ParentComponentType*> HierarchyComponentBase::FindLastParentImplementingType() const
	{
		return FindLastParentImplementingType<ParentComponentType>(GetSceneRegistry());
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildImplementingType(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<ChildComponentType>)
		{
			return FindFirstChildOfType<ChildComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				if (childComponent.template Implements<ChildComponentType>(sceneRegistry))
				{
					return static_cast<ChildComponentType&>(childComponent);
				}
			}

			return Invalid;
		}
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildImplementingType() const
	{
		return FindFirstChildImplementingType<ChildComponentType>(GetSceneRegistry());
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildImplementingTypeRecursive(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<ChildComponentType>)
		{
			return FindFirstChildOfTypeRecursive<ChildComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
			using TraverseChildren = Optional<ChildComponentType*> (*)(SceneRegistry& sceneRegistry, HierarchyComponentBase& component);
			static TraverseChildren traverseChildren =
				[](SceneRegistry& sceneRegistry, HierarchyComponentBase& component) -> Optional<ChildComponentType*>
			{
				if (component.template Implements<ChildComponentType>(sceneRegistry))
				{
					return static_cast<ChildComponentType&>(component);
				}
				else
				{
					for (HierarchyComponentBase& childComponent : component.GetChildren())
					{
						Optional<ChildComponentType*> pFoundComponent = traverseChildren(sceneRegistry, childComponent);
						if (pFoundComponent.IsValid())
						{
							return pFoundComponent;
						}
					}
				}

				return Invalid;
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				Optional<ChildComponentType*> pFoundComponent = traverseChildren(sceneRegistry, childComponent);
				if (pFoundComponent.IsValid())
				{
					return pFoundComponent;
				}
			}

			return Invalid;
		}
	}

	template<typename ChildComponentType>
	inline Optional<ChildComponentType*> HierarchyComponentBase::FindFirstChildImplementingTypeRecursive() const
	{
		return FindFirstChildImplementingTypeRecursive<ChildComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentImplementingTypeInChildrenRecursive(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return FindFirstDataComponentOfTypeInChildrenRecursive<DataComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
			using TraverseChildren =
				DataComponentResult<DataComponentType> (*)(HierarchyComponentBase& component, const SceneRegistry& sceneRegistry);
			static TraverseChildren traverseChildren =
				[](HierarchyComponentBase& component, const SceneRegistry& sceneRegistry) -> DataComponentResult<DataComponentType>
			{
				if (Optional<DataComponentType*> pDataComponent = component.template FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry))
				{
					return DataComponentResult<DataComponentType>{pDataComponent, component.AsExpected<typename DataComponentType::ParentType>(sceneRegistry)};
				}
				else
				{
					for (HierarchyComponentBase& childComponent : component.GetChildren())
					{
						if (DataComponentResult<DataComponentType> result = traverseChildren(childComponent, sceneRegistry))
						{
							return result;
						}
					}
				}

				return {};
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				if (DataComponentResult<DataComponentType> result = traverseChildren(childComponent, sceneRegistry))
				{
					return result;
				}
			}

			return {};
		}
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentImplementingTypeInSelfAndChildrenRecursive(SceneRegistry& sceneRegistry) const
	{
		if (Optional<DataComponentType*> pDataComponent = DataComponentOwner::FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry))
		{
			return DataComponentResult<DataComponentType>{
				pDataComponent,
				const_cast<HierarchyComponentBase&>(*this).AsExpected<typename DataComponentType::ParentType>(sceneRegistry)
			};
		}
		else
		{
			return FindFirstDataComponentImplementingTypeInChildrenRecursive<DataComponentType>(sceneRegistry);
		}
	}

	template<typename DataComponentType>
	inline DataComponentResult<DataComponentType>
	HierarchyComponentBase::FindFirstDataComponentImplementingTypeInParents(SceneRegistry& sceneRegistry) const
	{
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return FindFirstDataComponentOfTypeInParents<DataComponentType>(sceneRegistry);
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
			constexpr Guid typeGuid = Reflection::GetTypeGuid<DataComponentType>();

			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return {};
			}

			while (pParent.IsValid())
			{
				if (Optional<Data::Component*> pDataComponent = pParent->FindFirstDataComponentImplementingType(sceneRegistry, typeGuid))
				{
					return DataComponentResult<DataComponentType>{
						static_cast<DataComponentType&>(*pDataComponent),
						pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)
					};
				}
				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid())
			{
				if (Optional<Data::Component*> pDataComponent = pParent->FindFirstDataComponentImplementingType(sceneRegistry, typeGuid))
				{
					return DataComponentResult<DataComponentType>{
						static_cast<DataComponentType&>(*pDataComponent),
						pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)
					};
				}
			}

			return {};
		}
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenImplementingType(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		if constexpr (TypeTraits::IsFinal<ChildComponentType>)
		{
			return IterateChildrenOfType<ChildComponentType>(sceneRegistry, Forward<Callback>(callback));
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				if (childComponent.template Implements<ChildComponentType>(sceneRegistry))
				{
					switch (callback(static_cast<ChildComponentType&>(childComponent)))
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

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenImplementingType(Callback&& callback) const
	{
		IterateChildrenImplementingType<ChildComponentType, Callback>(GetSceneRegistry(), Forward<Callback>(callback));
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenImplementingTypeRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		if constexpr (TypeTraits::IsFinal<ChildComponentType>)
		{
			return IterateChildrenOfTypeRecursive<ChildComponentType>(sceneRegistry, Forward<Callback>(callback));
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<ChildType, ChildComponentType>);
			using TraverseChildren =
				Memory::CallbackResult (*)(SceneRegistry& sceneRegistry, HierarchyComponentBase& component, Callback& callback);
			static TraverseChildren traverseChildren =
				[](SceneRegistry& sceneRegistry, HierarchyComponentBase& component, Callback& callback) mutable
			{
				if (component.template Implements<ChildComponentType>(sceneRegistry))
				{
					switch (callback(static_cast<ChildComponentType&>(component)))
					{
						case Memory::CallbackResult::Break:
							return Memory::CallbackResult::Break;
						case Memory::CallbackResult::Continue:
							break;
					}
				}

				for (HierarchyComponentBase& childComponent : component.GetChildren())
				{
					switch (traverseChildren(sceneRegistry, childComponent, callback))
					{
						case Memory::CallbackResult::Break:
							return Memory::CallbackResult::Break;
						case Memory::CallbackResult::Continue:
							break;
					}
				}

				return Memory::CallbackResult::Continue;
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				switch (traverseChildren(sceneRegistry, childComponent, callback))
				{
					case Memory::CallbackResult::Break:
						return;
					case Memory::CallbackResult::Continue:
						break;
				}
			}
		}
	}

	template<typename ChildComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateChildrenImplementingTypeRecursive(Callback&& callback) const
	{
		IterateChildrenImplementingTypeRecursive<ChildComponentType, Callback>(GetSceneRegistry(), Forward<Callback>(callback));
	}

	template<typename DataComponentType, typename Callback>
	inline void
	HierarchyComponentBase::IterateDataComponentsOfTypeInChildrenRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			static_assert(
				!TypeTraits::IsAbstract<DataComponentType>,
				"Type is abstract! Consider using IterateDataComponentsImplementingTypeInChildrenRecursive()"
			);
			using TraverseChildren = Memory::CallbackResult (*)(
				HierarchyComponentBase& component,
				ComponentTypeSceneData<DataComponentType>& dataComponentSceneData,
				Callback& callback
			);
			static TraverseChildren traverseChildren =
				[](HierarchyComponentBase& component, ComponentTypeSceneData<DataComponentType>& dataComponentSceneData, Callback& callback) mutable
			{
				if (const Optional<DataComponentType*> pDataComponent = component.template FindDataComponentOfType<DataComponentType>(dataComponentSceneData))
				{
					switch (callback(component.AsExpected<typename DataComponentType::ParentType>(), *pDataComponent))
					{
						case Memory::CallbackResult::Break:
							return Memory::CallbackResult::Break;
						case Memory::CallbackResult::Continue:
							break;
					}
				}

				for (HierarchyComponentBase& childComponent : component.GetChildren())
				{
					switch (traverseChildren(childComponent, dataComponentSceneData, callback))
					{
						case Memory::CallbackResult::Break:
							return Memory::CallbackResult::Break;
						case Memory::CallbackResult::Continue:
							break;
					}
				}

				return Memory::CallbackResult::Continue;
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				switch (traverseChildren(childComponent, *dataComponentSceneData, callback))
				{
					case Memory::CallbackResult::Break:
						return;
					case Memory::CallbackResult::Continue:
						break;
				}
			}
		}
	}

	template<typename DataComponentType, typename Callback>
	inline void
	HierarchyComponentBase::IterateDataComponentsImplementingTypeInChildrenRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return IterateDataComponentsOfTypeInChildrenRecursive<DataComponentType>(sceneRegistry, Forward<Callback>(callback));
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
			using TraverseChildren =
				Memory::CallbackResult (*)(HierarchyComponentBase& component, const SceneRegistry& sceneRegistry, Callback& callback);
			static TraverseChildren traverseChildren =
				[](HierarchyComponentBase& component, const SceneRegistry& sceneRegistry, Callback& callback)
			{
				if (Optional<DataComponentType*> pDataComponent = component.template FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry))
				{
					switch (callback(component.AsExpected<typename DataComponentType::ParentType>(sceneRegistry), *pDataComponent))
					{
						case Memory::CallbackResult::Break:
							return Memory::CallbackResult::Break;
						case Memory::CallbackResult::Continue:
							break;
					}
				}
				else
				{
					for (HierarchyComponentBase& childComponent : component.GetChildren())
					{
						switch (traverseChildren(childComponent, sceneRegistry, callback))
						{
							case Memory::CallbackResult::Break:
								return Memory::CallbackResult::Break;
							case Memory::CallbackResult::Continue:
								break;
						}
					}
				}

				return Memory::CallbackResult::Continue;
			};

			for (HierarchyComponentBase& childComponent : GetChildren())
			{
				switch (traverseChildren(childComponent, sceneRegistry, callback))
				{
					case Memory::CallbackResult::Break:
						return;
					case Memory::CallbackResult::Continue:
						break;
				}
			}
		}
	}

	template<typename DataComponentType, typename Callback>
	inline void HierarchyComponentBase::IterateDataComponentsOfTypeInParentsRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
		if (Optional<ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>())
		{
			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return;
			}
			while (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindDataComponentOfType<DataComponentType>(*dataComponentSceneData))
				{
					switch (
						callback(DataComponentResult<DataComponentType>{pDataComponent, pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)})
					)
					{
						case Memory::CallbackResult::Break:
							return;
						case Memory::CallbackResult::Continue:
							break;
					}
				}

				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindDataComponentOfType<DataComponentType>(*dataComponentSceneData))
				{
					callback(DataComponentResult<DataComponentType>{pDataComponent, pParent->AsExpected<typename DataComponentType::ParentType>(sceneRegistry)});
				}
			}
		}
	}

	template<typename DataComponentType, typename Callback>
	inline void
	HierarchyComponentBase::IterateDataComponentsImplementingTypeInParentsRecursive(SceneRegistry& sceneRegistry, Callback&& callback) const
	{
		if constexpr (TypeTraits::IsFinal<DataComponentType>)
		{
			return IterateDataComponentsOfTypeInParentsRecursive<DataComponentType>(sceneRegistry, Forward<Callback>(callback));
		}
		else
		{
			static_assert(TypeTraits::IsBaseOf<Data::Component, DataComponentType>);
			constexpr Guid typeGuid = Reflection::GetTypeGuid<DataComponentType>();

			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			if (pParent.IsInvalid())
			{
				return;
			}
			while (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry, typeGuid))
				{
					switch (callback(DataComponentResult<DataComponentType>{pDataComponent, *pParent}))
					{
						case Memory::CallbackResult::Break:
							return;
						case Memory::CallbackResult::Continue:
							break;
					}
				}
				pParent = pParent->GetParentSafe();
			}

			if (pParent.IsValid())
			{
				if (Optional<DataComponentType*> pDataComponent = pParent->template FindFirstDataComponentImplementingType<DataComponentType>(sceneRegistry, typeGuid))
				{
					callback(DataComponentResult<DataComponentType>{pDataComponent, *pParent});
				}
			}
		}
	}
}
