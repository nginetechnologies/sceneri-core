#pragma once

#include "ComponentSoftReference.h"

#include <Common/Memory/CountBits.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Variant.h>

namespace ngine::Entity
{
	struct ComponentSoftReferences
	{
		inline static constexpr uint32 MaximumCompressedTypeCount = 50;
		inline static constexpr auto TypeCountBitCount = Memory::GetBitWidth(MaximumCompressedTypeCount);
		using TypeIndex = Memory::NumericSize<TypeCountBitCount>;
		inline static constexpr uint32 MaximumCompressedInstanceCount = 4096;
		using InstanceIndex = Memory::NumericSize<MaximumCompressedInstanceCount>;
		inline static constexpr auto IntanceCountBitCount = Memory::GetBitWidth(MaximumCompressedInstanceCount);

		void ClearAll()
		{
			m_componentTypeInstances.Clear();
		}

		void Add(const ComponentSoftReference softReference);
		void Add(const ComponentSoftReferences& other);

		[[nodiscard]] bool HasElements() const
		{
			return m_componentTypeInstances.HasElements();
		}

		[[nodiscard]] InstanceIndex GetInstanceCount() const;

		template<typename ComponentType, typename Callback>
		void IterateComponents(const SceneRegistry& sceneRegistry, Callback&& callback) const
		{
			for (const Entity::ComponentSoftReferences::TypeInstances& __restrict typeInstances : m_componentTypeInstances)
			{
				for (const Instance& instance : typeInstances.m_instances)
				{
					const Entity::ComponentSoftReference reference{typeInstances.m_typeIdentifier, instance};
					if (const Optional<ComponentType*> pComponent = reference.Find<ComponentType>(sceneRegistry))
					{
						callback(*pComponent);
					}
				}
			}
		}

		template<typename Callback>
		void IterateReferences(Callback&& callback) const
		{
			for (const Entity::ComponentSoftReferences::TypeInstances& __restrict typeInstances : m_componentTypeInstances)
			{
				for (const Instance& instance : typeInstances.m_instances)
				{
					callback(Entity::ComponentSoftReference{typeInstances.m_typeIdentifier, instance});
				}
			}
		}

		bool Serialize(Serialization::Writer serializer, const ComponentRegistry& componentRegistry) const;
		bool Serialize(const Serialization::Reader reader, ComponentRegistry& componentRegistry, SceneRegistry& sceneRegistry);

		[[nodiscard]] uint32 CalculateCompressedDataSize() const;
		bool Compress(BitView& target) const;
		bool Decompress(ConstBitView& source);
	protected:
		using Instance = ComponentSoftReference::Instance;
		using Instances = Vector<Instance, InstanceIndex>;
		struct TypeInstances
		{
			void Emplace(const Instance instance);

			ComponentTypeIdentifier m_typeIdentifier;
			Instances m_instances;
		};

		[[nodiscard]] TypeInstances& FindOrEmplaceType(const ComponentTypeIdentifier typeIdentifier);
	private:
		Vector<TypeInstances, TypeIndex> m_componentTypeInstances;
	};
}
