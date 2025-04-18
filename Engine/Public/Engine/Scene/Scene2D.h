#pragma once

#include "SceneBase.h"

namespace ngine
{
	namespace Entity
	{
		struct RootSceneComponent2D;
	}

	struct Scene2D : public SceneBase
	{
		Scene2D(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Entity::HierarchyComponentBase*> pRootParent,
			const Guid guid,
			const EnumFlags<Flags> flags
		);

		// SceneBase
		virtual void ProcessDestroyedComponentsQueueInternal(const ArrayView<ReferenceWrapper<Entity::HierarchyComponentBase>> components
		) override final;
		// ~SceneBase

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Entity::RootSceneComponent2D& GetRootComponent();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Entity::RootSceneComponent2D& GetRootComponent() const;
	};
}
