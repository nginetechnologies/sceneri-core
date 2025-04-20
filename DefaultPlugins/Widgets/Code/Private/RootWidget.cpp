#include "RootWidget.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Scene/SceneQuadtreeNode.h>
#include <Engine/Context/Context.h>

#include <Widgets/ToolWindow.h>
#include <Widgets/WidgetScene.h>
#include <Widgets/Style/CombinedEntry.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets
{
	[[nodiscard]] UniquePtr<Widgets::Style::Entry> GetRootWidgetStyle()
	{
		UniquePtr<Widgets::Style::Entry> pStyle{Memory::ConstructInPlace};
		Widgets::Style::Entry::ModifierValues& modifier = pStyle->EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
		modifier.ParseFromCSS("width: 100%; height: 100%;");
		pStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
		pStyle->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());
		return pStyle;
	}

	RootWidget::RootWidget(Initializer&& initializer)
		: Widget(
				initializer.m_parent.GetSceneRegistry(),
				*this,
				initializer.m_pOwningWindow.IsValid() ? initializer.m_pOwningWindow->GetClientAreaSize() : Math::Vector2ui{1920, 1080},
				initializer.m_pOwningWindow,
				initializer.m_parent.AsExpected<Entity::Component2D>(),
				Widget::Flags::IsInputDisabled,
				GetRootWidgetStyle()
			)
		, m_pOwningWindow(initializer.m_pOwningWindow)
		, m_pRecalculateWidgetsHierarchyStage(UniqueRef<Threading::Job>::FromRaw(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					ProcessWidgetHierarchyRecalculation();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::WidgetHierarchyRecalculation,
				"Recalculate Widget Hierarchy"
			)))
	{
#if PROFILE_BUILD
		m_debugName = "Root";
#endif

		// Ensure each window's root widget has a unique event's context
		Entity::ComponentTypeSceneData<Context::Data::Component>& contextSceneData =
			initializer.m_parent.GetSceneRegistry().GetCachedSceneData<Context::Data::Component>();
		[[maybe_unused]] Optional<Context::Data::Component*> pContext =
			CreateDataComponent(contextSceneData, Context::Data::Component::Initializer{});
	}

	RootWidget::~RootWidget() = default;

	[[nodiscard]] Widget* RootWidget::GetWidgetAtCoordinate(
		const WindowCoordinate coordinates, Optional<uint16> touchRadius, const EnumFlags<Widget::Flags> disallowedFlags
	)
	{
		const int16 diameter = touchRadius.IsValid() ? *touchRadius * 2 : 1;
		const Math::Rectanglei touchArea{(Math::Vector2i)coordinates - Math::Vector2i{diameter / 2}, Math::Vector2i{diameter}};

		struct PixelInfo
		{
			Widget* pBestWidget = nullptr;
			uint16 bestComputedDepthIndex = 0;
		};
		InlineVector<PixelInfo, 128> pixels(Memory::ConstructWithSize, Memory::Zeroed, diameter * diameter);

		struct Result
		{
			Widget* pBestTouchWidget = nullptr;
			float bestWidgetMaskedTouchAreaSize = 0;
		} result;

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();

		using IterateNode = void (*)(
			const SceneQuadtreeNode::BaseType& node,
			const Math::Rectanglef touchArea,
			const ArrayView<PixelInfo, uint32> pixels,
			const EnumFlags<Widget::Flags> disallowedFlags,
			Entity::SceneRegistry& sceneRegistry,
			ToolWindow& window,
			Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData
		);
		static IterateNode iterateNode = [](
																			 const SceneQuadtreeNode::BaseType& node,
																			 const Math::Rectanglef touchArea,
																			 const ArrayView<PixelInfo, uint32> pixels,
																			 const EnumFlags<Widget::Flags> disallowedFlags,
																			 Entity::SceneRegistry& sceneRegistry,
																			 ToolWindow& window,
																			 Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData
																		 )
		{
			for (const SceneQuadtreeNode::StoredElement& elementNode : node.GetElementView())
			{
				const Optional<Widget*> pWidget = elementNode.m_element->As<Widget>(sceneRegistry);
				if (UNLIKELY_ERROR(pWidget.IsInvalid()))
				{
					continue;
				}
				Widgets::Widget& widget = *pWidget;

				const Optional<Entity::Data::Flags*> pComponentFlags = componentFlagsSceneData.GetComponentImplementation(widget.GetIdentifier());
				const EnumFlags<Entity::ComponentFlags> componentFlags = pComponentFlags != nullptr
				                                                           ? (EnumFlags<Entity::ComponentFlags>)*pComponentFlags
				                                                           : EnumFlags<Entity::ComponentFlags>{};
				if (componentFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource | Entity::ComponentFlags::IsDestroying))
				{
					continue;
				}

				const Math::Rectanglei maskedTouchArea = widget.GetMaskedContentArea(sceneRegistry, window, (Math::Rectanglei)touchArea);

				if (widget.GetFlags().AreNoneSet(disallowedFlags |
	Widgets::Widget::Flags::IsIgnoredFromAnySource | Widgets::Widget::Flags::IsHiddenFromAnySource) && maskedTouchArea.HasSize())
				{
					const uint16 computedDepth = widget.GetDepthInfo().GetComputedDepthIndex();

					const uint32 diameter = Math::Sqrt(pixels.GetSize());
					for (int32 x = 0; x < maskedTouchArea.GetSize().x; ++x)
					{
						for (int32 y = 0; y < maskedTouchArea.GetSize().y; ++y)
						{
							const uint32 index = x * diameter + y;
							PixelInfo& pixel = pixels[index];
							if (computedDepth > pixel.bestComputedDepthIndex)
							{
								pixel.pBestWidget = &widget;
								pixel.bestComputedDepthIndex = computedDepth;
							}
						}
					}
				}
			}

			node.IterateChildren(
				[touchArea, pixels, disallowedFlags, &sceneRegistry, &window, &componentFlagsSceneData](SceneQuadtreeNode::BaseType& childNode)
				{
					if (childNode.GetChildContentArea().Overlaps(touchArea))
					{
						iterateNode(childNode, touchArea, pixels, disallowedFlags, sceneRegistry, window, componentFlagsSceneData);
					}
				}
			);
		};

		iterateNode(
			GetRootSceneComponent().GetQuadTree(),
			(Math::Rectanglef)touchArea,
			pixels.GetView(),
			disallowedFlags,
			sceneRegistry,
			*m_pOwningWindow,
			componentFlagsSceneData
		);

		for (const PixelInfo& pixel : pixels)
		{
			if (pixel.pBestWidget != nullptr)
			{
				const Math::Rectanglei widgetContentArea = pixel.pBestWidget->GetContentArea();

				const Math::Rectanglei maskedTouchArea = pixel.pBestWidget->GetMaskedContentArea(sceneRegistry, *m_pOwningWindow, touchArea);
				const float maskedTouchAreaSize = (float)maskedTouchArea.GetSize().GetComponentLength() /
				                                  (float)widgetContentArea.GetSize().GetComponentLength();

				if (maskedTouchAreaSize > result.bestWidgetMaskedTouchAreaSize)
				{
					result.pBestTouchWidget = pixel.pBestWidget;
					result.bestWidgetMaskedTouchAreaSize = maskedTouchAreaSize;
				}
			}
		}

		return result.pBestTouchWidget;
	}

	void RootWidget::ProcessWidgetHierarchyRecalculation()
	{
		if (m_flags.TryClearFlags(Flags::HasQueuedWidgetRecalculations))
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>(
			);

			for (const Entity::ComponentIdentifier::IndexType widgetIndex : m_queuedWidgetRecalculationsMask.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(widgetIndex);
				m_queuedWidgetRecalculationsMask.Clear(componentIdentifier);

				const Entity::ComponentSoftReference softReference = m_queuedWidgetRecalculations[componentIdentifier];
				if (const Optional<Widgets::Widget*> pWidget = softReference.Find<Widgets::Widget>(sceneRegistry))
				{
					const Optional<Entity::Data::Flags*> pComponentFlags = componentFlagsSceneData.GetComponentImplementation(pWidget->GetIdentifier()
					);
					const EnumFlags<Entity::ComponentFlags> componentFlags = pComponentFlags != nullptr
					                                                           ? (EnumFlags<Entity::ComponentFlags>)*pComponentFlags
					                                                           : EnumFlags<Entity::ComponentFlags>{};
					if (componentFlags.AreAnySet(Entity::ComponentFlags::IsDestroying))
					{
						continue;
					}

					pWidget->RecalculateHierarchyInternal(sceneRegistry, *m_pOwningWindow);
				}
			}
		}
	}

	void RootWidget::OnWidgetRemoved(Widgets::Widget& widget)
	{
		const Entity::ComponentIdentifier componentIdentifier = widget.GetIdentifier();
		m_queuedWidgetRecalculationsMask.Clear(componentIdentifier);
		m_queuedWidgetRecalculations[componentIdentifier] = {};
	}

	void RootWidget::QueueRecalculateWidgetHierarchy(Widget& widget)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		for (Widget* pWidget = &widget; pWidget != nullptr; pWidget = pWidget->GetParentSafe())
		{
			if (m_queuedWidgetRecalculationsMask.IsSet(pWidget->GetIdentifier()))
			{
				return;
			}
		}

		using RecurseChildren = void (*)(RootWidget&, Widget&);
		static RecurseChildren recurseChildren = [](RootWidget& rootWidget, Widget& widget)
		{
			for (Widget& child : widget.GetChildren())
			{
				[[maybe_unused]] const bool wasCleared = rootWidget.m_queuedWidgetRecalculationsMask.Clear(child.GetIdentifier());

				recurseChildren(rootWidget, child);
			}
		};
		recurseChildren(*this, widget);

		m_queuedWidgetRecalculations[widget.GetIdentifier()] = Entity::ComponentSoftReference{widget, sceneRegistry};
		m_queuedWidgetRecalculationsMask.Set(widget.GetIdentifier());

		m_flags |= Flags::HasQueuedWidgetRecalculations;
	}

	[[maybe_unused]] const bool wasRootWidgetTypeRegistered = Reflection::Registry::RegisterType<RootWidget>();
	[[maybe_unused]] const bool wasRootWidgetComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RootWidget>>::Make());
}
