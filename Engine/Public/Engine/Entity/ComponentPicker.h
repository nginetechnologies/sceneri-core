#pragma once

#include "ComponentSoftReference.h"
#include "Component3D.h"

namespace ngine::Entity
{
	struct ComponentPicker : public ComponentSoftReference
	{
		using BaseType = ComponentSoftReference;

		ComponentPicker() = delete;
		ComponentPicker(const ComponentPicker&) = default;
		ComponentPicker(ComponentPicker&&) = default;
		ComponentPicker& operator=(const ComponentPicker& other)
		{
			BaseType::operator=(other);
			m_pSceneRegistry = other.m_pSceneRegistry;
			return *this;
		}
		ComponentPicker& operator=(ComponentPicker&& other)
		{
			BaseType::operator=(other);
			m_pSceneRegistry = other.m_pSceneRegistry;
			return *this;
		}

		ComponentPicker& operator=(const InvalidType)
		{
			static_cast<BaseType&>(*this) = {};
			m_pSceneRegistry = Invalid;
			return *this;
		}

		ComponentPicker(const InvalidType)
			: BaseType()
			, m_pSceneRegistry(Invalid)
		{
		}

		ComponentPicker(SceneRegistry& sceneRegistry)
			: BaseType()
			, m_pSceneRegistry(&sceneRegistry)
		{
		}

		template<typename ComponentType>
		ComponentPicker(const ComponentPicker& other, SceneRegistry& sceneRegistry)
			: BaseType(ComponentSoftReference(other, *other.m_pSceneRegistry).Find<ComponentType>(sceneRegistry))
			, m_pSceneRegistry(&sceneRegistry)
		{
		}

		template<typename ComponentType>
		ComponentPicker(const Optional<ComponentType*> pComponent, SceneRegistry& sceneRegistry)
			: BaseType(pComponent)
			, m_pSceneRegistry(&sceneRegistry)
		{
		}
		template<typename ComponentType>
		ComponentPicker(ComponentType& component, SceneRegistry& sceneRegistry)
			: BaseType(&component, sceneRegistry)
			, m_pSceneRegistry(&sceneRegistry)
		{
		}
		template<typename ComponentType>
		ComponentPicker(ComponentType* pComponent, SceneRegistry& sceneRegistry)
			: BaseType(pComponent)
			, m_pSceneRegistry(&sceneRegistry)
		{
		}
		ComponentPicker(const ComponentSoftReference reference, SceneRegistry& sceneRegistry)
			: BaseType(reference)
			, m_pSceneRegistry(&sceneRegistry)
		{
		}
		using BaseType::operator=;

		template<typename ComponentType>
		[[nodiscard]] Optional<ComponentType*> Find() const
		{
			if (m_pSceneRegistry.IsValid())
			{
				return BaseType::Find<ComponentType>(*m_pSceneRegistry);
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] Optional<SceneRegistry*> GetSceneRegistry() const
		{
			return m_pSceneRegistry;
		}

		using BaseType::Serialize;
		bool Serialize(const Serialization::Reader reader);
		bool Serialize(const Serialization::Writer writer) const;
	protected:
		Optional<SceneRegistry*> m_pSceneRegistry;
	};

	struct Component3DPicker : public ComponentPicker
	{
		inline static constexpr Guid TypeGuid = "bc254e0a-d3fd-4550-8231-2d1f13c7c1de"_guid;

		using BaseType = ComponentPicker;
		using BaseType::BaseType;
		using BaseType::operator=;

		Component3DPicker(const ArrayView<const Guid> allowedComponentTypeGuids)
			: BaseType(Invalid)
			, m_allowedComponentTypeGuids(allowedComponentTypeGuids)
		{
		}

		[[nodiscard]] ArrayView<const Guid> GetAllowedComponentTypeGuids() const
		{
			return m_allowedComponentTypeGuids.GetView();
		}

		void SetAllowedComponentTypeGuids(const ArrayView<const Guid> guids)
		{
			m_allowedComponentTypeGuids = guids;
		}
	private:
		InlineVector<Guid, 2> m_allowedComponentTypeGuids;
	};
}
