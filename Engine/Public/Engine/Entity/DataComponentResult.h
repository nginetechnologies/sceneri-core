#pragma once

#include <Common/Memory/Optional.h>

namespace ngine::Entity
{
	template<typename DataComponentType>
	struct DataComponentResult
	{
		[[nodiscard]] operator bool() const
		{
			return m_pDataComponent.IsValid();
		}
		[[nodiscard]] bool IsValid() const
		{
			return m_pDataComponent.IsValid();
		}
		[[nodiscard]] operator Optional<DataComponentType*>() const
		{
			return m_pDataComponent;
		}
		[[nodiscard]] Optional<DataComponentType*> operator->() const
		{
			return m_pDataComponent;
		}

		template<typename OtherDataComponentType>
		[[nodiscard]] explicit operator DataComponentResult<OtherDataComponentType>() const
		{
			return DataComponentResult<OtherDataComponentType>{
				static_cast<OtherDataComponentType*>(m_pDataComponent.Get()),
				static_cast<typename OtherDataComponentType::ParentType*>(m_pDataComponentOwner.Get()),
			};
		}

		Optional<DataComponentType*> m_pDataComponent;
		Optional<typename DataComponentType::ParentType*> m_pDataComponentOwner;
	};
}
