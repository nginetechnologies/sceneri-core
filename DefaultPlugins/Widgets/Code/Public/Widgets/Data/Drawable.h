#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/ContentAreaChangeFlags.h>
#include <Widgets/Style/Modifier.h>
#include <Widgets/Style/Size.h>
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>
#include <Widgets/Style/ValueTypeIdentifier.h>

#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>

namespace ngine::Widgets
{
	struct LoadResourcesResult;

	namespace Style
	{
		struct CombinedEntry;
	}
}

namespace ngine::Rendering
{
	struct RenderCommandEncoderView;
	struct Pipelines;
}

namespace ngine::Widgets::Data
{
	struct Drawable : public Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;
		using BaseType = Component;

		using BaseType::BaseType;
		virtual ~Drawable() = default;

		[[nodiscard]] virtual bool ShouldDrawCommands(const Widget&, const Rendering::Pipelines&) const
		{
			return true;
		}
		virtual void RecordDrawCommands(
			const Widget& owner,
			const Rendering::RenderCommandEncoderView renderCommandEncoder,
			const Math::Rectangleui,
			const Math::Vector2f startPositionShaderSpace,
			const Math::Vector2f endPositionShaderSpace,
			const Rendering::Pipelines& pipelines
		) const = 0;

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount(const Widget& owner) const = 0;

		using ConstChangedStyleValuesView = const Bitset<(uint8)Style::ValueTypeIdentifier::Count>&;
		virtual void OnStyleChanged(
			Widget& owner,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		) = 0;
		virtual void OnContentAreaChanged([[maybe_unused]] Widget& owner, const EnumFlags<ContentAreaChangeFlags>)
		{
		}
		[[nodiscard]] virtual LoadResourcesResult TryLoadResources(Widget& owner) = 0;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Drawable>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Drawable>(
			"CB3B22CC-BC54-43AB-87D1-95F7D8B9074A"_guid,
			MAKE_UNICODE_LITERAL("Widget Drawable"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
