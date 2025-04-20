#pragma once

#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Data/Drawable.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Math/Color.h>

namespace ngine::Widgets::Data::Primitives
{
	struct CircleDrawable final : public Drawable
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
				Math::LinearGradient&& linearGradient = {},
				Math::LinearGradient&& conicGradient = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_backgroundColor(backgroundColor)
				, m_borderColor(borderColor)
				, m_borderSize(borderSize)
				, m_linearGradient(Forward<Math::LinearGradient>(linearGradient))
				, m_conicGradient(Forward<Math::LinearGradient>(conicGradient))
			{
			}

			Math::Color m_backgroundColor = "#FFFFFF"_colorf;
			Math::Color m_borderColor = "#00000000"_colorf;
			Style::SizeAxisEdges m_borderSize{1_px};
			Math::LinearGradient m_linearGradient;
			Math::LinearGradient m_conicGradient;
		};

		CircleDrawable(Initializer&& initializer);
		CircleDrawable(const CircleDrawable&) = delete;
		CircleDrawable& operator=(const CircleDrawable&) = delete;
		CircleDrawable(CircleDrawable&&) = delete;
		CircleDrawable& operator=(CircleDrawable&&) = delete;
		virtual ~CircleDrawable() = default;

		virtual void RecordDrawCommands(
			const Widget& owner,
			const Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Math::Rectangleui,
			const Math::Vector2f startPositionShaderSpace,
			const Math::Vector2f endPositionShaderSpace,
			const Rendering::Pipelines& pipelines
		) const override;
		virtual uint32 GetMaximumPushConstantInstanceCount([[maybe_unused]] const Widget& owner) const override
		{
			return (m_borderColor.a > 0) + (m_linearGradient.IsValid() | (m_backgroundColor.a > 0));
		}
		virtual void OnStyleChanged(
			Widget& owner,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) override;
		[[nodiscard]] virtual LoadResourcesResult TryLoadResources(Widget& owner) override;

		Math::Color m_backgroundColor = "#FFFFFF"_colorf;
		Math::Color m_borderColor = "#00000000"_colorf;
		Style::SizeAxisEdges m_borderSize{1_px};
		Math::LinearGradient m_linearGradient;
		Math::LinearGradient m_conicGradient;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Primitives::CircleDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Primitives::CircleDrawable>(
			"374EC458-AB57-42CA-86A7-0535BB2D51AD"_guid,
			MAKE_UNICODE_LITERAL("Circle Primitive Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
