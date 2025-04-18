#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Common/Math/Primitives/BoundingBox.h>

namespace ngine::Entity::Data
{
	//! Bounding box of a 3D component, excluding children
	//! Local coordinates (relative to the relative transform)
	struct BoundingBox final : public HierarchyComponent
	{
		BoundingBox(const Deserializer& deserializer) = delete;
		BoundingBox(const BoundingBox&, const Cloner&) = delete;
		BoundingBox(const Math::BoundingBox bounds)
			: m_bounds(bounds)
		{
		}

		void Set(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Math::BoundingBox bounds);

		[[nodiscard]] operator Math::BoundingBox() const
		{
			return m_bounds;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::BoundingBox>;

		Math::BoundingBox m_bounds;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::BoundingBox>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::BoundingBox>(
			"04BCCADC-F06F-4F51-97F9-19EB24CF9035"_guid,
			MAKE_UNICODE_LITERAL("Local Bounding Box"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
