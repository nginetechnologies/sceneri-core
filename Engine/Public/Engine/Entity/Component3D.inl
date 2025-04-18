#pragma once

#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.h>

namespace ngine::Entity
{
	template<typename DataComponentType, typename... Args>
	inline Optional<DataComponentType*> Component3D::CreateDataComponent(Args&&... args)
	{
		return DataComponentOwner::CreateDataComponent<DataComponentType>(GetSceneRegistry(), Forward<Args>(args)...);
	}

	template<typename DataComponentType>
	inline bool Component3D::RemoveDataComponentOfType()
	{
		return DataComponentOwner::RemoveDataComponentOfType<DataComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	inline bool Component3D::RemoveFirstDataComponentImplementingType()
	{
		return DataComponentOwner::RemoveFirstDataComponentImplementingType<DataComponentType>(GetSceneRegistry());
	}

	template<typename Callback>
	inline void Component3D::IterateDataComponents(Callback&& callback) const
	{
		return DataComponentOwner::IterateDataComponents(GetSceneRegistry(), Forward<Callback>(callback));
	}

	template<typename DataComponentType>
	[[nodiscard]] Optional<DataComponentType*> Component3D::FindFirstDataComponentImplementingType() const
	{
		return DataComponentOwner::FindFirstDataComponentImplementingType<DataComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Optional<DataComponentType*> Component3D::FindDataComponentOfType() const
	{
		return DataComponentOwner::FindDataComponentOfType<DataComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	[[nodiscard]] inline bool Component3D::HasDataComponentOfType() const
	{
		return DataComponentOwner::HasDataComponentOfType<DataComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	[[nodiscard]] inline bool Component3D::HasAnyDataComponentsImplementingType() const
	{
		return DataComponentOwner::HasAnyDataComponentsImplementingType<DataComponentType>(GetSceneRegistry());
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Component3D::DataComponentResult<DataComponentType>
	Component3D::FindFirstDataComponentOfTypeInChildrenRecursive() const
	{
		return (Component3D::DataComponentResult<DataComponentType>)
			HierarchyComponent<Component3D>::FindFirstDataComponentOfTypeInChildrenRecursive<DataComponentType>(GetSceneRegistry());
	}
	template<typename DataComponentType>
	[[nodiscard]] inline Component3D::DataComponentResult<DataComponentType>
	Component3D::FindFirstDataComponentImplementingTypeInChildrenRecursive() const
	{
		return (Component3D::DataComponentResult<DataComponentType>)
			HierarchyComponent<Component3D>::FindFirstDataComponentImplementingTypeInChildrenRecursive<DataComponentType>(GetSceneRegistry());
	}
	template<typename DataComponentType>
	[[nodiscard]] inline Component3D::DataComponentResult<DataComponentType>
	Component3D::FindFirstDataComponentImplementingTypeInSelfAndChildrenRecursive() const
	{
		if (Optional<DataComponentType*> pDataComponent = DataComponentOwner::FindFirstDataComponentImplementingType<DataComponentType>(GetSceneRegistry()))
		{
			return Component3D::DataComponentResult<DataComponentType>{pDataComponent, const_cast<Component3D*>(this)};
		}
		else
		{
			return (Component3D::DataComponentResult<DataComponentType>)
				HierarchyComponent<Component3D>::FindFirstDataComponentImplementingTypeInChildrenRecursive<DataComponentType>(GetSceneRegistry());
		}
	}

	template<typename DataComponentType>
	[[nodiscard]] inline Component3D::DataComponentResult<DataComponentType> Component3D::FindFirstDataComponentOfTypeInParents() const
	{
		return (Component3D::DataComponentResult<DataComponentType>)
			HierarchyComponent<Component3D>::FindFirstDataComponentOfTypeInParents<DataComponentType>(GetSceneRegistry());
	}
	template<typename DataComponentType>
	[[nodiscard]] inline Component3D::DataComponentResult<DataComponentType>
	Component3D::FindFirstDataComponentImplementingTypeInParents() const
	{
		return (Component3D::DataComponentResult<DataComponentType>)
			HierarchyComponent<Component3D>::FindFirstDataComponentImplementingTypeInParents<DataComponentType>(GetSceneRegistry());
	}

	template<typename ChildComponentType, typename Callback>
	inline void Component3D::IterateDataComponentsOfTypeInChildrenRecursive(Callback&& callback) const
	{
		HierarchyComponent<Component3D>::IterateDataComponentsOfTypeInChildrenRecursive<ChildComponentType, Callback>(
			GetSceneRegistry(),
			Forward<Callback>(callback)
		);
	}
	template<typename ChildComponentType, typename Callback>
	inline void Component3D::IterateDataComponentsImplementingTypeInChildrenRecursive(Callback&& callback) const
	{
		HierarchyComponent<Component3D>::IterateDataComponentsImplementingTypeInChildrenRecursive<ChildComponentType, Callback>(
			GetSceneRegistry(),
			Forward<Callback>(callback)
		);
	}

	template<typename DataComponentType, typename Callback>
	inline void Component3D::IterateDataComponentsOfTypeInParentsRecursive(Callback&& callback) const
	{
		HierarchyComponent<Component3D>::IterateDataComponentsOfTypeInParentsRecursive<DataComponentType, Callback>(
			GetSceneRegistry(),
			Forward<Callback>(callback)
		);
	}

	template<typename DataComponentType, typename Callback>
	inline void Component3D::IterateDataComponentsImplementingTypeInParentsRecursive(Callback&& callback) const
	{
		HierarchyComponent<Component3D>::IterateDataComponentsImplementingTypeInParentsRecursive<DataComponentType, Callback>(
			GetSceneRegistry(),
			Forward<Callback>(callback)
		);
	}
}
