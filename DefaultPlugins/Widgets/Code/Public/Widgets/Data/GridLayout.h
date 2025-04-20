#pragma once

#include "Layout.h"

#include <Common/Threading/Jobs/Job.h>

namespace ngine::Widgets::Data
{
	struct GridLayout : public Layout
	{
		using BaseType = Data::Layout;

		GridLayout(Initializer&& initializer);
		virtual ~GridLayout() = default;

		virtual void PopulateItemWidgets(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;
		virtual bool UpdateVirtualItemVisibilityInternal(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;
		virtual float CalculateNewScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, float position) override;
		virtual void SetInlineVirtualScrollPosition(
			Widget& owner, Entity::SceneRegistry& sceneRegistry, const float previousScrollPosition, const float newScrollPosition
		) override;
		virtual Math::Vector2i GetInitialChildLocation(
			const Widget& owner, Entity::SceneRegistry& sceneRegistry, const Widget& childWidget, const Math::Vector2i size
		) const override;
		virtual bool OnStyleChanged(
			Widget& owner,
			Entity::SceneRegistry& sceneRegistry,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) override;

		[[nodiscard]] Style::Size GetChildSize() const
		{
			return m_childSize;
		}
	protected:
		void UpdateGridSize(Widget& owner, Entity::SceneRegistry& sceneRegistry);

		virtual void EnableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) override final;
		virtual void DisableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) override final;

		[[nodiscard]] virtual float GetMaximumVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry) const override;
	protected:
		uint16 m_maximumVisibleRowCount = 0;
		uint16 m_maximumVisibleColumnCount = 0;

		Style::Size m_childSize;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<ngine::Widgets::Data::GridLayout>
	{
		inline static constexpr auto Type = Reflection::Reflect<ngine::Widgets::Data::GridLayout>(
			"{1C249F4A-BFA0-449B-86DC-A51D4A48B0B1}"_guid,
			MAKE_UNICODE_LITERAL("Grid Layout"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
