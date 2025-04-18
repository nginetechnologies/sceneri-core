#include "Entity/ComponentSoftReference.h"
#include "Entity/ComponentSoftReferences.h"
#include "Entity/ComponentPicker.h"
#include "Entity/ComponentSoftReference.inl"

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneDataInterface.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/Manager.h>

#include <Common/Serialization/Guid.h>
#include <Common/System/Query.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Containers/BitView.h>

namespace ngine::Entity
{
	ComponentSoftReference::Instance::Instance(
		const Instance& other,
		const SceneRegistry& otherSceneRegistry,
		const SceneRegistry& thisSceneRegistry,
		const ComponentTypeIdentifier typeIdentifier
	)
		: m_guid(other.m_guid)
	{
		if (IsPotentiallyValid())
		{
			if (const Optional<ComponentTypeSceneDataInterface*> pSceneTypeInterface = thisSceneRegistry.FindComponentTypeData(typeIdentifier))
			{
				if (m_guid.IsInvalid() && other.m_identifier.IsValid())
				{
					if (const Optional<ComponentTypeSceneDataInterface*> pTemplateSceneTypeInterface = otherSceneRegistry.FindComponentTypeData(typeIdentifier))
					{
						const GenericComponentInstanceIdentifier oldIdentifier = other.m_identifier;
						m_guid = pTemplateSceneTypeInterface->FindComponentInstanceGuid(oldIdentifier);
					}
				}
				Assert(m_guid.IsValid(), "Instance guid is required for cloning!");
				if (m_guid.IsValid())
				{
					m_identifier = pSceneTypeInterface->FindComponentInstanceIdentifier(m_guid);
				}
			}
		}
	}

	bool ComponentSoftReference::Instance::Serialize(Serialization::Writer serializer) const
	{
		if (!IsPotentiallyValid())
		{
			return false;
		}

		Assert(m_guid.IsValid());
		if (serializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
		{
			if (m_identifier.IsValid())
			{
				serializer.Serialize("instanceIdentifier", m_identifier.GetValue());
			}
			serializer.Serialize("instanceGuid", m_guid);
			return true;
		}
		else
		{
			return serializer.Serialize("instanceGuid", m_guid);
		}
	}

	bool ComponentSoftReference::Instance::Serialize(
		const Serialization::Reader reader,
		ComponentRegistry& componentRegistry,
		SceneRegistry& sceneRegistry,
		const ComponentTypeIdentifier typeIdentifier
	)
	{
		if (reader.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
		{
			using IdentifierInternalType = typename InstanceIdentifier::InternalType;
			const InstanceIdentifier instanceIdentifier = InstanceIdentifier::MakeFromValue(
				reader.ReadWithDefaultValue<IdentifierInternalType>("instanceIdentifier", IdentifierInternalType(InstanceIdentifier::Invalid))
			);
			const Guid instanceGuid = reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid());
			m_guid = instanceGuid;

			if (instanceIdentifier.IsValid())
			{
				m_identifier = instanceIdentifier;
				return true;
			}
			else if (instanceGuid.IsValid())
			{
				ComponentTypeInterface& componentType = *componentRegistry.Get(typeIdentifier);

				if (const Optional<ComponentTypeSceneDataInterface*> pSceneData = sceneRegistry.GetOrCreateComponentTypeData(typeIdentifier, componentType))
				{
					const GenericComponentInstanceIdentifier genericInstanceIdentifier = pSceneData->FindComponentInstanceIdentifier(instanceGuid);
					if (genericInstanceIdentifier.IsValid())
					{
						m_identifier = genericInstanceIdentifier;
						return true;
					}
				}

				return true;
			}
			return false;
		}
		else
		{
			ComponentTypeInterface& componentType = *componentRegistry.Get(typeIdentifier);

			const Optional<ComponentTypeSceneDataInterface*> pSceneData =
				sceneRegistry.GetOrCreateComponentTypeData(typeIdentifier, componentType);
			if (pSceneData.IsInvalid())
			{
				return false;
			}

			const Optional<Guid> instanceGuid = reader.Read<Guid>("instanceGuid");
			if (instanceGuid.IsInvalid())
			{
				return false;
			}
			m_guid = *instanceGuid;

			const GenericComponentInstanceIdentifier genericInstanceIdentifier = pSceneData->FindComponentInstanceIdentifier(*instanceGuid);
			if (genericInstanceIdentifier.IsValid())
			{
				m_identifier = genericInstanceIdentifier;
			}
			return true;
		}
	}

	ComponentSoftReference::ComponentSoftReference(const ComponentSoftReference& templateReference, const Cloner& cloner)
		: m_typeIdentifier(templateReference.m_typeIdentifier)
		, m_instance(templateReference.m_instance, cloner.templateSceneRegistry, cloner.sceneRegistry, templateReference.m_typeIdentifier)
	{
	}

	Optional<Component*>
	ComponentSoftReference::Instance::Find(const SceneRegistry& sceneRegistry, const ComponentTypeIdentifier typeIdentifier) const
	{
		if (IsPotentiallyValid())
		{
			if (const Optional<ComponentTypeSceneDataInterface*> pSceneTypeInterface = sceneRegistry.FindComponentTypeData(typeIdentifier))
			{
				if (m_identifier.IsValid())
				{
					const GenericComponentInstanceIdentifier instanceIdentifier{m_identifier};
					if (instanceIdentifier.IsValid())
					{
						return pSceneTypeInterface->FindComponent(instanceIdentifier);
					}
				}

				if (m_guid.IsValid())
				{
					const GenericComponentInstanceIdentifier instanceIdentifier = pSceneTypeInterface->FindComponentInstanceIdentifier(m_guid);
					if (instanceIdentifier.IsValid())
					{
						m_identifier = instanceIdentifier;
						return pSceneTypeInterface->FindComponent(instanceIdentifier);
					}
				}

				return Invalid;
			}
		}
		return Invalid;
	}

	bool ComponentSoftReference::Serialize(Serialization::Writer serializer) const
	{
		if (!IsPotentiallyValid())
		{
			return false;
		}

		serializer.GetValue().SetObject();

		if (serializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
		{
			serializer.Serialize("typeIdentifier", m_typeIdentifier.GetValue());
			return m_instance.Serialize(serializer);
		}
		else
		{
			const ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
			bool success = serializer.Serialize("typeGuid", componentRegistry.GetGuid(m_typeIdentifier));
			success &= m_instance.Serialize(serializer);
			return success;
		}
	}

	bool ComponentSoftReference::Serialize(const Serialization::Reader reader, SceneRegistry& sceneRegistry)
	{
		ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
		if (reader.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
		{
			using TypeIdentifierInternalType = typename ComponentTypeIdentifier::InternalType;
			const ComponentTypeIdentifier typeIdentifier = ComponentTypeIdentifier::MakeFromValue(
				reader
					.ReadWithDefaultValue<TypeIdentifierInternalType>("typeIdentifier", TypeIdentifierInternalType(ComponentTypeIdentifier::Invalid))
			);
			if (typeIdentifier.IsInvalid())
			{
				return false;
			}
			m_typeIdentifier = typeIdentifier;

			return m_instance.Serialize(reader, componentRegistry, sceneRegistry, typeIdentifier);
		}
		else
		{
			const Optional<Guid> typeGuid = reader.Read<Guid>("typeGuid");
			if (typeGuid.IsInvalid())
			{
				return false;
			}

			const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(typeGuid.Get());
			if (typeIdentifier.IsInvalid())
			{
				return false;
			}
			m_typeIdentifier = typeIdentifier;

			return m_instance.Serialize(reader, componentRegistry, sceneRegistry, typeIdentifier);
		}
	}

	bool ComponentSoftReference::Compress(BitView& target) const
	{
		// TODO: Integrate with BoundComponent, if component is bound then just send that identifier.
		// TODO: Support binding types to the network, then send that identifier instead if available.

		ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
		Guid typeGuid;
		if (m_typeIdentifier.IsValid())
		{
			const Optional<const ComponentTypeInterface*> pComponentTypeInterface = componentRegistry.Get(m_typeIdentifier);
			typeGuid = pComponentTypeInterface.IsValid() ? pComponentTypeInterface->GetTypeInterface().GetGuid() : Guid{};
		}
		const bool wasTypeGuidPacked = target.PackAndSkip(ConstBitView::Make(typeGuid));
		const bool wasInstanceGuidPacked = target.PackAndSkip(ConstBitView::Make(m_instance.m_guid));
		return wasTypeGuidPacked & wasInstanceGuidPacked;
	}

	bool ComponentSoftReference::Decompress(ConstBitView& source)
	{
		const Guid typeGuid = source.UnpackAndSkip<Guid>();
		const Guid instanceGuid = source.UnpackAndSkip<Guid>();
		m_instance.m_guid = instanceGuid;

		ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
		if (typeGuid.IsValid())
		{
			const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(typeGuid);
			if (typeIdentifier.IsInvalid())
			{
				return false;
			}
			m_typeIdentifier = typeIdentifier;
		}
		else
		{
			m_typeIdentifier = {};
		}

		return true;
	}

	ComponentSoftReferences::TypeInstances& ComponentSoftReferences::FindOrEmplaceType(const ComponentTypeIdentifier typeIdentifier)
	{
		Assert(typeIdentifier.IsValid());
		auto it = m_componentTypeInstances.FindIf(
			[typeIdentifier](const TypeInstances& typeInstances)
			{
				return typeInstances.m_typeIdentifier == typeIdentifier;
			}
		);
		if (it != m_componentTypeInstances.end())
		{
			return *it;
		}
		else
		{
			return m_componentTypeInstances.EmplaceBack(TypeInstances{typeIdentifier});
		}
	}

	void ComponentSoftReferences::TypeInstances::Emplace(const Instance instance)
	{
		auto it = m_instances.Find(instance);
		if (it != m_instances.end())
		{
			Assert(it->m_guid.IsInvalid() || it->m_guid == instance.m_guid);
			if (it->m_guid.IsInvalid())
			{
				it->m_guid = instance.m_guid;
			}
			Assert(it->m_identifier.IsInvalid() || it->m_identifier == instance.m_identifier);
			if (it->m_identifier.IsInvalid())
			{
				it->m_identifier = instance.m_identifier;
			}
		}
		else
		{
			m_instances.EmplaceBack(Instance{instance});
		}
	}

	void ComponentSoftReferences::Add(const ComponentSoftReference softReference)
	{
		Assert(softReference.IsPotentiallyValid());
		TypeInstances& typeInstances = FindOrEmplaceType(softReference.GetTypeIdentifier());
		typeInstances.Emplace(softReference.GetInstance());
	}

	void ComponentSoftReferences::Add(const ComponentSoftReferences& other)
	{
		m_componentTypeInstances.Reserve(m_componentTypeInstances.GetSize() + other.m_componentTypeInstances.GetSize());
		for (const TypeInstances& otherTypeInstances : other.m_componentTypeInstances)
		{
			auto it = m_componentTypeInstances.FindIf(
				[typeIdentifier = otherTypeInstances.m_typeIdentifier](const TypeInstances& typeInstances)
				{
					return typeInstances.m_typeIdentifier == typeIdentifier;
				}
			);
			if (it != m_componentTypeInstances.end())
			{
				it->m_instances.ReserveAdditionalCapacity(otherTypeInstances.m_instances.GetSize());
				for (const Instance& instance : otherTypeInstances.m_instances)
				{
					it->Emplace(Instance(instance));
				}
			}
			else
			{
				m_componentTypeInstances.EmplaceBack(otherTypeInstances);
			}
		}
	}

	[[nodiscard]] ComponentSoftReferences::InstanceIndex ComponentSoftReferences::GetInstanceCount() const
	{
		InstanceIndex count{0};
		for (const TypeInstances& typeInstances : m_componentTypeInstances)
		{
			return typeInstances.m_instances.GetSize();
		}
		return count;
	}

	bool ComponentSoftReferences::Serialize(Serialization::Writer serializer, const ComponentRegistry& componentRegistry) const
	{
		return serializer.SerializeArrayCallbackInPlace(
			[&componentRegistry,
		   typeInstancesView = m_componentTypeInstances.GetView()](Serialization::Writer typeSerializer, const TypeIndex typeIndex)
			{
				const TypeInstances& typeInstances = typeInstancesView[typeIndex];
				if (typeSerializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
				{
					typeSerializer.Serialize("typeIdentifier", typeInstances.m_typeIdentifier.GetValue());
				}
				else
				{
					typeSerializer.Serialize("typeGuid", componentRegistry.GetGuid(typeInstances.m_typeIdentifier));
				}

				return typeSerializer.Serialize("instances", typeInstances.m_instances);
			},
			m_componentTypeInstances.GetSize()
		);
	}

	bool
	ComponentSoftReferences::Serialize(const Serialization::Reader reader, ComponentRegistry& componentRegistry, SceneRegistry& sceneRegistry)
	{
		m_componentTypeInstances.Reserve(m_componentTypeInstances.GetSize() + (TypeIndex)reader.GetArraySize());
		bool success = true;
		for (const Serialization::Reader typeReader : reader.GetArrayView())
		{
			ComponentTypeIdentifier typeIdentifier;
			if (typeReader.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
			{
				using TypeIdentifierInternalType = typename ComponentTypeIdentifier::InternalType;
				typeIdentifier = ComponentTypeIdentifier::MakeFromValue(typeReader.ReadWithDefaultValue<TypeIdentifierInternalType>(
					"typeIdentifier",
					TypeIdentifierInternalType(ComponentTypeIdentifier::Invalid)
				));
			}
			else
			{
				const Optional<Guid> typeGuid = typeReader.Read<Guid>("typeGuid");
				if (UNLIKELY(typeGuid.IsInvalid()))
				{
					success = false;
					continue;
				}

				typeIdentifier = componentRegistry.FindIdentifier(typeGuid.Get());
			}

			if (UNLIKELY(typeIdentifier.IsInvalid()))
			{
				success = false;
				continue;
			}

			TypeInstances& typeInstances = FindOrEmplaceType(typeIdentifier);

			const Optional<Serialization::Reader> instancesReader = typeReader.FindSerializer("instances");
			Assert(instancesReader.IsValid());
			if (LIKELY(instancesReader.IsValid()))
			{
				typeInstances.m_instances.ReserveAdditionalCapacity((InstanceIndex)instancesReader->GetArraySize());

				for (const Serialization::Reader instanceReader : instancesReader->GetArrayView())
				{
					const Optional<Instance> instance = instanceReader.ReadInPlace<Instance>(componentRegistry, sceneRegistry, typeIdentifier);
					if (LIKELY(instance.IsValid()))
					{
						typeInstances.Emplace(instance.Get());
					}
				}
			}
			else
			{
				success = false;
			}
		}
		return success;
	}

	uint32 ComponentSoftReferences::CalculateCompressedDataSize() const
	{
		Assert(m_componentTypeInstances.GetSize() <= MaximumCompressedTypeCount);
		const TypeIndex typeInstanceCount = (TypeIndex)Math::Min(m_componentTypeInstances.GetSize(), MaximumCompressedTypeCount);

		InstanceIndex instanceCount{0};
		for (const TypeInstances& typeInstances : m_componentTypeInstances.GetSubView(0, typeInstanceCount))
		{
			instanceCount = (InstanceIndex
			)Math::Min((size)instanceCount + (size)typeInstances.m_instances.GetSize(), (size)MaximumCompressedInstanceCount);
		}

		constexpr size typeDataSize = sizeof(Guid) * 8 + IntanceCountBitCount;
		constexpr size instanceDataSize = sizeof(Guid) * 8;

		return TypeCountBitCount + typeDataSize * typeInstanceCount + instanceDataSize * instanceDataSize;
	}

	bool ComponentSoftReferences::Compress(BitView& target) const
	{
		Assert(m_componentTypeInstances.GetSize() <= MaximumCompressedTypeCount);
		const TypeIndex typeInstanceCount = (TypeIndex)Math::Min(m_componentTypeInstances.GetSize(), MaximumCompressedTypeCount);
		target.PackAndSkip(ConstBitView::Make(typeInstanceCount, Math::Range<size>::Make(0, TypeCountBitCount)));

		ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();

		for (const TypeInstances& typeInstances : m_componentTypeInstances.GetSubView(0, typeInstanceCount))
		{
			const Optional<const ComponentTypeInterface*> pComponentTypeInterface = componentRegistry.Get(typeInstances.m_typeIdentifier);
			Assert(pComponentTypeInterface.IsValid());
			if (LIKELY(pComponentTypeInterface.IsValid()))
			{
				const Guid typeGuid = pComponentTypeInterface->GetTypeInterface().GetGuid();
				target.PackAndSkip(ConstBitView::Make(typeGuid));

				Assert(typeInstances.m_instances.GetSize() <= MaximumCompressedInstanceCount);
				const InstanceIndex instanceCount = (InstanceIndex)Math::Min(typeInstances.m_instances.GetSize(), MaximumCompressedInstanceCount);
				target.PackAndSkip(ConstBitView::Make(instanceCount, Math::Range<size>::Make(0, IntanceCountBitCount)));

				for (const Instance& instance : typeInstances.m_instances.GetSubView(0, instanceCount))
				{
					Assert(instance.m_guid.IsValid());
					target.PackAndSkip(ConstBitView::Make(instance.m_guid));
				}
			}
			else
			{
				const Guid typeGuid = {};
				target.PackAndSkip(ConstBitView::Make(typeGuid));
				const uint32 instanceCount = 0;
				target.PackAndSkip(ConstBitView::Make(instanceCount, Math::Range<size>::Make(0, IntanceCountBitCount)));
			}
		}

		return true;
	}

	bool ComponentSoftReferences::Decompress(ConstBitView& source)
	{
		const TypeIndex typeInstanceCount = source.UnpackAndSkip<TypeIndex>(Math::Range<size>::Make(0, TypeCountBitCount));
		m_componentTypeInstances.Clear();
		m_componentTypeInstances.Reserve(typeInstanceCount);

		ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();

		for (uint16 typeInstanceIndex = 0; typeInstanceIndex < typeInstanceCount; ++typeInstanceIndex)
		{
			const Guid typeGuid = source.UnpackAndSkip<Guid>();
			const InstanceIndex instanceCount = source.UnpackAndSkip<InstanceIndex>(Math::Range<size>::Make(0, IntanceCountBitCount));

			const ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(typeGuid);
			Assert(typeIdentifier.IsValid());

			TypeInstances& typeInstances = m_componentTypeInstances.EmplaceBack();
			typeInstances.m_typeIdentifier = typeIdentifier;
			typeInstances.m_instances.Reserve(instanceCount);

			for (uint32 instanceIndex = 0; instanceIndex < instanceCount; ++instanceIndex)
			{
				const Guid instanceGuid = source.UnpackAndSkip<Guid>();
				typeInstances.Emplace(Instance{instanceGuid});
			}
		}
		return true;
	}

	bool ComponentPicker::Serialize(const Serialization::Reader reader)
	{
		if (m_pSceneRegistry.IsValid())
		{
			return BaseType::Serialize(reader, *m_pSceneRegistry);
		}
		return false;
	}

	bool ComponentPicker::Serialize(const Serialization::Writer writer) const
	{
		if (m_pSceneRegistry.IsValid())
		{
			return BaseType::Serialize(writer);
		}
		return false;
	}
}
