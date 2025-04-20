#include "Widgets/Primitives/CircleDrawable.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Pipelines/Pipelines.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/LoadResourcesResult.h>

#include <Common/Math/Color.h>
#include <Common/Math/Vector2/MultiplicativeInverse.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentType.h>

namespace ngine::Widgets::Data::Primitives
{
	CircleDrawable::CircleDrawable(Initializer&& initializer)
		: m_backgroundColor(initializer.m_backgroundColor)
		, m_borderColor(initializer.m_borderColor)
		, m_borderSize(initializer.m_borderSize)
		, m_linearGradient(Move(initializer.m_linearGradient))
		, m_conicGradient(Move(initializer.m_conicGradient))
	{
	}

	void CircleDrawable::RecordDrawCommands(
		const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const Math::Rectangleui renderViewport,
		const Math::Vector2f startPositionShaderSpace,
		const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		const Math::Vector2f contentRatio = Math::MultiplicativeInverse((Math::Vector2f)renderViewport.GetSize());

		const Style::Size borderSizeStyleTopLeft = {m_borderSize.m_left, m_borderSize.m_top};
		const Style::Size borderSizeStyleBottomRight = {m_borderSize.m_right, m_borderSize.m_bottom};
		const Math::Vector2f borderSizeTopLeft =
			contentRatio * (Math::Vector2f)borderSizeStyleTopLeft.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties()) *
			2.f;
		const Math::Vector2f borderSizeBottomRight =
			contentRatio *
			(Math::Vector2f)borderSizeStyleBottomRight.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties()) * 2.f;

		const Math::WorldRotation2D angle = owner.GetWorldRotation(owner.GetSceneRegistry());
		if (m_borderColor.a > 0)
		{
			const Rendering::CirclePipeline& circlePipeline = pipelines.GetCirclePipeline();
			renderCommandEncoder.BindPipeline(circlePipeline);
			circlePipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace,
				endPositionShaderSpace,
				angle,
				m_borderColor,
				owner.GetDepthRatio()
			);
		}

		if (m_linearGradient.IsValid())
		{
			const Rendering::CircleLinearGradientPipeline& circleLinearGradientPipeline = pipelines.GetCircleLinearGradientPipeline();
			renderCommandEncoder.BindPipeline(circleLinearGradientPipeline);
			circleLinearGradientPipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace + borderSizeTopLeft,
				endPositionShaderSpace - borderSizeBottomRight,
				angle,
				owner.GetDepthRatio(),
				m_linearGradient
			);
		}
		else if (m_conicGradient.IsValid())
		{
			const Rendering::CircleConicGradientPipeline& circleConicGradientPipeline = pipelines.GetCircleConicGradientPipeline();
			renderCommandEncoder.BindPipeline(circleConicGradientPipeline);
			circleConicGradientPipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace + borderSizeTopLeft,
				endPositionShaderSpace - borderSizeBottomRight,
				angle,
				owner.GetDepthRatio(),
				m_conicGradient
			);
		}
		else if (m_backgroundColor.a > 0)
		{
			const Rendering::CirclePipeline& circlePipeline = pipelines.GetCirclePipeline();
			renderCommandEncoder.BindPipeline(circlePipeline);
			circlePipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace + borderSizeTopLeft,
				endPositionShaderSpace - borderSizeBottomRight,
				angle,
				m_backgroundColor,
				owner.GetDepthRatio()
			);
		}
	}

	void CircleDrawable::OnStyleChanged(
		Widget&,
		const Style::CombinedEntry& styleEntry,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundLinearGradient))
		{
			m_linearGradient =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundLinearGradient, Math::LinearGradient(), matchingModifiers);
			m_conicGradient = {};

			if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
			{
				m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			}
		}
		else if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundConicGradient))
		{
			m_conicGradient =
				styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundConicGradient, Math::LinearGradient(), matchingModifiers);
			m_linearGradient = {};

			if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
			{
				m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			}
		}
		else if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
		{
			m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			m_linearGradient = {};
			m_conicGradient = {};
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BorderColor))
		{
			m_borderColor =
				styleEntry.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BorderColor, "#00000000"_colorf, matchingModifiers);
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BorderThickness))
		{
			m_borderSize = styleEntry.GetWithDefault<Style::SizeAxisEdges>(
				Style::ValueTypeIdentifier::BorderThickness,
				Style::SizeAxisEdges{1_px},
				matchingModifiers
			);
		}

		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Opacity))
		{
			if (const Optional<const Math::Ratiof*> pOpacity = styleEntry.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
			{
				m_backgroundColor.a = *pOpacity;
				if (m_borderColor.a > 0)
				{
					m_borderColor.a = *pOpacity;
				}
			}
		}
	}

	LoadResourcesResult CircleDrawable::TryLoadResources([[maybe_unused]] Widget& owner)
	{
		return LoadResourcesResult::Status::Valid;
	}

	[[maybe_unused]] const bool wasCircleDrawableTypeRegistered = Reflection::Registry::RegisterType<CircleDrawable>();
	[[maybe_unused]] const bool wasCircleDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CircleDrawable>>::Make());
}

namespace ngine::Widgets
{
	//! Helper widget to spawn a circle
	//! Never instantiated, substituted with an asset on disk
	struct CircleWidget final : public Widget
	{
		using BaseType = Widget;
		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::CircleWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::CircleWidget>(
			"77805536-083f-4f07-9534-484a24c40ba0"_guid,
			MAKE_UNICODE_LITERAL("Circle"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{},
			Events{},
			Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"610facbb-d902-dd5c-cd3f-5a0a9c885060"_asset,
				"138411d8-ced1-431d-a5ef-ba44f2960dea"_guid,
				"926e9843-e938-46c7-bb46-1580a18e2032"_asset,
				Reflection::GetTypeGuid<Widgets::Widget>()
			}}
		);
	};
}

namespace ngine::Widgets
{
	[[maybe_unused]] const bool wasCircleTypeRegistered = Reflection::Registry::RegisterType<CircleWidget>();
	[[maybe_unused]] const bool wasCircleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CircleWidget>>::Make());
}
