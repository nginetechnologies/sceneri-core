#pragma once

#include <Engine/Entity/ForwardDeclarations/ComponentSoftReference.h>
#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/ComponentInstanceIdentifier.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>

#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Memory/Containers/ForwardDeclarations/BitView.h>

namespace ngine::Entity
{
	struct Component;
	struct ComponentRegistry;
	struct SceneRegistry;

	//! Represents a soft reference to an existing component in the scene
	//! No pointers are stored so this can be safely used to avoid keeping a reference to a dangling component
	struct ComponentSoftReference
	{
		using InstanceIdentifier = GenericComponentInstanceIdentifier;
		struct Instance
		{
			[[nodiscard]] bool IsPotentiallyValid() const
			{
				return m_identifier.IsValid() | m_guid.IsValid();
			}

			Instance() = default;
			Instance(const InstanceIdentifier identifier)
				: m_identifier(identifier)
			{
			}
			Instance(const Guid guid)
				: m_guid(guid)
			{
			}
			Instance(const InstanceIdentifier identifier, const Guid guid)
				: m_identifier(identifier)
				, m_guid(guid)
			{
			}
			Instance(
				const Instance& other,
				const SceneRegistry& otherSceneRegistry,
				const SceneRegistry& thisSceneRegistry,
				const ComponentTypeIdentifier typeIdentifier
			);

			[[nodiscard]] bool operator==(const Instance other) const
			{
				return (m_identifier == other.m_identifier && m_identifier.IsValid()) | (m_guid == other.m_guid && m_guid.IsValid());
			}
			[[nodiscard]] bool operator!=(const Instance other) const
			{
				return !operator==(other);
			}

			bool Serialize(Serialization::Writer serializer) const;
			bool Serialize(
				const Serialization::Reader reader,
				ComponentRegistry& componentRegistry,
				SceneRegistry& sceneRegistry,
				ComponentTypeIdentifier typeIdentifier
			);

			[[nodiscard]] Optional<Component*> Find(const SceneRegistry& sceneRegistry, const ComponentTypeIdentifier typeIdentifier) const;

			mutable InstanceIdentifier m_identifier;
			Guid m_guid;
		};

		ComponentSoftReference() = default;
		template<typename ComponentType>
		ComponentSoftReference(const ComponentTypeIdentifier typeIdentifier, const ComponentType&, const SceneRegistry& sceneRegistry);
		template<typename ComponentType>
		ComponentSoftReference(const ComponentType&, const SceneRegistry& sceneRegistry);
		template<typename ComponentType>
		ComponentSoftReference(
			const ComponentTypeIdentifier typeIdentifier, const Optional<ComponentType*>, const SceneRegistry& sceneRegistry
		);
		template<typename ComponentType>
		ComponentSoftReference(const Optional<ComponentType*>, const SceneRegistry& sceneRegistry);
		ComponentSoftReference(const ComponentTypeIdentifier typeIdentifier, const Instance instance)
			: m_typeIdentifier(typeIdentifier)
			, m_instance{instance}
		{
		}

		struct Cloner
		{
			const SceneRegistry& templateSceneRegistry;
			const SceneRegistry& sceneRegistry;
		};
		ComponentSoftReference(const ComponentSoftReference& templateReference, const Cloner& cloner);

		[[nodiscard]] ComponentTypeIdentifier GetTypeIdentifier() const
		{
			return m_typeIdentifier;
		}
		[[nodiscard]] Instance GetInstance() const
		{
			return m_instance;
		}
		[[nodiscard]] InstanceIdentifier GetInstanceIdentifier() const
		{
			return m_instance.m_identifier;
		}
		[[nodiscard]] Guid GetInstanceGuid() const
		{
			return m_instance.m_guid;
		}

		//! Checks whether the identifiers stored within this soft reference are valid
		//! This does *not* check if the component still exists!
		//! To check that, use the Find method.
		[[nodiscard]] bool IsPotentiallyValid() const
		{
			return m_typeIdentifier.IsValid() & m_instance.IsPotentiallyValid();
		}
		[[nodiscard]] Optional<Component*> Find(const SceneRegistry& sceneRegistry) const
		{
			return m_typeIdentifier.IsValid() ? m_instance.Find(sceneRegistry, m_typeIdentifier) : Optional<Component*>{};
		}
		template<typename ComponentType>
		[[nodiscard]] Optional<ComponentType*> Find(const SceneRegistry& sceneRegistry) const;

		bool Serialize(Serialization::Writer serializer) const;
		bool Serialize(const Serialization::Reader reader, SceneRegistry& sceneRegistry);

		[[nodiscard]] static constexpr uint32 CalculateCompressedDataSize()
		{
			// Worst case if neither instance nor type is bound to network we'll have to send the two guids.
			return sizeof(Guid) * 2 * 8;
		}
		bool Compress(BitView& target) const;
		bool Decompress(ConstBitView& source);

		void Reset()
		{
			m_instance = {};
			m_typeIdentifier = Invalid;
		}

		[[nodiscard]] bool operator==(const ComponentSoftReference other) const
		{
			return (m_typeIdentifier == other.m_typeIdentifier) & (m_instance == other.m_instance);
		}
		[[nodiscard]] bool operator!=(const ComponentSoftReference other) const
		{
			return !operator==(other);
		}
	protected:
		ComponentTypeIdentifier m_typeIdentifier;
		Instance m_instance;
	};
}
