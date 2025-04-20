#include "Widgets/Primitives/GridDrawable.h"

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
	GridDrawable::GridDrawable(Initializer&& initializer)
		: m_backgroundColor(initializer.m_backgroundColor)
		, m_gridColor(initializer.m_gridColor)
	{
	}

	void GridDrawable::RecordDrawCommands(
		[[maybe_unused]] const Widget& owner,
		const Rendering::RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const Math::Rectangleui renderViewport,
		const Math::Vector2f startPositionShaderSpace,
		const Math::Vector2f endPositionShaderSpace,
		const Rendering::Pipelines& pipelines
	) const
	{
		const Math::WorldRotation2D angle = owner.GetWorldRotation(owner.GetSceneRegistry());

		const Rendering::RectangleGridPipeline& rectangleGridPipeline = pipelines.GetRectangleGridPipeline();
		renderCommandEncoder.BindPipeline(rectangleGridPipeline);

		const Math::Vector2i ownerSize = owner.GetSize();
		const float aspectRatio = (float)ownerSize.x / (float)ownerSize.y;
		rectangleGridPipeline.Draw(
			owner.GetOwningWindow()->GetLogicalDevice(),
			renderCommandEncoder,
			startPositionShaderSpace,
			endPositionShaderSpace,
			angle,
			owner.GetDepthRatio(),
			m_backgroundColor,
			m_gridColor,
			m_offset,
			aspectRatio
		);
	}

	uint32 GridDrawable::GetMaximumPushConstantInstanceCount([[maybe_unused]] const Widget& owner) const
	{
		uint32 count{0};

		if (m_backgroundColor.a > 0)
		{
			count++;
		}
		// TODO: What is the correct value here?
		return count + 2;
	}

	void GridDrawable::OnStyleChanged(
		Widget&,
		const Style::CombinedEntry& styleEntry,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::BackgroundColor))
		{
			m_backgroundColor = styleEntry.GetWithDefault(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
		}
		if (changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::Opacity))
		{
			if (const Optional<const Math::Ratiof*> pOpacity = styleEntry.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
			{
				m_backgroundColor.a = *pOpacity;
			}
		}
	}

	LoadResourcesResult GridDrawable::TryLoadResources([[maybe_unused]] Widget& owner)
	{
		return LoadResourcesResult::Status::Valid;
	}

	[[maybe_unused]] const bool wasGridDrawableTypeRegistered = Reflection::Registry::RegisterType<GridDrawable>();
	[[maybe_unused]] const bool wasGridDrawableComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<GridDrawable>>::Make());
}
