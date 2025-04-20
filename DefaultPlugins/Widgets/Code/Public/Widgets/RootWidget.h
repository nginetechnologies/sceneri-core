#pragma once

#include <Widgets/Widget.h>

#include <Engine/Entity/ComponentSoftReference.h>

#include <Common/Memory/UniqueRef.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	struct ToolWindow;
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Widgets
{
	struct RootWidget final : public Widget
	{
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType = Widget;
		using ParentType = HierarchyComponentBase;

		static constexpr Guid TypeGuid = "{6078575C-70F7-4FC1-95B3-3B9F875D4481}"_guid;

		struct Initializer : public Reflection::TypeInitializer
		{
			using BaseType = Reflection::TypeInitializer;

			Initializer(Entity::HierarchyComponentBase& parent, const Optional<Rendering::ToolWindow*> pOwningWindow)
				: m_parent(parent)
				, m_pOwningWindow(pOwningWindow)
			{
			}

			Entity::HierarchyComponentBase& m_parent;
			Optional<Rendering::ToolWindow*> m_pOwningWindow;
		};
		RootWidget(Initializer&& initializer);
		RootWidget(RootWidget&&) = delete;
		RootWidget(const RootWidget&) = delete;
		RootWidget& operator=(RootWidget&&) = delete;
		RootWidget& operator=(const RootWidget&) = delete;
		~RootWidget();

		[[nodiscard]] Widget* GetWidgetAtCoordinate(
			const WindowCoordinate coordinates,
			const Optional<uint16> touchRadius = Invalid,
			const EnumFlags<Widget::Flags> disallowedFlags = Widget::Flags::IsInputDisabled
		);

		[[nodiscard]] Threading::Job& GetRecalculateWidgetsHierarchyStage() const
		{
			return *m_pRecalculateWidgetsHierarchyStage;
		}

		enum class Flags : uint8
		{
			HasQueuedWidgetRecalculations = 1 << 0
		};

		void QueueRecalculateWidgetHierarchy(Widget& widget);

		[[nodiscard]] Optional<Rendering::ToolWindow*> GetOwningWindow() const
		{
			return m_pOwningWindow;
		}
	protected:
		friend Rendering::ToolWindow;
		friend Widget;

		void ProcessWidgetHierarchyRecalculation();
		void OnWidgetRemoved(Widget& widget);
	protected:
		Optional<Rendering::ToolWindow*> m_pOwningWindow;
		UniqueRef<Threading::Job> m_pRecalculateWidgetsHierarchyStage;

		Threading::AtomicIdentifierMask<Entity::ComponentIdentifier> m_queuedWidgetRecalculationsMask;
		TIdentifierArray<Entity::ComponentSoftReference, Entity::ComponentIdentifier> m_queuedWidgetRecalculations;
		AtomicEnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(RootWidget::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::RootWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::RootWidget>(
			Widgets::RootWidget::TypeGuid,
			MAKE_UNICODE_LITERAL("Root Widget"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization
		);
	};
}
