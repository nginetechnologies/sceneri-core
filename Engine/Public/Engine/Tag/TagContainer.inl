#pragma once

#include <Engine/Tag/TagRegistry.h>

namespace ngine::Tag
{
	template<typename OtherIdentifierType>
	void AtomicMaskContainer<OtherIdentifierType>::Destroy(const Registry& registry)
	{
		for (AtomicElementMask* pTag : registry.GetValidElementView(m_tags.GetView()))
		{
			if (pTag != nullptr)
			{
				delete pTag;
			}
		}
	}
}
