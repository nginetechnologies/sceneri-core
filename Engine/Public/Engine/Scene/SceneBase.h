#pragma once

#include "SceneFlags.h"

#include <Engine/Entity/RenderItemIdentifier.h>

#include <Common/AtomicEnumFlags.h>
#include <Common/Time/Stopwatch.h>
#include <Common/Time/FrameTime.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Rendering
{
	struct SceneViewBase;
}

namespace ngine
{
	namespace Entity
	{
		struct SceneRegistry;
		struct HierarchyComponentBase;
	}

	namespace Threading
	{
		struct Job;
	}

	struct SceneBase
	{
		using Flags = SceneFlags;

		SceneBase(Entity::SceneRegistry& sceneRegistry, const Guid guid, const EnumFlags<Flags> flags);
		SceneBase(const SceneBase&) = delete;
		SceneBase(SceneBase&&) = delete;
		SceneBase& operator=(const SceneBase&) = delete;
		SceneBase& operator=(SceneBase&&) = delete;
		virtual ~SceneBase();

		[[nodiscard]] bool IsEnabled() const
		{
			return m_flags.IsNotSet(Flags::IsDisabled);
		}
		[[nodiscard]] bool IsDisabled() const
		{
			return m_flags.IsSet(Flags::IsDisabled);
		}
		[[nodiscard]] bool IsTemplate() const
		{
			return m_flags.IsSet(Flags::IsTemplate);
		}
		[[nodiscard]] bool IsEditing() const
		{
			return m_flags.IsSet(Flags::IsEditing);
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Guid GetGuid() const
		{
			return m_guid;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Guid GetSessionGuid() const
		{
			return m_sessionGuid;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Entity::SceneRegistry& GetEntitySceneRegistry()
		{
			return m_sceneRegistry;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Entity::SceneRegistry& GetEntitySceneRegistry() const
		{
			return m_sceneRegistry;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Entity::HierarchyComponentBase& GetRootComponent()
		{
			return *m_pRootComponent;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Entity::HierarchyComponentBase& GetRootComponent() const
		{
			return *m_pRootComponent;
		}

		[[nodiscard]] FrameTime GetCurrentFrameTime() const
		{
			return m_frameTime;
		}
		[[nodiscard]] Time::Durationf GetTime() const
		{
			return m_time;
		}
		[[nodiscard]] uint64 GetFrameCounter() const
		{
			return m_frameCounter;
		}

		[[nodiscard]] Threading::Job& GetStartFrameStage() const
		{
			return *m_pStartFrameJob;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Threading::IntermediateStage& GetEndFrameStage()
		{
			return m_endFrameStage;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Threading::IntermediateStage& GetPolledForInputStage()
		{
			return m_polledForInputStage;
		}
		[[nodiscard]] Threading::Job& GetDestroyComponentsStage() const
		{
			return *m_pDestroyComponentsJob;
		}

		//! Queues a function to be called when the frame graph is safe to modify for this scene.
		//! Can execute immediately if it is currently safe!
		void ModifyFrameGraph(Function<void(), 24>&& callback);

		void Enable();
		void Disable();
		ThreadSafe::Event<void(void*), 24> OnEnabledUpdate;
		ThreadSafe::Event<void(void*), 24> OnDisabledUpdate;

		void OnBeforeUnload();

		void QueueComponentDestruction(Entity::HierarchyComponentBase& component)
		{
			Threading::UniqueLock lock(m_queuedComponentDestructionsMutex);
			Assert(!m_queuedComponentDestructions.Contains(component));
			m_queuedComponentDestructions.EmplaceBack(component);
		}

		void ProcessFullDestroyedComponentsQueue();
		void ProcessDestroyedComponentsQueue();

		[[nodiscard]] Entity::RenderItemIdentifier AcquireNewRenderItemIdentifier()
		{
			return m_renderItemIdentifiers.AcquireIdentifier();
		}
		void ReturnRenderItemIdentifier(const Entity::RenderItemIdentifier identifier)
		{
			m_renderItemIdentifiers.ReturnIdentifier(identifier);
		}

		[[nodiscard]] Entity::RenderItemIdentifier GetActiveRenderItemIdentifier(const Entity::RenderItemIdentifier identifier)
		{
			return m_renderItemIdentifiers.GetActiveIdentifier(identifier);
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, Entity::RenderItemIdentifier>
		GetValidRenderItemView(IdentifierArrayView<ElementType, Entity::RenderItemIdentifier> view) const
		{
			return m_renderItemIdentifiers.GetValidElementView(view);
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, Entity::RenderItemIdentifier>
		GetValidRenderItemView(FixedIdentifierArrayView<ElementType, Entity::RenderItemIdentifier> view) const
		{
			return m_renderItemIdentifiers.GetValidElementView(view);
		}

		[[nodiscard]] typename Entity::RenderItemIdentifier::IndexType GetMaximumUsedRenderItemCount() const
		{
			return m_renderItemIdentifiers.GetMaximumUsedElementCount();
		}

		void AssignSceneView(Rendering::SceneViewBase& view)
		{
			m_views.EmplaceBack(view);
		}

		void OnSceneViewRemoved(Rendering::SceneViewBase& view)
		{
			m_views.RemoveFirstOccurrence(view);
		}

		using ViewContainer = FlatVector<ReferenceWrapper<Rendering::SceneViewBase>, 2>;
		using ActiveViews = ViewContainer::ConstView;
		[[nodiscard]] PURE_STATICS ActiveViews GetActiveViews() const
		{
			return ActiveViews{m_views.begin(), m_views.end()};
		}
	protected:
		void OnStartFrame();
		void OnEnabledInternal();
		void OnDisabledInternal();

		virtual void ProcessDestroyedComponentsQueueInternal(const ArrayView<ReferenceWrapper<Entity::HierarchyComponentBase>> components) = 0;
	protected:
		Entity::SceneRegistry& m_sceneRegistry;
		Guid m_guid;
		AtomicEnumFlags<Flags> m_flags;
		const Guid m_sessionGuid{Guid::Generate()};
		Optional<Entity::HierarchyComponentBase*> m_pRootComponent;

		UniquePtr<Threading::Job> m_pStartFrameJob;
		Threading::IntermediateStage m_endFrameStage{"Scene End Frame"};
		Threading::IntermediateStage m_polledForInputStage{"Scene Polled For Input"};
		UniquePtr<Threading::Job> m_pDestroyComponentsJob;

		Time::Stopwatch m_stopwatch;
		FrameTime m_frameTime;
		Time::Durationf m_time = 0_seconds;
		uint64 m_frameCounter = 0;
	private:
		Threading::Mutex m_queuedComponentDestructionsMutex;
		inline static constexpr size InitialQueuedComponentsDestructionCapacity = 1024;
		using QueuedComponentDestructionsContainer = Vector<ReferenceWrapper<Entity::HierarchyComponentBase>>;
		QueuedComponentDestructionsContainer m_queuedComponentDestructions{Memory::Reserve, InitialQueuedComponentsDestructionCapacity};
		QueuedComponentDestructionsContainer m_queuedComponentDestructionsCopy{Memory::Reserve, InitialQueuedComponentsDestructionCapacity};

		TSaltedIdentifierStorage<Entity::RenderItemIdentifier> m_renderItemIdentifiers;
		ViewContainer m_views;
	};
}
