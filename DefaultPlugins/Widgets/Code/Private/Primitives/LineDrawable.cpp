#include "Widgets/Primitives/LineDrawable.h"

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
	LineDrawable::LineDrawable(Initializer&& initializer)
		: m_spline(Move(initializer.m_spline))
		, m_thickness(initializer.m_thickness)
		, m_lineColor(initializer.m_lineColor)
	{
	}

	void LineDrawable::RecordDrawCommands(
		[[maybe_unused]] const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const Math::Rectangleui renderViewport,
		const Math::Vector2f startPositionShaderSpace,
		const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		const Math::WorldRotation2D angle = owner.GetWorldRotation(owner.GetSceneRegistry());

		const Rendering::LinePipeline& linePipeline = pipelines.GetLinePipeline();
		renderCommandEncoder.BindPipeline(linePipeline);

		const Math::Vector2i ownerSize = owner.GetSize();
		const float aspectRatio = (float)ownerSize.x / (float)ownerSize.y;

		linePipeline.Draw(
			owner.GetOwningWindow()->GetLogicalDevice(),
			renderCommandEncoder,
			startPositionShaderSpace,
			endPositionShaderSpace,
			angle,
			owner.GetDepthRatio(),
			m_spline,
			m_lineColor,
			m_thickness,
			aspectRatio
		);
	}

	uint32 LineDrawable::GetMaximumPushConstantInstanceCount([[maybe_unused]] const Widget& owner) const
	{
		uint32 count{0};

		if (m_lineColor.a > 0)
		{
			count++;
		}
		// TODO: What is the correct value here?
		return count;
	}

	void LineDrawable::OnStyleChanged(
		Widget&,
		const Style::CombinedEntry& styleEntry,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
		{
			m_lineColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Opacity))
		{
			if (const Optional<const Math::Ratiof*> pOpacity = styleEntry.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
			{
				m_lineColor.a = *pOpacity;
			}
		}
	}

	LoadResourcesResult LineDrawable::TryLoadResources([[maybe_unused]] Widget& owner)
	{
		return LoadResourcesResult::Status::Valid;
	}

	[[maybe_unused]] const bool wasLineDrawableTypeRegistered = Reflection::Registry::RegisterType<LineDrawable>();
	[[maybe_unused]] const bool wasLineDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LineDrawable>>::Make());
}
