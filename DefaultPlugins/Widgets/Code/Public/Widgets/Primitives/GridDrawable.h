#pragma once

#include <Widgets/Data/Drawable.h>
#include <Common/Math/Color.h>

namespace ngine::Widgets::Data::Primitives
{
	struct GridDrawable final : public Drawable
	{
		using BaseType = Drawable;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(
				BaseType&& initializer, const Math::Color backgroundColor = "#FFFFFF"_colorf, const Math::Color gridColor = "#00000000"_colorf
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_backgroundColor(backgroundColor)
				, m_gridColor(gridColor)
			{
			}

			Math::Color m_backgroundColor = "#FFFFFF"_colorf;
			Math::Color m_gridColor = "#00000000"_colorf;
		};

		GridDrawable(Initializer&& initializer);
		GridDrawable(const GridDrawable&) = delete;
		GridDrawable& operator=(const GridDrawable&) = delete;
		GridDrawable(GridDrawable&&) = delete;
		GridDrawable& operator=(GridDrawable&&) = delete;
		virtual ~GridDrawable() = default;

		void SetOffset(const Math::Vector2f offset)
		{
			m_offset = offset;
		}

		virtual void RecordDrawCommands(
			const Widget& owner,
			const Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Math::Rectangleui,
			const Math::Vector2f startPositionShaderSpace,
			const Math::Vector2f endPositionShaderSpace,
			const Rendering::Pipelines& pipelines
		) const override;
		virtual uint32 GetMaximumPushConstantInstanceCount(const Widget& owner) const override;
		virtual void OnStyleChanged(
			Widget& owner,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) override;
		[[nodiscard]] virtual LoadResourcesResult TryLoadResources(Widget& owner) override;

		Math::Color m_backgroundColor = "#FFFFFF"_colorf;
		Math::Color m_gridColor = "#00000000"_colorf;
		Math::Vector2f m_offset = Math::Vector2f(Math::Zero);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Primitives::GridDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Primitives::GridDrawable>(
			"481ccc45-a6e6-4c5b-9030-03c6745f0614"_guid,
			MAKE_UNICODE_LITERAL("Grid Primitive Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
