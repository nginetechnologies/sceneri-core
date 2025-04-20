#pragma once

#include <Widgets/Data/Drawable.h>
#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Math/Vector2.h>

namespace ngine::Widgets::Data::Primitives
{
	struct LineDrawable final : public Drawable
	{
		using BaseType = Drawable;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(BaseType&& initializer, Math::Spline2f&& spline, const float thickness, const Math::Color lineColor)
				: BaseType(Forward<BaseType>(initializer))
				, m_spline(Forward<Math::Spline2f>(spline))
				, m_thickness(thickness)
				, m_lineColor(lineColor)
			{
			}

			Math::Spline2f m_spline;
			float m_thickness;
			Math::Color m_lineColor = "#fff"_colorf;
		};

		LineDrawable(Initializer&& initializer);
		LineDrawable(const LineDrawable&) = delete;
		LineDrawable& operator=(const LineDrawable&) = delete;
		LineDrawable(LineDrawable&&) = delete;
		LineDrawable& operator=(LineDrawable&&) = delete;
		virtual ~LineDrawable() = default;

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

		Math::Spline2f m_spline;
		float m_thickness;
		Math::Color m_lineColor = "#fff"_colorf;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Primitives::LineDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Primitives::LineDrawable>(
			"f37535ea-a692-4538-87ea-a9c0ff326c7f"_guid,
			MAKE_UNICODE_LITERAL("Line Primitive Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
