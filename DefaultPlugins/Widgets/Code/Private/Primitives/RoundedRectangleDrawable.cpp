#include "Widgets/Primitives/RoundedRectangleDrawable.h"

#include <Widgets/ToolWindow.h>
#include <Widgets/Pipelines/Pipelines.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/LoadResourcesResult.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Color.h>
#include <Common/Math/Vector2/MultiplicativeInverse.h>
#include <Common/Math/Primitives/Serialization/RectangleCorners.h>

#include <Engine/Entity/ComponentType.h>

namespace ngine::Widgets::Data::Primitives
{
	RoundedRectangleDrawable::RoundedRectangleDrawable(Initializer&& initializer)
		: m_backgroundColor(initializer.m_backgroundColor)
		, m_cornerRoundingRadiuses(initializer.m_cornerRoundingRadiuses)
		, m_borderColor(initializer.m_borderColor)
		, m_borderSize(initializer.m_borderSize)
		, m_linearGradient(initializer.m_linearGradient)
	{
	}

	bool RoundedRectangleDrawable::ShouldDrawCommands(const Widget&, const Rendering::Pipelines& pipelines) const
	{
		return pipelines.GetRoundedImagePipeline().IsValid();
	}

	void RoundedRectangleDrawable::RecordDrawCommands(
		const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		const Math::Rectangleui renderViewport,
		const Math::Vector2f startPositionShaderSpace,
		const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		const Math::Vector2f contentRatio = Math::MultiplicativeInverse((Math::Vector2f)renderViewport.GetSize());
		const Math::Vector2i ownerSize = owner.GetSize();
		const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
		const float aspectRatio = (float)ownerSize.x / (float)ownerSize.y;
		auto calculateRoundingRadius = [ownerSize, aspectRatio, screenProperties](const Widgets::Style::SizeAxisExpression& size)
		{
			const Math::Vector2f roundingRadius = Math::Min(
				Math::Vector2f{size.GetPoint((float)ownerSize.x, screenProperties), size.GetPoint((float)ownerSize.y, screenProperties)} /
					(Math::Vector2f)ownerSize,
				Math::Vector2f{0.5f}
			);

			if (roundingRadius.x > roundingRadius.y)
			{
				return roundingRadius.x * aspectRatio;
			}
			else
			{
				return roundingRadius.y;
			}
		};

		const Style::Size borderSizeStyleTopLeft = {m_borderSize.m_left, m_borderSize.m_top};
		const Style::Size borderSizeStyleBottomRight = {m_borderSize.m_right, m_borderSize.m_bottom};
		const Math::Vector2f borderSizeTopLeft = contentRatio * (Math::Vector2f)borderSizeStyleTopLeft.Get(ownerSize, screenProperties) * 2.f;
		const Math::Vector2f borderSizeBottomRight = contentRatio *
		                                             (Math::Vector2f)borderSizeStyleBottomRight.Get(ownerSize, screenProperties) * 2.f;

		const Math::RectangleCornersf roundingRadiuses{
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_topLeft),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_topRight),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_bottomRight),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_bottomLeft),
		};
		const Math::WorldRotation2D angle = owner.GetWorldRotation(owner.GetSceneRegistry());

		const bool isUniform = (roundingRadiuses.m_topLeft == roundingRadiuses.m_topRight) &
		                       (roundingRadiuses.m_topRight == roundingRadiuses.m_bottomLeft) &
		                       (roundingRadiuses.m_bottomLeft == roundingRadiuses.m_bottomRight);

		if (m_borderColor.a > 0)
		{
			if (isUniform)
			{
				const Rendering::UniformRoundedBorderRectanglePipeline& uniformRoundedBorderRectanglePipeline =
					pipelines.GetUniformRoundedBorderRectanglePipeline();
				renderCommandEncoder.BindPipeline(uniformRoundedBorderRectanglePipeline);

				uniformRoundedBorderRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace,
					endPositionShaderSpace,
					angle,
					owner.GetDepthRatio(),
					m_borderColor,
					roundingRadiuses.m_topLeft,
					aspectRatio,
					Math::Vector2f(0.5f) - borderSizeTopLeft
				);
			}
			else
			{
				const Rendering::RoundedBorderRectanglePipeline& roundedBorderRectanglePipeline = pipelines.GetRoundedBorderRectanglePipeline();
				renderCommandEncoder.BindPipeline(roundedBorderRectanglePipeline);

				roundedBorderRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace,
					endPositionShaderSpace,
					angle,
					owner.GetDepthRatio(),
					m_borderColor,
					Math::Vector4f{
						roundingRadiuses.m_bottomRight,
						roundingRadiuses.m_topRight,
						roundingRadiuses.m_bottomLeft,
						roundingRadiuses.m_topLeft
					},
					aspectRatio,
					Math::Vector2f(0.5f) - borderSizeTopLeft
				);
			}
		}

		if (m_linearGradient.IsValid())
		{
			if (isUniform)
			{
				const Rendering::UniformRoundedRectangleLinearGradientPipeline& uniformRoundedGradientRectanglePipeline =
					pipelines.GetUniformRoundedGradientRectanglePipeline();
				renderCommandEncoder.BindPipeline(uniformRoundedGradientRectanglePipeline);

				uniformRoundedGradientRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace + borderSizeTopLeft,
					endPositionShaderSpace - borderSizeBottomRight,
					angle,
					owner.GetDepthRatio(),
					m_backgroundColor,
					roundingRadiuses.m_topLeft,
					aspectRatio,
					m_linearGradient
				);
			}
			else
			{
				const Rendering::RoundedRectangleLinearGradientPipeline& roundedGradientRectanglePipeline =
					pipelines.GetRoundedGradientRectanglePipeline();
				renderCommandEncoder.BindPipeline(roundedGradientRectanglePipeline);

				roundedGradientRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace + borderSizeTopLeft,
					endPositionShaderSpace - borderSizeBottomRight,
					angle,
					owner.GetDepthRatio(),
					Math::Vector4f{
						roundingRadiuses.m_bottomRight,
						roundingRadiuses.m_topRight,
						roundingRadiuses.m_bottomLeft,
						roundingRadiuses.m_topLeft
					},
					aspectRatio,
					m_linearGradient
				);
			}
		}
		else if (m_backgroundColor.a > 0)
		{
			if (isUniform)
			{
				const Rendering::UniformRoundedRectanglePipeline& uniformRoundedRectanglePipeline = pipelines.GetUniformRoundedRectanglePipeline();
				renderCommandEncoder.BindPipeline(uniformRoundedRectanglePipeline);

				uniformRoundedRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace + borderSizeTopLeft,
					endPositionShaderSpace - borderSizeBottomRight,
					angle,
					owner.GetDepthRatio(),
					m_backgroundColor,
					roundingRadiuses.m_topLeft,
					aspectRatio
				);
			}
			else
			{
				const Rendering::RoundedRectanglePipeline& roundedRectanglePipeline = pipelines.GetRoundedRectanglePipeline();
				renderCommandEncoder.BindPipeline(roundedRectanglePipeline);

				roundedRectanglePipeline.Draw(
					owner.GetOwningWindow()->GetLogicalDevice(),
					renderCommandEncoder,
					startPositionShaderSpace + borderSizeTopLeft,
					endPositionShaderSpace - borderSizeBottomRight,
					angle,
					owner.GetDepthRatio(),
					m_backgroundColor,
					Math::Vector4f{
						roundingRadiuses.m_bottomRight,
						roundingRadiuses.m_topRight,
						roundingRadiuses.m_bottomLeft,
						roundingRadiuses.m_topLeft
					},
					aspectRatio
				);
			}
		}
	}

	uint32 RoundedRectangleDrawable::GetMaximumPushConstantInstanceCount(const Widget& owner) const
	{
		uint32 count{0};

		const Math::Vector2i ownerSize = owner.GetSize();
		const Rendering::ScreenProperties screenProperties = owner.GetOwningWindow()->GetCurrentScreenProperties();
		const float aspectRatio = (float)ownerSize.x / (float)ownerSize.y;
		auto calculateRoundingRadius = [ownerSize, aspectRatio, screenProperties](const Widgets::Style::SizeAxisExpression& size)
		{
			const Math::Vector2f roundingRadius = Math::Min(
				Math::Vector2f{size.GetPoint((float)ownerSize.x, screenProperties), size.GetPoint((float)ownerSize.y, screenProperties)} /
					(Math::Vector2f)ownerSize,
				Math::Vector2f{0.5f}
			);

			if (roundingRadius.x > roundingRadius.y)
			{
				return roundingRadius.x * aspectRatio;
			}
			else
			{
				return roundingRadius.y;
			}
		};

		const Math::RectangleCornersf roundingRadiuses{
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_topLeft),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_topRight),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_bottomRight),
			calculateRoundingRadius(m_cornerRoundingRadiuses.m_bottomLeft),
		};

		const bool isUniform = (roundingRadiuses.m_topLeft == roundingRadiuses.m_topRight) &
		                       (roundingRadiuses.m_topRight == roundingRadiuses.m_bottomLeft) &
		                       (roundingRadiuses.m_bottomLeft == roundingRadiuses.m_bottomRight);

		if (m_borderColor.a > 0)
		{
			count++;
		}

		if (m_linearGradient.IsValid())
		{
			if (isUniform)
			{
				count++;
			}
			else
			{
				count++;
			}
		}
		else if (m_backgroundColor.a > 0)
		{
			if (isUniform)
			{
				count++;
			}
			else
			{
				count++;
			}
		}

		return count;
	}

	void RoundedRectangleDrawable::OnStyleChanged(
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
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::RoundingRadius))
		{
			m_cornerRoundingRadiuses = styleEntry.GetWithDefault(
				Style::ValueTypeIdentifier::RoundingRadius,
				Style::SizeAxisCorners{(Math::Ratiof)100_percent},
				matchingModifiers
			);
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

	LoadResourcesResult RoundedRectangleDrawable::TryLoadResources([[maybe_unused]] Widget& owner)
	{
		return LoadResourcesResult::Status::Valid;
	}

	[[maybe_unused]] const bool wasRoundedRectangleTypeRegistered = Reflection::Registry::RegisterType<RoundedRectangleDrawable>();
	[[maybe_unused]] const bool wasRoundedRectangleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RoundedRectangleDrawable>>::Make());
}

namespace ngine::Widgets
{
	//! Helper widget to spawn a rounded rectangle
	//! Never instantiated, substituted with an asset on disk
	struct RoundedRectangleWidget final : public Widget
	{
		using BaseType = Widget;
		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::RoundedRectangleWidget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::RoundedRectangleWidget>(
			"67f8d7a2-a430-49e7-ae92-57d57fa0e966"_guid,
			MAKE_UNICODE_LITERAL("Rounded Rectangle"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{},
			Events{},
			Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"e9aaec34-6755-0b15-b8cb-2e8c58507f6c"_asset,
				"138411d8-ced1-431d-a5ef-ba44f2960dea"_guid,
				"71eee20b-aee9-4b47-ba57-536f0f050ab9"_asset,
				Reflection::GetTypeGuid<Widgets::Widget>()
			}}
		);
	};
}

namespace ngine::Widgets
{
	[[maybe_unused]] const bool wasRoundedRectangleTypeRegistered = Reflection::Registry::RegisterType<RoundedRectangleWidget>();
	[[maybe_unused]] const bool wasRoundedRectangleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RoundedRectangleWidget>>::Make());
}
