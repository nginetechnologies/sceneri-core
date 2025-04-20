#pragma once

#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Data/Drawable.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Math/Color.h>

namespace ngine::Widgets::Data::Primitives
{
	struct RectangleDrawable final : public Drawable
	{
		using BaseType = Drawable;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(
				BaseType&& initializer,
				const Math::Color backgroundColor = "#FFFFFF"_colorf,
				const Math::Color borderColor = "#00000000"_colorf,
				const Style::SizeAxisEdges borderSize = Style::SizeAxisEdges{1_px},
				const Math::LinearGradient&& gradient = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_backgroundColor(backgroundColor)
				, m_borderColor(borderColor)
				, m_borderSize(borderSize)
				, m_linearGradient(gradient)
			{
			}

			Math::Color m_backgroundColor = "#FFFFFF"_colorf;
			Math::Color m_borderColor = "#00000000"_colorf;
			Style::SizeAxisEdges m_borderSize{1_px};
			Math::LinearGradient m_linearGradient;
		};

		RectangleDrawable(Initializer&& initializer);
		RectangleDrawable(const RectangleDrawable&) = delete;
		RectangleDrawable& operator=(const RectangleDrawable&) = delete;
		RectangleDrawable(RectangleDrawable&&) = delete;
		RectangleDrawable& operator=(RectangleDrawable&&) = delete;
		virtual ~RectangleDrawable() = default;

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
		Math::Color m_borderColor = "#00000000"_colorf;
		Style::SizeAxisEdges m_borderSize{0_px};
		Math::LinearGradient m_linearGradient;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Primitives::RectangleDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Primitives::RectangleDrawable>(
			"EF84FA1D-DAFC-40A3-A08E-84C52CEE9AC7"_guid,
			MAKE_UNICODE_LITERAL("Rectangle Primitive Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
