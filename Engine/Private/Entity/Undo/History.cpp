#include <Engine/Entity/Undo/Entry.h>
#include <Engine/Entity/Undo/History.h>
#include <Engine/Entity/HierarchyComponentBase.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component2D.h>
#include <Engine/Entity/Serialization/ComponentReference.h>

#include <Common/Serialization/Reader.h>

namespace ngine::Entity::Undo
{
	Scope::Scope(
		History& history,
		SceneRegistry& sceneRegistry,
		ConstStringView name,
		const ComponentView components,
		Entry::StoreCallback&& storeCallback
	)
		: m_pHistory(&history)
		, m_components(components)
		, m_pSceneRegistry(sceneRegistry)
		, m_entry(Entry{name, components, Forward<Entry::StoreCallback>(storeCallback)})
	{
	}

	Scope::Scope(History& history, SceneRegistry& sceneRegistry, ConstStringView name, const ComponentView components)
		: m_pHistory(&history)
		, m_components(components)
		, m_pSceneRegistry(sceneRegistry)
		, m_entry(Entry{name, components})
	{
	}

	Scope::~Scope()
	{
		if (IsRecording())
		{
			Store();
		}
	}

	void Scope::Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name, Entry::StoreCallback&& storeCallback)
	{
		Assert(!IsRecording());
		m_pHistory = &history;
		Assert(m_entry.IsInvalid());
		m_entry = Entry{name, m_components, Forward<Entry::StoreCallback>(storeCallback)};
		m_pSceneRegistry = sceneRegistry;
	}

	void Scope::Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name)
	{
		Assert(!IsRecording());
		m_pHistory = &history;
		Assert(m_entry.IsInvalid());
		m_entry = Entry{name, m_components};
		m_pSceneRegistry = sceneRegistry;
	}

	void Scope::Start(
		History& history,
		SceneRegistry& sceneRegistry,
		ConstStringView name,
		const ComponentView components,
		Entry::StoreCallback&& storeCallback
	)
	{
		Assert(!IsRecording());
		m_pHistory = &history;
		Assert(m_entry.IsInvalid());
		m_components = components;
		m_pSceneRegistry = sceneRegistry;
		m_entry = Entry{name, components, Forward<Entry::StoreCallback>(storeCallback)};
	}

	void Scope::Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name, const ComponentView components)
	{
		Assert(!IsRecording());
		m_pHistory = &history;
		Assert(m_entry.IsInvalid());
		m_components = components;
		m_pSceneRegistry = sceneRegistry;
		m_entry = Entry{name, components};
	}

	void Scope::AddComponent(Entity::HierarchyComponentBase& component)
	{
		if (!m_components.Contains(component))
		{
			m_components.EmplaceBack(component);
			if (m_entry.IsValid())
			{
				m_entry->AddComponent(component);
			}
		}
	}

	void Scope::AddComponents(const ComponentView components)
	{
		m_components.Reserve(m_components.GetSize() + components.GetSize());
		for (Entity::HierarchyComponentBase& component : components)
		{
			AddComponent(component);
		}
	}

	void Scope::Store()
	{
		Assert(m_pHistory.IsValid());

		Assert(m_entry.IsValid());
		if (LIKELY(m_entry.IsValid()))
		{
			m_entry->Store(m_entry->m_dataAfter, m_components);

			m_pHistory->AddEntry(Move(*m_entry), *m_pSceneRegistry);
			m_components.Clear();
			m_entry = Invalid;
			m_pSceneRegistry = Invalid;
		}
	}

	void Scope::Cancel()
	{
		m_entry = Invalid;
		m_components.Clear();
		m_pSceneRegistry = Invalid;
	}

	void Scope::Restore()
	{
		Assert(m_entry.IsValid());
		if (LIKELY(m_entry.IsValid()))
		{
			m_entry->Undo(*m_pSceneRegistry);
		}
	}

	Entry::Entry(const ConstStringView name, const ComponentView components, const StoreCallback& storeCallback)
		: m_name(name)
		, m_redoComponentAction(
				[](HierarchyComponentBase& component, const Serialization::Reader componentReader)
				{
					component.Serialize(componentReader);
				}
			)
		, m_undoComponentAction(
				[](HierarchyComponentBase& component, const Serialization::Reader componentReader)
				{
					component.Serialize(componentReader);
				}
			)
	{
		Store(m_dataBefore, components, storeCallback);
	}

	Entry::Entry(const ConstStringView name, const ComponentView components)
		: m_name(name)
		, m_redoComponentAction(
				[](HierarchyComponentBase& component, const Serialization::Reader componentReader)
				{
					component.Serialize(componentReader);
				}
			)
		, m_undoComponentAction(
				[](HierarchyComponentBase& component, const Serialization::Reader componentReader)
				{
					component.Serialize(componentReader);
				}
			)
	{
		Store(m_dataBefore, components);
	}

	void Entry::AddComponent(const HierarchyComponentBase& component)
	{
		if (!m_dataBefore.GetDocument().IsArray())
		{
			m_dataBefore.GetDocument().SetArray();
		}

		Serialization::Value value;
		Serialization::Writer componentWriter(value, m_dataBefore);
		const ComponentReference<const HierarchyComponentBase> componentReference = component;
		[[maybe_unused]] const bool wasReferenceSerialized = componentWriter.SerializeInPlace(componentReference);
		Assert(wasReferenceSerialized);
		[[maybe_unused]] const bool wasComponentSerialized = componentWriter.SerializeInPlace(*componentReference);
		Assert(wasComponentSerialized);
		m_dataBefore.GetDocument().PushBack(Move(value), m_dataBefore.GetDocument().GetAllocator());
	}

	void Entry::SetSpawnComponentsCallbacks()
	{
		m_undoComponentAction = [](Entity::HierarchyComponentBase& component, Serialization::Reader reader)
		{
			component.Serialize(reader);

			Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
			if (const Optional<Entity::Component3D*> pComponent3D = component.As<Entity::Component3D>(sceneRegistry))
			{
				pComponent3D->DisableWithChildren(sceneRegistry);
				pComponent3D->DetachFromOctree(sceneRegistry);
			}
		};
		m_redoComponentAction = [](Entity::HierarchyComponentBase& component, Serialization::Reader reader)
		{
			component.Serialize(reader);

			Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
			if (const Optional<Entity::Component3D*> pComponent3D = component.As<Entity::Component3D>(sceneRegistry))
			{
				pComponent3D->EnableWithChildren(sceneRegistry);
				pComponent3D->AttachToOctree(sceneRegistry);
			}
		};
		m_finalizeComponentAction = [](Entity::HierarchyComponentBase& component, Serialization::Reader)
		{
			if (const Optional<Entity::Component3D*> pComponent3D = component.As<Entity::Component3D>())
			{
				pComponent3D->Destroy(pComponent3D->GetSceneRegistry());
			}
			else if (const Optional<Entity::Component2D*> pComponent2D = component.As<Entity::Component2D>())
			{
				pComponent2D->Destroy(pComponent2D->GetSceneRegistry());
			}
			else
			{
				component.Destroy(component.GetSceneRegistry());
			}
		};
	}

	void Entry::FinalizeUndo(SceneRegistry& sceneRegistry)
	{
		if (m_finalizeComponentAction.IsValid())
		{
			Restore(sceneRegistry, m_dataBefore, Move(m_finalizeComponentAction));
		}
	}

	void Entry::Undo(SceneRegistry& sceneRegistry)
	{
		Restore(sceneRegistry, m_dataBefore, Move(m_undoComponentAction));
		m_undoCallback();
	}

	void Entry::Redo(SceneRegistry& sceneRegistry)
	{
		Restore(sceneRegistry, m_dataAfter, Move(m_redoComponentAction));
		m_postAction();
	}

	void Entry::Store(Serialization::Data& data, const ComponentView components, const StoreCallback& callback) const
	{
		const uint32 componentCount = components.GetSize();

		Serialization::Writer writer = Serialization::Writer(data);
		writer.SerializeArrayCallbackInPlace(
			[components, &callback](Serialization::Writer componentWriter, uint32 index) mutable -> bool
			{
				const ComponentReference<const HierarchyComponentBase> componentReference = *components[index];
				if (componentReference.IsValid())
				{
					[[maybe_unused]] const bool wasReferenceSerialized = componentWriter.SerializeInPlace(componentReference);
					Assert(wasReferenceSerialized);
					[[maybe_unused]] const bool wasComponentSerialized = componentWriter.SerializeInPlace(*componentReference);
					Assert(wasComponentSerialized);
					callback(*componentReference, componentWriter);
					return true;
				}
				else
				{
					return false;
				}
			},
			componentCount
		);
	}

	void Entry::Store(Serialization::Data& data, const ComponentView components) const
	{
		const uint32 componentCount = components.GetSize();

		Serialization::Writer writer = Serialization::Writer(data);
		writer.SerializeArrayCallbackInPlace(
			[components](Serialization::Writer componentWriter, uint32 index) mutable -> bool
			{
				const ComponentReference<const HierarchyComponentBase> componentReference = *components[index];
				if (componentReference.IsValid())
				{
					[[maybe_unused]] const bool wasReferenceSerialized = componentWriter.SerializeInPlace(componentReference);
					Assert(wasReferenceSerialized);
					[[maybe_unused]] const bool wasComponentSerialized = componentWriter.SerializeInPlace(*componentReference);
					Assert(wasComponentSerialized);
					return true;
				}
				else
				{
					return false;
				}
			},
			componentCount
		);
	}

	void Entry::Restore(SceneRegistry& sceneRegistry, Serialization::Data& data, const RestoreCallback& callback) const
	{
		Serialization::Reader reader = Serialization::Reader(data);

		for (const Serialization::Reader componentReader : reader.GetArrayView())
		{
			const Optional<ComponentReference<HierarchyComponentBase>> componentReference =
				componentReader.ReadInPlace<ComponentReference<HierarchyComponentBase>>(sceneRegistry);
			Assert(componentReference.IsValid());
			if (LIKELY(componentReference.IsValid()))
			{
				HierarchyComponentBase& component = *componentReference->Get();
				[[maybe_unused]] const bool wasRead = componentReader.SerializeInPlace(component);
				Assert(wasRead);
				if (callback.IsValid())
				{
					callback(component, componentReader);
				}
			}
		}
	}
}
