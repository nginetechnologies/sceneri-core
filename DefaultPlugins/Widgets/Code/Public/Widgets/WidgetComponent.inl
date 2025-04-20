#pragma once

#include <Engine/Entity/ComponentTypeSceneData.h>

namespace ngine::Widgets
{
	template<typename DataComponentType, typename DataComponentSceneDataType, typename... Args>
	Optional<DataComponentType*> Widget::CreateDataComponent(DataComponentSceneDataType& sceneData, Args&&... args)
	{
		return sceneData.CreateInstance(GetIdentifier(), Forward<Args>(args)...);
	}

	template<typename DataComponentType, typename... Args>
	Optional<DataComponentType*> Widget::CreateDataComponent(Args&&... args)
	{
		Entity::ComponentTypeSceneData<DataComponentType>& dataComponentSceneData =
			m_owningWindow->GetSceneManager().GetOrCreateComponentTypeData<DataComponentType>();
		return CreateDataComponent<DataComponentType>(dataComponentSceneData, Forward<Args>(args)...);
	}

	template<typename DataComponentType, typename DataComponentSceneDataType>
	[[nodiscard]] Optional<DataComponentType*> Widget::FindDataComponentOfType(DataComponentSceneDataType& sceneData) const
	{
		return sceneData.GetComponentImplementation(GetIdentifier());
	}

	template<typename DataComponentType>
	[[nodiscard]] Optional<DataComponentType*> Widget::FindDataComponentOfType() const
	{
		if (Optional<Entity::ComponentTypeSceneData<DataComponentType>*> dataComponentSceneData = m_owningWindow->GetSceneManager().FindComponentTypeData<DataComponentType>())
		{
			return FindDataComponentOfType<DataComponentType>(*dataComponentSceneData.Get());
		}

		return Invalid;
	}
}
