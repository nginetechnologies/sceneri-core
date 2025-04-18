#pragma once

#include <Common/Memory/ForwardDeclarations/ReferenceWrapper.h>
#include <Common/Memory/Containers/ForwardDeclarations/FixedArrayView.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Function/Function.h>
#include <Common/Serialization/SerializedData.h>

namespace ngine
{
	namespace Entity
	{
		struct HierarchyComponentBase;
		struct SceneRegistry;
	}
}

namespace ngine::Entity::Undo
{
	using ComponentView = ArrayView<const ReferenceWrapper<Entity::HierarchyComponentBase>>;

	struct Entry
	{
		using StoreCallback = Function<void(const Entity::HierarchyComponentBase& component, Serialization::Writer), 16>;
		using RestoreCallback = Function<void(Entity::HierarchyComponentBase& component, Serialization::Reader), 16>;
		using PostAction = Function<void(), 16>;
		using UndoCallback = Function<void(), 16>;

		Entry() = default;
		Entry(ConstStringView name, const ComponentView components, const StoreCallback& storeCallback);
		Entry(ConstStringView name, const ComponentView components);
		Entry(const Entry&) = delete;
		Entry& operator=(const Entry&) = delete;
		Entry(Entry&& other)
			: m_name(other.m_name)
			, m_dataBefore(Move(other.m_dataBefore))
			, m_dataAfter(Move(other.m_dataAfter))
			, m_redoComponentAction(Move(other.m_redoComponentAction))
			, m_undoComponentAction(Move(other.m_undoComponentAction))
			, m_finalizeComponentAction(Move(other.m_finalizeComponentAction))
			, m_postAction(Move(other.m_postAction))
			, m_undoCallback(Move(other.m_undoCallback))
		{
			// Ensure other is always in a good state
			other.m_dataBefore =
				Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
			other.m_dataAfter =
				Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
			other.m_postAction = []()
			{
			};
			other.m_undoCallback = []()
			{
			};
		}
		Entry& operator=(Entry&& other)
		{
			m_name = other.m_name;
			m_dataBefore = Move(other.m_dataBefore);
			m_dataAfter = Move(other.m_dataAfter);
			m_redoComponentAction = Move(other.m_redoComponentAction);
			m_undoComponentAction = Move(other.m_undoComponentAction);
			m_finalizeComponentAction = Move(other.m_finalizeComponentAction);
			m_postAction = Move(other.m_postAction);
			m_undoCallback = Move(other.m_undoCallback);

			// Ensure other is always in a good state
			other.m_dataBefore =
				Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
			other.m_dataAfter =
				Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
			other.m_postAction = []()
			{
			};
			other.m_undoCallback = []()
			{
			};
			return *this;
		}

		void AddComponent(const HierarchyComponentBase& component);

		void SetRedoComponentCallback(RestoreCallback&& action)
		{
			m_redoComponentAction = Forward<RestoreCallback>(action);
		}
		void SetUndoComponentCallback(RestoreCallback&& action)
		{
			m_undoComponentAction = Forward<RestoreCallback>(action);
		}
		void SetFinalizeComponentCallback(RestoreCallback&& action)
		{
			m_finalizeComponentAction = Forward<RestoreCallback>(action);
		}
		// Used to automatically set callbacks for enabling / disabling newly spawned components
		void SetSpawnComponentsCallbacks();

		void SetRedoCallback(PostAction&& action)
		{
			m_postAction = Forward<PostAction>(action);
		}
		void SetUndoCallback(UndoCallback&& action)
		{
			m_undoCallback = Forward<UndoCallback>(action);
		}

		void Undo(SceneRegistry& sceneRegistry);
		void Redo(SceneRegistry& sceneRegistry);
		void FinalizeUndo(SceneRegistry& sceneRegistry);

		[[nodiscard]] const Serialization::Data& GetDataBefore() const
		{
			return m_dataBefore;
		}
		[[nodiscard]] const Serialization::Data& GetDataAfter() const
		{
			return m_dataAfter;
		}

		[[nodiscard]] ConstStringView GetName() const
		{
			return m_name;
		}
	protected:
		void Store(Serialization::Data& data, const ComponentView components, const StoreCallback& callback) const;
		void Store(Serialization::Data& data, const ComponentView components) const;
		void Restore(SceneRegistry& sceneRegistry, Serialization::Data& data, const RestoreCallback& callback) const;
	protected:
		friend struct Scope;

		ConstStringView m_name;
		Serialization::Data m_dataBefore =
			Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
		Serialization::Data m_dataAfter =
			Serialization::Data(Serialization::ContextFlags::UndoHistory | Serialization::ContextFlags::UseWithinSessionInstance);
		RestoreCallback m_redoComponentAction;
		RestoreCallback m_undoComponentAction;
		RestoreCallback m_finalizeComponentAction;
		PostAction m_postAction = []()
		{
		};
		UndoCallback m_undoCallback = []()
		{
		};
	};
}
