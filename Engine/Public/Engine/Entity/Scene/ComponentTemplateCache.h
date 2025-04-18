#pragma once

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Engine/Asset/AssetType.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine
{
	struct Engine;
	struct Scene3D;

	namespace Threading
	{
		struct JobBatch;
	}
}

namespace ngine::Entity
{
	struct Component3D;

	struct SceneTemplate
	{
		SceneTemplate() = default;
		SceneTemplate(const SceneTemplate&) = delete;
		SceneTemplate& operator=(const SceneTemplate&) = delete;
		SceneTemplate(SceneTemplate&&) = default;
		SceneTemplate& operator=(SceneTemplate&&) = default;
		~SceneTemplate();

		Optional<Component3D*> m_pRootComponent;
	};

	struct ComponentTemplateCache final : public Asset::Type<ComponentTemplateIdentifier, SceneTemplate>
	{
		using BaseType = Asset::Type<ComponentTemplateIdentifier, SceneTemplate>;

		ComponentTemplateCache(Asset::Manager& assetManager);
		~ComponentTemplateCache();

		void Reset();
		void Reset(const ComponentTemplateIdentifier identifier);

		[[nodiscard]] ComponentTemplateIdentifier FindOrRegister(const Asset::Guid guid);

		using LoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const ComponentTemplateIdentifier), 24>;
		using LoadListenerData = LoadEvent::ListenerData;
		using LoadListenerIdentifier = LoadEvent::ListenerIdentifier;

		[[nodiscard]] Threading::JobBatch TryLoadScene(const ComponentTemplateIdentifier identifier, LoadListenerData&& newListenerData);
		//! Migrates an instance to the cache, and maps its contents to the new cached component
		[[nodiscard]] bool
		MigrateInstance(const ComponentTemplateIdentifier identifier, Component3D& otherComponent, SceneRegistry& otherRegistry);

		[[nodiscard]] bool HasSceneLoaded(const ComponentTemplateIdentifier identifier) const;

		[[nodiscard]] Entity::SceneRegistry& GetTemplateSceneRegistry()
		{
			return m_templateSceneRegistry;
		}
	protected:
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;

		Threading::AtomicIdentifierMask<ComponentTemplateIdentifier> m_loadingScenes;
		Threading::AtomicIdentifierMask<ComponentTemplateIdentifier> m_loadedScenes;

		Threading::SharedMutex m_sceneRequesterMutex;
		UnorderedMap<ComponentTemplateIdentifier, UniquePtr<LoadEvent>, ComponentTemplateIdentifier::Hash> m_sceneRequesterMap;

		Entity::SceneRegistry m_templateSceneRegistry;
		// The scene in which we instantiate scene components used for templates
		UniquePtr<Scene3D> m_pTemplateScene;
	};
}
