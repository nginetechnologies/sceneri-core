#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

#include <Common/Function/Function.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Entity::Data::RenderItem
{
	struct VisibilityListener final : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 11>;
		using Function = Function<void(Rendering::LogicalDevice&, const Threading::JobBatch&), 8>;

		VisibilityListener(Function&& function)
			: m_function(Forward<Function>(function))
		{
		}

		void Invoke(Rendering::LogicalDevice& logicalDevice, const Threading::JobBatch& jobBatch)
		{
			m_function(logicalDevice, jobBatch);
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::VisibilityListener>;

		Function m_function;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::VisibilityListener>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::VisibilityListener>(
			"92862099-9ab9-4f04-9870-5df0bedf48a1"_guid,
			MAKE_UNICODE_LITERAL("Render Item Visibility Listener"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
