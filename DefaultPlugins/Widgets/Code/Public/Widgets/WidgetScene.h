#pragma once

#include <Engine/Scene/Scene2D.h>

namespace ngine::Rendering
{
	struct ToolWindow;
}

namespace ngine::Widgets
{
	struct RootWidget;

	struct Scene final : public Scene2D
	{
		Scene(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Entity::HierarchyComponentBase*> pRootParent,
			const Optional<Rendering::ToolWindow*> pWindow,
			const Guid guid,
			const EnumFlags<Flags> flags
		);

		[[nodiscard]] PURE_LOCALS_AND_POINTERS RootWidget& GetRootWidget() const
		{
			return m_rootWidget;
		}
	protected:
		RootWidget& m_rootWidget;
	};
}
