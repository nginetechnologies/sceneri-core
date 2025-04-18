#pragma once

#include "Entry.h"

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/InlineVector.h>

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Entity::Undo
{
	struct History;

	struct Scope
	{
		Scope() = default;
		Scope(
			History& history,
			SceneRegistry& sceneRegistry,
			ConstStringView name,
			const ComponentView components,
			Entry::StoreCallback&& storeCallback
		);
		Scope(History& history, SceneRegistry& sceneRegistry, ConstStringView name, const ComponentView components);
		template<typename... ComponentViews>
		Scope(History& history, SceneRegistry& sceneRegistry, ConstStringView name, ComponentViews&&... componentViews)
		{
			Start(history, sceneRegistry, name, Forward<ComponentViews>(componentViews)...);
		}
		Scope(const Scope&) = delete;
		Scope(Scope&& other)
			: m_pHistory(other.m_pHistory)
			, m_components(Move(other.m_components))
			, m_pSceneRegistry(other.m_pSceneRegistry)
			, m_entry(Move(other.m_entry))
		{
		}
		Scope& operator=(const Scope&) = delete;
		Scope& operator=(Scope&& other)
		{
			m_pHistory = other.m_pHistory;
			m_components = Move(other.m_components);
			m_pSceneRegistry = other.m_pSceneRegistry;
			m_entry = Move(other.m_entry);
			return *this;
		}
		~Scope();

		void Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name, Entry::StoreCallback&& storeCallback);
		void Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name);
		void Start(
			History& history,
			SceneRegistry& sceneRegistry,
			ConstStringView name,
			const ComponentView components,
			Entry::StoreCallback&& storeCallback
		);
		void Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name, const ComponentView components);
		template<typename... ComponentViews>
		void Start(History& history, SceneRegistry& sceneRegistry, ConstStringView name, ComponentViews&&... componentViews)
		{
			Assert(!IsRecording());
			m_pHistory = &history;
			Array<ComponentView, sizeof...(ComponentViews)> views = {componentViews...};
			uint32 componentCount = 0;
			for (const ComponentView components : views)
			{
				componentCount += components.GetSize();
			}
			m_components.Reserve(componentCount);

			for (const ComponentView components : views)
			{
				Assert(!m_components.ContainsAny(components));
				m_components.CopyEmplaceRangeBack(components);
			}

			m_pSceneRegistry = sceneRegistry;
			m_entry = Entry{name, m_components};
		}

		void AddComponent(Entity::HierarchyComponentBase& component);
		void AddComponents(const ComponentView components);

		void Store();
		void Cancel();
		void Restore();

		enum class State : uint8
		{
			//! Default state, indicates that the scope hasn't recorded the before change state of components
			AwaitingStart,
			//! Indicates that the scope has recorded the before change state of components, and is awaiting recording the after state
			Recording,
			Finished
		};

		[[nodiscard]] inline bool IsRecording() const
		{
			return m_entry.IsValid();
		}

		[[nodiscard]] Optional<Entry*> GetEntry()
		{
			return Optional<Entry*>{&m_entry.GetUnsafe(), m_entry.IsValid()};
		}
		[[nodiscard]] Optional<const Entry*> GetEntry() const
		{
			return Optional<const Entry*>{&m_entry.GetUnsafe(), m_entry.IsValid()};
		}
	private:
		Optional<History*> m_pHistory;
		using ComponentContainer = InlineVector<ReferenceWrapper<HierarchyComponentBase>, 2>;
		ComponentContainer m_components;
		Optional<SceneRegistry*> m_pSceneRegistry;
		Optional<Entry> m_entry;
	};
}
