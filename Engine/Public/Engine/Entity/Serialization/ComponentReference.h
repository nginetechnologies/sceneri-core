#pragma once

#include "../ComponentReference.h"
#include "../ComponentSoftReference.inl"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

namespace ngine::Entity
{
	template<typename ComponentType>
	inline bool ComponentReference<ComponentType>::Serialize(Serialization::Writer serializer) const
	{
		Optional<ComponentType*> pComponent = m_pComponent;
		if (pComponent.IsInvalid())
		{
			return false;
		}

		const SceneRegistry& sceneRegistry = pComponent->GetSceneRegistry();
		ComponentSoftReference softComponentReference = {*pComponent, sceneRegistry};
		return softComponentReference.Serialize(serializer);
	}

	template<typename ComponentType>
	inline bool ComponentReference<ComponentType>::Serialize(const Serialization::Reader reader, SceneRegistry& sceneRegistry)
	{
		ComponentSoftReference softComponentReference;
		if (UNLIKELY(!softComponentReference.Serialize(reader, sceneRegistry)))
		{
			return false;
		}

		const Optional<ComponentType*> pComponent = softComponentReference.Find<ComponentType>(sceneRegistry);
		m_pComponent = pComponent;
		return pComponent.IsValid();
	}
}
