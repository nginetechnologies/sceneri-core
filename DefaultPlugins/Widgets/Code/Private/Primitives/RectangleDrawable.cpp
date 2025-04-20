#include "Widgets/Primitives/RectangleDrawable.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Pipelines/Pipelines.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/LoadResourcesResult.h>

#include <Common/Math/Color.h>
#include <Common/Math/Vector2/MultiplicativeInverse.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component2D.h>

namespace ngine::Widgets::Data::Primitives
{
	RectangleDrawable::RectangleDrawable(Initializer&& initializer)
		: m_backgroundColor(initializer.m_backgroundColor)
		, m_borderColor(initializer.m_borderColor)
		, m_borderSize(initializer.m_borderSize)
		, m_linearGradient(initializer.m_linearGradient)
	{
	}

	void RectangleDrawable::RecordDrawCommands(
		const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		const Math::Rectangleui renderViewport,
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
			if (borderSizeTopLeft.GetComponentLength() * borderSizeBottomRight.GetComponentLength() != 0)
			{
				const Rendering::BorderRectanglePipeline& borderRectanglePipeline = pipelines.GetBorderRectanglePipeline();
				renderCommandEncoder.BindPipeline(borderRectanglePipeline);

				borderRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace,
					endPositionShaderSpace,
					angle,
					owner.GetDepthRatio(),
					m_borderColor,
					Math::Vector2f(0.5f) - borderSizeTopLeft
				);
			}
			else
			{
				const Rendering::RectanglePipeline& rectanglePipeline = pipelines.GetRectanglePipeline();
				renderCommandEncoder.BindPipeline(rectanglePipeline);

				if (borderSizeTopLeft.x > 0)
				{
					rectanglePipeline.Draw(
						owner.GetOwningWindow()->GetLogicalDevice(),
						renderCommandEncoder,
						startPositionShaderSpace,
						Math::Vector2f{startPositionShaderSpace.x + borderSizeTopLeft.x, endPositionShaderSpace.y},
						angle,
						owner.GetDepthRatio(),
						m_borderColor
					);
				}
				if (borderSizeTopLeft.y > 0)
				{
					rectanglePipeline.Draw(
						owner.GetOwningWindow()->GetLogicalDevice(),
						renderCommandEncoder,
						startPositionShaderSpace,
						Math::Vector2f{endPositionShaderSpace.x, startPositionShaderSpace.y + borderSizeTopLeft.y},
						angle,
						owner.GetDepthRatio(),
						m_borderColor
					);
				}
				if (borderSizeBottomRight.x > 0)
				{
					rectanglePipeline.Draw(
						owner.GetOwningWindow()->GetLogicalDevice(),
						renderCommandEncoder,
						Math::Vector2f{endPositionShaderSpace.x - borderSizeBottomRight.x, startPositionShaderSpace.y},
						endPositionShaderSpace,
						angle,
						owner.GetDepthRatio(),
						m_borderColor
					);
				}
				if (borderSizeBottomRight.y > 0)
				{
					rectanglePipeline.Draw(
						owner.GetOwningWindow()->GetLogicalDevice(),
						renderCommandEncoder,
						Math::Vector2f{
							startPositionShaderSpace.x,
							endPositionShaderSpace.y - borderSizeBottomRight.y,
						},
						endPositionShaderSpace,
						angle,
						owner.GetDepthRatio(),
						m_borderColor
					);
				}
			}
		}

		if (m_linearGradient.IsValid())
		{
			const Rendering::RectangleLinearGradientPipeline& gradientRectanglePipeline = pipelines.GetGradientRectanglePipeline();
			renderCommandEncoder.BindPipeline(gradientRectanglePipeline);

			gradientRectanglePipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace,
				endPositionShaderSpace,
				angle,
				owner.GetDepthRatio(),
				m_linearGradient
			);
		}
		else if (m_backgroundColor.a > 0)
		{
			const Rendering::RectanglePipeline& rectanglePipeline = pipelines.GetRectanglePipeline();
			renderCommandEncoder.BindPipeline(rectanglePipeline);

			rectanglePipeline.Draw(
				owner.GetOwningWindow()->GetLogicalDevice(),
				renderCommandEncoder,
				startPositionShaderSpace + borderSizeTopLeft,
				endPositionShaderSpace - borderSizeBottomRight,
				angle,
				owner.GetDepthRatio(),
				m_backgroundColor
			);
		}
	}

	uint32 RectangleDrawable::GetMaximumPushConstantInstanceCount(const Widget& owner) const
	{
		uint32 count{0};

		const Style::Size borderSizeStyleTopLeft = {m_borderSize.m_left, m_borderSize.m_top};
		const Style::Size borderSizeStyleBottomRight = {m_borderSize.m_right, m_borderSize.m_bottom};
		const Math::Vector2f borderSizeTopLeft =
			(Math::Vector2f)borderSizeStyleTopLeft.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties()) * 2.f;
		const Math::Vector2f borderSizeBottomRight =
			(Math::Vector2f)borderSizeStyleBottomRight.Get(owner.GetSize(), owner.GetOwningWindow()->GetCurrentScreenProperties()) * 2.f;

		if (m_borderColor.a > 0)
		{
			if (borderSizeTopLeft.GetComponentLength() * borderSizeBottomRight.GetComponentLength() != 0)
			{
				count++;
			}
			else
			{
				if (borderSizeTopLeft.x > 0)
				{
					count++;
				}
				if (borderSizeTopLeft.y > 0)
				{
					count++;
				}
				if (borderSizeBottomRight.x > 0)
				{
					count++;
				}
				if (borderSizeBottomRight.y > 0)
				{
					count++;
				}
			}
		}

		if (m_linearGradient.IsValid())
		{
			count++;
		}
		else if (m_backgroundColor.a > 0)
		{
			count++;
		}

		return count;
	}

	void RectangleDrawable::OnStyleChanged(
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

			if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
			{
				m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			}
		}
		else if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
		{
			m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			m_linearGradient = {};
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
				Style::SizeAxisEdges{0_px},
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

	LoadResourcesResult RectangleDrawable::TryLoadResources([[maybe_unused]] Widget& owner)
	{
		return LoadResourcesResult::Status::Valid;
	}

	[[maybe_unused]] const bool wasRectangleDrawableTypeRegistered = Reflection::Registry::RegisterType<RectangleDrawable>();
	[[maybe_unused]] const bool wasRectangleDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RectangleDrawable>>::Make());
}

namespace ngine::Widgets
{
	//! Helper widget to spawn a rectangle
	//! Never instantiated, substituted with an asset on disk
	struct RectangleWidget final : public Widget
	{
		using BaseType = Widget;
		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::RectangleWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::RectangleWidget>(
			"bbba6c89-b5d5-4dee-ab3c-189df9acc097"_guid,
			MAKE_UNICODE_LITERAL("Rectangle"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{},
			Events{},
			Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"0f3e1754-773a-a616-b839-05f81b9d6143"_asset,
				"138411d8-ced1-431d-a5ef-ba44f2960dea"_guid,
				"a1f56f8f-2edd-4b7c-b643-e31bb6f9e251"_asset,
				Reflection::GetTypeGuid<Widgets::Widget>()
			}}
		);
	};
}

namespace ngine::Widgets
{
	[[maybe_unused]] const bool wasRectangleTypeRegistered = Reflection::Registry::RegisterType<RectangleWidget>();
	[[maybe_unused]] const bool wasRectangleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RectangleWidget>>::Make());
}
