#pragma once

#include <Widgets/Style/SizeCorners.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Data/Drawable.h>
#include <Common/Math/Radius.h>
#include <Common/Math/LinearGradient.h>
#include <Common/Math/Color.h>

namespace ngine::Widgets::Data::Primitives
{
	struct RoundedRectangleDrawable final : public Drawable
	{
		using BaseType = Drawable;

		struct Initializer : public Drawable::Initializer
		{
			using BaseType = Drawable::Initializer;

			Initializer(
				BaseType&& initializer,
				const Math::Color backgroundColor = "#FFFFFF"_colorf,
				const Widgets::Style::SizeAxisCorners cornerRoundingRadiuses = Style::SizeAxisCorners{(Math::Ratiof)100_percent},
				const Math::Color borderColor = "#00000000"_colorf,
				const Style::SizeAxisEdges borderSize = Style::SizeAxisEdges{1_px},
				const Math::LinearGradient&& gradient = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_backgroundColor(backgroundColor)
				, m_cornerRoundingRadiuses(cornerRoundingRadiuses)
				, m_borderColor(borderColor)
				, m_borderSize(borderSize)
				, m_linearGradient(gradient)
			{
			}

			Math::Color m_backgroundColor = "#FFFFFF"_colorf;
			Widgets::Style::SizeAxisCorners m_cornerRoundingRadiuses = Style::SizeAxisCorners{(Math::Ratiof)100_percent};
			Math::Color m_borderColor = "#00000000"_colorf;
			Style::SizeAxisEdges m_borderSize{1_px};
			Math::LinearGradient m_linearGradient;
		};

		RoundedRectangleDrawable(Initializer&& initializer);
		RoundedRectangleDrawable(const RoundedRectangleDrawable&) = delete;
		RoundedRectangleDrawable& operator=(const RoundedRectangleDrawable&) = delete;
		RoundedRectangleDrawable(RoundedRectangleDrawable&&) = delete;
		RoundedRectangleDrawable& operator=(RoundedRectangleDrawable&&) = delete;
		virtual ~RoundedRectangleDrawable() = default;

		[[nodiscard]] virtual bool ShouldDrawCommands(const Widget& owner, const Rendering::Pipelines& pipelines) const override;
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
		Widgets::Style::SizeAxisCorners m_cornerRoundingRadiuses = Style::SizeAxisCorners{(Math::Ratiof)100_percent};
		Math::Color m_borderColor = "#00000000"_colorf;
		Style::SizeAxisEdges m_borderSize{1_px};
		Math::LinearGradient m_linearGradient;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Primitives::RoundedRectangleDrawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Primitives::RoundedRectangleDrawable>(
			"A3A49377-3BCB-4D10-A4DB-88D25DD622DC"_guid,
			MAKE_UNICODE_LITERAL("Rounded Rectangle Primitive Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
