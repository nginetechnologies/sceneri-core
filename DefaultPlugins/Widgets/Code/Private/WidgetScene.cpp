#include "WidgetScene.h"
#include "RootWidget.h"

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

namespace ngine::Widgets
{
	Scene::Scene(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Entity::HierarchyComponentBase*> pRootParent,
		const Optional<Rendering::ToolWindow*> pWindow,
		const Guid guid,
		const EnumFlags<Flags> flags
	)
		: Scene2D(sceneRegistry, pRootParent, guid, flags)
		, m_rootWidget(*sceneRegistry.GetOrCreateComponentTypeData<Widgets::RootWidget>()->CreateInstance(Widgets::RootWidget::Initializer{
				*m_pRootComponent,
				pWindow,
			}))
	{
	}
}
