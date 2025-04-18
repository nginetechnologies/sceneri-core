#pragma once

#include "ForwardDeclarations/ComponentReference.h"
#include <Common/TypeTraits/IsBaseOf.h>
#include <Common/Memory/Optional.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Entity
{
	struct SceneRegistry;
	struct Component3D;

	//! Represents the reference type of a component
	//! When serialized from disk this finds the existing instance that was written and never spawns a new instance.
	template<typename ComponentType>
	struct ComponentReference
	{
		ComponentReference() = default;
		ComponentReference(const Optional<ComponentType*> pComponent)
			: m_pComponent(pComponent)
		{
		}
		ComponentReference(ComponentType& component)
			: m_pComponent(&component)
		{
		}
		ComponentReference(ComponentType* pComponent)
			: m_pComponent(pComponent)
		{
		}

		bool Serialize(Serialization::Writer serializer) const;
		bool Serialize(const Serialization::Reader reader, SceneRegistry& sceneRegistry);

		[[nodiscard]] ComponentType& operator*() const
		{
			return *m_pComponent;
		}

		[[nodiscard]] ComponentType* operator->() const noexcept
		{
			return m_pComponent;
		}

		[[nodiscard]] operator Optional<ComponentType*>() const
		{
			return m_pComponent;
		}

		[[nodiscard]] Optional<ComponentType*> Get() const
		{
			return m_pComponent;
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_pComponent != nullptr;
		}

		[[nodiscard]] bool operator==(const ComponentReference other) const
		{
			return m_pComponent == other.m_pComponent;
		}
		[[nodiscard]] bool operator!=(const ComponentReference other) const
		{
			return m_pComponent != other.m_pComponent;
		}
		[[nodiscard]] bool operator==(const ComponentType& other) const
		{
			return m_pComponent == Memory::GetAddressOf(other);
		}
		[[nodiscard]] bool operator!=(const ComponentType& other) const
		{
			return m_pComponent != Memory::GetAddressOf(other);
		}
	protected:
		Optional<ComponentType*> m_pComponent;
	};
}
