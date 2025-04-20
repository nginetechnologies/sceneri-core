#include "Widget.h"
#include "RootWidget.h"
#include "Widget.inl"
#include "WidgetScene.h"
#include <Widgets/Manager.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/DynamicEntry.h>
#include <Widgets/Style/ComputedStylesheet.h>
#include <Widgets/Style/StylesheetCache.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Style/SizeCorners.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Primitives/RectangleDrawable.h>
#include <Widgets/Primitives/RoundedRectangleDrawable.h>
#include <Widgets/Primitives/CircleDrawable.h>
#include <Widgets/Primitives/LineDrawable.h>
#include <Widgets/Primitives/GridDrawable.h>
#include <Widgets/Data/ImageDrawable.h>
#include <Widgets/Data/TextDrawable.h>
#include <Widgets/Data/Layout.h>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/DataSourceEntry.h>
#include <Widgets/Data/ContextMenu.h>
#include <Widgets/Data/ContextMenuEntries.h>
#include <Widgets/Data/ContentAreaListener.h>
#include <Widgets/Data/EventData.h>
#include <Widgets/Data/SubscribedEvents.h>
#include <Widgets/Data/Stylesheet.h>
#include <Widgets/Data/ExternalStyle.h>
#include <Widgets/Data/InlineStyle.h>
#include <Widgets/Data/DynamicStyle.h>
#include <Widgets/Data/Modifiers.h>
#include <Widgets/Documents/DocumentWidget.h>
#include <Widgets/Documents/DocumentWindow.h>
#include <Widgets/LoadResourcesResult.h>

#include "Data/Layout.h"
#include "Data/GridLayout.h"
#include "Data/Drawable.h"
#include "Data/Input.h"

#include <Engine/Asset/Serialization/Mask.h>
#include <Engine/Asset/AssetTypeEditorExtension.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/DynamicDataSource.h>
#include <Engine/DataSource/DynamicPropertySource.h>
#include <Engine/DataSource/PropertySourceInterface.h>
#include <Engine/DataSource/DataSourceState.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyMask.h>
#include <Engine/Scene/Scene2D.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/RootComponent.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/Data/WorldTransform2D.h>
#include <Engine/Entity/Data/LocalTransform2D.h>
#include <Engine/Entity/Data/ParentComponent.h>
#include <Engine/Entity/Data/QuadtreeNode.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Data/PreferredIndex.h>
#include <Engine/Entity/Data/ExternalScene.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/Data/RenderItem/TransformChangeTracker.h>
#include <Engine/Context/EventManager.inl>
#include <Engine/Context/Utils.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Renderer.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Commands/RenderCommandEncoderView.h>
#include <Renderer/Window/DocumentData.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Scene/SceneView.h>

#include <FontRendering/Manager.h>

#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Math/Color.h>
#include <Common/Math/Transform2D.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Serialization/MergedReader.h>
#include <Common/Format/Guid.h>
#include <Common/Asset/Picker.h>
#include <Common/IO/URI.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Memory/DynamicBitset.h>
#include <Common/Function/CopyableFunction.h>
#include <Common/Platform/MustTail.h>
#include <Common/Platform/OpenInFileBrowser.h>
#include <Common/Format/Guid.h>
#include <Common/System/Query.h>
#include <Common/IO/Log.h>

namespace ngine::Widgets
{
	Widget::Initializer::Initializer(Widget& parent, const EnumFlags<Flags> flags, UniquePtr<Style::Entry>&& pInlineStyle)
		: BaseType(parent, Math::Identity, {})
		, m_widgetFlags(flags)
		, m_pInlineStyle(Forward<UniquePtr<Style::Entry>>(pInlineStyle))
	{
	}
	Widget::Initializer::Initializer(DynamicInitializer&& dynamicInitializer)
		: BaseType(Forward<DynamicInitializer>(dynamicInitializer))
	{
	}
	Widget::Initializer::Initializer(Initializer&& other) = default;
	Widget::Initializer::~Initializer() = default;

	Widget::Widget(
		Entity::SceneRegistry& sceneRegistry,
		RootWidget& thisRootWidget,
		const Math::Vector2ui size,
		const Optional<Rendering::ToolWindow*> pOwningWindow,
		Component2D& parent,
		const EnumFlags<Flags> flags,
		UniquePtr<Style::Entry>&& pInlineStyle
	)
		: Component2D(
				sceneRegistry.AcquireNewComponentIdentifier(),
				parent,
				parent.GetRootSceneComponent(),
				sceneRegistry,
				Math::WorldTransform2D{0_degrees, Math::Vector2f{Math::Zero}, (Math::Vector2f)size},
				Guid::Generate()
			)
		, m_rootWidget(thisRootWidget)
		, m_flags(flags)
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::ExternalStyle>();
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::Modifiers>();

		m_depthInfo = DepthInfo{
			DepthInfo(),
			DepthInfo(),
			GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData),
			GetActiveModifiers(sceneRegistry, modifiersSceneData)
		};

		Assert(&thisRootWidget == this);
		if (pInlineStyle.IsValid())
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::InlineStyle>(
				inlineStyleSceneData,
				Data::InlineStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, sceneRegistry},
					Forward<UniquePtr<Style::Entry>>(pInlineStyle)
				}
			);
			Assert(jobBatch.IsInvalid());
		}

		if (pOwningWindow.IsValid())
		{
			const Rendering::ScreenProperties screenProperties = pOwningWindow->GetCurrentScreenProperties();
			ApplyInitialStyle(
				sceneRegistry,
				screenProperties,
				*pOwningWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::ImageDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::CircleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::GridDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::LineDrawable>();

			ApplyStyleInternal(
				sceneRegistry,
				*pOwningWindow,
				screenProperties,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}
	}

	EnumFlags<Widget::Flags> GetInheritedFlags(Widget& parent)
	{
		const EnumFlags<Widget::Flags> parentFlags = parent.GetFlags();
		EnumFlags<Widget::Flags> flags;
		flags |= Widget::Flags::IsParentHidden * parentFlags.AreAnySet(Widget::Flags::IsHiddenFromAnySource);

		flags |= Widget::Flags::IsToggledOffFromParent *
		         parentFlags.AreAnySet(Widget::Flags::IsToggledOff | Widget::Flags::IsToggledOffFromParent);
		flags |= Widget::Flags::FailedValidationFromParent *
		         parentFlags.AreAnySet(Widget::Flags::FailedValidation | Widget::Flags::FailedValidationFromParent);
		flags |= Widget::Flags::HasRequirementsFromParent *
		         parentFlags.AreAnySet(Widget::Flags::HasRequirements | Widget::Flags::HasRequirementsFromParent);
		flags |= Widget::Flags::HasCustomFramegraphFromParent *
		         parentFlags.AreAnySet(Widget::Flags::HasCustomFramegraph | Widget::Flags::HasCustomFramegraphFromParent);

		return flags;
	}

	[[nodiscard]] LayoutType GetLayoutFromStyle(
		const Style::CombinedEntry& style, const Style::CombinedEntry::MatchingModifiersView matchingModifiers, const Widget& widget
	)
	{
		for (const Widget* pWidget = &widget; pWidget != nullptr; pWidget = pWidget->GetParentSafe())
		{
			if (pWidget->GetFlags().AreAnySet(Widget::Flags::IsIgnoredFromAnySource))
			{
				return LayoutType::None;
			}
		}

		return style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
	}

	namespace Data
	{
		Widget& Component::Initializer::GetParent() const
		{
			return static_cast<Widget&>(BaseType::GetParent());
		}

		Widget& Component::Cloner::GetParent() const
		{
			return static_cast<Widget&>(BaseType::GetParent());
		}

		Widget& Component::Deserializer::GetParent() const
		{
			return static_cast<Widget&>(BaseType::GetParent());
		}
	}

	[[nodiscard]] Math::Vector2i GetClampedSize(
		const Math::Vector2i requestedSize,
		Rendering::ToolWindow& owningWindow,
		Entity::SceneRegistry& sceneRegistry,
		const Rendering::ScreenProperties screenProperties,
		Widget* pParent,
		const Style::Entry& style,
		const Style::Entry::MatchingModifiersView matchingModifiers
	)
	{
		const Math::Vector2i parentAvailableContentArea = pParent != nullptr
		                                                    ? pParent->GetAvailableChildContentArea(sceneRegistry, owningWindow).GetSize()
		                                                    : (Math::Vector2i)owningWindow.GetClientAreaSize();

		return Math::Clamp(
			requestedSize,
			style.GetSize(Style::ValueTypeIdentifier::MinimumSize, Math::Zero, matchingModifiers)
				.Get(parentAvailableContentArea, screenProperties),
			style
				.GetSize(
					Style::ValueTypeIdentifier::MaximumSize,
					Style::Size{Style::SizeAxis{Style::NoMaximum}, Style::SizeAxis{Style::NoMaximum}},
					matchingModifiers
				)
				.Get(parentAvailableContentArea, screenProperties)
		);
	}

	Widget::Widget(Widget& parent, const Math::Rectanglei contentArea, const EnumFlags<Flags> flags, UniquePtr<Style::Entry>&& pInlineStyle)
		: Component2D(Component2D::Initializer{
				parent,
				Math::WorldTransform2D{
					0_degrees,
					(Math::Vector2f)contentArea.GetPosition(),
					(Math::Vector2f)GetClampedSize(
						contentArea.GetSize(),
						*parent.GetOwningWindow(),
						parent.GetSceneRegistry(),
						parent.GetOwningWindow()->GetCurrentScreenProperties(),
						&parent,
						pInlineStyle.IsValid() ? *pInlineStyle : Style::Entry::GetDummy(),
						(pInlineStyle.IsValid() ? *pInlineStyle : Style::Entry::GetDummy()).GetMatchingModifiers(Style::Modifier::None).GetView()
					)
				},
				Entity::ComponentFlags{},
				Guid::Generate()
			})
		, m_rootWidget(parent.m_rootWidget)
		, m_flags(flags | GetInheritedFlags(parent))
	{
		Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		m_depthInfo = DepthInfo{
			parent.m_depthInfo,
			parent.GetLastNestedChildOrOwnDepthInfo(),
			GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData),
			GetActiveModifiers(sceneRegistry, modifiersSceneData)
		};

		Assert(GetSize(sceneRegistry) >= Math::Zero);
		if (pInlineStyle.IsValid())
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::InlineStyle>(
				inlineStyleSceneData,
				Data::InlineStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, sceneRegistry},
					Forward<UniquePtr<Style::Entry>>(pInlineStyle)
				}
			);
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();
			ApplyInitialStyle(
				sceneRegistry,
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::ImageDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::CircleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::GridDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::LineDrawable>();

			ApplyStyleInternal(
				sceneRegistry,
				*pWindow,
				screenProperties,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}
	}

	[[nodiscard]] Math::Vector2i GetInitialPreferredSize(
		Rendering::ToolWindow& owningWindow,
		const Rendering::ScreenProperties screenProperties,
		Widget* pParent,
		const Style::CombinedEntry& style
	)
	{
		return style
		  .GetSize(
				Style::ValueTypeIdentifier::PreferredSize,
				Style::Size{(Math::Ratiof)100_percent, (Math::Ratiof)0_percent},
				style.GetMatchingModifiers(Style::Modifier::None)
			)
		  .Get(pParent != nullptr ? pParent->GetSize() : (Math::Vector2i)owningWindow.GetClientAreaSize(), screenProperties);
	}

	Widget::Widget(Initializer&& initializer)
		: Component2D(Component2D::Initializer{
				*initializer.GetParent(), Math::WorldTransform2D{Math::Identity}, Entity::ComponentFlags{}, Guid::Generate()
			})
		, m_rootWidget(initializer.GetParent()->m_rootWidget)
		, m_flags(initializer.m_widgetFlags | GetInheritedFlags(*initializer.GetParent()))
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
			*initializer.GetSceneRegistry().FindComponentTypeData<Data::ExternalStyle>();
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData =
			*initializer.GetSceneRegistry().FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
			*initializer.GetSceneRegistry().FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData =
			*initializer.GetSceneRegistry().FindComponentTypeData<Data::Modifiers>();

		m_depthInfo = DepthInfo{
			initializer.GetParent()->m_depthInfo,
			initializer.GetParent()->GetLastNestedChildOrOwnDepthInfo(),
			GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData),
			GetActiveModifiers(initializer.GetSceneRegistry(), modifiersSceneData)
		};

		if (initializer.m_pInlineStyle.IsValid())
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::InlineStyle>(
				inlineStyleSceneData,
				Data::InlineStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, initializer.GetSceneRegistry()},
					Move(initializer.m_pInlineStyle)
				}
			);
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();
			ApplyInitialStyle(
				initializer.GetSceneRegistry(),
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			ApplyStyleInternal(initializer.GetSceneRegistry(), *pWindow, screenProperties);
		}
	}

	EnumFlags<Widget::Flags> GetClonedFlags(const Widget& templateWidget)
	{
		const EnumFlags<Widget::Flags> templateFlags = templateWidget.GetFlags();
		EnumFlags<Widget::Flags> flags;
		flags |= Widget::Flags::IsHidden * templateFlags.IsSet(Widget::Flags::IsHidden);
		flags |= Widget::Flags::IsInputDisabled * templateFlags.IsSet(Widget::Flags::IsInputDisabled);
		flags |= Widget::Flags::IsAssetRootWidget * templateFlags.IsSet(Widget::Flags::IsAssetRootWidget);
		flags |= Widget::Flags::BlockPointerInputs * templateFlags.IsSet(Widget::Flags::BlockPointerInputs);
		flags |= Widget::Flags::IsIgnored * templateFlags.IsSet(Widget::Flags::IsIgnored);
		flags |= Widget::Flags::IsToggledOff * templateFlags.IsSet(Widget::Flags::IsToggledOff);
		flags |= Widget::Flags::FailedValidation * templateFlags.IsSet(Widget::Flags::FailedValidation);
		flags |= Widget::Flags::HasRequirements * templateFlags.IsSet(Widget::Flags::HasRequirements);
		flags |= Widget::Flags::HasCustomFramegraph * templateFlags.IsSet(Widget::Flags::HasCustomFramegraph);

		return flags;
	}

	Widget::Widget(const Widget& templateWidget, const Cloner& cloner)
		: Component2D(templateWidget, cloner)
#if PROFILE_BUILD
		, m_debugName(templateWidget.m_debugName)
#endif
		, m_rootWidget(cloner.GetParent()->m_rootWidget)
		, m_flags(GetClonedFlags(templateWidget) | GetInheritedFlags(*cloner.GetParent()))
		, m_windowTitlePropertyIdentifier(templateWidget.m_windowTitlePropertyIdentifier)
		, m_windowUriPropertyIdentifier(templateWidget.m_windowUriPropertyIdentifier)
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
			*cloner.GetSceneRegistry().FindComponentTypeData<Data::ExternalStyle>();
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData =
			*cloner.GetSceneRegistry().FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
			*cloner.GetSceneRegistry().FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *cloner.GetSceneRegistry().FindComponentTypeData<Data::Modifiers>(
		);

		m_depthInfo = DepthInfo{
			cloner.GetParent()->m_depthInfo,
			cloner.GetParent()->GetLastNestedChildOrOwnDepthInfo(),
			GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData),
			GetActiveModifiers(cloner.GetSceneRegistry(), modifiersSceneData)
		};

		Entity::ComponentTypeSceneData<Context::Data::Component>& templateContextSceneData =
			cloner.GetTemplateSceneRegistry().GetCachedSceneData<Context::Data::Component>();
		if (const Optional<const Context::Data::Component*> pTemplateContext = templateContextSceneData.GetComponentImplementation(templateWidget.GetIdentifier()))
		{
			Entity::ComponentTypeSceneData<Context::Data::Component>& contextSceneData =
				cloner.GetSceneRegistry().GetCachedSceneData<Context::Data::Component>();
			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Context::Data::Component*> pContext = CreateDataComponent<Context::Data::Component>(
				contextSceneData,
				*pTemplateContext,
				Context::Data::Component::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
			);
			Assert(pContext.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::Modifiers>*> pModifiersDataSourceSceneData = cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::Modifiers>())
		{
			if (const Optional<const Data::Modifiers*> pTemplateModifiers = pModifiersDataSourceSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::Modifiers*> pModifiers = CreateDataComponent<Data::Modifiers>(
					modifiersSceneData,
					*pTemplateModifiers,
					Data::Modifiers::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pModifiers.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::DataSource>*> pTemplateDataSourceSceneData = cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::DataSource>())
		{
			if (const Optional<const Data::DataSource*> pTemplateDataSource = pTemplateDataSourceSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData =
					*cloner.GetSceneRegistry().GetOrCreateComponentTypeData<Data::DataSource>();
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::DataSource*> pDataSource = CreateDataComponent<Data::DataSource>(
					dataSourceSceneData,
					*pTemplateDataSource,
					Data::DataSource::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pDataSource.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::PropertySource>*> pTemplatePropertySourceSceneData =
			cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::PropertySource>())
		{
			if (const Optional<const Data::PropertySource*> pTemplatePropertySource = pTemplatePropertySourceSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData =
					*cloner.GetSceneRegistry().FindComponentTypeData<Data::PropertySource>();
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::PropertySource*> pPropertySource = CreateDataComponent<Data::PropertySource>(
					propertySourceSceneData,
					*pTemplatePropertySource,
					Data::PropertySource::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pPropertySource.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::EventData>*> pTemplateEventDataSceneData = cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::EventData>())
		{
			if (const Optional<const Data::EventData*> pTemplateEventData = pTemplateEventDataSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Entity::ComponentTypeSceneData<Data::EventData>& eventDataSceneData =
					*cloner.GetSceneRegistry().GetOrCreateComponentTypeData<Data::EventData>();
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::EventData*> pEventData = CreateDataComponent<Data::EventData>(
					eventDataSceneData,
					*pTemplateEventData,
					Data::EventData::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pEventData.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::SubscribedEvents>*> pTemplateSubscribedEventsSceneData = cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::SubscribedEvents>())
		{
			if (const Optional<const Data::SubscribedEvents*> pTemplateSubscribedEvents = pTemplateSubscribedEventsSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Entity::ComponentTypeSceneData<Data::SubscribedEvents>& subscribedEventsSceneData =
					*cloner.GetSceneRegistry().GetOrCreateComponentTypeData<Data::SubscribedEvents>();
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::SubscribedEvents*> pSubscribedEvents = CreateDataComponent<Data::SubscribedEvents>(
					subscribedEventsSceneData,
					*pTemplateSubscribedEvents,
					Data::SubscribedEvents::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pSubscribedEvents.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		if (const Optional<Entity::ComponentTypeSceneData<Data::ContextMenuEntries>*> pTemplateContextMenuEntriesSceneData =
			cloner.GetTemplateSceneRegistry().FindComponentTypeData<Data::ContextMenuEntries>())
		{
			if (const Optional<const Data::ContextMenuEntries*> pTemplateContextMenuEntries = pTemplateContextMenuEntriesSceneData->GetComponentImplementation(templateWidget.GetIdentifier()))
			{
				Entity::ComponentTypeSceneData<Data::ContextMenuEntries>& contextMenuEntriesSceneData =
					*cloner.GetSceneRegistry().GetOrCreateComponentTypeData<Data::ContextMenuEntries>();
				Threading::JobBatch jobBatch;
				[[maybe_unused]] const Optional<Data::ContextMenuEntries*> pContextMenuEntries = CreateDataComponent<Data::ContextMenuEntries>(
					contextMenuEntriesSceneData,
					*pTemplateContextMenuEntries,
					Data::ContextMenuEntries::Cloner{jobBatch, *this, templateWidget, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
				);
				Assert(pContextMenuEntries.IsValid());
				Assert(jobBatch.IsInvalid());
			}
		}

		const Optional<Data::ExternalStyle*> pExternalStyle = FindDataComponentOfType<Data::ExternalStyle>(externalStyleSceneData);
		const Optional<const Widgets::Style::Entry*> pExternalStyleEntry = pExternalStyle.IsValid() ? pExternalStyle->GetEntry()
		                                                                                            : Optional<const Widgets::Style::Entry*>{};
		const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(inlineStyleSceneData);
		const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData);
		if ((pExternalStyleEntry.IsValid() && pExternalStyleEntry->HasElements()) || pInlineStyle.IsValid() || pDynamicStyle.IsValid())
		{
			const Style::CombinedEntry& style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
			[[maybe_unused]] const EnumFlags<DepthInfo::ChangeFlags> depthInfoChangeFlags =
				m_depthInfo.OnStyleChanged(style, style.GetMatchingModifiers(GetActiveModifiers(cloner.GetSceneRegistry(), modifiersSceneData)));
			Assert(depthInfoChangeFlags.AreNoneSet(DepthInfo::ChangeFlags::ShouldRecalculateSubsequentDepthInfo) || !HasChildren());
		}

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();
			ApplyInitialStyle(
				cloner.GetSceneRegistry(),
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			ApplyStyleInternal(cloner.GetSceneRegistry(), *pWindow, screenProperties);
		}
	}

	Optional<Rendering::ToolWindow*> Widget::GetOwningWindow() const
	{
		return m_rootWidget->GetOwningWindow();
	}

	PURE_STATICS bool Widget::IsRootWidget() const
	{
		return this == &m_rootWidget;
	}

	/* static */ EventInfo Widget::DeserializeEventInfo(const ConstStringView eventString)
	{
		if (const Optional<Guid> guid = Guid::TryParse(eventString))
		{
			return *guid;
		}
		else
		{
			if (eventString.GetSize() > 2 && eventString[0] == '{' && eventString.GetLastElement() == '}')
			{
				const ConstStringView propertyName = eventString.GetSubstring(1, eventString.GetSize() - 2);
				if (propertyName == "this_widget_parent_asset_root_instance_guid")
				{
					return EventInstanceGuidType::ThisWidgetParentAssetRootInstanceGuid;
				}
				else if (propertyName == "this_widget_asset_root_instance_guid")
				{
					return EventInstanceGuidType::ThisWidgetAssetRootInstanceGuid;
				}
				else if (propertyName == "this_widget_parent_instance_guid")
				{
					return EventInstanceGuidType::ThisWidgetParentInstanceGuid;
				}
				else if (propertyName == "this_widget_instance_guid")
				{
					return EventInstanceGuidType::ThisWidgetInstanceGuid;
				}
				else
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
					return dataSourceCache.FindOrRegisterPropertyIdentifier(propertyName);
				}
			}
		}
		Assert(false);
		return {};
	}

	/* static */ String Widget::GetEventInfoKey(const EventInfo& eventInfo)
	{
		return eventInfo.Visit(
			[](const Guid guid) -> String
			{
				return guid.ToString();
			},
			[](const EventInstanceGuidType instanceGuidType) -> String
			{
				switch (instanceGuidType)
				{
					case EventInstanceGuidType::ThisWidgetParentAssetRootInstanceGuid:
						return "{this_widget_parent_asset_root_instance_guid}";
					case EventInstanceGuidType::ThisWidgetAssetRootInstanceGuid:
						return "{this_widget_asset_root_instance_guid}";
					case EventInstanceGuidType::ThisWidgetParentInstanceGuid:
						return "{this_widget_parent_instance_guid}";
					case EventInstanceGuidType::ThisWidgetInstanceGuid:
						return "{this_widget_instance_guid}";
				}
				ExpectUnreachable();
			},
			[](const ngine::DataSource::PropertyIdentifier propertyIdentifier) -> String
			{
				ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(propertyIdentifier));
				return format;
			},
			[]() -> String
			{
				ExpectUnreachable();
			}
		);
	}

	/* static */ EventInfo Widget::DeserializeEventInfo(const Serialization::Reader reader)
	{
		if (const Optional<Guid> guid = reader.ReadInPlace<Guid>())
		{
			return *guid;
		}
		else if (const Optional<ConstStringView> eventString = reader.ReadInPlace<ConstStringView>())
		{
			return DeserializeEventInfo(*eventString);
		}
		else
		{
			Assert(false);
			return {};
		}
	}

	Widget::Widget(const Deserializer& deserializer)
		: Component2D(Component2D::Initializer{
				*deserializer.GetParent(),
				Math::WorldTransform2D{
					0_degrees, deserializer.GetParent()->GetWorldLocation(deserializer.GetSceneRegistry()), Math::Vector2f{Math::Zero}
				},
				Entity::ComponentFlags::SaveToDisk,
				deserializer.m_reader.ReadWithDefaultValue<Guid>("instanceGuid", Guid::Generate()),
			})
		, m_rootWidget(deserializer.GetParent()->m_rootWidget)
		, m_flags(
				deserializer.m_widgetFlags | Flags::IsInputDisabled | GetInheritedFlags(*deserializer.GetParent()) |
				(Flags::IsAssetRootWidget * deserializer.m_reader.HasSerializer("guid"))
			)
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Data::ExternalStyle>();
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Data::Modifiers>();

		m_depthInfo = DepthInfo{
			deserializer.GetParent()->m_depthInfo,
			deserializer.GetParent()->GetLastNestedChildOrOwnDepthInfo(),
			GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData),
			GetActiveModifiers(deserializer.GetSceneRegistry(), modifiersSceneData)
		};

		if (const Optional<Asset::Guid> externalWidgetAssetGuid = deserializer.m_reader.Read<Asset::Guid>("asset"))
		{
			Entity::SceneRegistry& sceneRegistry = deserializer.GetSceneRegistry();
			Entity::Manager& entityManager = System::Get<Entity::Manager>();
			Entity::ComponentTemplateCache& sceneTemplateCache = entityManager.GetComponentTemplateCache();
			const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(*externalWidgetAssetGuid);
			Assert(sceneTemplateIdentifier.IsValid());

			[[maybe_unused]] const Optional<Entity::Data::ExternalScene*> pExternalScene =
				CreateDataComponent<Entity::Data::ExternalScene>(sceneRegistry, sceneTemplateIdentifier);
			Assert(pExternalScene.IsValid());
		}
		else if (const Optional<Asset::Guid> rootWidgetAssetGuid = deserializer.m_reader.Read<Asset::Guid>("guid"))
		{
			Entity::SceneRegistry& sceneRegistry = deserializer.GetSceneRegistry();
			Entity::Manager& entityManager = System::Get<Entity::Manager>();
			Entity::ComponentTemplateCache& sceneTemplateCache = entityManager.GetComponentTemplateCache();
			const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(*rootWidgetAssetGuid);
			Assert(sceneTemplateIdentifier.IsValid());

			[[maybe_unused]] const Optional<Entity::Data::ExternalScene*> pExternalScene =
				CreateDataComponent<Entity::Data::ExternalScene>(sceneRegistry, sceneTemplateIdentifier);
			Assert(pExternalScene.IsValid());
		}

		if (deserializer.m_preferredChildIndex.IsValid())
		{
			Entity::ComponentTypeSceneData<Entity::Data::PreferredIndex>& preferredIndexSceneData =
				deserializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::PreferredIndex>();
			[[maybe_unused]] const Optional<Entity::Data::PreferredIndex*> pPreferredIndex =
				CreateDataComponent<Entity::Data::PreferredIndex>(preferredIndexSceneData, *deserializer.m_preferredChildIndex);
			Assert(pPreferredIndex.IsValid());
		}

		if (const Optional<ConstStringView> stylesheetEntryString = deserializer.m_reader.Read<ConstStringView>("style_entry"))
		{
			Guid stylesheetEntryGuid;
			if (const Optional<Guid> parsedEntryGuid = Guid::TryParse(*stylesheetEntryString))
			{
				stylesheetEntryGuid = *parsedEntryGuid;
			}
			else
			{
				// Generate a guid from string
				// These are not guaranteed to be globally unique, but for local css files it's considered good enough
				const Guid cssEntryBaseGuid = "4DAD5E5B-B94C-438E-A16E-F001420DD049"_guid;
				stylesheetEntryGuid = Guid::FromString(*stylesheetEntryString, cssEntryBaseGuid);
			}

			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::ExternalStyle*> pExternalStyle = CreateDataComponent<Data::ExternalStyle>(
				externalStyleSceneData,
				Data::ExternalStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, deserializer.GetSceneRegistry()},
					stylesheetEntryGuid
				}
			);
			Assert(pExternalStyle.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<ConstStringView> inlineStyle = deserializer.m_reader.Read<ConstStringView>("style"))
		{
			UniquePtr<Style::Entry> pInlineStyle = UniquePtr<Style::Entry>::Make();

			Style::Entry::ModifierValues& modifier = pInlineStyle->EmplaceExactModifierMatch(Style::Modifier::None);
			modifier.ParseFromCSS(*inlineStyle);
			pInlineStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
			pInlineStyle->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());

			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::InlineStyle*> pInlineStyleComponent = CreateDataComponent<Data::InlineStyle>(
				inlineStyleSceneData,
				Data::InlineStyle::Initializer{Data::Component::Initializer{jobBatch, *this, deserializer.GetSceneRegistry()}, Move(pInlineStyle)}
			);
			Assert(pInlineStyleComponent.IsValid());
			Assert(jobBatch.IsInvalid());
		}
		if (const Optional<ConstStringView> inlineDynamicStyle = deserializer.m_reader.Read<ConstStringView>("dynamic_style"))
		{
			UniquePtr<Style::DynamicEntry> pDynamicStyle = UniquePtr<Style::DynamicEntry>::Make();

			Style::Entry::ModifierValues& modifier = pDynamicStyle->m_dynamicEntry.EmplaceExactModifierMatch(Style::Modifier::None);
			modifier.ParseFromCSS(*inlineDynamicStyle);
			pDynamicStyle->m_dynamicEntry.OnValueTypesAdded(modifier.GetValueTypeMask());
			pDynamicStyle->m_dynamicEntry.OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());

			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::DynamicStyle*> pInlineStyleComponent = CreateDataComponent<Data::DynamicStyle>(
				dynamicStyleSceneData,
				Data::DynamicStyle::Initializer{Data::Component::Initializer{jobBatch, *this, deserializer.GetSceneRegistry()}, Move(pDynamicStyle)}
			);
			Assert(pInlineStyleComponent.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

		if (deserializer.m_reader.ReadWithDefaultValue<bool>("new_context", false))
		{
			Entity::ComponentTypeSceneData<Context::Data::Component>& layoutSceneData =
				deserializer.GetSceneRegistry().GetCachedSceneData<Context::Data::Component>();

			[[maybe_unused]] Optional<Context::Data::Component*> pContext =
				CreateDataComponent(layoutSceneData, Context::Data::Component::Initializer{});
		}

		if (deserializer.m_reader.HasSerializer("disabled") ||
			deserializer.m_reader.HasSerializer("hidden") ||
			deserializer.m_reader.HasSerializer("active") ||
			deserializer.m_reader.HasSerializer("toggled") ||
			deserializer.m_reader.HasSerializer("ignored") ||
			deserializer.m_reader.HasSerializer("required") ||
			deserializer.m_reader.HasSerializer("valid") ||
			deserializer.m_reader.HasSerializer("block_pointer_inputs"))
		{
			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::Modifiers*> pModifiers = DataComponentOwner::CreateDataComponent<Data::Modifiers>(
				modifiersSceneData,
				Data::Modifiers::Deserializer{
					deserializer.m_reader,
					System::Get<Reflection::Registry>(),
					jobBatch,
					*this,
					deserializer.GetSceneRegistry()
				}
			);
			Assert(pModifiers.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<Serialization::Reader> windowTitleReader = deserializer.m_reader.FindSerializer("window_title"))
		{
			if (const Optional<ngine::DataSource::PropertyIdentifier> propertyIdentifier = windowTitleReader->ReadInPlace<ngine::DataSource::PropertyIdentifier>(dataSourceCache))
			{
				m_windowTitlePropertyIdentifier = *propertyIdentifier;
			}
			else if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
			{
				const ConstStringView newWindowTitle = windowTitleReader->ReadInPlaceWithDefaultValue<ConstStringView>({});
				pWindow->SetTitle(NativeString(newWindowTitle));
			}
			else
			{
				Assert(false, "TODO: Make sure this is cloned");
			}
		}

		if (const Optional<Serialization::Reader> windowUriReader = deserializer.m_reader.FindSerializer("window_uri"))
		{
			if (const Optional<ngine::DataSource::PropertyIdentifier> propertyIdentifier = windowUriReader->ReadInPlace<ngine::DataSource::PropertyIdentifier>(dataSourceCache))
			{
				m_windowUriPropertyIdentifier = *propertyIdentifier;
			}
			else if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
			{
				const ConstStringView newWindowUri = windowUriReader->ReadInPlaceWithDefaultValue<ConstStringView>({});
				pWindow->SetUri(IO::URI::Combine(IO::Path::GetExecutableDirectory().GetView().GetStringView(), IO::URI(newWindowUri)));
			}
			else
			{
				Assert(false, "TODO: Make sure this is cloned");
			}
		}

		if (const Optional<Serialization::Reader> dataSourceReader = deserializer.m_reader.FindSerializer("data_source"))
		{
			[[maybe_unused]] const Optional<Data::DataSource*> pDataSource = DataComponentOwner::CreateDataComponent<Data::DataSource>(
				deserializer.GetSceneRegistry(),
				*this,
				*dataSourceReader,
				deserializer.m_reader
			);
			Assert(pDataSource.IsValid());
		}

		if (deserializer.m_reader.HasSerializer("notify_events"))
		{
			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::EventData*> pEventData = DataComponentOwner::CreateDataComponent<Data::EventData>(
				deserializer.GetSceneRegistry(),
				Data::EventData::Deserializer{
					deserializer.m_reader,
					System::Get<Reflection::Registry>(),
					jobBatch,
					*this,
					deserializer.GetSceneRegistry()
				}
			);
			Assert(pEventData.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (deserializer.m_reader.HasSerializer("events"))
		{
			Threading::JobBatch jobBatch;
			[[maybe_unused]] const Optional<Data::SubscribedEvents*> pSubscribedEvents =
				DataComponentOwner::CreateDataComponent<Data::SubscribedEvents>(
					deserializer.GetSceneRegistry(),
					Data::SubscribedEvents::Deserializer{
						deserializer.m_reader,
						System::Get<Reflection::Registry>(),
						jobBatch,
						*this,
						deserializer.GetSceneRegistry()
					}
				);
			Assert(pSubscribedEvents.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (const Optional<Serialization::Reader> contextMenuEntriesReader = deserializer.m_reader.FindSerializer("context_menu_entries"))
		{
			Threading::JobBatch jobBatch;
			[[maybe_unused]] Optional<Data::ContextMenuEntries*> pContextMenuEntries = CreateDataComponent<Data::ContextMenuEntries>(
				deserializer.GetSceneRegistry(),
				Data::ContextMenuEntries::Deserializer{
					*contextMenuEntriesReader,
					System::Get<Reflection::Registry>(),
					jobBatch,
					*this,
					deserializer.GetSceneRegistry()
				}
			);
			Assert(pContextMenuEntries.IsValid());
			Assert(jobBatch.IsInvalid());
		}

		if (Optional<Tag::Mask> tagMask = deserializer.m_reader.Read<Tag::Mask>("tags", System::Get<Tag::Registry>());
		    tagMask.IsValid() && tagMask->AreAnySet())
		{
			CreateDataComponent<Entity::Data::Tags>(
				deserializer.GetSceneRegistry(),
				Entity::Data::Tags::Initializer{
					Entity::Data::Tags::BaseType::DynamicInitializer{*this, deserializer.GetSceneRegistry()},
					Move(*tagMask)
				}
			);
		}

		const Optional<Data::ExternalStyle*> pExternalStyle = FindDataComponentOfType<Data::ExternalStyle>(externalStyleSceneData);
		const Optional<const Widgets::Style::Entry*> pExternalStyleEntry = pExternalStyle.IsValid() ? pExternalStyle->GetEntry()
		                                                                                            : Optional<const Widgets::Style::Entry*>{};
		const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(inlineStyleSceneData);
		const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData);
		if ((pExternalStyleEntry.IsValid() && pExternalStyleEntry->HasElements()) || pInlineStyle.IsValid() || pDynamicStyle.IsValid())
		{
			const Style::CombinedEntry& style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
			[[maybe_unused]] const EnumFlags<DepthInfo::ChangeFlags> depthInfoChangeFlags =
				m_depthInfo.OnStyleChanged(style, style.GetMatchingModifiers(GetActiveModifiers(deserializer.GetSceneRegistry())));
			Assert(depthInfoChangeFlags.AreNoneSet(DepthInfo::ChangeFlags::ShouldRecalculateSubsequentDepthInfo) || !HasChildren());
		}

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();
			ApplyInitialStyle(
				deserializer.GetSceneRegistry(),
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			ApplyStyleInternal(deserializer.GetSceneRegistry(), *pWindow, screenProperties);
		}
	}

	Widget::Widget(Widget& parent, const Coordinate position, const EnumFlags<Flags> flags, UniquePtr<Style::Entry>&& pInlineStyle)
		: Widget(
				parent,
				Math::Rectanglei{
					position,
					GetInitialPreferredSize(
						*parent.GetOwningWindow(),
						parent.GetOwningWindow()->GetCurrentScreenProperties(),
						&parent,
						pInlineStyle.IsValid() ? *pInlineStyle : Style::Entry::GetDummy()
					)
				},
				flags,
				Forward<UniquePtr<Style::Entry>>(pInlineStyle)
			)
	{
	}

	Widget::~Widget()
	{
		Assert(IsDisabled());

		// Make sure there are no queued events that will trigger on this widget
		Threading::UniqueLock lock(m_deferredEventMutex);
	}

	void Widget::ApplyStyle(Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& window)
	{
		// Make sure we automatically apply style again in the future
		m_flags |= Flags::ShouldAutoApplyStyle;

		const Rendering::ScreenProperties screenProperties = window.GetCurrentScreenProperties();
		ApplyStyleInternal(sceneRegistry, window, screenProperties);
	}

	[[nodiscard]] Style::SizeAxisEdges GetEdgesFromStyle(
		const Style::CombinedEntry& entry,
		const Style::ValueTypeIdentifier styleEntryIdentifier,
		const Style::CombinedEntry::MatchingModifiersView matchingModifiers
	)
	{
		const Optional<const Style::EntryValue*> value = entry.Find(styleEntryIdentifier, matchingModifiers);
		if (value.IsValid())
		{
			if (const Optional<const Style::Size*> pSize = value->Get<Style::Size>())
			{
				const Style::Size size = *pSize;
				return {size.x, size.y, size.x, size.y};
			}
			else if (const Optional<const Style::SizeAxisEdges*> pEdges = value->Get<Style::SizeAxisEdges>())
			{
				return *pEdges;
			}
		}

		return Style::SizeAxisEdges{Math::Zero};
	}

	void Widget::ApplyInitialStyle(
		Entity::SceneRegistry& sceneRegistry,
		const Rendering::ScreenProperties screenProperties,
		Rendering::ToolWindow& window,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	)
	{
		Math::Vector2i parentAvailableSize;
		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry, modifiersSceneData);

			const Style::CombinedEntry parentStyle = pParent->GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
			const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);

			const Style::SizeAxisEdges parentPaddingStyle =
				GetEdgesFromStyle(parentStyle, Style::ValueTypeIdentifier::Padding, parentMatchingModifiers.GetView());
			const Math::Vector2i parentParentSize = pParent->GetParentSize(sceneRegistry, window);

			const Math::RectangleEdgesi padding{
				parentPaddingStyle.m_top.Get(parentParentSize.y, screenProperties),
				parentPaddingStyle.m_right.Get(parentParentSize.x, screenProperties),
				parentPaddingStyle.m_bottom.Get(parentParentSize.y, screenProperties),
				parentPaddingStyle.m_left.Get(parentParentSize.x, screenProperties),
			};

			parentAvailableSize = Math::Max(pParent->GetSize(sceneRegistry) - padding.GetSum(), (Math::Vector2i)Math::Zero);
		}
		else
		{
			parentAvailableSize = (Math::Vector2i)window.GetClientAreaSize();
		}

		const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

		Math::Vector2i smallestChildSize{Math::Zero};
		Math::Vector2i largestChildSize{Math::Zero};
		if (ChildView childView = GetChildren(); childView.HasElements())
		{
			const Widget& firstChild = childView[0];
			const Math::Vector2i firstChildSize = firstChild.GetSize(sceneRegistry);
			smallestChildSize = largestChildSize = firstChildSize;

			for (ChildView::IteratorType it = childView.begin() + 1, endIt = childView.end(); it < endIt; ++it)
			{
				const Widget& child = *it;
				const Math::Vector2i childSize = child.GetSize(sceneRegistry);
				smallestChildSize = Math::Min(smallestChildSize, childSize);
				largestChildSize = Math::Max(largestChildSize, childSize);
			}
		}

		Math::Vector2i size =
			GetStylePreferredSize(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData)
				.Get(parentAvailableSize, smallestChildSize, largestChildSize, screenProperties);
		const Math::Vector2i minimumSize = style.GetSize(Style::ValueTypeIdentifier::MinimumSize, Style::Size{Math::Zero}, matchingModifiers)
		                                     .Get(parentAvailableSize, smallestChildSize, largestChildSize, screenProperties);
		const Math::Vector2i maximumSize = style
		                                     .GetSize(
																					 Style::ValueTypeIdentifier::MaximumSize,
																					 Style::Size{Style::SizeAxis{Style::NoMaximum}, Style::SizeAxis{Style::NoMaximum}},
																					 matchingModifiers
																				 )
		                                     .Get(parentAvailableSize, smallestChildSize, largestChildSize, screenProperties);
		size = Math::Clamp(size, minimumSize, maximumSize);

		const Math::Rectanglei parentContentArea = GetParentAvailableChildContentArea(sceneRegistry, window);
		Math::Rectanglei newContentArea{};

		switch (style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers))
		{
			case PositionType::Static:
			{
				Assert(!style.Contains(Style::ValueTypeIdentifier::Position, matchingModifiers));

				if (const Optional<Widget*> pParent = GetParentSafe())
				{
					if (const Optional<Data::FlexLayout*> pParentFlexLayout = pParent->FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
					{
						newContentArea = {pParentFlexLayout->GetInitialChildLocation(*pParent, sceneRegistry, *this, size), size};
					}
					else if (const Optional<Data::GridLayout*> pParentGridLayout = pParent->FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
					{
						newContentArea = {pParentGridLayout->GetInitialChildLocation(*pParent, sceneRegistry, *this, size), size};
					}
					else
					{
						newContentArea = {parentContentArea.GetPosition(), size};
					}
				}
				else
				{
					newContentArea = {parentContentArea.GetPosition(), size};
				}
			}
			break;
			case PositionType::Relative:
			{
				if (const Optional<Widget*> pParent = GetParentSafe())
				{
					if (const Optional<Data::FlexLayout*> pParentFlexLayout = pParent->FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
					{
						newContentArea = {pParentFlexLayout->GetInitialChildLocation(*pParent, sceneRegistry, *this, size), size};
					}
					else if (const Optional<Data::GridLayout*> pParentGridLayout = pParent->FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
					{
						newContentArea = {pParentGridLayout->GetInitialChildLocation(*pParent, sceneRegistry, *this, size), size};
					}
					else
					{
						Math::Vector2i relativePosition = Math::Zero;
						const Style::SizeAxisEdges positionOffsetEdges = style.GetWithDefault<Style::SizeAxisEdges>(
							Style::ValueTypeIdentifier::Position,
							Style::SizeAxisEdges{Invalid},
							matchingModifiers
						);
						if (positionOffsetEdges.m_left.IsValid())
						{
							relativePosition.x = positionOffsetEdges.m_left.Get(parentAvailableSize.x, screenProperties);
						}
						else if (positionOffsetEdges.m_right.IsValid())
						{
							relativePosition.x = -positionOffsetEdges.m_right.Get(parentAvailableSize.x, screenProperties);
						}
						if (positionOffsetEdges.m_top.IsValid())
						{
							relativePosition.y = positionOffsetEdges.m_top.Get(parentAvailableSize.y, screenProperties);
						}
						else if (positionOffsetEdges.m_bottom.IsValid())
						{
							relativePosition.y = -positionOffsetEdges.m_bottom.Get(parentAvailableSize.y, screenProperties);
						}

						newContentArea = {relativePosition + parentContentArea.GetPosition(), size};
					}
				}
				else
				{
					Math::Vector2i relativePosition = Math::Zero;
					const Style::SizeAxisEdges positionOffsetEdges = style.GetWithDefault<Style::SizeAxisEdges>(
						Style::ValueTypeIdentifier::Position,
						Style::SizeAxisEdges{Invalid},
						matchingModifiers
					);
					if (positionOffsetEdges.m_left.IsValid())
					{
						relativePosition.x = positionOffsetEdges.m_left.Get(parentAvailableSize.x, screenProperties);
					}
					else if (positionOffsetEdges.m_right.IsValid())
					{
						relativePosition.x = -positionOffsetEdges.m_right.Get(parentAvailableSize.x, screenProperties);
					}
					if (positionOffsetEdges.m_top.IsValid())
					{
						relativePosition.y = positionOffsetEdges.m_top.Get(parentAvailableSize.y, screenProperties);
					}
					else if (positionOffsetEdges.m_bottom.IsValid())
					{
						relativePosition.y = -positionOffsetEdges.m_bottom.Get(parentAvailableSize.y, screenProperties);
					}

					newContentArea = {relativePosition + parentContentArea.GetPosition(), size};
				}
			}
			break;
			case PositionType::Absolute:
			{
				auto getParentPadding = [](
																	Entity::SceneRegistry& sceneRegistry,
																	const Optional<const Widget*> pParent,
																	const Rendering::ScreenProperties screenProperties,
																	Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
																	Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
																	Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
																	Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
																) -> Math::RectangleEdgesi
				{
					if (pParent.IsValid())
					{
						const Style::CombinedEntry parentStyle = pParent->GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
						const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry, modifiersSceneData);
						const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);

						const Style::SizeAxisEdges parentPaddingStyle =
							GetEdgesFromStyle(parentStyle, Style::ValueTypeIdentifier::Padding, parentMatchingModifiers);
						const Math::Vector2i parentSize = pParent->GetSize();
						return Math::RectangleEdgesi{
							parentPaddingStyle.m_top.Get(parentSize.y, screenProperties),
							parentPaddingStyle.m_right.Get(parentSize.x, screenProperties),
							parentPaddingStyle.m_bottom.Get(parentSize.y, screenProperties),
							parentPaddingStyle.m_left.Get(parentSize.x, screenProperties)
						};
					}
					else
					{
						return Math::RectangleEdgesi{0};
					}
				};
				const Math::RectangleEdgesi parentPadding = getParentPadding(
					sceneRegistry,
					GetParentSafe(),
					screenProperties,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData
				);

				const Style::SizeAxisEdges positionOffsetEdges = style.GetWithDefault<Style::SizeAxisEdges>(
					Style::ValueTypeIdentifier::Position,
					Style::SizeAxisEdges{Invalid},
					matchingModifiers
				);

				Math::Vector2i relativePosition = Math::Zero;
				if (positionOffsetEdges.m_left.IsValid())
				{
					relativePosition.x = parentPadding.m_left + positionOffsetEdges.m_left.Get(parentAvailableSize.x, screenProperties);
				}
				else if (positionOffsetEdges.m_right.IsValid())
				{
					const float ratio = (float)positionOffsetEdges.m_right.Get(parentAvailableSize.x, screenProperties) /
					                    (float)parentAvailableSize.x;
					relativePosition.x = int32(float(parentAvailableSize.x - size.x) * (1.f - ratio)) - parentPadding.m_right;
				}
				if (positionOffsetEdges.m_top.IsValid())
				{
					relativePosition.y = parentPadding.m_top + positionOffsetEdges.m_top.Get(parentAvailableSize.y, screenProperties);
				}
				else if (positionOffsetEdges.m_bottom.IsValid())
				{
					const float ratio = (float)positionOffsetEdges.m_bottom.Get(parentAvailableSize.y, screenProperties) /
					                    (float)parentAvailableSize.y;
					relativePosition.y = int32((float(parentAvailableSize.y - size.y) * (1.f - ratio))) - parentPadding.m_bottom;
				}

				newContentArea = {relativePosition + parentContentArea.GetPosition(), size};
			}
			break;
			case PositionType::Dynamic:
				newContentArea = {GetPosition(sceneRegistry), size};
				break;
		}

		SetWorldTransform(
			Math::WorldTransform2D{0_degrees, (Math::Vector2f)newContentArea.GetPosition(), (Math::Vector2f)newContentArea.GetSize()},
			sceneRegistry
		);
	}

	void Widget::ApplyStyleInternal(
		Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& window, const Rendering::ScreenProperties screenProperties
	)
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::ImageDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::LineDrawable>();

		ApplyStyleInternal(
			sceneRegistry,
			window,
			screenProperties,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);
	}

	void Widget::ApplyStyleInternal(
		Entity::SceneRegistry& sceneRegistry,
		Rendering::ToolWindow& window,
		const Rendering::ScreenProperties screenProperties,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

		const bool requiresInputEvents = style.Contains(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers) ||
		                                 style.Contains(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers) ||
		                                 style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) ||
		                                 style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers) ||
		                                 style.HasAnyModifiers(Style::Modifier::Active | Style::Modifier::Hover);
		m_flags &= ~(Flags::IsInputDisabled * requiresInputEvents);

		const LayoutType layout =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		switch (layout)
		{
			case LayoutType::None:
			{
				const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsIgnoredFromStyle);
				if (previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource))
				{
					OnIgnoredInternal(sceneRegistry, window);
				}

				if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
				{
					OnBecomeHiddenInternal(sceneRegistry);
				}
			}
			break;
			default:
			{
				const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsIgnoredFromStyle);
				if (previousFlags.IsSet(Flags::IsIgnoredFromStyle) & previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource & ~Flags::IsIgnoredFromStyle))
				{
					OnUnignoredInternal(sceneRegistry, window);
				}

				if (previousFlags.IsSet(Flags::IsIgnoredFromStyle) & previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsIgnoredFromStyle))
				{
					OnBecomeVisibleInternal(sceneRegistry, window);
				}
			}
			break;
		}

		if (!style.GetWithDefault(Style::ValueTypeIdentifier::Visibility, true, matchingModifiers))
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsHiddenFromStyle);
			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
			{
				OnBecomeHiddenInternal(sceneRegistry);
			}
		}
		else
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsHiddenFromStyle);
			if (previousFlags.IsSet(Flags::IsHiddenFromStyle) & previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsHiddenFromStyle))
			{
				OnBecomeVisibleInternal(sceneRegistry, window);
			}
		}

		ApplyDrawablePrimitive(
			GetSize(sceneRegistry),
			style,
			matchingModifiers,
			screenProperties,
			sceneRegistry,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);

		if (Entity::HierarchyComponentBase::GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsConstructing))
		{
			RecalculateHierarchy(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData);
		}
	}

	void Widget::ApplyDrawablePrimitive(
		const Math::Vector2i size,
		const Style::CombinedEntry style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const Rendering::ScreenProperties screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		if (const Optional<const Asset::Guid*> assetGuid = style.Find<Asset::Guid>(Style::ValueTypeIdentifier::AssetIdentifier, matchingModifiers))
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			Guid assetTypeGuid = assetManager.GetAssetTypeGuid(*assetGuid);
			if (assetTypeGuid.IsInvalid())
			{
				assetTypeGuid = assetManager.GetAssetLibrary().GetAssetTypeGuid(*assetGuid);
			}
			if (assetTypeGuid == TextureAssetType::AssetFormat.assetTypeGuid)
			{
				Math::Color color = style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::Color, "#FFFFFF"_colorf, matchingModifiers);
				if (const Optional<const Math::Ratiof*> pOpacity = style.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
				{
					color.a = *pOpacity;
				}

				Style::SizeAxisCorners roundingRadiusCorners = style.GetWithDefault<Style::SizeAxisCorners>(
					Style::ValueTypeIdentifier::RoundingRadius,
					Style::SizeAxisCorners{},
					matchingModifiers
				);
				const Style::SizeAxisExpressionView roundingRadius = roundingRadiusCorners.m_topLeft;
				const Rendering::TextureIdentifier textureIdentifier =
					System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(*assetGuid);

				// Create a image drawable
				if (const Optional<Data::ImageDrawable*> pImageDrawable = FindDataComponentOfType<Data::ImageDrawable>(imageDrawableSceneData))
				{
					pImageDrawable->SetColor(color);
					pImageDrawable->SetRoundingRadius(roundingRadius);
					if (pImageDrawable->GetTextureIdentifier() != textureIdentifier)
					{
						pImageDrawable->ChangeImage(*this, textureIdentifier);
					}
				}
				else
				{
					RemoveDrawable(
						sceneRegistry,
						textDrawableSceneData,
						imageDrawableSceneData,
						roundedRectangleDrawableSceneData,
						rectangleDrawableSceneData,
						circleDrawableSceneData,
						gridDrawableSceneData,
						lineDrawableSceneData
					);

					DataComponentOwner::CreateDataComponent<Data::ImageDrawable>(
						imageDrawableSceneData,
						Data::ImageDrawable::Initializer{Data::Component::Initializer{*this, sceneRegistry}, color, textureIdentifier, roundingRadius}
					);

					OnDrawableAdded(sceneRegistry);
				}
			}
			else
			{
				// TODO: Support scenes etc from CSS
			}
		}
		else if (const Optional<const Style::EntryValue*> text = style.Find(Style::ValueTypeIdentifier::Text, matchingModifiers))
		{
			if (text.IsValid())
			{
				if (const Optional<const UnicodeString*> textValue = text->Get<UnicodeString>())
				{
					auto findOrRegisterFont = [](const Asset::Guid fontGuid) -> Font::Identifier
					{
						Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
						Font::Cache& fontCache = fontManager.GetCache();
						return fontCache.FindOrRegisterAsset(fontGuid);
					};

					// Create a text drawable
					if (!HasDataComponentOfType(sceneRegistry, textDrawableSceneData.GetIdentifier()))
					{
						RemoveDrawable(
							sceneRegistry,
							textDrawableSceneData,
							imageDrawableSceneData,
							roundedRectangleDrawableSceneData,
							rectangleDrawableSceneData,
							circleDrawableSceneData,
							gridDrawableSceneData,
							lineDrawableSceneData
						);
						ResetLoadedResources();

						Math::Color color = style.GetWithDefault<Math::Color>(
							Style::ValueTypeIdentifier::Color,
							Math::Color(Data::TextDrawable::DefaultColor),
							matchingModifiers
						);
						if (const Optional<const Math::Ratiof*> pOpacity = style.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
						{
							color.a = *pOpacity;
						}

						Style::SizeAxis fontSize =
							style.GetWithDefault<Style::SizeAxis>(Style::ValueTypeIdentifier::PointSize, 16_px, matchingModifiers);

						DataComponentOwner::CreateDataComponent<Data::TextDrawable>(
							textDrawableSceneData,
							Data::TextDrawable::Initializer{
								Data::Component::Initializer{*this, sceneRegistry},
								UnicodeString(*textValue),
								findOrRegisterFont(style.GetWithDefault<Asset::Guid>(
									Style::ValueTypeIdentifier::FontAsset,
									Asset::Guid(Data::TextDrawable::DefaultFontAssetGuid),
									matchingModifiers
								)),
								fontSize,
								style.GetWithDefault(Style::ValueTypeIdentifier::LineHeight, Style::SizeAxis{120_percent}, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::FontWeight, Font::Weight{Font::DefaultFontWeight}, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::WordWrapType, WordWrapType::Normal, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::FontModifiers, EnumFlags<Font::Modifier>(), matchingModifiers),
								color,
								style.GetWithDefault(Style::ValueTypeIdentifier::HorizontalAlignment, Widgets::Alignment::Start, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::VerticalAlignment, Widgets::Alignment::Center, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::TextOverflowType, Widgets::TextOverflowType::Clip, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::WhiteSpaceType, Widgets::WhiteSpaceType::Normal, matchingModifiers),
								style.GetWithDefault(Style::ValueTypeIdentifier::BackgroundLinearGradient, Math::LinearGradient(), matchingModifiers)
							}
						);

						OnDrawableAdded(sceneRegistry);
					}
				}
			}
		}
		else
		{
			Math::Color borderColor =
				style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BorderColor, "#00000000"_colorf, matchingModifiers);
			Math::Color backgroundColor =
				style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			Math::LinearGradient linearGradient = style.GetWithDefault<Math::LinearGradient>(
				Style::ValueTypeIdentifier::BackgroundLinearGradient,
				Math::LinearGradient(),
				matchingModifiers
			);
			Math::LinearGradient conicGradient = style.GetWithDefault<Math::LinearGradient>(
				Style::ValueTypeIdentifier::BackgroundConicGradient,
				Math::LinearGradient(),
				matchingModifiers
			);
			const bool shouldApply = borderColor.a + backgroundColor.a > 0;
			if (shouldApply)
			{
				if (const Optional<const Math::Ratiof*> pOpacity = style.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers))
				{
					backgroundColor.a = *pOpacity;
					if (borderColor.a > 0)
					{
						borderColor.a = *pOpacity;
					}
				}
			}

			if ((shouldApply && (borderColor.a + backgroundColor.a > 0)) || linearGradient.IsValid() || conicGradient.IsValid())
			{
				const Style::SizeAxisCorners roundingRadius = style.GetWithDefault<Style::SizeAxisCorners>(
					Style::ValueTypeIdentifier::RoundingRadius,
					Style::SizeAxisCorners{},
					matchingModifiers
				);
				const Math::TRectangleCorners<int32> roundingRadiusPixels{
					roundingRadius.m_topLeft.Get(size.x, screenProperties),
					roundingRadius.m_topRight.Get(size.x, screenProperties),
					roundingRadius.m_bottomRight.Get(size.x, screenProperties),
					roundingRadius.m_bottomLeft.Get(size.x, screenProperties),
				};
				const bool hasRoundingRadius = (roundingRadiusPixels.m_topLeft != 0) | (roundingRadiusPixels.m_topRight != 0) ||
				                               (roundingRadiusPixels.m_bottomLeft != 0) | (roundingRadiusPixels.m_bottomRight != 0);

				if (style.GetWithDefault<bool>(Style::ValueTypeIdentifier::BackgroundGrid, false, matchingModifiers))
				{
					// Create a grid primitive
					if (const Optional<Data::Primitives::GridDrawable*> pGridDrawable = FindDataComponentOfType<Data::Primitives::GridDrawable>(gridDrawableSceneData))
					{
						pGridDrawable->m_backgroundColor = backgroundColor;
						pGridDrawable->m_gridColor = borderColor;
					}
					else
					{
						RemoveDrawable(
							sceneRegistry,
							textDrawableSceneData,
							imageDrawableSceneData,
							roundedRectangleDrawableSceneData,
							rectangleDrawableSceneData,
							circleDrawableSceneData,
							gridDrawableSceneData,
							lineDrawableSceneData
						);
						ResetLoadedResources();

						DataComponentOwner::CreateDataComponent<Data::Primitives::GridDrawable>(
							gridDrawableSceneData,
							Data::Primitives::GridDrawable::Initializer{Data::Component::Initializer{*this, sceneRegistry}, backgroundColor, borderColor}
						);
						OnDrawableAdded(sceneRegistry);
					}
				}
				else if (const Optional<const Math::Spline2f*> p2DSpline = style.Find<Math::Spline2f>(Style::ValueTypeIdentifier::BackgroundSpline, matchingModifiers))
				{
					// Create a spline primitive
					if (const Optional<Data::Primitives::LineDrawable*> pSplineDrawable = FindDataComponentOfType<Data::Primitives::LineDrawable>(lineDrawableSceneData))
					{
						pSplineDrawable->m_spline = *p2DSpline;
						pSplineDrawable->m_lineColor = backgroundColor;
					}
					else
					{
						RemoveDrawable(
							sceneRegistry,
							textDrawableSceneData,
							imageDrawableSceneData,
							roundedRectangleDrawableSceneData,
							rectangleDrawableSceneData,
							circleDrawableSceneData,
							gridDrawableSceneData,
							lineDrawableSceneData
						);
						ResetLoadedResources();

						DataComponentOwner::CreateDataComponent<Data::Primitives::LineDrawable>(
							lineDrawableSceneData,
							Data::Primitives::LineDrawable::Initializer{
								Data::Component::Initializer{*this, sceneRegistry},
								Math::Spline2f{*p2DSpline},
								5.f,
								backgroundColor
							}
						);
						OnDrawableAdded(sceneRegistry);
					}
				}
				else if (hasRoundingRadius)
				{
					const bool isCircle = (roundingRadiusPixels.m_topLeft == roundingRadiusPixels.m_topRight) &
					                      (roundingRadiusPixels.m_topRight == roundingRadiusPixels.m_bottomLeft) &
					                      (roundingRadiusPixels.m_bottomLeft == roundingRadiusPixels.m_bottomRight) &
					                      (roundingRadiusPixels.m_topLeft >= size.x / 2) & (size.x > 0) & (size.x == size.y);

					if (isCircle)
					{
						// Create a circle primitive
						if (const Optional<Data::Primitives::CircleDrawable*> pCircleDrawable = FindDataComponentOfType<Data::Primitives::CircleDrawable>(circleDrawableSceneData))
						{
							pCircleDrawable->m_backgroundColor = backgroundColor;
							pCircleDrawable->m_borderColor = borderColor;
							pCircleDrawable->m_linearGradient = Move(linearGradient);
							pCircleDrawable->m_conicGradient = Move(conicGradient);
							pCircleDrawable->m_borderSize = style.GetWithDefault<Style::SizeAxisEdges>(
								Style::ValueTypeIdentifier::BorderThickness,
								Style::SizeAxisEdges{0_px},
								matchingModifiers
							);
						}
						else
						{
							RemoveDrawable(
								sceneRegistry,
								textDrawableSceneData,
								imageDrawableSceneData,
								roundedRectangleDrawableSceneData,
								rectangleDrawableSceneData,
								circleDrawableSceneData,
								gridDrawableSceneData,
								lineDrawableSceneData
							);
							ResetLoadedResources();

							DataComponentOwner::CreateDataComponent<Data::Primitives::CircleDrawable>(
								circleDrawableSceneData,
								Data::Primitives::CircleDrawable::Initializer{
									Data::Component::Initializer{*this, sceneRegistry},
									backgroundColor,
									borderColor,
									style.GetWithDefault<Style::SizeAxisEdges>(
										Style::ValueTypeIdentifier::BorderThickness,
										Style::SizeAxisEdges{0_px},
										matchingModifiers
									),
									Move(linearGradient),
									Move(conicGradient)
								}
							);
							OnDrawableAdded(sceneRegistry);
						}
					}
					else
					{
						// Create a rounded rectangle primitive
						if (const Optional<Data::Primitives::RoundedRectangleDrawable*> pRoundedRectangleDrawable = FindDataComponentOfType<Data::Primitives::RoundedRectangleDrawable>(roundedRectangleDrawableSceneData))
						{
							pRoundedRectangleDrawable->m_backgroundColor = backgroundColor;
							pRoundedRectangleDrawable->m_cornerRoundingRadiuses = roundingRadius;
							pRoundedRectangleDrawable->m_borderColor = borderColor;
							pRoundedRectangleDrawable->m_linearGradient = Move(linearGradient);
							pRoundedRectangleDrawable->m_borderSize = style.GetWithDefault<Style::SizeAxisEdges>(
								Style::ValueTypeIdentifier::BorderThickness,
								Style::SizeAxisEdges{0_px},
								matchingModifiers
							);
						}
						else
						{
							RemoveDrawable(
								sceneRegistry,
								textDrawableSceneData,
								imageDrawableSceneData,
								roundedRectangleDrawableSceneData,
								rectangleDrawableSceneData,
								circleDrawableSceneData,
								gridDrawableSceneData,
								lineDrawableSceneData
							);
							ResetLoadedResources();

							DataComponentOwner::CreateDataComponent<Data::Primitives::RoundedRectangleDrawable>(
								roundedRectangleDrawableSceneData,
								Data::Primitives::RoundedRectangleDrawable::Initializer{
									Data::Component::Initializer{*this, sceneRegistry},
									backgroundColor,
									roundingRadius,
									borderColor,
									style.GetWithDefault<Style::SizeAxisEdges>(
										Style::ValueTypeIdentifier::BorderThickness,
										Style::SizeAxisEdges{0_px},
										matchingModifiers
									),
									Move(linearGradient)
								}
							);

							OnDrawableAdded(sceneRegistry);
						}
					}
				}
				else
				{
					// Create a rectangle primitive
					if (const Optional<Data::Primitives::RectangleDrawable*> pRectangleDrawable = FindDataComponentOfType<Data::Primitives::RectangleDrawable>(rectangleDrawableSceneData))
					{
						pRectangleDrawable->m_backgroundColor = backgroundColor;
						pRectangleDrawable->m_borderColor = borderColor;
						pRectangleDrawable->m_linearGradient = Move(linearGradient);
						pRectangleDrawable->m_borderSize = style.GetWithDefault<Style::SizeAxisEdges>(
							Style::ValueTypeIdentifier::BorderThickness,
							Style::SizeAxisEdges{0_px},
							matchingModifiers
						);
					}
					else
					{
						RemoveDrawable(
							sceneRegistry,
							textDrawableSceneData,
							imageDrawableSceneData,
							roundedRectangleDrawableSceneData,
							rectangleDrawableSceneData,
							circleDrawableSceneData,
							gridDrawableSceneData,
							lineDrawableSceneData
						);
						ResetLoadedResources();

						DataComponentOwner::CreateDataComponent<Data::Primitives::RectangleDrawable>(
							rectangleDrawableSceneData,
							Data::Primitives::RectangleDrawable::Initializer{
								Data::Component::Initializer{*this, sceneRegistry},
								backgroundColor,
								borderColor,
								style.GetWithDefault<Style::SizeAxisEdges>(
									Style::ValueTypeIdentifier::BorderThickness,
									Style::SizeAxisEdges{0_px},
									matchingModifiers
								),
								Move(linearGradient)
							}
						);
						OnDrawableAdded(sceneRegistry);
					}
				}
			}
			else
			{
				RemoveDrawable(
					sceneRegistry,
					textDrawableSceneData,
					imageDrawableSceneData,
					roundedRectangleDrawableSceneData,
					rectangleDrawableSceneData,
					circleDrawableSceneData,
					gridDrawableSceneData,
					lineDrawableSceneData
				);
			}
		}
	}

	void Widget::OnDrawableAdded(Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& stageMaskSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::Identifier>& identifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::TransformChangeTracker>& transformChangeTrackerSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::TransformChangeTracker>();
		Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Tags>();

		// TODO: Move drawables to use stage masks
		Rendering::RenderItemStageMask stageMask;

		[[maybe_unused]] const Optional<Entity::Data::RenderItem::StageMask*> pStageMask =
			CreateDataComponent<Entity::Data::RenderItem::StageMask>(stageMaskSceneData, stageMask);

		if (!HasDataComponentOfType(sceneRegistry, identifierSceneData.GetIdentifier()))
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = GetRootScene().AcquireNewRenderItemIdentifier();
			[[maybe_unused]] const Optional<Entity::Data::RenderItem::Identifier*> pIdentifier =
				CreateDataComponent<Entity::Data::RenderItem::Identifier>(identifierSceneData, renderItemIdentifier);
			if (UNLIKELY(!pIdentifier.IsValid()))
			{
				GetRootScene().ReturnRenderItemIdentifier(renderItemIdentifier);
			}
		}
		[[maybe_unused]] const Optional<Entity::Data::RenderItem::TransformChangeTracker*> pTransformChangeTracker =
			CreateDataComponent<Entity::Data::RenderItem::TransformChangeTracker>(transformChangeTrackerSceneData);

		Optional<Entity::Data::Tags*> pTagComponent = FindDataComponentOfType<Entity::Data::Tags>(tagsSceneData);
		if (pTagComponent.IsInvalid())
		{
			pTagComponent = CreateDataComponent<Entity::Data::Tags>(
				tagsSceneData,
				Entity::Data::Tags::Initializer{Entity::Data::Component::DynamicInitializer{*this, sceneRegistry}}
			);
			if (pTagComponent.IsInvalid())
			{
				pTagComponent = FindDataComponentOfType<Entity::Data::Tags>(tagsSceneData);
				Assert(pTagComponent.IsValid());
			}
		}

		if (LIKELY(pTagComponent.IsValid()))
		{
			const Tag::Identifier renderItemTagIdentifier =
				System::Get<Tag::Registry>().FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);
			pTagComponent->SetTag(GetIdentifier(), sceneRegistry, renderItemTagIdentifier);
		}
	}

	bool Widget::RemoveDrawable(
		Entity::SceneRegistry& sceneRegistry,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		bool removedDrawable = RemoveDataComponentOfType<Data::TextDrawable>(textDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::ImageDrawable>(imageDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::Primitives::RoundedRectangleDrawable>(roundedRectangleDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::Primitives::RectangleDrawable>(rectangleDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::Primitives::CircleDrawable>(circleDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::Primitives::GridDrawable>(gridDrawableSceneData);
		removedDrawable |= RemoveDataComponentOfType<Data::Primitives::LineDrawable>(lineDrawableSceneData);
		if (removedDrawable)
		{
			Entity::ComponentTypeSceneData<Entity::Data::RenderItem::Identifier>& identifierSceneData =
				sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>();
			Entity::ComponentTypeSceneData<Entity::Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Tags>();
			const Entity::RenderItemIdentifier renderItemIdentifier = identifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
			Entity::Data::Tags& widgetTags = tagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
			const Tag::Identifier renderItemTagIdentifier =
				System::Get<Tag::Registry>().FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);
			widgetTags.ClearTag(GetIdentifier(), sceneRegistry, renderItemTagIdentifier);

			RemoveDataComponentOfType<Entity::Data::RenderItem::StageMask>(sceneRegistry);
			RemoveDataComponentOfType<Entity::Data::RenderItem::TransformChangeTracker>(sceneRegistry);
			RemoveDataComponentOfType<Entity::Data::RenderItem::Identifier>(identifierSceneData);

			// TODO: Do this deferred alongside widget drawing, just like SceneView does
			Scene2D& scene = GetRootScene();
			Entity::RenderItemMask mask;
			mask.Set(renderItemIdentifier);
			for (Rendering::SceneViewBase& sceneView : scene.GetActiveViews())
			{
				sceneView.ProcessRemovedRenderItems(scene, mask);
			}
		}

		return removedDrawable;
	}

	[[nodiscard]] uint16 GetTotalWidgetCountInternal(Widget& widget)
	{
		uint16 widgetCount = 1;
		for (Widget& child : widget.GetChildren())
		{
			widgetCount += GetTotalWidgetCountInternal(child);
		}
		return widgetCount;
	}

	[[nodiscard]] uint16 GetTotalWidgetCount(Widget& widget)
	{
		uint16 widgetCount = 0;
		for (Widget* pParent = widget.GetParentSafe(); pParent != nullptr; pParent = pParent->GetParentSafe())
		{
			widgetCount++;
		}

		return widgetCount + GetTotalWidgetCountInternal(widget);
	}

	enum class RecalculationFlags : uint32
	{
		IsRelativePositionXFinal = 1 << 0,
		IsRelativePositionYFinal = 1 << 1,

		IsRelativeSizeXFinal = 1 << 2,
		IsRelativeSizeYFinal = 1 << 3,

		HasPositionedChildrenX = 1 << 4,
		HasPositionedChildrenY = 1 << 5,
		HasBeenPositionedByParentX = 1 << 6,
		HasBeenPositionedByParentY = 1 << 7,

		HasGrownChildrenX = 1 << 8,
		HasGrownChildrenY = 1 << 9,
		HasBeenGrownByParentX = 1 << 10,
		HasBeenGrownByParentY = 1 << 11,

		ExpandToFitChildrenX = 1 << 12,
		ExpandToFitChildrenY = 1 << 13,
		ExpandToFitChildren = ExpandToFitChildrenX | ExpandToFitChildrenY,

		HasSpawnedDynamicChildren = 1 << 14,

		//! Whether this widget is a dynamic layout such as a flex layout or grid that repositions its children
		IsDynamicLayout = 1 << 15,
		//! Whether this widget is ignored within its parents auto layout processing
		IsRemovedFromParentLayout = 1 << 16,

		HasBeenPositionedByParent = HasBeenPositionedByParentX | HasBeenPositionedByParentY,
		HasPositionedChildren = HasPositionedChildrenX | HasPositionedChildrenY,
		HasGrownChildren = HasGrownChildrenX | HasGrownChildrenY,
		HasBeenGrownByParent = HasBeenGrownByParentX | HasBeenGrownByParentY,

		IsRelativePositionFinal = IsRelativePositionXFinal | IsRelativePositionYFinal,
		IsAbsolutePositionXFinal = IsRelativePositionXFinal | HasBeenPositionedByParentX,
		IsAbsolutePositionYFinal = IsRelativePositionYFinal | HasBeenPositionedByParentY,
		IsAbsolutePositionFinal = IsRelativePositionFinal | HasBeenPositionedByParent,

		IsRelativeSizeFinal = IsRelativeSizeXFinal | IsRelativeSizeYFinal,
		IsAbsoluteSizeXFinal = IsRelativeSizeXFinal | HasBeenGrownByParentX,
		IsAbsoluteSizeYFinal = IsRelativeSizeYFinal | HasBeenGrownByParentY,
		IsAbsoluteSizeFinal = IsRelativeSizeFinal | HasBeenGrownByParent,

		IsAbsoluteContentAreaXFinal = IsAbsolutePositionXFinal | IsAbsoluteSizeXFinal,
		IsAbsoluteContentAreaYFinal = IsAbsolutePositionYFinal | IsAbsoluteSizeYFinal,
		IsAbsoluteContentAreaFinal = IsAbsolutePositionFinal | IsAbsoluteSizeFinal,

		IsFinal = IsAbsoluteContentAreaFinal | HasPositionedChildren | HasGrownChildren | HasSpawnedDynamicChildren
	};
	ENUM_FLAG_OPERATORS(RecalculationFlags);

	struct WidgetInfo
	{
		[[nodiscard]] static Optional<WidgetInfo*>
		Get(const Variant<uint16, WidgetInfo*> widgetInfoVariant, const ArrayView<WidgetInfo> widgetInfos)
		{
			if (const Optional<const uint16*> parentIndex = widgetInfoVariant.Get<uint16>())
			{
				return widgetInfos[*parentIndex];
			}
			else if (const Optional<WidgetInfo* const *> pWidgetInfo = widgetInfoVariant.Get<WidgetInfo*>())
			{
				return *pWidgetInfo;
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] Optional<WidgetInfo*> GetParentInfo(const ArrayView<WidgetInfo> widgetInfos) const
		{
			return Get(m_parentInfo, widgetInfos);
		}

		[[nodiscard]] Alignment GetPrimaryAlignment(const Alignment parentPrimaryAlignment) const
		{
			return m_childPrimaryAlignment != Alignment::Inherit ? m_childPrimaryAlignment : parentPrimaryAlignment;
		}
		[[nodiscard]] Alignment GetSecondaryAlignment(const Alignment parentSecondaryAlignment) const
		{
			return m_childSecondaryAlignment != Alignment::Inherit ? m_childSecondaryAlignment : parentSecondaryAlignment;
		}

		[[nodiscard]] EnumFlags<RecalculationFlags> GetRecalculationFlags() const
		{
			return m_recalculationFlags;
		}

		Optional<Widget*> m_pWidget;
		Variant<uint16, WidgetInfo*> m_parentInfo;
		InlineVector<uint16, 16, uint16> m_childrenIndices;
		Math::Vector2i m_relativePosition = Math::Zero;
		Math::Vector2i m_size = Math::Zero;
		EnumFlags<RecalculationFlags> m_recalculationFlags;
		Style::CombinedEntry m_style;
		Style::CombinedEntry::MatchingModifiers m_matchingStyleModifiers;
		EnumFlags<ContentAreaChangeFlags> m_contentAreaChangeFlags;
		Variant<Data::FlexLayout*, Data::GridLayout*> m_pLayout;

		Style::Size m_minimumSize;
		Style::Size m_preferredSize;
		Style::Size m_maximumSize;
		Style::SizeAxisEdges m_positionOffset;
		Style::Size m_childOffset;
		Style::Size m_childEntrySize;
		float m_growthFactor;
		float m_shrinkFactor;
		LayoutType m_layoutType;
		Orientation m_orientation;
		OverflowType m_overflowType;
		ElementSizingType m_elementSizingType;
		WrapType m_wrapType;
		Alignment m_defaultParentPrimaryAlignment;
		Alignment m_defaultParentSecondaryAlignment;
		Alignment m_childPrimaryAlignment;
		Alignment m_childSecondaryAlignment;
		PositionType m_positionType;
		Style::SizeAxisEdges m_padding;
		Style::SizeAxisEdges m_margin;
	};

	using WidgetInfoContainer = Vector<WidgetInfo, uint16>;
	using RemainingWidgetsMask = DynamicBitset<uint16>;
	using PositionedWidgetsMask = DynamicBitset<uint16>;

	auto getAvailableChildContentAreaSize(
		const ArrayView<WidgetInfo> widgetInfos,
		const WidgetInfo& __restrict widgetInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		const uint8 mask = (1 << 0) | (1 << 1)
	) -> Math::Vector2i
	{
		Assert(mask != 0);

		if (widgetInfo.m_pWidget.IsValid())
		{
			Math::Vector2i availableContentAreaSize{widgetInfo.m_size};

			const WidgetInfo& parentInfo = *widgetInfo.GetParentInfo(widgetInfos);
			Math::Vector2i parentAvailableContentAreaSize{Math::Zero};

			uint8 checkedMask{0};
			checkedMask |= (1 << 0) * widgetInfo.GetRecalculationFlags().AreAnySet(RecalculationFlags::ExpandToFitChildrenX);
			checkedMask |= (1 << 1) * widgetInfo.GetRecalculationFlags().AreAnySet(RecalculationFlags::ExpandToFitChildrenY);
			if (parentInfo.m_recalculationFlags.IsSet(RecalculationFlags::IsDynamicLayout) && parentInfo.m_defaultParentSecondaryAlignment == Alignment::Stretch)
			{
				const uint8 parentDirection = (uint8)parentInfo.m_orientation;
				if (widgetInfo.m_preferredSize[!parentDirection].Is<Style::AutoType>())
				{
					checkedMask |= 1 << uint8(!parentDirection);
				}
			}
			checkedMask &= mask;

			if (checkedMask != 0)
			{
				parentAvailableContentAreaSize =
					getAvailableChildContentAreaSize(widgetInfos, parentInfo, screenProperties, sceneRegistry, checkedMask);
			}

			if (widgetInfo.GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenX))
			{
				availableContentAreaSize.x = parentAvailableContentAreaSize.x;
			}
			if (widgetInfo.GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenY))
			{
				availableContentAreaSize.y = parentAvailableContentAreaSize.y;
			}

			if (parentInfo.m_recalculationFlags.IsSet(RecalculationFlags::IsDynamicLayout) && parentInfo.m_defaultParentSecondaryAlignment == Alignment::Stretch)
			{
				const uint8 parentDirection = (uint8)parentInfo.m_orientation;
				if (widgetInfo.m_preferredSize[!parentDirection].Is<Style::AutoType>())
				{
					availableContentAreaSize[!parentDirection] = parentAvailableContentAreaSize[!parentDirection];
				}
			}

			const Math::RectangleEdgesi padding{
				widgetInfo.m_padding.m_top.Get(availableContentAreaSize.y, screenProperties),
				widgetInfo.m_padding.m_right.Get(availableContentAreaSize.x, screenProperties),
				widgetInfo.m_padding.m_bottom.Get(availableContentAreaSize.y, screenProperties),
				widgetInfo.m_padding.m_left.Get(availableContentAreaSize.x, screenProperties)
			};
			availableContentAreaSize = Math::Max(availableContentAreaSize - padding.GetSum(), (Math::Vector2i)Math::Zero);

			const Math::RectangleEdgesi margin{
				widgetInfo.m_margin.m_top.Get(availableContentAreaSize.y, screenProperties),
				widgetInfo.m_margin.m_right.Get(availableContentAreaSize.x, screenProperties),
				widgetInfo.m_margin.m_bottom.Get(availableContentAreaSize.y, screenProperties),
				widgetInfo.m_margin.m_left.Get(availableContentAreaSize.x, screenProperties)
			};
			availableContentAreaSize = Math::Max(availableContentAreaSize - margin.GetSum(), (Math::Vector2i)Math::Zero);

			return availableContentAreaSize;
		}
		else
		{
			return {widgetInfo.m_size};
		}
	};

	struct State
	{
		State(
			WidgetInfoContainer& widgetInfoContainer, RemainingWidgetsMask& remainingWidgetsMask, PositionedWidgetsMask& positionedWidgetsMask
		)
			: widgetInfos(widgetInfoContainer)
			, remainingWidgets(remainingWidgetsMask)
			, positionedWidgets(positionedWidgetsMask)
		{
		}

		[[nodiscard]] bool ShouldRun() const
		{
			return remainingWidgets.AreAnySet() && changedAny;
		}

		void Start()
		{
			changedAny = false;
		}

		void Restart()
		{
			changedAny = true;
		}

		bool AppendWidgetFlags(const WidgetInfo& widgetInfo, const EnumFlags<RecalculationFlags> newFlags)
		{
			Assert(!widgetInfo.m_recalculationFlags.AreAllSet(RecalculationFlags::IsFinal));
			WidgetInfo& mutableWidgetInfo = const_cast<WidgetInfo&>(widgetInfo);
			if (mutableWidgetInfo.m_recalculationFlags.AreAnyNotSet(newFlags))
			{
				mutableWidgetInfo.m_recalculationFlags |= newFlags;
				changedAny = true;
				if (mutableWidgetInfo.m_recalculationFlags.AreAllSet(RecalculationFlags::IsFinal))
				{
					const uint16 index = widgetInfos.GetIteratorIndex(&mutableWidgetInfo);
					remainingWidgets.Clear(index);
					return true;
				}
			}
			return false;
		}

		void ClearWidgetFlags(const WidgetInfo& widgetInfo, const EnumFlags<RecalculationFlags> newFlags)
		{
			Assert(!widgetInfo.m_recalculationFlags.AreAllSet(RecalculationFlags::IsFinal));
			changedAny |= widgetInfo.m_recalculationFlags.AreAnySet(newFlags);
			const_cast<WidgetInfo&>(widgetInfo).m_recalculationFlags &= ~newFlags;
		}

		WidgetInfoContainer& widgetInfos;
		RemainingWidgetsMask& remainingWidgets;
		PositionedWidgetsMask& positionedWidgets;
	private:
		bool changedAny = true;
	};

	[[nodiscard]] constexpr Orientation GetDefaultOrientation(const LayoutType layoutType)
	{
		switch (layoutType)
		{
			case LayoutType::None:
			case LayoutType::Flex:
			case LayoutType::Grid:
				return Orientation::Horizontal;
			case LayoutType::Block:
				return Orientation::Vertical;
		}
		ExpectUnreachable();
	}

	void populateWidgetInfos(
		State& state,
		Widget& widget,
		Entity::SceneRegistry& sceneRegistry,
		Variant<uint16, WidgetInfo*> parentInfoVariant,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		const Rendering::ScreenProperties& __restrict screenProperties
	)
	{
		Assert(!widget.IsDestroying(sceneRegistry));

		const Style::CombinedEntry style = widget.GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		EnumFlags<RecalculationFlags> initialRecalculationFlags;

		LayoutType layoutType = GetLayoutFromStyle(style, matchingModifiers, widget);
		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);

		bool includeChildren = true;
		Variant<Data::FlexLayout*, Data::GridLayout*> pLayoutComponent;

		const Alignment defaultParentPrimaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::PrimaryDirectionAlignment, Alignment::Start, matchingModifiers);
		const Alignment defaultParentSecondaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::SecondaryDirectionAlignment, Alignment::Stretch, matchingModifiers);
		const Alignment childPrimaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::ChildPrimaryAlignment, Alignment::Inherit, matchingModifiers);
		const Alignment childSecondaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::ChildSecondaryAlignment, Alignment::Inherit, matchingModifiers);

		switch (layoutType)
		{
			case LayoutType::None:
				widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				return;
			case LayoutType::Block:
			{
				widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);

				const uint8 direction = (uint8)orientation;
				initialRecalculationFlags |= RecalculationFlags::HasGrownChildren | RecalculationFlags::HasSpawnedDynamicChildren |
				                             (RecalculationFlags::HasPositionedChildrenX << !direction);
			}
			break;
			case LayoutType::Flex:
			{
				initialRecalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::FlexLayout*> pFlexLayoutComponent = widget.FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				if (!pFlexLayoutComponent.IsValid())
				{
					widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);

					pFlexLayoutComponent = widget.CreateDataComponent<Data::FlexLayout>(
						flexLayoutSceneData,
						Data::FlexLayout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}
					);
				}

				bool hasDynamicChildren{false};
				if (LIKELY(pFlexLayoutComponent.IsValid()))
				{
					const bool hasDataSource = widget.HasDataComponentOfType(sceneRegistry, dataSourceSceneData.GetIdentifier());
					hasDynamicChildren = hasDataSource && pFlexLayoutComponent->GetFilteredDataCount(widget, sceneRegistry) > 0;
					pLayoutComponent = pFlexLayoutComponent.Get();
				}

				includeChildren &= !hasDynamicChildren;
				initialRecalculationFlags |= RecalculationFlags::HasSpawnedDynamicChildren * !hasDynamicChildren;

				const bool hasChildren = widget.HasChildren() | hasDynamicChildren;
				initialRecalculationFlags |= RecalculationFlags::HasPositionedChildren * !hasChildren;
				initialRecalculationFlags |= RecalculationFlags::HasGrownChildren * !hasChildren;

				switch (defaultParentSecondaryAlignment)
				{
					case Alignment::Start:
					case Alignment::Center:
					case Alignment::End:
					case Alignment::Inherit:
						initialRecalculationFlags |= RecalculationFlags::HasGrownChildrenX * (orientation == Orientation::Vertical);
						initialRecalculationFlags |= RecalculationFlags::HasGrownChildrenY * (orientation == Orientation::Horizontal);
						break;
					case Alignment::Stretch:
						break;
				}
			}
			break;
			case LayoutType::Grid:
			{
				initialRecalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::GridLayout*> pGridLayoutComponent = widget.FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				if (pGridLayoutComponent.IsInvalid())
				{
					widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);

					pGridLayoutComponent = widget.CreateDataComponent<Data::GridLayout>(
						gridLayoutSceneData,
						Data::GridLayout::Initializer{Data::Layout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}}
					);
				}

				Assert(pGridLayoutComponent.IsValid());
				bool hasDynamicChildren{false};
				if (LIKELY(pGridLayoutComponent.IsValid()))
				{
					const bool hasDataSource = widget.HasDataComponentOfType(sceneRegistry, dataSourceSceneData.GetIdentifier());
					hasDynamicChildren = hasDataSource && pGridLayoutComponent->GetFilteredDataCount(widget, sceneRegistry) > 0;
					pLayoutComponent = pGridLayoutComponent.Get();
				}

				includeChildren &= !hasDynamicChildren;
				initialRecalculationFlags |= RecalculationFlags::HasSpawnedDynamicChildren * !hasDynamicChildren;

				const bool hasChildren = widget.HasChildren() | hasDynamicChildren;
				initialRecalculationFlags |= RecalculationFlags::HasPositionedChildren * !hasChildren;
				initialRecalculationFlags |= RecalculationFlags::HasGrownChildren;
			}
			break;
		}

		Math::Vector2i initialRelativePosition{Math::Zero};

		const PositionType positionType =
			style.GetWithDefault<PositionType>(Widgets::Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);
		switch (positionType)
		{
			case PositionType::Static:
				initialRecalculationFlags |= RecalculationFlags::IsRelativePositionFinal;
				break;
			case PositionType::Relative:
				break;
			case PositionType::Absolute:
				initialRecalculationFlags |= RecalculationFlags::HasBeenPositionedByParent | RecalculationFlags::HasBeenGrownByParent |
				                             RecalculationFlags::IsRemovedFromParentLayout;
				break;
			case PositionType::Dynamic:
				initialRecalculationFlags |= RecalculationFlags::HasBeenPositionedByParent | RecalculationFlags::IsRelativePositionFinal |
				                             RecalculationFlags::HasBeenGrownByParent | RecalculationFlags::IsRemovedFromParentLayout;
				initialRelativePosition = widget.GetRelativeLocation(sceneRegistry);
				break;
		}

		const float growthFactor = style.GetWithDefault(Style::ValueTypeIdentifier::ChildGrowthFactor, 0.f, matchingModifiers);
		const float shrinkFactor = style.GetWithDefault(Style::ValueTypeIdentifier::ChildShrinkFactor, 0.f, matchingModifiers);

		Style::Size minimumSize;
		if (const Optional<const Style::EntryValue*> pMinimumSize = style.Find(Style::ValueTypeIdentifier::MinimumSize, matchingModifiers))
		{
			minimumSize = *pMinimumSize->Get<Style::Size>();
		}
		else
		{
			minimumSize = Style::Size{0_percent, 0_percent};
		}

		// Always try to load resources for text drawable widgets, since they will update the relative size during calculation
		// TODO: Change this to only happen if the drawable is potentially within view
		if (sceneRegistry.HasDataComponentOfType(widget.GetIdentifier(), textDrawableSceneData.GetIdentifier()))
		{
			widget.LoadResources(sceneRegistry);
		}

		Style::Size preferredSize = widget.GetPreferredSize(
			sceneRegistry,
			style,
			matchingModifiers,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData
		);
		Style::Size maximumSize = widget.GetMaximumSize(style, matchingModifiers, textDrawableSceneData);
		Style::SizeAxisEdges positionOffset =
			style.GetWithDefault(Style::ValueTypeIdentifier::Position, Style::SizeAxisEdges{Invalid}, matchingModifiers);
		Style::Size childOffset = style.GetSize(Style::ValueTypeIdentifier::ChildOffset, 0_px, matchingModifiers);
		Style::Size childEntrySize =
			style.GetSize(Style::ValueTypeIdentifier::ChildEntrySize, Style::Size{100_percent, 100_percent}, matchingModifiers);

		EnumFlags<RecalculationFlags> parentRecalculationFlags;
		uint16 parentIndex;
		{
			WidgetInfo& parentInfo = *WidgetInfo::Get(parentInfoVariant, state.widgetInfos.GetView());
			parentIndex = state.widgetInfos.GetIteratorIndex(&parentInfo);

			parentRecalculationFlags = parentInfo.GetRecalculationFlags();
			initialRecalculationFlags |= RecalculationFlags::HasBeenGrownByParentX *
			                             parentRecalculationFlags.IsSet(RecalculationFlags::HasGrownChildrenX);
			initialRecalculationFlags |= RecalculationFlags::HasBeenGrownByParentY *
			                             parentRecalculationFlags.IsSet(RecalculationFlags::HasGrownChildrenY);
			initialRecalculationFlags |= RecalculationFlags::HasBeenPositionedByParentX *
			                             parentRecalculationFlags.IsSet(RecalculationFlags::HasPositionedChildrenX);
			initialRecalculationFlags |= RecalculationFlags::HasBeenPositionedByParentY *
			                             parentRecalculationFlags.IsSet(RecalculationFlags::HasPositionedChildrenY);

			if (!initialRecalculationFlags.AreAllSet(RecalculationFlags::HasBeenGrownByParent))
			{
				const uint8 parentDirection = (uint8)parentInfo.m_orientation;
				initialRecalculationFlags |= (RecalculationFlags::HasBeenGrownByParentX << parentDirection) *
				                             (growthFactor == 0 && shrinkFactor == 0);
				const bool canStretch = parentInfo.m_defaultParentSecondaryAlignment == Alignment::Stretch &&
				                        preferredSize[!parentDirection].Is<Style::AutoType>();
				initialRecalculationFlags |= (RecalculationFlags::HasBeenGrownByParentX << !parentDirection) * (!canStretch);
			}
		}

		const OverflowType overflowType =
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);
		const ElementSizingType elementSizingType =
			style.GetWithDefault(Style::ValueTypeIdentifier::ElementSizingType, ElementSizingType(ElementSizingType::Default), matchingModifiers);
		const WrapType wrapType = style.GetWithDefault(Style::ValueTypeIdentifier::WrapType, WrapType(WrapType::NoWrap), matchingModifiers);

		const Style::SizeAxisEdges padding = GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Padding, matchingModifiers);
		const Style::SizeAxisEdges margin = GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Margin, matchingModifiers);

		const Math::Vector2i initialSize = [](
																				 const ElementSizingType elementSizingType,
																				 const Style::CombinedEntry& style,
																				 const Style::CombinedEntry::MatchingModifiers& matchingModifiers,
																				 const Rendering::ScreenProperties& screenProperties
																			 )
		{
			switch (elementSizingType)
			{
				case ElementSizingType::ContentBox:
				{
					const Style::SizeAxisEdges borderThicknessStyle = style.GetWithDefault<Style::SizeAxisEdges>(
						Style::ValueTypeIdentifier::BorderThickness,
						Style::SizeAxisEdges{0_px},
						matchingModifiers
					);

					const Math::RectangleEdgesi borderThickness{
						borderThicknessStyle.m_top.Get(0, screenProperties),
						borderThicknessStyle.m_right.Get(0, screenProperties),
						borderThicknessStyle.m_bottom.Get(0, screenProperties),
						borderThicknessStyle.m_left.Get(0, screenProperties),
					};

					return borderThickness.GetSum();
				}
				case ElementSizingType::BorderBox:
					return Math::Vector2i{Math::Zero};
			}
			ExpectUnreachable();
		}(elementSizingType, style, matchingModifiers, screenProperties);

		WidgetInfo& __restrict widgetInfo = state.widgetInfos.EmplaceBack(WidgetInfo{
			widget,
			parentIndex,
			{},
			initialRelativePosition,
			initialSize,
			initialRecalculationFlags,
			style,
			matchingModifiers,
			ContentAreaChangeFlags{},
			pLayoutComponent,
			minimumSize,
			preferredSize,
			maximumSize,
			positionOffset,
			childOffset,
			childEntrySize,
			growthFactor,
			shrinkFactor,
			layoutType,
			orientation,
			overflowType,
			elementSizingType,
			wrapType,
			defaultParentPrimaryAlignment,
			defaultParentSecondaryAlignment,
			childPrimaryAlignment,
			childSecondaryAlignment,
			positionType,
			padding,
			margin
		});
		const uint16 widgetIndex = state.widgetInfos.GetIteratorIndex(&widgetInfo);

		if (!widgetInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsFinal))
		{
			state.remainingWidgets.Set(widgetIndex);
		}

		if (positionType != PositionType::Dynamic)
		{
			state.positionedWidgets.Set(widgetIndex);
		}

		{
			WidgetInfo& parentInfo = state.widgetInfos[parentIndex];
			parentInfo.m_childrenIndices.EmplaceBack(widgetIndex);
		}

		if (includeChildren)
		{
			const uint16 childCount = (uint16)widget.GetChildCount();
			widgetInfo.m_childrenIndices.Reserve(childCount);
			for (uint16 childIndex = 0; childIndex < childCount; ++childIndex)
			{
				Optional<Widget*> pChildWidget;
				{
					const Widget::ChildView children = widget.GetChildren();
					if (childIndex < children.GetSize())
					{
						pChildWidget = children[childIndex];
					}
					else
					{
						break;
					}
				}

				populateWidgetInfos(
					state,
					*pChildWidget,
					sceneRegistry,
					widgetIndex,
					worldTransformSceneData,
					flexLayoutSceneData,
					gridLayoutSceneData,
					textDrawableSceneData,
					dataSourceSceneData,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData,
					screenProperties
				);
			}
		}
	}

	void Widget::RecalculateHierarchy()
	{
		RecalculateHierarchy(GetSceneRegistry());
	}

	void Widget::RecalculateHierarchy(Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();
		RecalculateHierarchy(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData);
	}

	void Widget::RecalculateHierarchy(
		Entity::SceneRegistry& sceneRegistry,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	)
	{
		Widget* pParent = GetParentSafe();
		if (pParent != nullptr)
		{
			const Style::CombinedEntry parentStyle = pParent->GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
			const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry, modifiersSceneData);
			const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);
			const Style::Size parentMinimumSize = parentStyle.GetSize(
				Style::ValueTypeIdentifier::MinimumSize,
				Style::Size{Style::Size{0_percent, 0_percent}},
				parentMatchingModifiers
			);
			const Style::Size parentPreferredSize =
				parentStyle.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, parentMatchingModifiers);
			const Style::Size parentMaximumSize = parentStyle.GetSize(
				Style::ValueTypeIdentifier::MaximumSize,
				Style::Size{Style::SizeAxis{Style::NoMaximum}, Style::SizeAxis{Style::NoMaximum}},
				parentMatchingModifiers
			);

			const LayoutType parentWidgetLayoutType = GetLayoutFromStyle(parentStyle, parentMatchingModifiers, *pParent);
			switch (parentWidgetLayoutType)
			{
				case LayoutType::Block:
					break;
				case LayoutType::None:
					pParent
						->RecalculateHierarchy(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData);
					return;
				case LayoutType::Flex:
				case LayoutType::Grid:
				{
					const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
					const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
					const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

					const PositionType positionType =
						style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);
					switch (positionType)
					{
						case PositionType::Static:
						case PositionType::Relative:
							pParent->RecalculateHierarchy(
								sceneRegistry,
								externalStyleSceneData,
								inlineStyleSceneData,
								dynamicStyleSceneData,
								modifiersSceneData
							);
							return;
						case PositionType::Absolute:
						case PositionType::Dynamic:
							break;
					}
				}
				break;
			}

			if (parentMinimumSize.x.DependsOnContentDimensions() || parentMinimumSize.y.DependsOnContentDimensions() || parentPreferredSize.x.DependsOnContentDimensions() || parentPreferredSize.y.DependsOnContentDimensions() || parentMaximumSize.x.DependsOnContentDimensions() || parentMaximumSize.y.DependsOnContentDimensions() || parentPreferredSize.x.Is<Style::AutoType>() || parentPreferredSize.y.Is<Style::AutoType>())
			{
				return pParent
				  ->RecalculateHierarchy(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData);
			}
		}

		static_cast<RootWidget&>(GetRootWidget()).QueueRecalculateWidgetHierarchy(*this);
	}

	Optional<WidgetInfo*> EmplaceSimpleWidget(
		Widget& widget,
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry style,
		const Style::CombinedEntry::MatchingModifiers matchingModifiers,
		const LayoutType layoutType,
		EnumFlags<RecalculationFlags> recalculationFlags,
		Variant<uint16, WidgetInfo*> parentInfo,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		State& state
	)
	{
		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);

		const Entity::Data::WorldTransform2D& __restrict worldTransform =
			worldTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
		const Math::Vector2i size = (Math::Vector2i)worldTransform.GetScale();

		const PositionType positionType =
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);
		const Math::Vector2i position = widget.GetRelativeLocation(sceneRegistry);

		Variant<Data::FlexLayout*, Data::GridLayout*> pLayoutComponent;

		switch (layoutType)
		{
			case LayoutType::None:
				widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				return Invalid;
			case LayoutType::Block:
				break;
			case LayoutType::Flex:
			{
				recalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::FlexLayout*> pFlexLayoutComponent = widget.FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				if (!pFlexLayoutComponent.IsValid())
				{
					widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);

					pFlexLayoutComponent = widget.CreateDataComponent<Data::FlexLayout>(
						flexLayoutSceneData,
						Data::FlexLayout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}
					);
				}

				if (LIKELY(pFlexLayoutComponent.IsValid()))
				{
					pLayoutComponent = pFlexLayoutComponent.Get();
				}
			}
			break;
			case LayoutType::Grid:
			{
				recalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::GridLayout*> pGridLayoutComponent = widget.FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				if (pGridLayoutComponent.IsInvalid())
				{
					widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);

					pGridLayoutComponent = widget.CreateDataComponent<Data::GridLayout>(
						gridLayoutSceneData,
						Data::GridLayout::Initializer{Data::Layout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}}
					);
				}

				if (LIKELY(pGridLayoutComponent.IsValid()))
				{
					pLayoutComponent = pGridLayoutComponent.Get();
				}
			}
			break;
		}

		return state.widgetInfos.EmplaceBack(WidgetInfo{
			widget,
			parentInfo,
			{},
			position,
			size,
			recalculationFlags,
			style,
			matchingModifiers,
			ContentAreaChangeFlags{},
			pLayoutComponent,
			style.GetSize(Style::ValueTypeIdentifier::MinimumSize, Style::Size{0_percent, 0_percent}, matchingModifiers),
			Style::Size{PixelValue{size.x}, PixelValue{size.y}},
			widget.GetMaximumSize(style, matchingModifiers, textDrawableSceneData),
			style.GetWithDefault<Style::SizeAxisEdges>(Style::ValueTypeIdentifier::Position, Style::SizeAxisEdges{Invalid}, matchingModifiers),
			style.GetSize(Style::ValueTypeIdentifier::ChildOffset, Math::Zero, matchingModifiers),
			style.GetSize(Style::ValueTypeIdentifier::ChildEntrySize, Style::Size{100_percent, 100_percent}, matchingModifiers),
			style.GetWithDefault(Style::ValueTypeIdentifier::ChildGrowthFactor, 0.f, matchingModifiers),
			style.GetWithDefault(Style::ValueTypeIdentifier::ChildShrinkFactor, 0.f, matchingModifiers),
			layoutType,
			orientation,
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers),
			style.GetWithDefault(Style::ValueTypeIdentifier::ElementSizingType, ElementSizingType(ElementSizingType::Default), matchingModifiers),
			style.GetWithDefault(Style::ValueTypeIdentifier::WrapType, WrapType(WrapType::NoWrap), matchingModifiers),
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::PrimaryDirectionAlignment, Alignment::Start, matchingModifiers),
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::SecondaryDirectionAlignment, Alignment::Stretch, matchingModifiers),
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::ChildPrimaryAlignment, Alignment::Inherit, matchingModifiers),
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::ChildSecondaryAlignment, Alignment::Inherit, matchingModifiers),
			positionType,
			GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Padding, matchingModifiers),
			GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Margin, matchingModifiers)
		});
	}

	Optional<WidgetInfo*> EmplaceWithParent(
		Widget& widget,
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry style,
		const Style::CombinedEntry::MatchingModifiers matchingModifiers,
		const LayoutType layoutType,
		const EnumFlags<RecalculationFlags> recalculationFlags,
		Variant<uint16, WidgetInfo*> rootParentInfo,
		uint16& parentIndex,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		State& state
	)
	{
		Widget* pParent = widget.GetParentSafe();

		if (pParent != nullptr)
		{
			const Style::CombinedEntry parentStyle = pParent->GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
			const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry, modifiersSceneData);
			const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);
			const LayoutType parentLayoutType = GetLayoutFromStyle(parentStyle, parentMatchingModifiers, *pParent);

			EmplaceWithParent(
				*pParent,
				sceneRegistry,
				parentStyle,
				parentMatchingModifiers,
				parentLayoutType,
				RecalculationFlags::IsFinal,
				rootParentInfo,
				parentIndex,
				worldTransformSceneData,
				flexLayoutSceneData,
				gridLayoutSceneData,
				textDrawableSceneData,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				state
			);
			return EmplaceSimpleWidget(
				widget,
				sceneRegistry,
				style,
				matchingModifiers,
				layoutType,
				recalculationFlags,
				parentIndex++,
				worldTransformSceneData,
				flexLayoutSceneData,
				gridLayoutSceneData,
				textDrawableSceneData,
				state
			);
		}
		else
		{
			return EmplaceSimpleWidget(
				widget,
				sceneRegistry,
				style,
				matchingModifiers,
				layoutType,
				recalculationFlags,
				rootParentInfo,
				worldTransformSceneData,
				flexLayoutSceneData,
				gridLayoutSceneData,
				textDrawableSceneData,
				state
			);
		}
	}

	Optional<WidgetInfo*> EmplaceRootWidget(
		Widget& widget,
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry style,
		const Style::CombinedEntry::MatchingModifiers matchingModifiers,
		const LayoutType layoutType,
		Variant<uint16, WidgetInfo*> rootParentInfo,
		uint16& parentIndex,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		State& state
	)
	{
		Assert(!widget.IsDestroying(sceneRegistry));

		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);

		RecalculationFlags recalculationFlags = RecalculationFlags::IsFinal;
		switch (layoutType)
		{
			case LayoutType::None:
				widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				return Invalid;
			case LayoutType::Block:
			{
				widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);

				const bool hasChildren = widget.HasChildren();
				const uint8 direction = (uint8)orientation;
				recalculationFlags &= ~((RecalculationFlags::HasPositionedChildrenX << direction) * hasChildren);
			}
			break;
			case LayoutType::Flex:
			{
				recalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::FlexLayout*> pFlexLayoutComponent = widget.FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
				if (!pFlexLayoutComponent.IsValid())
				{
					widget.RemoveDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);

					pFlexLayoutComponent = widget.CreateDataComponent<Data::FlexLayout>(
						flexLayoutSceneData,
						Data::FlexLayout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}
					);
				}
				Assert(pFlexLayoutComponent.IsValid());
				const bool hasDataSource = widget.HasDataComponentOfType(sceneRegistry, dataSourceSceneData.GetIdentifier());
				const bool hasDynamicChildren = pFlexLayoutComponent.IsValid() && hasDataSource &&
				                                pFlexLayoutComponent->GetFilteredDataCount(widget, sceneRegistry) > 0;

				recalculationFlags &= ~(RecalculationFlags::HasSpawnedDynamicChildren * hasDynamicChildren);

				const bool hasChildren = widget.HasChildren() | hasDynamicChildren;
				recalculationFlags &= ~(RecalculationFlags::HasPositionedChildren * hasChildren);

				recalculationFlags &= ~(RecalculationFlags::HasGrownChildrenX * (orientation == Orientation::Horizontal && hasChildren));
				recalculationFlags &= ~(RecalculationFlags::HasGrownChildrenY * (orientation == Orientation::Vertical && hasChildren));
			}
			break;
			case LayoutType::Grid:
			{
				recalculationFlags |= RecalculationFlags::IsDynamicLayout;

				Optional<Data::GridLayout*> pGridLayoutComponent = widget.FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
				if (pGridLayoutComponent.IsInvalid())
				{
					widget.RemoveDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);

					pGridLayoutComponent = widget.CreateDataComponent<Data::GridLayout>(
						gridLayoutSceneData,
						Data::GridLayout::Initializer{Data::Layout::Initializer{Data::Component::Initializer{widget, sceneRegistry}}}
					);
				}
				Assert(pGridLayoutComponent.IsValid());
				const bool hasDataSource = widget.HasDataComponentOfType(sceneRegistry, dataSourceSceneData.GetIdentifier());
				const bool hasDynamicChildren = pGridLayoutComponent != nullptr && hasDataSource &&
				                                pGridLayoutComponent->GetFilteredDataCount(widget, sceneRegistry) > 0;
				recalculationFlags &= ~(RecalculationFlags::HasSpawnedDynamicChildren * hasDynamicChildren);

				const bool hasChildren = widget.HasChildren() | hasDynamicChildren;
				recalculationFlags &= ~(RecalculationFlags::HasPositionedChildren * hasChildren);
			}
			break;
		}

		return EmplaceWithParent(
			widget,
			sceneRegistry,
			style,
			matchingModifiers,
			layoutType,
			recalculationFlags,
			rootParentInfo,
			parentIndex,
			worldTransformSceneData,
			flexLayoutSceneData,
			gridLayoutSceneData,
			textDrawableSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			state
		);
	}

	[[nodiscard]] Math::Rectanglei GetMaskedContentArea(const WidgetInfo& __restrict widgetInfo, const ArrayView<WidgetInfo> widgetInfos)
	{
		Math::Rectanglei contentArea{widgetInfo.m_relativePosition, widgetInfo.m_size};
		for (Optional<WidgetInfo*> pParentInfo = widgetInfo.GetParentInfo(widgetInfos); pParentInfo.IsValid();
		     pParentInfo = pParentInfo->GetParentInfo(widgetInfos))
		{
			const bool shouldMask = pParentInfo->m_overflowType != OverflowType::Visible;
			if (shouldMask)
			{
				Math::Rectanglei parentContentArea = {pParentInfo->m_relativePosition, pParentInfo->m_size};
				contentArea = parentContentArea.Mask(contentArea);
			}
		}
		return contentArea;
	};

	constexpr uint16
	GetLastPositionedChildIndex(const ArrayView<const uint16, uint16> childIndices, const ArrayView<const WidgetInfo, uint16> widgets)
	{
		Assert(childIndices.HasElements());
		uint16 lastIndex = childIndices.GetSize() - 1;
		while (widgets[childIndices[lastIndex]].GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout) && lastIndex > 0)
		{
			lastIndex--;
		}
		return lastIndex;
	}

	constexpr uint16 GetLeftPositionedChildNeighborIndex(
		const uint16 currentIndex, const ArrayView<const uint16, uint16> childIndices, const ArrayView<const WidgetInfo, uint16> widgets
	)
	{
		uint16 leftNeighborIndex = Math::Max(currentIndex, (uint16)1u) - 1;
		while (widgets[childIndices[leftNeighborIndex]].GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout) &&
		       leftNeighborIndex > 0)
		{
			leftNeighborIndex--;
		}
		return leftNeighborIndex;
	}

	constexpr uint16 GetRightPositionedChildNeighborIndex(
		const uint16 currentIndex, const ArrayView<const uint16, uint16> childIndices, const ArrayView<const WidgetInfo, uint16> widgets
	)
	{
		uint16 rightNeighborIndex = Math::Min(uint16(currentIndex + 1), uint16(childIndices.GetSize() - 1));
		while (rightNeighborIndex < childIndices.GetSize() &&
		       widgets[childIndices[rightNeighborIndex]].GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout))
		{
			rightNeighborIndex++;
		}
		return rightNeighborIndex;
	}

	[[nodiscard]] bool TryResolveWidgetSize(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Optional<WidgetInfo*> pParentInfo,
		const EnumFlags<RecalculationFlags> parentRecalculationFlags,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry
	)
	{
		Style::Size preferredSize = widgetInfo->m_preferredSize;

		// TODO: Move these into the population
		if (preferredSize.x.Is<Style::AutoType>())
		{
			preferredSize.x = 100_percent;

			if (pParentInfo->GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenX))
			{
				preferredSize.x = 0_px;
			}
			else if (pParentInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout))
			{
				switch (pParentInfo->m_orientation)
				{
					case Orientation::Horizontal:
					{
						const float growthFactor = widgetInfo->m_growthFactor;
						const float shrinkFactor = widgetInfo->m_shrinkFactor;
						if (growthFactor > 0 || shrinkFactor > 0)
						{
							preferredSize.x = 0_px;
						}
					}
					break;
					case Orientation::Vertical:
					{
						if (pParentInfo->m_childSecondaryAlignment == Alignment::Stretch)
						{
							preferredSize.x = 0_px;
						}
					}
					break;
				}
			}
		}
		else if (preferredSize.x.Is<Style::FitContentSizeType>())
		{
			preferredSize.x = 0_px;
			const EnumFlags<RecalculationFlags> newFlags =
				RecalculationFlags::ExpandToFitChildrenX *
				(widgetInfo->m_childrenIndices.HasElements() |
			   (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::HasSpawnedDynamicChildren)));
			state.AppendWidgetFlags(widgetInfo, newFlags);
		}
		if (preferredSize.y.Is<Style::AutoType>() || preferredSize.y.Is<Style::FitContentSizeType>())
		{
			preferredSize.y = 0_px;
			const EnumFlags<RecalculationFlags> newFlags =
				RecalculationFlags::ExpandToFitChildrenY *
				(widgetInfo->m_childrenIndices.HasElements() |
			   (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::HasSpawnedDynamicChildren)));
			state.AppendWidgetFlags(widgetInfo, newFlags);
		}
		if (pParentInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout))
		{
			const float growthFactor = widgetInfo->m_growthFactor;
			const float shrinkFactor = widgetInfo->m_shrinkFactor;
			if (growthFactor > 0 || shrinkFactor > 0)
			{
				const uint8 parentFlexDirection = (uint8)pParentInfo->m_orientation;
				state.ClearWidgetFlags(widgetInfo, RecalculationFlags::ExpandToFitChildrenX << parentFlexDirection);
			}
		}

		Style::Size minimumSizeStyle = widgetInfo->m_minimumSize;
		Style::Size maximumSizeStyle = widgetInfo->m_maximumSize;
		const bool shouldSkipMaximumX = parentRecalculationFlags.IsSet(RecalculationFlags::ExpandToFitChildrenX);
		const bool shouldSkipMaximumY = parentRecalculationFlags.IsSet(RecalculationFlags::ExpandToFitChildrenY);
		maximumSizeStyle.x = shouldSkipMaximumX ? Style::SizeAxis{Style::NoMaximum} : maximumSizeStyle.x;
		maximumSizeStyle.y = shouldSkipMaximumY ? Style::SizeAxis{Style::NoMaximum} : maximumSizeStyle.y;

		EnumFlags<RecalculationFlags> checkableFlags;

		if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeXFinal))
		{
			if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenX))
			{
				bool canCheckSizeX = widgetInfo->GetRecalculationFlags().AreAllSet(
					RecalculationFlags::HasPositionedChildrenX | RecalculationFlags::HasSpawnedDynamicChildren
				);
				EnumFlags<RecalculationFlags> checkedFlags = RecalculationFlags::IsRelativePositionXFinal;
				if (widgetInfo->m_defaultParentSecondaryAlignment != Alignment::Stretch || widgetInfo->m_orientation == Orientation::Horizontal)
				{
					checkedFlags |= RecalculationFlags::IsAbsoluteSizeXFinal;
				}
				else
				{
					checkedFlags |= RecalculationFlags::IsRelativeSizeXFinal;
				}
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
					if (childInfo.m_preferredSize.x.DependsOnParentDimensions())
					{
						continue;
					}
					if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
					{
						canCheckSizeX &= childInfo.GetRecalculationFlags().AreAllSet(checkedFlags);
					}
				}

				checkableFlags |= RecalculationFlags::IsRelativeSizeXFinal * canCheckSizeX;
			}
			else
			{
				const bool isConstantX = preferredSize.x.IsConstantValue() & minimumSizeStyle.x.IsConstantValue() &
				                         maximumSizeStyle.x.IsConstantValue();
				const bool canCheckSizeX = isConstantX | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal);
				checkableFlags |= RecalculationFlags::IsRelativeSizeXFinal * canCheckSizeX;
			}
		}

		if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeYFinal))
		{
			if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenY))
			{
				bool canCheckSizeY = widgetInfo->GetRecalculationFlags().AreAllSet(
					RecalculationFlags::HasPositionedChildrenY | RecalculationFlags::HasSpawnedDynamicChildren
				);
				EnumFlags<RecalculationFlags> checkedFlags = RecalculationFlags::IsRelativePositionYFinal;
				if (widgetInfo->m_defaultParentSecondaryAlignment != Alignment::Stretch || widgetInfo->m_orientation == Orientation::Vertical)
				{
					checkedFlags |= RecalculationFlags::IsAbsoluteSizeYFinal;
				}
				else
				{
					checkedFlags |= RecalculationFlags::IsRelativeSizeYFinal;
				}
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
					if (childInfo.m_preferredSize.y.DependsOnParentDimensions())
					{
						continue;
					}
					if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
					{
						canCheckSizeY &= childInfo.GetRecalculationFlags().AreAllSet(checkedFlags);
					}
				}

				checkableFlags |= RecalculationFlags::IsRelativeSizeYFinal * canCheckSizeY;
			}
			else
			{
				const bool isConstantY = preferredSize.y.IsConstantValue() & minimumSizeStyle.y.IsConstantValue() &
				                         maximumSizeStyle.y.IsConstantValue();
				const bool canCheckSizeY = isConstantY | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal);
				checkableFlags |= RecalculationFlags::IsRelativeSizeYFinal * canCheckSizeY;
			}
		}

		if (checkableFlags.AreAnySet(RecalculationFlags::IsRelativeSizeFinal))
		{
			const Math::Vector2i parentAvailableContentAreaSize =
				getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);
			Math::Vector2i size = preferredSize.Get(parentAvailableContentAreaSize, screenProperties);
			if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenX) & checkableFlags.IsSet(RecalculationFlags::IsRelativeSizeXFinal))
			{
				if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout) && widgetInfo->m_orientation == Orientation::Horizontal && widgetInfo->m_layoutType == LayoutType::Grid)
				{
					const Math::Vector2i availableChildContentSize =
						getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);

					const Style::Size& entrySizeStyle = widgetInfo->m_childEntrySize;
					const Math::Vector2i entrySize = entrySizeStyle.Get(availableChildContentSize, screenProperties);

					const Math::Vector2i entryOffset = widgetInfo->m_childOffset.Get(availableChildContentSize, screenProperties);

					const uint16 maximumVisibleColumnCount = (uint16
					)Math::Floor((float)availableChildContentSize.x / (float)(entrySize.x /* + entryOffset.x */));

					size.x = entrySize.x * maximumVisibleColumnCount + entryOffset.x * (Math::Max(maximumVisibleColumnCount, 1) - 1);
				}
				else
				{
					int maximumEndPositionX = 0;
					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
						if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
						{
							maximumEndPositionX = Math::Max(maximumEndPositionX, childInfo.m_relativePosition.x + childInfo.m_size.x);
						}
					}

					const Style::SizeAxisEdges& __restrict paddingStyle = widgetInfo->m_padding;
					const int paddingLeft = paddingStyle.m_left.Get(parentAvailableContentAreaSize.x, screenProperties);
					const int paddingRight = paddingStyle.m_right.Get(parentAvailableContentAreaSize.x, screenProperties);
					const int totalPadding = (paddingLeft + paddingRight); // * (widgetInfo->m_elementSizingType == ElementSizingType::ContentBox);

					const Style::SizeAxisEdges& __restrict marginStyle = widgetInfo->m_margin;
					const int marginLeft = marginStyle.m_left.Get(parentAvailableContentAreaSize.x, screenProperties);
					const int marginRight = marginStyle.m_right.Get(parentAvailableContentAreaSize.x, screenProperties);
					const int totalMargin = marginLeft + marginRight;

					size.x += maximumEndPositionX + totalPadding + totalMargin;
				}
			}
			else
			{
				const Style::SizeAxisEdges& __restrict paddingStyle = widgetInfo->m_padding;
				const int paddingLeft = paddingStyle.m_left.Get(parentAvailableContentAreaSize.x, screenProperties);
				const int paddingRight = paddingStyle.m_right.Get(parentAvailableContentAreaSize.x, screenProperties);
				const int totalPadding = (paddingLeft + paddingRight) * (widgetInfo->m_elementSizingType == ElementSizingType::ContentBox);

				const Style::SizeAxisEdges& __restrict marginStyle = widgetInfo->m_margin;
				const int marginLeft = marginStyle.m_left.Get(parentAvailableContentAreaSize.x, screenProperties);
				const int marginRight = marginStyle.m_right.Get(parentAvailableContentAreaSize.x, screenProperties);
				const int totalMargin = marginLeft + marginRight;

				size.x += totalPadding + totalMargin;
			}

			if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::ExpandToFitChildrenY) & checkableFlags.IsSet(RecalculationFlags::IsRelativeSizeYFinal))
			{
				if (widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout) && widgetInfo->m_orientation == Orientation::Vertical && widgetInfo->m_layoutType == LayoutType::Grid)
				{
					const Optional<Data::GridLayout* const *> pLayoutComponent = widgetInfo->m_pLayout.Get<Data::GridLayout*>();
					Assert(pLayoutComponent.IsValid());

					const Math::Vector2i availableChildContentSize =
						getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);

					const Style::Size& entrySizeStyle = widgetInfo->m_childEntrySize;
					const Math::Vector2i entrySize = entrySizeStyle.Get(availableChildContentSize, screenProperties);

					const Math::Vector2i entryOffset = widgetInfo->m_childOffset.Get(availableChildContentSize, screenProperties);

					const uint16 maximumVisibleColumnCount = (uint16
					)Math::Floor((float)availableChildContentSize.x / (float)(entrySize.x /* + entryOffset.x */));

					const uint32 itemCount = (*pLayoutComponent)->GetFilteredDataCount(*widgetInfo->m_pWidget, sceneRegistry);
					const uint16 maximumVisibleRowCount = uint16(maximumVisibleColumnCount ? itemCount / maximumVisibleColumnCount : 0) + 1;

					size.y = entrySize.y * maximumVisibleRowCount + entryOffset.y * (Math::Max(maximumVisibleRowCount, 1) - 1);
				}
				else
				{
					int maximumEndPositionY = 0;
					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
						if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
						{
							maximumEndPositionY = Math::Max(maximumEndPositionY, childInfo.m_relativePosition.y + childInfo.m_size.y);
						}
					}

					const Style::SizeAxisEdges& __restrict paddingStyle = widgetInfo->m_padding;
					const int paddingTop = paddingStyle.m_top.Get(parentAvailableContentAreaSize.y, screenProperties);
					const int paddingBottom = paddingStyle.m_bottom.Get(parentAvailableContentAreaSize.y, screenProperties);
					const int totalPadding = (paddingTop + paddingBottom); // * (widgetInfo->m_elementSizingType == ElementSizingType::ContentBox);

					const Style::SizeAxisEdges& __restrict marginStyle = widgetInfo->m_margin;
					const int marginTop = marginStyle.m_top.Get(parentAvailableContentAreaSize.y, screenProperties);
					const int marginBottom = marginStyle.m_bottom.Get(parentAvailableContentAreaSize.y, screenProperties);
					const int totalMargin = marginTop + marginBottom;

					size.y = maximumEndPositionY + totalPadding + totalMargin;
				}
			}
			else
			{
				const Style::SizeAxisEdges& __restrict paddingStyle = widgetInfo->m_padding;
				const int paddingTop = paddingStyle.m_top.Get(parentAvailableContentAreaSize.y, screenProperties);
				const int paddingBottom = paddingStyle.m_bottom.Get(parentAvailableContentAreaSize.y, screenProperties);
				const int totalPadding = (paddingTop + paddingBottom) * (widgetInfo->m_elementSizingType == ElementSizingType::ContentBox);

				const Style::SizeAxisEdges& __restrict marginStyle = widgetInfo->m_margin;
				const int marginTop = marginStyle.m_top.Get(parentAvailableContentAreaSize.y, screenProperties);
				const int marginBottom = marginStyle.m_bottom.Get(parentAvailableContentAreaSize.y, screenProperties);
				const int totalMargin = marginTop + marginBottom;

				size.y += totalPadding + totalMargin;
			}

			const Math::Vector2i minimumSize = minimumSizeStyle.Get(parentAvailableContentAreaSize, screenProperties);
			const Math::Vector2i maximumSize = maximumSizeStyle.Get(parentAvailableContentAreaSize, screenProperties);
			size = Math::Clamp(size, minimumSize, maximumSize);

			const bool canApplyX = !widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeXFinal) &
			                       checkableFlags.IsSet(RecalculationFlags::IsRelativeSizeXFinal);
			const bool canApplyY = !widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeYFinal) &
			                       checkableFlags.IsSet(RecalculationFlags::IsRelativeSizeYFinal);
			widgetInfo->m_size += {size.x * canApplyX, size.y * canApplyY};

			if (state.AppendWidgetFlags(widgetInfo, checkableFlags))
			{
				return true;
			}
		}

		return false;
	}

	[[nodiscard]] bool TryResolveWidgetRelativePosition(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Optional<WidgetInfo*> pParentInfo,
		const EnumFlags<RecalculationFlags> parentRecalculationFlags,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry
	)
	{
		EnumFlags<RecalculationFlags> newFlags;
		switch (widgetInfo->m_positionType)
		{
			case PositionType::Relative:
			{
				const Math::Vector2i parentAvailableContentAreaSize =
					getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);

				const Style::SizeAxisEdges& __restrict positionOffsetEdges = widgetInfo->m_positionOffset;
				if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativePositionXFinal))
				{
					if (positionOffsetEdges.m_left.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_left.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal);
						newFlags |= RecalculationFlags::IsRelativePositionXFinal * canCheckPosition;

						const int offset = positionOffsetEdges.m_left.Get(parentAvailableContentAreaSize.x, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.x += offset;
					}
					else if (positionOffsetEdges.m_right.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_right.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal);
						newFlags |= RecalculationFlags::IsRelativePositionXFinal * canCheckPosition;

						const int offset = -positionOffsetEdges.m_right.Get(parentAvailableContentAreaSize.x, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.x += offset;
					}
					else
					{
						newFlags |= RecalculationFlags::IsRelativePositionXFinal;
					}
				}

				if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativePositionYFinal))
				{
					if (positionOffsetEdges.m_top.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_top.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal);
						newFlags |= RecalculationFlags::IsRelativePositionYFinal * canCheckPosition;

						const int offset = positionOffsetEdges.m_top.Get(parentAvailableContentAreaSize.y, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.y += offset;
					}
					else if (positionOffsetEdges.m_bottom.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_bottom.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal);
						newFlags |= RecalculationFlags::IsRelativePositionYFinal * canCheckPosition;

						const int offset = -positionOffsetEdges.m_bottom.Get(parentAvailableContentAreaSize.y, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.y += offset;
					}
					else
					{
						newFlags |= RecalculationFlags::IsRelativePositionYFinal;
					}
				}
			}
			break;
			case PositionType::Absolute:
			{
				const Math::Vector2i parentAvailableChildContentSize =
					getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);
				const Style::SizeAxisEdges& __restrict positionOffsetEdges = widgetInfo->m_positionOffset;

				if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativePositionXFinal))
				{
					if (positionOffsetEdges.m_left.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_left.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal);
						newFlags |= RecalculationFlags::IsRelativePositionXFinal * canCheckPosition;

						const int offset = positionOffsetEdges.m_left.Get(parentAvailableChildContentSize.x, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.x += offset;
					}
					else if (positionOffsetEdges.m_right.IsValid())
					{
						const bool canCheckPosition = parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal) &
						                              widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal);
						newFlags |= RecalculationFlags::IsRelativePositionXFinal * canCheckPosition;

						const float ratio = (float)positionOffsetEdges.m_right.Get(parentAvailableChildContentSize.x, screenProperties) /
						                    (float)parentAvailableChildContentSize.x;
						const int offset = (int32((float(parentAvailableChildContentSize.x - widgetInfo->m_size.x) * (1.f - ratio)))) *
						                   canCheckPosition;
						widgetInfo->m_relativePosition.x += offset * canCheckPosition;
					}
					else
					{
						newFlags |= RecalculationFlags::IsRelativePositionXFinal;
					}
				}

				if (!widgetInfo->GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativePositionYFinal))
				{
					if (positionOffsetEdges.m_top.IsValid())
					{
						const bool isConstant = positionOffsetEdges.m_top.IsConstantValue();
						const bool canCheckPosition = isConstant | parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal);
						newFlags |= RecalculationFlags::IsRelativePositionYFinal * canCheckPosition;

						const int offset = positionOffsetEdges.m_top.Get(parentAvailableChildContentSize.y, screenProperties) * canCheckPosition;
						widgetInfo->m_relativePosition.y += offset;
					}
					else if (positionOffsetEdges.m_bottom.IsValid())
					{
						const bool canCheckPosition = parentRecalculationFlags.AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal) &
						                              widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeYFinal);
						newFlags |= RecalculationFlags::IsRelativePositionYFinal * canCheckPosition;

						const float ratio = (float)positionOffsetEdges.m_bottom.Get(parentAvailableChildContentSize.y, screenProperties) /
						                    (float)parentAvailableChildContentSize.y;
						const int offset = (int32((float(parentAvailableChildContentSize.y - widgetInfo->m_size.y) * (1.f - ratio)))) *
						                   canCheckPosition;
						widgetInfo->m_relativePosition.y += offset;
					}
					else
					{
						newFlags |= RecalculationFlags::IsRelativePositionYFinal;
					}
				}
			}
			break;
			case PositionType::Static:
			case PositionType::Dynamic:
				Assert(false, "Should never be hit");
				return false;
		}

		return state.AppendWidgetFlags(widgetInfo, newFlags);
	}

	[[nodiscard]] bool ResolveUnspawnedWidgetDynamicChildren(
		State& state,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 widgetIndex,
		ReferenceWrapper<WidgetInfo> widgetInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	)
	{
		Assert(!widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::HasPositionedChildren));

		Optional<Data::Layout*> pLayoutComponent;
		if (Optional<Data::FlexLayout**> pFlexLayout = widgetInfo->m_pLayout.Get<Data::FlexLayout*>())
		{
			pLayoutComponent = *pFlexLayout;
		}
		else if (Optional<Data::GridLayout**> pGridLayout = widgetInfo->m_pLayout.Get<Data::GridLayout*>())
		{
			pLayoutComponent = *pGridLayout;
		}
		Assert(pLayoutComponent.IsValid());
		if (LIKELY(pLayoutComponent.IsValid() && widgetInfo->m_pWidget.IsValid()))
		{
			pLayoutComponent->PopulateItemWidgets(*widgetInfo->m_pWidget, sceneRegistry);
		}

		const uint16 totalChildWidgetCount = widgetInfo->m_pWidget.IsValid() ? GetTotalWidgetCount(*widgetInfo->m_pWidget) : 0;
		state.widgetInfos.Reserve(state.widgetInfos.GetSize() + totalChildWidgetCount);
		state.remainingWidgets.Reserve(state.widgetInfos.GetSize() + totalChildWidgetCount);

		// widgetInfos capacity may have changed, refresh iterators
		widgetInfo = state.widgetInfos[(uint16)widgetIndex];

		if (widgetInfo->m_pWidget.IsValid())
		{
			const uint16 childCount = widgetInfo->m_pWidget->GetChildCount();
			for (uint16 childIndex = 0; childIndex < childCount; ++childIndex)
			{
				Optional<Widget*> pChildWidget;
				{
					const Widget::ChildView children = widgetInfo->m_pWidget->GetChildren();
					if (childIndex < children.GetSize())
					{
						pChildWidget = children[childIndex];
					}
					else
					{
						break;
					}
				}

				if (!widgetInfo->m_childrenIndices.GetView().ContainsIf(
							[&childWidget = *pChildWidget, widgetInfos = state.widgetInfos.GetView()](const uint16 childIndex)
							{
								return widgetInfos[childIndex].m_pWidget == &childWidget;
							}
						))
				{
					populateWidgetInfos(
						state,
						*pChildWidget,
						sceneRegistry,
						(uint16)widgetIndex,
						worldTransformSceneData,
						flexLayoutSceneData,
						gridLayoutSceneData,
						textDrawableSceneData,
						dataSourceSceneData,
						externalStyleSceneData,
						inlineStyleSceneData,
						dynamicStyleSceneData,
						modifiersSceneData,
						screenProperties
					);

					// widgetInfos capacity may have changed, refresh iterators
					widgetInfo = state.widgetInfos[(uint16)widgetIndex];
				}
			}
		}

		if (state.AppendWidgetFlags(widgetInfo, RecalculationFlags::HasSpawnedDynamicChildren))
		{
			return true;
		}

		// Restart the loop
		state.Restart();
		return false;
	}

	[[nodiscard]] bool TryResolveWidgetGrowChildrenInPrimaryDirection(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Optional<WidgetInfo*> pParentInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		const uint8 direction
	)
	{
		const Math::Vector2i parentAvailableContentAreaSize =
			getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);

		const Math::Vector2i childOffset = widgetInfo->m_childOffset.Get(parentAvailableContentAreaSize, screenProperties);
		Assert(direction == 0 || direction == 1);

		// Start by normalizing the growth factors
		float growthNormalizationFactor = 0.f;
		float shrinkageNormalizationFactor = 0.f;
		{
			float totalGrowthFactor = 0.f;
			float totalShrinkageFactor = 0.f;
			for (const uint16 childIndex : widgetInfo->m_childrenIndices)
			{
				const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
				if (childInfo.GetRecalculationFlags().AreNoneSet(
							(RecalculationFlags::HasBeenGrownByParentX << direction) | RecalculationFlags::IsRemovedFromParentLayout
						))
				{
					const float childGrowthFactor = childInfo.m_growthFactor;
					const float childShrinkageFactor = childInfo.m_shrinkFactor;
					totalGrowthFactor += childGrowthFactor;
					totalShrinkageFactor += childShrinkageFactor;
					state.AppendWidgetFlags(
						childInfo,
						(RecalculationFlags::HasBeenGrownByParentX << direction) * (childGrowthFactor == 0.f && childShrinkageFactor == 0.f)
					);
				}
			}
			growthNormalizationFactor = totalGrowthFactor > 0 ? Math::MultiplicativeInverse(totalGrowthFactor) : 0;
			shrinkageNormalizationFactor = totalShrinkageFactor > 0 ? Math::MultiplicativeInverse(totalShrinkageFactor) : 0;
		}

		if (growthNormalizationFactor > 0 || shrinkageNormalizationFactor > 0)
		{
			const Math::Vector2i availableChildContentAreaSize =
				getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);
			int32 growableSpace = availableChildContentAreaSize[direction] -
			                      childOffset[direction] * (widgetInfo->m_childrenIndices.GetSize() - 1);
			bool canGrow = widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsRelativeSizeXFinal << direction);
			for (const uint16 childIndex : widgetInfo->m_childrenIndices)
			{
				const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
				if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
				{
					growableSpace -= childInfo.m_size[direction];
					canGrow &= childInfo.GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeXFinal << direction);
				}
			}
			growableSpace = Math::Max(growableSpace, 0);

			// TODO: Implement shrinking

			if (canGrow)
			{
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];

					if (childInfo.GetRecalculationFlags().AreAnySet(
								(RecalculationFlags::HasBeenGrownByParentX << direction) | RecalculationFlags::IsRemovedFromParentLayout
							))
					{
						continue;
					}

					const float childGrowthFactor = childInfo.m_growthFactor;
					const int growth = (int)Math::Floor((float)growableSpace * childGrowthFactor * growthNormalizationFactor);
					childInfo.m_size[direction] = Math::Clamp(
						childInfo.m_size[direction] + growth,
						childInfo.m_minimumSize[direction].Get(availableChildContentAreaSize[direction], screenProperties),
						childInfo.m_maximumSize[direction].Get(availableChildContentAreaSize[direction], screenProperties)
					);
					state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenGrownByParentX << direction);
				}

				if (state.AppendWidgetFlags(widgetInfo, RecalculationFlags::HasGrownChildrenX << direction))
				{
					return true;
				}
			}
		}
		else if (state.AppendWidgetFlags(widgetInfo, RecalculationFlags::HasGrownChildrenX << direction))
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool TryResolveWidgetGrowChildrenInSecondaryDirection(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		const uint8 direction
	)
	{
		Assert(direction == 0 || direction == 1);

		const uint8 stretchDirection = !direction;
		if (widgetInfo->m_defaultParentSecondaryAlignment == Alignment::Stretch)
		{
			const Math::Vector2i availableChildContentAreaSize =
				getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);
			bool canStretch = widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsRelativeSizeXFinal << stretchDirection);
			for (const uint16 childIndex : widgetInfo->m_childrenIndices)
			{
				const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
				if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
				{
					canStretch &= childInfo.GetRecalculationFlags().IsSet(RecalculationFlags::IsRelativeSizeXFinal << stretchDirection);
				}
			}

			if (canStretch)
			{
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];

					if (childInfo.GetRecalculationFlags().AreAnySet(
								(RecalculationFlags::HasBeenGrownByParentX << stretchDirection) | RecalculationFlags::IsRemovedFromParentLayout
							))
					{
						continue;
					}

					if (childInfo.m_preferredSize[stretchDirection].Is<Style::AutoType>())
					{
						childInfo.m_size[stretchDirection] = Math::Clamp(
							widgetInfo->m_size[stretchDirection],
							childInfo.m_minimumSize[stretchDirection].Get(availableChildContentAreaSize[stretchDirection], screenProperties),
							childInfo.m_maximumSize[stretchDirection].Get(availableChildContentAreaSize[stretchDirection], screenProperties)
						);
					}

					state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenGrownByParentX << stretchDirection);
				}

				if (state.AppendWidgetFlags(widgetInfo, RecalculationFlags::HasGrownChildrenX << stretchDirection))
				{
					return true;
				}
			}
		}
		else if (state.AppendWidgetFlags(widgetInfo, RecalculationFlags::HasGrownChildrenX << stretchDirection))
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool TryResolveWidgetPositionChildrenInPrimaryDirection(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Optional<WidgetInfo*> pParentInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		const uint8 primaryDirection
	)
	{
		const Math::Vector2i parentAvailableContentAreaSize =
			getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);

		const Math::Vector2i childOffset = widgetInfo->m_childOffset.Get(parentAvailableContentAreaSize, screenProperties);

		uint16 finalizedChildDynamicPositionCountPrimary = 0;
		for (const uint16 childIndex : widgetInfo->m_childrenIndices)
		{
			const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
			finalizedChildDynamicPositionCountPrimary +=
				childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
		}

		const Math::Vector2i availableChildContentSize =
			getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);

		const LayoutType layoutType = widgetInfo->m_layoutType;

		const Alignment defaultPrimaryAlignment = widgetInfo->m_defaultParentPrimaryAlignment;

		const uint16 totalChildCount = widgetInfo->m_childrenIndices.GetSize();

		switch (layoutType)
		{
			case LayoutType::Flex:
			{
				uint16 previousFinalizedChildDynamicPositionCountPrimary = finalizedChildDynamicPositionCountPrimary + 1;
				while ((previousFinalizedChildDynamicPositionCountPrimary != finalizedChildDynamicPositionCountPrimary) &&
				       (finalizedChildDynamicPositionCountPrimary != totalChildCount))
				{
					previousFinalizedChildDynamicPositionCountPrimary = finalizedChildDynamicPositionCountPrimary;

					uint16 i = 0;
					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						WidgetInfo& childInfo = state.widgetInfos[childIndex];

						if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << primaryDirection))
						{
							++i;
							continue;
						}

						const bool canCheckPrimaryAlignment =
							childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
						if (canCheckPrimaryAlignment)
						{
							// Adjust this widget's position
							Alignment childPrimaryAlignment = childInfo.GetPrimaryAlignment(defaultPrimaryAlignment);
							if (widgetInfo->m_recalculationFlags.IsSet(RecalculationFlags::ExpandToFitChildrenX << primaryDirection))
							{
								childPrimaryAlignment = Alignment::Start;
							}

							switch (childPrimaryAlignment)
							{
								case Alignment::Start:
								case Alignment::Stretch:
									break;
								case Alignment::Center:
									break;
								case Alignment::End:
									if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::ExpandToFitChildrenX << primaryDirection))
									{
										const uint16 lastIndex =
											GetLastPositionedChildIndex(widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
										if (i == lastIndex)
										{
											childPrimaryAlignment = Alignment::Start;
										}
									}
									break;
								case Alignment::Inherit:
									ExpectUnreachable();
							}

							switch (childPrimaryAlignment)
							{
								case Alignment::Start:
								case Alignment::Stretch:
								{
									if (i == 0)
									{
										state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
										finalizedChildDynamicPositionCountPrimary++;
									}
									else
									{
										const uint16 leftNeighborChildIndex =
											GetLeftPositionedChildNeighborIndex(i, widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
										const uint16 leftNeighborWidgetIndex = widgetInfo->m_childrenIndices[leftNeighborChildIndex];
										if (state.widgetInfos[leftNeighborWidgetIndex].GetRecalculationFlags().AreAllSet(
													RecalculationFlags::IsAbsoluteContentAreaXFinal << primaryDirection
												))
										{
											const WidgetInfo& __restrict leftNeighbor = state.widgetInfos[leftNeighborWidgetIndex];
											childInfo.m_relativePosition[primaryDirection] += leftNeighbor.m_relativePosition[primaryDirection] +
											                                                  leftNeighbor.m_size[primaryDirection] +
											                                                  childOffset[primaryDirection];
											state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
											finalizedChildDynamicPositionCountPrimary++;
										}
									}
								}
								break;
								case Alignment::Center:
								{
									if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection))
									{
										int leftNeighborRelativeEndPosition;
										if (i != 0)
										{
											const uint16 leftNeighborChildIndex =
												GetLeftPositionedChildNeighborIndex(i, widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
											const uint16 leftNeighborWidgetIndex = widgetInfo->m_childrenIndices[leftNeighborChildIndex];
											const WidgetInfo& __restrict leftNeighbor = state.widgetInfos[leftNeighborWidgetIndex];
											if (!leftNeighbor.GetRecalculationFlags().AreAllSet(
														RecalculationFlags::IsAbsoluteContentAreaXFinal << primaryDirection
													))
											{
												break;
											}

											leftNeighborRelativeEndPosition = leftNeighbor.m_relativePosition[primaryDirection] +
											                                  leftNeighbor.m_size[primaryDirection];
										}
										else
										{
											leftNeighborRelativeEndPosition = 0;
										}

										const uint16 lastIndex =
											GetLastPositionedChildIndex(widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
										int rightNeighborPosition;
										if (i != lastIndex)
										{
											const uint16 rightNeighborChildIndex =
												GetRightPositionedChildNeighborIndex(i, widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
											const uint16 rightNeighborWidgetIndex = widgetInfo->m_childrenIndices[rightNeighborChildIndex];

											const WidgetInfo& rightNeighbor = state.widgetInfos[rightNeighborWidgetIndex];
											// If the right neigbhor is also center oriented that means we can center them as a group together.
											if (rightNeighbor.GetPrimaryAlignment(defaultPrimaryAlignment) == Alignment::Center)
											{
												// First find the last centered neighbor
												uint16 lastCenteredNeighborIndex = rightNeighborChildIndex;
												bool canCheckNeighborSizes =
													rightNeighbor.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection);
												if (!canCheckNeighborSizes)
												{
													break;
												}

												for (uint16 centeredChildIndex = lastCenteredNeighborIndex + 1; centeredChildIndex < lastIndex + 1;
												     ++centeredChildIndex)
												{
													const WidgetInfo& centeredNeighbor = state.widgetInfos[widgetInfo->m_childrenIndices[centeredChildIndex]];
													if (centeredNeighbor.GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout))
													{
														continue;
													}

													if (rightNeighbor.GetPrimaryAlignment(defaultPrimaryAlignment) == Alignment::Center)
													{
														lastCenteredNeighborIndex = centeredChildIndex;
														canCheckNeighborSizes &= centeredNeighbor.GetRecalculationFlags().AreAllSet(
															RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection
														);
													}
													else
													{
														break;
													}
												}

												if (!canCheckNeighborSizes)
												{
													break;
												}

												const uint16 firstNonCenteredChildIndex = GetRightPositionedChildNeighborIndex(
													lastCenteredNeighborIndex,
													widgetInfo->m_childrenIndices.GetView(),
													state.widgetInfos.GetView()
												);
												int firstNonCenteredNeighborPosition;
												if (firstNonCenteredChildIndex < lastIndex)
												{
													const WidgetInfo& __restrict firstNonCenteredNeighbor =
														state.widgetInfos[widgetInfo->m_childrenIndices[firstNonCenteredChildIndex]];
													if (firstNonCenteredNeighbor.GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout))
													{
														continue;
													}

													if (!firstNonCenteredNeighbor.GetRecalculationFlags().AreAllSet(
																RecalculationFlags::IsAbsolutePositionXFinal << primaryDirection
															))
													{
														break;
													}
													firstNonCenteredNeighborPosition = firstNonCenteredNeighbor.m_relativePosition[primaryDirection];
												}
												else
												{
													if (!widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection))
													{
														break;
													}
													// Use the full available content area
													firstNonCenteredNeighborPosition = availableChildContentSize[primaryDirection];
												}

												rightNeighborPosition = leftNeighborRelativeEndPosition;
												for (uint16 centeredChildIndex = i; centeredChildIndex < lastIndex; ++centeredChildIndex)
												{
													WidgetInfo& centeredChildInfo = state.widgetInfos[widgetInfo->m_childrenIndices[centeredChildIndex]];
													rightNeighborPosition += (centeredChildInfo.m_size[primaryDirection] + childOffset[primaryDirection]);
												}

												rightNeighborPosition += rightNeighbor.m_size[primaryDirection];

												const int usedArea = rightNeighborPosition - leftNeighborRelativeEndPosition;
												const int availableArea = firstNonCenteredNeighborPosition - leftNeighborRelativeEndPosition;
												const int unusedArea = Math::Max(availableArea - usedArea, 0);

												int nextNeighborPosition = leftNeighborRelativeEndPosition + unusedArea / 2;

												for (uint16 centeredChildIndex = i; centeredChildIndex < lastCenteredNeighborIndex + 1; ++centeredChildIndex)
												{
													WidgetInfo& centeredChildInfo = state.widgetInfos[widgetInfo->m_childrenIndices[centeredChildIndex]];
													if (centeredChildInfo.GetRecalculationFlags().IsSet(RecalculationFlags::IsRemovedFromParentLayout))
													{
														continue;
													}

													centeredChildInfo.m_relativePosition[primaryDirection] += nextNeighborPosition;
													state.AppendWidgetFlags(centeredChildInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
													finalizedChildDynamicPositionCountPrimary++;

													nextNeighborPosition += centeredChildInfo.m_size[primaryDirection] + childOffset[primaryDirection];
												}

												break;
											}

											if (!rightNeighbor.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsolutePositionXFinal))
											{
												break;
											}
											rightNeighborPosition = rightNeighbor.m_relativePosition[primaryDirection];
										}
										else
										{
											if (!widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal))
											{
												break;
											}
											rightNeighborPosition = availableChildContentSize[primaryDirection];
										}

										const int availableArea = rightNeighborPosition - leftNeighborRelativeEndPosition;
										childInfo.m_relativePosition[primaryDirection] += leftNeighborRelativeEndPosition +
										                                                  (availableArea - childInfo.m_size[primaryDirection]) / 2;
										state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
										finalizedChildDynamicPositionCountPrimary++;
									}
								}
								break;
								case Alignment::End:
								{
									if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection))
									{
										const uint16 lastIndex =
											GetLastPositionedChildIndex(widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
										if (i == lastIndex)
										{
											if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << primaryDirection))
											{
												childInfo.m_relativePosition[primaryDirection] += availableChildContentSize[primaryDirection] -
												                                                  childInfo.m_size[primaryDirection];
												state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
												finalizedChildDynamicPositionCountPrimary++;
											}
										}
										else
										{
											const uint16 rightNeighborChildIndex =
												GetRightPositionedChildNeighborIndex(i, widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
											const uint16 rightNeighborWidgetIndex = widgetInfo->m_childrenIndices[rightNeighborChildIndex];
											if (state.widgetInfos[rightNeighborWidgetIndex].GetRecalculationFlags().AreAllSet(
														RecalculationFlags::IsAbsolutePositionXFinal << primaryDirection
													))
											{
												const WidgetInfo& __restrict rightNeighbor = state.widgetInfos[rightNeighborWidgetIndex];
												childInfo.m_relativePosition[primaryDirection] += rightNeighbor.m_relativePosition[primaryDirection] -
												                                                  childInfo.m_size[primaryDirection] -
												                                                  childOffset[primaryDirection];
												state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
												finalizedChildDynamicPositionCountPrimary++;
											}
										}
									}
								}
								break;
								case Alignment::Inherit:
									ExpectUnreachable();
							}
						}

						i++;
					}
				}
			}
			break;
			case LayoutType::Grid:
			{
				const Style::Size& entrySizeStyle = widgetInfo->m_childEntrySize;
				const Math::Vector2i entrySize = entrySizeStyle.Get(availableChildContentSize, screenProperties);

				const uint16 maximumVisibleColumnCount = (uint16)Math::Floor((float)availableChildContentSize.x / (float)entrySize.x);

				uint16 rowIndex = 0;
				uint16 columnIndex = 0;
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];

					/*if(childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParent))
					{
					  ++i;
					  columnIndex++;
					  if (columnIndex == maximumVisibleColumnCount)
					  {
					  rowIndex++;
					  columnIndex = 0;
					  }
					  continue;
					}*/

					childInfo.m_relativePosition[primaryDirection] =
						((entrySize + childOffset) * Math::Vector2i{columnIndex, rowIndex})[primaryDirection];

					state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);

					finalizedChildDynamicPositionCountPrimary++;

					columnIndex++;
					if (columnIndex == maximumVisibleColumnCount)
					{
						rowIndex++;
						columnIndex = 0;
					}
				}
			}
			break;
			case LayoutType::Block:
			{
				uint16 previousFinalizedChildDynamicPositionCountPrimary = finalizedChildDynamicPositionCountPrimary + 1;
				while ((previousFinalizedChildDynamicPositionCountPrimary != finalizedChildDynamicPositionCountPrimary) &&
				       (finalizedChildDynamicPositionCountPrimary != totalChildCount))
				{
					previousFinalizedChildDynamicPositionCountPrimary = finalizedChildDynamicPositionCountPrimary;

					uint16 i = 0;
					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						WidgetInfo& childInfo = state.widgetInfos[childIndex];

						if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << primaryDirection))
						{
							++i;
							continue;
						}

						const bool canCheckPrimaryAlignment =
							childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);

						if (canCheckPrimaryAlignment)
						{
							if (i == 0)
							{
								state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
								finalizedChildDynamicPositionCountPrimary++;
							}
							else
							{
								const uint16 leftNeighborChildIndex =
									GetLeftPositionedChildNeighborIndex(i, widgetInfo->m_childrenIndices.GetView(), state.widgetInfos.GetView());
								const uint16 leftNeighborWidgetIndex = widgetInfo->m_childrenIndices[leftNeighborChildIndex];
								if (state.widgetInfos[leftNeighborWidgetIndex].GetRecalculationFlags().AreAllSet(
											RecalculationFlags::IsAbsoluteContentAreaXFinal << primaryDirection
										))
								{
									const WidgetInfo& __restrict leftNeighbor = state.widgetInfos[leftNeighborWidgetIndex];
									childInfo.m_relativePosition[primaryDirection] += leftNeighbor.m_relativePosition[primaryDirection] +
									                                                  leftNeighbor.m_size[primaryDirection] + childOffset[primaryDirection];
									state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << primaryDirection);
									finalizedChildDynamicPositionCountPrimary++;
								}
							}
						}

						i++;
					}
				}
			}
			break;
			case LayoutType::None:
				break;
		}

		if (state.AppendWidgetFlags(
					widgetInfo,
					(RecalculationFlags::HasPositionedChildrenX << primaryDirection) * (finalizedChildDynamicPositionCountPrimary == totalChildCount)
				))
		{
			return true;
		}
		return false;
	}

	[[nodiscard]] bool TryResolveWidgetPositionChildrenInSecondaryDirection(
		State& state,
		const ReferenceWrapper<WidgetInfo> widgetInfo,
		const Optional<WidgetInfo*> pParentInfo,
		const Rendering::ScreenProperties& __restrict screenProperties,
		Entity::SceneRegistry& sceneRegistry,
		const uint8 primaryDirection
	)
	{
		const uint8 secondaryDirection = primaryDirection ^ 1;

		const Math::Vector2i parentAvailableContentAreaSize =
			getAvailableChildContentAreaSize(state.widgetInfos.GetView(), *pParentInfo, screenProperties, sceneRegistry);

		const Math::Vector2i childOffset = widgetInfo->m_childOffset.Get(parentAvailableContentAreaSize, screenProperties);

		uint16 finalizedChildDynamicPositionCountSecondary = 0;
		for (const uint16 childIndex : widgetInfo->m_childrenIndices)
		{
			const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
			finalizedChildDynamicPositionCountSecondary +=
				childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
		}

		const Math::Vector2i availableChildContentSize =
			getAvailableChildContentAreaSize(state.widgetInfos.GetView(), widgetInfo, screenProperties, sceneRegistry);

		const LayoutType layoutType = widgetInfo->m_layoutType;

		const Alignment defaultSecondaryAlignment = widgetInfo->m_defaultParentSecondaryAlignment;

		const uint16 totalChildCount = widgetInfo->m_childrenIndices.GetSize();

		switch (layoutType)
		{
			case LayoutType::Flex:
			{
				uint16 previousFinalizedChildDynamicPositionCountSecondary = finalizedChildDynamicPositionCountSecondary + 1;
				while ((previousFinalizedChildDynamicPositionCountSecondary != finalizedChildDynamicPositionCountSecondary) &&
				       (finalizedChildDynamicPositionCountSecondary != totalChildCount))
				{
					previousFinalizedChildDynamicPositionCountSecondary = finalizedChildDynamicPositionCountSecondary;

					// const bool canWrap = widgetInfo->m_wrapType != WrapType::NoWrap;

					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						WidgetInfo& childInfo = state.widgetInfos[childIndex];

						if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection))
						{
							continue;
						}

						const bool canCheckSecondaryAlignment =
							childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
						if (canCheckSecondaryAlignment)
						{
							Alignment childSecondaryAlignment = childInfo.GetSecondaryAlignment(defaultSecondaryAlignment);
							if (widgetInfo->m_recalculationFlags.IsSet(RecalculationFlags::ExpandToFitChildrenX << secondaryDirection))
							{
								childSecondaryAlignment = Alignment::Start;
							}

							switch (childSecondaryAlignment)
							{
								case Alignment::Start:
								case Alignment::Stretch:
								{
									state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
									finalizedChildDynamicPositionCountSecondary++;
								}
								break;
								case Alignment::Center:
								{
									if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << secondaryDirection)
								&& childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << secondaryDirection))
									{
										childInfo.m_relativePosition[secondaryDirection] += (availableChildContentSize[secondaryDirection] -
										                                                     childInfo.m_size[secondaryDirection]) /
										                                                    2;
										state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
										finalizedChildDynamicPositionCountSecondary++;
									}
								}
								break;
								case Alignment::End:
								{
									if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << secondaryDirection)
								&& childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsAbsoluteSizeXFinal << secondaryDirection))
									{
										childInfo.m_relativePosition[secondaryDirection] += availableChildContentSize[secondaryDirection] -
										                                                    childInfo.m_size[secondaryDirection];
										state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
										finalizedChildDynamicPositionCountSecondary++;
									}
								}
								break;
								case Alignment::Inherit:
									ExpectUnreachable();
							}
						}
					}
				}
			}
			break;
			case LayoutType::Grid:
			{
				const Style::Size& entrySizeStyle = widgetInfo->m_childEntrySize;
				const Math::Vector2i entrySize = entrySizeStyle.Get(availableChildContentSize, screenProperties);

				const uint16 maximumVisibleColumnCount = (uint16)Math::Floor((float)availableChildContentSize.x / (float)entrySize.x);

				uint16 rowIndex = 0;
				uint16 columnIndex = 0;
				for (const uint16 childIndex : widgetInfo->m_childrenIndices)
				{
					WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];

					/*if(childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParent))
					{
					  ++i;
					  columnIndex++;
					  if (columnIndex == maximumVisibleColumnCount)
					  {
					  rowIndex++;
					  columnIndex = 0;
					  }
					  continue;
					}*/

					childInfo.m_relativePosition[secondaryDirection] =
						((entrySize + childOffset) * Math::Vector2i{columnIndex, rowIndex})[secondaryDirection];

					state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);

					finalizedChildDynamicPositionCountSecondary++;

					columnIndex++;
					if (columnIndex == maximumVisibleColumnCount)
					{
						rowIndex++;
						columnIndex = 0;
					}
				}
			}
			break;
			case LayoutType::Block:
			{
				uint16 previousFinalizedChildDynamicPositionCountSecondary = finalizedChildDynamicPositionCountSecondary + 1;
				while ((previousFinalizedChildDynamicPositionCountSecondary != finalizedChildDynamicPositionCountSecondary) &&
				       (finalizedChildDynamicPositionCountSecondary != totalChildCount))
				{
					previousFinalizedChildDynamicPositionCountSecondary = finalizedChildDynamicPositionCountSecondary;

					for (const uint16 childIndex : widgetInfo->m_childrenIndices)
					{
						WidgetInfo& childInfo = state.widgetInfos[childIndex];

						if (childInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection))
						{
							continue;
						}

						state.AppendWidgetFlags(childInfo, RecalculationFlags::HasBeenPositionedByParentX << secondaryDirection);
						finalizedChildDynamicPositionCountSecondary++;
					}
				}
			}
			break;
			case LayoutType::None:
				break;
		}

		if (state.AppendWidgetFlags(
					widgetInfo,
					(RecalculationFlags::HasPositionedChildrenX << secondaryDirection) *
						(finalizedChildDynamicPositionCountSecondary == totalChildCount)
				))
		{
			return true;
		}
		return false;
	}

	void Widget::RecalculateHierarchyInternal(Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& owningWindow)
	{
		WidgetInfoContainer widgetInfoContainer;
		widgetInfoContainer.Clear();
		widgetInfoContainer.Reserve(GetTotalWidgetCount(*this));

		RemainingWidgetsMask remainingWidgets;
		PositionedWidgetsMask positionedWidgets;

		State state{widgetInfoContainer, remainingWidgets, positionedWidgets};

		state.widgetInfos.Clear();
		state.remainingWidgets.ClearAll();
		state.positionedWidgets.ClearAll();

		state.remainingWidgets.Reserve(widgetInfoContainer.GetSize());
		state.widgetInfos.Reserve(widgetInfoContainer.GetSize());
		state.positionedWidgets.Reserve(widgetInfoContainer.GetSize());

		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform2D>& localTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::LocalTransform2D>();
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::GridLayout>();
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::DataSource>();
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		const Math::Vector2i rootSize = (Math::Vector2i)owningWindow.GetClientAreaSize();
		WidgetInfo rootWidgetDummyParentInfo{
			nullptr,
			{},
			{},
			Math::Zero,
			rootSize,
			RecalculationFlags::IsFinal,
			Style::CombinedEntry{},
			Style::CombinedEntry::MatchingModifiers{},
			ContentAreaChangeFlags{},
			{},
			Style::Size{0_percent, 0_percent},
			Style::Size{PixelValue{rootSize.x}, PixelValue{rootSize.y}},
			Style::Size{100_percent, 100_percent},
			Style::SizeAxisEdges{Invalid},
			Style::Size{Math::Zero},
			Style::Size{100_percent, 100_percent},
			0.f,
			0.f,
			LayoutType::Block,
			Orientation::Vertical,
			DefaultOverflowType,
			ElementSizingType::Default,
			WrapType::NoWrap,
			Alignment::Start,
			Alignment::Stretch,
			Alignment::Inherit,
			Alignment::Inherit,
			PositionType::Static,
			Style::SizeAxisEdges{Math::Zero}
		};

		const Style::CombinedEntry rootWidgetStyle = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> rootWidgetActiveModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers rootWidgetMatchingModifiers =
			rootWidgetStyle.GetMatchingModifiers(rootWidgetActiveModifiers);

		const LayoutType rootWidgetLayoutType = GetLayoutFromStyle(rootWidgetStyle, rootWidgetMatchingModifiers, *this);
		switch (rootWidgetLayoutType)
		{
			case LayoutType::None:
				return;
			case LayoutType::Block:
			case LayoutType::Flex:
			case LayoutType::Grid:
				break;
		}

		const Rendering::ScreenProperties screenProperties = owningWindow.GetCurrentScreenProperties();

		uint16 thisParentIndex = 0;
		{
			const Optional<WidgetInfo*> pThisWidgetInfo = EmplaceRootWidget(
				*this,
				sceneRegistry,
				rootWidgetStyle,
				rootWidgetMatchingModifiers,
				rootWidgetLayoutType,
				&rootWidgetDummyParentInfo,
				thisParentIndex,
				worldTransformSceneData,
				flexLayoutSceneData,
				gridLayoutSceneData,
				textDrawableSceneData,
				dataSourceSceneData,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				state
			);
			if (pThisWidgetInfo.IsInvalid())
			{
				return;
			}

			state.remainingWidgets.Reserve(state.widgetInfos.GetSize());
			if (!pThisWidgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsFinal))
			{
				state.remainingWidgets.Set(thisParentIndex);
			}

			const uint16 childCount = GetChildCount();
			for (uint16 childIndex = 0; childIndex < childCount; ++childIndex)
			{
				Optional<Widget*> pChildWidget;
				{
					const Widget::ChildView children = GetChildren();
					if (childIndex < children.GetSize())
					{
						pChildWidget = children[childIndex];
					}
					else
					{
						break;
					}
				}

				populateWidgetInfos(
					state,
					*pChildWidget,
					sceneRegistry,
					thisParentIndex,
					worldTransformSceneData,
					flexLayoutSceneData,
					gridLayoutSceneData,
					textDrawableSceneData,
					dataSourceSceneData,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData,
					screenProperties
				);
			}

			if constexpr (ENABLE_ASSERTS)
			{
				for ([[maybe_unused]] const uint32 widgetIndex : state.remainingWidgets.GetSetBitsIterator())
				{
					Assert(!state.widgetInfos[(uint16)widgetIndex].GetRecalculationFlags().AreAllSet(RecalculationFlags::IsFinal));
				}

				for (WidgetInfo& widgetInfo : state.widgetInfos)
				{
					[[maybe_unused]] const uint16 index = state.widgetInfos.GetIteratorIndex(&widgetInfo);
					if (widgetInfo.GetRecalculationFlags().AreAllSet(RecalculationFlags::IsFinal))
					{
						Assert(!state.remainingWidgets.IsSet(index));
					}
					else
					{
						Assert(state.remainingWidgets.IsSet(index));
					}
				}
			}

			while (state.ShouldRun())
			{
				state.Start();
				for (const uint32 widgetIndex : state.remainingWidgets.GetSetBitsIterator())
				{
					ReferenceWrapper<WidgetInfo> widgetInfo = state.widgetInfos[(uint16)widgetIndex];

					const Optional<WidgetInfo*> pParentInfo = widgetInfo->GetParentInfo(state.widgetInfos.GetView());

					const EnumFlags<RecalculationFlags> parentRecalculationFlags = pParentInfo->GetRecalculationFlags();

					if (!widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsRelativeSizeFinal))
					{
						if (TryResolveWidgetSize(state, widgetInfo, pParentInfo, parentRecalculationFlags, screenProperties, sceneRegistry))
						{
							continue;
						}
					}

					if (!widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::IsRelativePositionFinal))
					{
						if (TryResolveWidgetRelativePosition(state, widgetInfo, pParentInfo, parentRecalculationFlags, screenProperties, sceneRegistry))
						{
							continue;
						}
					}

					if (widgetInfo->GetRecalculationFlags().IsNotSet(RecalculationFlags::HasSpawnedDynamicChildren))
					{
						if (ResolveUnspawnedWidgetDynamicChildren(
									state,
									sceneRegistry,
									widgetIndex,
									widgetInfo,
									screenProperties,
									worldTransformSceneData,
									flexLayoutSceneData,
									gridLayoutSceneData,
									dataSourceSceneData,
									textDrawableSceneData,
									externalStyleSceneData,
									inlineStyleSceneData,
									dynamicStyleSceneData,
									modifiersSceneData
								))
						{
							continue;
						}
						else
						{
							// Restart the loop
							break;
						}
					}

					const uint8 direction = (uint8)widgetInfo->m_orientation;
					if (widgetInfo->GetRecalculationFlags().AreAllSet(
						RecalculationFlags::HasSpawnedDynamicChildren | (RecalculationFlags::HasBeenGrownByParentX << direction)
					) && widgetInfo->GetRecalculationFlags().AreAnyNotSet(RecalculationFlags::HasGrownChildrenX << direction))
					{
						if (TryResolveWidgetGrowChildrenInPrimaryDirection(state, widgetInfo, pParentInfo, screenProperties, sceneRegistry, direction))
						{
							continue;
						}
					}
					if (widgetInfo->GetRecalculationFlags().AreAllSet(
						RecalculationFlags::HasSpawnedDynamicChildren | (RecalculationFlags::HasBeenGrownByParentX << (!direction))
					) && widgetInfo->GetRecalculationFlags().AreAnyNotSet(RecalculationFlags::HasGrownChildrenX << (!direction)))
					{
						if (TryResolveWidgetGrowChildrenInSecondaryDirection(state, widgetInfo, screenProperties, sceneRegistry, direction))
						{
							continue;
						}
					}

					if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::HasSpawnedDynamicChildren) && widgetInfo->GetRecalculationFlags().AreAnyNotSet(RecalculationFlags::HasPositionedChildrenX << direction))
					{
						if (TryResolveWidgetPositionChildrenInPrimaryDirection(
									state,
									widgetInfo,
									pParentInfo,
									screenProperties,
									sceneRegistry,
									direction
								))
						{
							continue;
						}
					}
					if (widgetInfo->GetRecalculationFlags().AreAllSet(RecalculationFlags::HasSpawnedDynamicChildren)
					 && widgetInfo->GetRecalculationFlags().AreAnyNotSet(RecalculationFlags::HasPositionedChildrenX << (!direction)))
					{
						if (TryResolveWidgetPositionChildrenInSecondaryDirection(
									state,
									widgetInfo,
									pParentInfo,
									screenProperties,
									sceneRegistry,
									direction
								))
						{
							continue;
						}
					}
				}
			}

			// Apply padding
			for (const WidgetInfo& widgetInfo : state.widgetInfos)
			{
				const Style::SizeAxisEdges& __restrict paddingStyle = widgetInfo.m_padding;
				const Math::Vector2i padding{
					paddingStyle.m_left.Get(widgetInfo.m_size.x, screenProperties),
					paddingStyle.m_top.Get(widgetInfo.m_size.y, screenProperties)
				};
				if (padding.GetLengthSquared() > 0)
				{
					for (const uint16 childIndex : widgetInfo.m_childrenIndices)
					{
						WidgetInfo& childWidgetInfo = state.widgetInfos[childIndex];
						switch (childWidgetInfo.m_positionType)
						{
							case PositionType::Static:
							case PositionType::Relative:
							case PositionType::Absolute:
								childWidgetInfo.m_relativePosition += padding;
								break;
							case PositionType::Dynamic:
								break;
						}
					}
				}
			}

			// Adjust for margin
			for (WidgetInfo& widgetInfo : state.widgetInfos)
			{
				const Style::SizeAxisEdges& __restrict marginStyle = widgetInfo.m_margin;
				const Math::RectangleEdgesi margin{
					marginStyle.m_top.Get(widgetInfo.m_size.y, screenProperties),
					marginStyle.m_right.Get(widgetInfo.m_size.x, screenProperties),
					marginStyle.m_bottom.Get(widgetInfo.m_size.y, screenProperties),
					marginStyle.m_left.Get(widgetInfo.m_size.x, screenProperties),
				};

				widgetInfo.m_relativePosition += {margin.m_left, margin.m_top};
				widgetInfo.m_size -= margin.GetSum();
			}
		}

		Assert(!state.remainingWidgets.AreAnySet(), "Finished hierarchy update without resolving all widgets!");
		if constexpr (ENABLE_ASSERTS)
		{
			for ([[maybe_unused]] const uint32 widgetIndex : state.remainingWidgets.GetSetBitsIterator())
			{
				Assert(!state.widgetInfos[(uint16)widgetIndex].GetRecalculationFlags().AreAllSet(RecalculationFlags::IsFinal));
			}
		}

		for (const WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout) && widgetInfo.m_childrenIndices.HasElements() && widgetInfo.m_pWidget != nullptr)
			{
				switch (widgetInfo.m_overflowType)
				{
					case OverflowType::Visible:
					case OverflowType::Hidden:
						break;
					case OverflowType::Scroll:
					case OverflowType::Auto:
					{
						Optional<Data::Layout*> pLayoutComponent;
						if (Optional<Data::FlexLayout* const *> pFlexLayout = widgetInfo.m_pLayout.Get<Data::FlexLayout*>())
						{
							pLayoutComponent = *pFlexLayout;
						}
						else if (Optional<Data::GridLayout* const *> pGridLayout = widgetInfo.m_pLayout.Get<Data::GridLayout*>())
						{
							pLayoutComponent = *pGridLayout;
						}

						const uint8 direction = (uint8)widgetInfo.m_orientation;

						Math::Vector2i maximumEndPosition{Math::Zero};
						for (const uint16 childIndex : widgetInfo.m_childrenIndices)
						{
							const WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
							if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
							{
								maximumEndPosition = Math::Max(maximumEndPosition, childInfo.m_relativePosition + childInfo.m_size);
							}
						}

						const Math::Rectanglei maskedWidgetArea = Widgets::GetMaskedContentArea(widgetInfo, state.widgetInfos.GetView());

						const int32 endPos = maximumEndPosition[direction];
						const float maximumScrollPosition = Math::Max(float(endPos - maskedWidgetArea.GetSize()[direction]), 0.f);
						if (LIKELY(pLayoutComponent.IsValid()))
						{
							pLayoutComponent->SetMaximumScrollPosition(*widgetInfo.m_pWidget, maximumScrollPosition);
						}
						if (maximumScrollPosition > 0)
						{
							widgetInfo.m_pWidget->EnableInput();
						}
					}
					break;
				}
			}
		}

		for (WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.GetRecalculationFlags().IsSet(RecalculationFlags::IsDynamicLayout) & widgetInfo.m_childrenIndices.HasElements())
			{
				Optional<Data::Layout*> pLayoutComponent;
				if (Optional<Data::FlexLayout**> pFlexLayout = widgetInfo.m_pLayout.Get<Data::FlexLayout*>())
				{
					pLayoutComponent = *pFlexLayout;
				}
				else if (Optional<Data::GridLayout**> pGridLayout = widgetInfo.m_pLayout.Get<Data::GridLayout*>())
				{
					pLayoutComponent = *pGridLayout;
				}
				if (pLayoutComponent.IsValid())
				{
					const uint8 direction = (uint8)widgetInfo.m_orientation;

					const int32 scrollOffset = pLayoutComponent->GetScrollPosition();
					if (scrollOffset != 0)
					{
						for (const uint16 childIndex : widgetInfo.m_childrenIndices)
						{
							WidgetInfo& __restrict childInfo = state.widgetInfos[childIndex];
							if (childInfo.GetRecalculationFlags().IsNotSet(RecalculationFlags::IsRemovedFromParentLayout))
							{
								childInfo.m_relativePosition[direction] += scrollOffset;
							}
						}
					}
				}
			}
		}

		for (const uint32 widgetIndex : state.positionedWidgets.GetSetBitsIterator())
		{
			WidgetInfo& widgetInfo = state.widgetInfos[uint16(widgetIndex)];
			if (widgetInfo.m_pWidget.IsValid())
			{
				Widget& widget = *widgetInfo.m_pWidget;

				Entity::Data::LocalTransform2D& __restrict localTransform =
					localTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
				localTransform.SetLocation((Math::Vector2f)widgetInfo.m_relativePosition);
				localTransform.SetScale((Math::Vector2f)widgetInfo.m_size);
			}
		}

		// Transform relative location to relative to root
		for (WidgetInfo& __restrict widgetInfo : state.widgetInfos.GetView().GetSubViewFrom(1))
		{
			widgetInfo.m_relativePosition = widgetInfo.GetParentInfo(state.widgetInfos.GetView())->m_relativePosition +
			                                widgetInfo.m_relativePosition;
		}

		for (WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.m_pWidget.IsValid())
			{
				Widget& widget = *widgetInfo.m_pWidget;
				const Entity::Data::WorldTransform2D& worldTransform =
					worldTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
				const Math::Rectanglei previousContentArea{(Math::Vector2i)worldTransform.GetLocation(), (Math::Vector2i)worldTransform.GetScale()};

				const typename Math::Vector2i::BoolType wasSizeChanged = widgetInfo.m_size != previousContentArea.GetSize();
				const typename Math::Vector2i::BoolType wasPositionChanged = widgetInfo.m_relativePosition != previousContentArea.GetPosition();

				EnumFlags<ContentAreaChangeFlags> contentAreaChangeFlags;
				contentAreaChangeFlags |= ContentAreaChangeFlags::SizeXChanged * wasSizeChanged.IsXSet();
				contentAreaChangeFlags |= ContentAreaChangeFlags::SizeYChanged * wasSizeChanged.IsYSet();
				contentAreaChangeFlags |= ContentAreaChangeFlags::PositionXChanged * wasPositionChanged.IsXSet();
				contentAreaChangeFlags |= ContentAreaChangeFlags::PositionYChanged * wasPositionChanged.IsYSet();
				widgetInfo.m_contentAreaChangeFlags |= contentAreaChangeFlags;
			}
		}

		for (const WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.m_contentAreaChangeFlags.AreAnySet() && widgetInfo.m_pWidget.IsValid())
			{
				widgetInfo.m_pWidget->OnBeforeContentAreaChanged(widgetInfo.m_contentAreaChangeFlags);
			}
		}

		for (const WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.m_pWidget.IsValid())
			{
				Widget& widget = *widgetInfo.m_pWidget;

				Entity::Data::WorldTransform2D& __restrict worldTransform =
					worldTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
				worldTransform.SetLocation((Math::Vector2f)widgetInfo.m_relativePosition);
				worldTransform.SetScale((Math::Vector2f)widgetInfo.m_size);
			}
		}

		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		Entity::RootSceneComponent2D& rootSceneComponent = GetRootScene().GetRootComponent();
		for (const WidgetInfo& __restrict widgetInfo : state.widgetInfos)
		{
			if (widgetInfo.m_pWidget.IsInvalid())
			{
				continue;
			}

			Widget& widget = *widgetInfo.m_pWidget;

			Optional<Data::Layout*> pLayoutComponent;
			if (Optional<Data::FlexLayout* const *> pFlexLayout = widgetInfo.m_pLayout.Get<Data::FlexLayout*>())
			{
				pLayoutComponent = *pFlexLayout;
			}
			else if (Optional<Data::GridLayout* const *> pGridLayout = widgetInfo.m_pLayout.Get<Data::GridLayout*>())
			{
				pLayoutComponent = *pGridLayout;
			}
			if (pLayoutComponent.IsValid())
			{
				if (pLayoutComponent->UpdateVirtualItemVisibility(widget, sceneRegistry))
				{
					for (uint16 childIndex : widgetInfo.m_childrenIndices)
					{
						WidgetInfo& childWidgetInfo = state.widgetInfos[childIndex];
						if (childWidgetInfo.m_pWidget.IsValid())
						{
							Widget& childWidget = *childWidgetInfo.m_pWidget;

							childWidgetInfo.m_style = childWidget.GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
							childWidgetInfo.m_matchingStyleModifiers =
								childWidgetInfo.m_style.GetMatchingModifiers(childWidget.GetActiveModifiers(sceneRegistry, modifiersSceneData));
						}
					}
				}
			}

			if (Widgets::GetMaskedContentArea(widgetInfo, state.widgetInfos.GetView()).HasSize() && widget.IsVisible())
			{
				if (widget.HasDataComponentOfType<Entity::Data::QuadtreeNode>(sceneRegistry))
				{
					if (widgetInfo.m_contentAreaChangeFlags.AreAnySet())
					{
						rootSceneComponent.OnComponentWorldLocationOrBoundsChanged(
							widget,
							widget.GetDepthRatio(),
							Math::Rectanglef{(Math::Vector2f)widgetInfo.m_relativePosition, (Math::Vector2f)widgetInfo.m_size},
							sceneRegistry
						);
					}
				}
				else
				{
					rootSceneComponent.AddComponent(
						widget,
						widget.GetDepthRatio(),
						Math::Rectanglef{(Math::Vector2f)widgetInfo.m_relativePosition, (Math::Vector2f)widgetInfo.m_size}
					);
				}
			}
			else if (widget.HasDataComponentOfType<Entity::Data::QuadtreeNode>(sceneRegistry))
			{
				rootSceneComponent.RemoveComponent(widget);
			}

			if (widgetInfo.m_contentAreaChangeFlags.AreAnySet())
			{
				if (widgetInfo.m_style.Contains(Style::ValueTypeIdentifier::RoundingRadius, widgetInfo.m_matchingStyleModifiers) && widgetInfo.m_pWidget.IsValid())
				{
					widgetInfo.m_pWidget->ApplyDrawablePrimitive(
						widgetInfo.m_size,
						widgetInfo.m_style,
						widgetInfo.m_matchingStyleModifiers,
						screenProperties,
						sceneRegistry,
						textDrawableSceneData,
						imageDrawableSceneData,
						roundedRectangleDrawableSceneData,
						rectangleDrawableSceneData,
						circleDrawableSceneData,
						gridDrawableSceneData,
						lineDrawableSceneData
					);
				}

				if (const Optional<Data::Drawable*> pDrawableComponent = widget.FindDrawable(textDrawableSceneData, imageDrawableSceneData, roundedRectangleDrawableSceneData, rectangleDrawableSceneData, circleDrawableSceneData, gridDrawableSceneData, lineDrawableSceneData))
				{
					pDrawableComponent->OnContentAreaChanged(widget, widgetInfo.m_contentAreaChangeFlags);
				}

				widget.OnContentAreaChangedInternal(sceneRegistry, widgetInfo.m_contentAreaChangeFlags);
			}
		}
	}

	Optional<Data::Drawable*> Widget::FindDrawable(
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	) const
	{
		if (const Optional<Data::TextDrawable*> pTextDrawable = FindDataComponentOfType(textDrawableSceneData))
		{
			return pTextDrawable;
		}
		else if (const Optional<Data::ImageDrawable*> pImageDrawable = FindDataComponentOfType(imageDrawableSceneData))
		{
			return pImageDrawable;
		}
		else if (const Optional<Data::Primitives::CircleDrawable*> pCircleDrawable = FindDataComponentOfType(circleDrawableSceneData))
		{
			return pCircleDrawable;
		}
		else if (const Optional<Data::Primitives::GridDrawable*> pGridDrawable = FindDataComponentOfType(gridDrawableSceneData))
		{
			return pGridDrawable;
		}
		else if (const Optional<Data::Primitives::LineDrawable*> pLineDrawable = FindDataComponentOfType(lineDrawableSceneData))
		{
			return pLineDrawable;
		}
		else if (const Optional<Data::Primitives::RectangleDrawable*> pRectangleDrawable = FindDataComponentOfType(rectangleDrawableSceneData))
		{
			return pRectangleDrawable;
		}
		else if (const Optional<Data::Primitives::RoundedRectangleDrawable*> pRoundedRectangleDrawable = FindDataComponentOfType(roundedRectangleDrawableSceneData))
		{
			return pRoundedRectangleDrawable;
		}
		else
		{
			return Invalid;
		}
	}

	LoadResourcesResult Widget::TryLoadResources(Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		if (const Optional<Data::Drawable*> pDrawableComponent = FindDrawable(textDrawableSceneData, imageDrawableSceneData, roundedRectangleDrawableSceneData, rectangleDrawableSceneData, circleDrawableSceneData, gridDrawableSceneData, lineDrawableSceneData))
		{
			return pDrawableComponent->TryLoadResources(*this);
		}

		return LoadResourcesResult::Status::Valid;
	}

	Math::Rectanglei Widget::GetContentArea(Entity::SceneRegistry& sceneRegistry) const
	{
		const Math::WorldTransform2D transform = GetWorldTransform(sceneRegistry);
		return {(Math::Vector2i)transform.GetLocation(), (Math::Vector2i)transform.GetScale()};
	}

	Math::Rectanglei Widget::GetContentArea() const
	{
		return GetContentArea(GetSceneRegistry());
	}

	Widget::Coordinate Widget::GetPosition(Entity::SceneRegistry& sceneRegistry) const
	{
		return (Coordinate)GetWorldLocation(sceneRegistry);
	}

	Widget::Coordinate Widget::GetPosition() const
	{
		return GetPosition(GetSceneRegistry());
	}

	LocalWidgetCoordinate Widget::GetRelativeLocation(Entity::SceneRegistry& sceneRegistry) const
	{
		return (LocalCoordinate)Component2D::GetRelativeLocation(sceneRegistry);
	}

	LocalWidgetCoordinate Widget::GetRelativeLocation() const
	{
		return GetRelativeLocation(GetSceneRegistry());
	}

	Math::Vector2i Widget::GetSize(Entity::SceneRegistry& sceneRegistry) const
	{
		return (Math::Vector2i)GetWorldScale(sceneRegistry);
	}

	Math::Vector2i Widget::GetSize() const
	{
		return GetSize(GetSceneRegistry());
	}

	Math::Vector2i Widget::GetParentSize(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const
	{
		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			return pParent->GetSize(sceneRegistry);
		}
		else if (pWindow.IsValid())
		{
			return (Math::Vector2i)pWindow->GetClientAreaSize();
		}
		else
		{
			return Math::Zero;
		}
	}

	Math::Vector2i Widget::GetParentSize() const
	{
		return GetParentSize(GetSceneRegistry(), GetOwningWindow());
	}

	Math::Rectanglei
	Widget::GetParentAvailableChildContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const
	{
		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			return pParent->GetAvailableChildContentArea(sceneRegistry, pWindow);
		}
		else if (pWindow.IsValid())
		{
			return (Math::Rectanglei)pWindow->GetLocalClientArea();
		}
		else
		{
			return Math::Zero;
		}
	}

	Math::Rectanglei Widget::GetParentAvailableChildContentArea() const
	{
		return GetParentAvailableChildContentArea(GetSceneRegistry(), GetOwningWindow());
	}

	Math::Rectanglei Widget::GetParentContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const
	{
		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			return pParent->GetContentArea(sceneRegistry);
		}
		else if (pWindow.IsValid())
		{
			return (Math::Rectanglei)pWindow->GetLocalClientArea();
		}
		else
		{
			return Math::Zero;
		}
	}

	Math::Rectanglei Widget::GetParentContentArea() const
	{
		return GetParentContentArea(GetSceneRegistry(), GetOwningWindow());
	}

	Math::Rectanglei
	Widget::GetAvailableChildContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const
	{
		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

		Math::Rectanglei availableArea = GetContentArea(sceneRegistry);

		switch (GetLayoutFromStyle(style, matchingModifiers, *this))
		{
			case LayoutType::None:
				return Math::Zero;
			case LayoutType::Block:
			{
				const Style::Size preferredSize =
					style.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, matchingModifiers);

				if (preferredSize.y.Is<Style::AutoType>())
				{
					availableArea.m_size.y = GetParentAvailableChildContentArea(sceneRegistry, pWindow).GetSize().y;
				}
			}
			break;
			case LayoutType::Flex:
			{
				if (const Optional<Data::FlexLayout*> pLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
				{
					return pLayoutComponent->GetAvailableChildContentArea(*this, sceneRegistry);
				}
			}
			break;
			case LayoutType::Grid:
			{
				if (const Optional<Data::GridLayout*> pLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
				{
					return pLayoutComponent->GetAvailableChildContentArea(*this, sceneRegistry);
				}
			}
			break;
		}

		return availableArea;
	}

	Math::Rectanglei Widget::GetAvailableChildContentArea() const
	{
		return GetAvailableChildContentArea(GetSceneRegistry(), GetOwningWindow());
	}

	void Widget::RotateChildren(const ChildIndex offset)
	{
		BaseType::RotateChildren(offset);
		RecalculateSubsequentDepthInfo();
	}

	void Widget::OnAttachedToNewParent()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow();
		if (m_flags.IsSet(Flags::ShouldAutoApplyStyle))
		{
			ApplyStyle(sceneRegistry, *pWindow);
		}

		if (GetParent().IsVisible())
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsParentHidden);
			if (previousFlags.IsSet(Flags::IsParentHidden) & previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsParentHidden))
			{
				OnBecomeVisibleInternal(sceneRegistry, pWindow);
			}
		}
		else
		{
			const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsParentHidden);
			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
			{
				OnBecomeHiddenInternal(sceneRegistry);
			}
		}
	}

	void Widget::OnChildAttached(HierarchyComponentBase& newChild, const ChildIndex childIndex, const Optional<uint16> preferredChildIndex)
	{
		Widget& precedingWidget = childIndex > 0 ? GetChildren()[childIndex - 1] : *this;
		precedingWidget.RecalculateSubsequentDepthInfo();

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnChildAdded(*this, newChild.AsExpected<Widget>(), childIndex, preferredChildIndex);
		}
	}

	void Widget::RemoveChildInternal(Widget& removedChild, Entity::SceneRegistry& sceneRegistry)
	{
		ChildIndex removedChildIndex;
		{
			const ChildView children = GetChildren();
			removedChildIndex = children.GetIteratorIndex(children.Find(removedChild));
		}

		HierarchyComponent::RemoveChildAndClearParent(removedChild, sceneRegistry);

		Widget* pPreviousWidget;
		{
			const ChildView children = GetChildren();
			pPreviousWidget = removedChildIndex > 0 ? &children[removedChildIndex - 1] : this;
		}
		pPreviousWidget->RecalculateSubsequentDepthInfo();

		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnChildRemoved(*this, removedChild, removedChildIndex);
		}
	}

	Optional<Widget*> Widget::GetFirstChild() const
	{
		if (ChildView children = GetChildren(); children.HasElements())
		{
			return children[0];
		}
		else
		{
			return {};
		}
	}

	Optional<Widget*> Widget::GetLastChild() const
	{
		if (ChildView children = GetChildren(); children.HasElements())
		{
			return children.GetLastElement();
		}
		else
		{
			return {};
		}
	}

	Optional<Widget*> Widget::GetSubsequentSibling() const
	{
		if (Optional<Widget*> pParent = GetParentSafe())
		{
			ChildView parentChildren = pParent->GetChildren();
			if (const auto it = parentChildren.Find(*this))
			{
				const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
				if (parentChildIndex + 1 < parentChildren.GetSize())
				{
					return parentChildren[parentChildIndex + 1];
				}
			}
			return Invalid;
		}
		else
		{
			return Invalid;
		}
	}

	Optional<Widget*> Widget::GetSubsequentSiblingInHierarchy() const
	{
		if (Optional<Widget*> pParent = GetParentSafe())
		{
			ChildView parentChildren = pParent->GetChildren();
			if (const auto it = parentChildren.Find(*this))
			{
				const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
				if (parentChildIndex + 1 < parentChildren.GetSize())
				{
					return parentChildren[parentChildIndex + 1];
				}
			}
			return pParent->GetSubsequentSiblingInHierarchy();
		}
		else
		{
			return Invalid;
		}
	}

	Optional<Widget*> Widget::GetSubsequentWidgetInHierarchy() const
	{
		if (const Optional<Widget*> pFirstChild = GetFirstChild())
		{
			return pFirstChild;
		}
		else
		{
			return GetSubsequentSiblingInHierarchy();
		}
	}

	Optional<Widget*> Widget::GetPreviousSibling() const
	{
		if (Optional<Widget*> pParent = GetParentSafe())
		{
			ChildView parentChildren = pParent->GetChildren();
			if (const auto it = parentChildren.Find(*this))
			{
				const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
				if (parentChildIndex - 1 < parentChildren.GetSize())
				{
					return parentChildren[parentChildIndex - 1];
				}
			}
			return Invalid;
		}
		else
		{
			return Invalid;
		}
	}

	Optional<Widget*> Widget::GetPreviousSiblingInHierarchy() const
	{
		if (Optional<Widget*> pParent = GetParentSafe())
		{
			ChildView parentChildren = pParent->GetChildren();
			if (const auto it = parentChildren.Find(*this))
			{
				const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
				if (parentChildIndex > 0 && parentChildIndex - 1 < parentChildren.GetSize())
				{
					return parentChildren[parentChildIndex - 1];
				}
			}
			return pParent;
		}
		else
		{
			return Invalid;
		}
	}

	Optional<Widget*> Widget::GetPreviousWidgetInHierarchy() const
	{
		if (Optional<Widget*> pParent = GetParentSafe())
		{
			ChildView parentChildren = pParent->GetChildren();
			if (const auto it = parentChildren.Find(*this))
			{
				const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
				if (parentChildIndex > 0 && parentChildIndex - 1 < parentChildren.GetSize())
				{
					Widget& previousSibling = parentChildren[parentChildIndex - 1];
					Optional<Widget*> pPreviousWidget = previousSibling;
					Optional<Widget*> pLastChild = previousSibling.GetLastChild();
					while (pLastChild.IsValid())
					{
						pPreviousWidget = pLastChild;
						pLastChild = pLastChild->GetLastChild();
					}
					return pPreviousWidget;
				}
			}
			return pParent;
		}
		else
		{
			return Invalid;
		}
	}

	bool Widget::CanReceiveInputFocus(Entity::SceneRegistry& sceneRegistry) const
	{
		if (Optional<Data::EventData*> pEventData = FindDataComponentOfType<Data::EventData>(sceneRegistry);
		    pEventData.IsValid() && pEventData->HasEvents(EventType::Tap))
		{
			return true;
		}
		else if (HasAnyDataComponentsImplementingType<Data::Input>(sceneRegistry))
		{
			return true;
		}
		return false;
	}

	void Widget::RecalculateSubsequentDepthInfo()
	{
		// Assert(!HasParent() || m_depthInfo.GetComputedDepthIndex() > GetParent().m_depthInfo.GetComputedDepthIndex());

		if (const Optional<Widget*> pFirstChild = GetFirstChild())
		{
			const DepthInfo depthInfo = m_depthInfo;

			const EnumFlags<DepthInfo::ChangeFlags> childDepthInfoChangeFlags =
				pFirstChild->m_depthInfo.OnParentOrPrecedingWidgetChanged(depthInfo, depthInfo);
			if (childDepthInfoChangeFlags.AreAnySet(DepthInfo::ChangeFlags::ShouldRecalculateSubsequentDepthInfo))
			{
				MUST_TAIL return pFirstChild->RecalculateSubsequentDepthInfo();
			}
		}
		else if (Optional<Widget*> pParent = GetParentSafe())
		{
			const static auto getSubsequentSibling = [](Widget& parent, Widget& widget) -> Optional<Widget*>
			{
				ChildView parentChildren = parent.GetChildren();
				if (const auto it = parentChildren.Find(widget))
				{
					const ChildIndex parentChildIndex = parentChildren.GetIteratorIndex(it);
					if (parentChildIndex + 1 < parentChildren.GetSize())
					{
						return parentChildren[parentChildIndex + 1];
					}
				}
				return Invalid;
			};

			ReferenceWrapper<Widget> widget = *this;
			do
			{
				if (const Optional<Widget*> pSubsequentSibling = getSubsequentSibling(*pParent, widget))
				{
					const EnumFlags<DepthInfo::ChangeFlags> parentDepthInfoChangeFlags =
						pSubsequentSibling->m_depthInfo
							.OnParentOrPrecedingWidgetChanged(pParent->m_depthInfo, widget->GetLastNestedChildOrOwnDepthInfo());
					if (parentDepthInfoChangeFlags.AreAnySet(DepthInfo::ChangeFlags::ShouldRecalculateSubsequentDepthInfo))
					{
						MUST_TAIL return pSubsequentSibling->RecalculateSubsequentDepthInfo();
					}
				}

				widget = *pParent;
				pParent = pParent->GetParentSafe();

			} while (pParent.IsValid());
		}
	}

	[[nodiscard]] DepthInfo Widget::GetLastNestedChildOrOwnDepthInfo() const
	{
		Optional<const Widget*> pWidget = this;
		DepthInfo depthInfo = m_depthInfo;
		do
		{
			const Widget::ChildView children = pWidget->GetChildren();
			if (children.HasElements())
			{
				pWidget = &children.GetLastElement();
				depthInfo = pWidget->m_depthInfo;
			}
			else
			{
				pWidget = nullptr;
			}
		} while (pWidget != nullptr);
		return depthInfo;
	}

	Threading::JobBatch Widget::DeserializeDataComponentsAndChildren(Serialization::Reader reader)
	{
		if (reader.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UndoHistory))
		{
			return BaseType::DeserializeDataComponentsAndChildren(reader);
		}

		/*if (const Optional<Asset::Guid> childWidgetAssetGuid = reader.Read<Asset::Guid>("asset"))
		{
		  Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		  Entity::Manager& entityManager = System::Get<Entity::Manager>();
		  Entity::ComponentTemplateCache& sceneTemplateCache = entityManager.GetComponentTemplateCache();
		  const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier =
		    sceneTemplateCache.FindOrRegister(*childWidgetAssetGuid);
		  Assert(sceneTemplateIdentifier.IsValid());

		  [[maybe_unused]] const Optional<Entity::Data::ExternalScene*> pExternalScene =
		    CreateDataComponent<Entity::Data::ExternalScene>(sceneRegistry, sceneTemplateIdentifier);
		  Assert(pExternalScene.IsValid());

		  Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};

		  Threading::DynamicIntermediateStage& finishedStage = Threading::CreateIntermediateStage();
		  finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

		  Manager& widgetManager = *System::FindPlugin<Manager>();
		  WidgetCache& widgetCache = widgetManager.GetWidgetCache();

		  Entity::ComponentSoftReference softReference{*this, sceneRegistry};

		  const TemplateIdentifier childWidgetTemplateIdentifier = widgetCache.FindOrRegister(*childWidgetAssetGuid);

		  Serialization::Document copiedDocument;
		  copiedDocument.CopyFrom(reader.GetValue().GetValue(), copiedDocument.GetAllocator());

		  Threading::JobBatch cachedWidgetLoadJobBatch = widgetManager.GetWidgetCache().TryLoad(
		    childWidgetTemplateIdentifier,
		    WidgetCache::LoadListenerData{
		      &widgetCache,
		      [&widgetTemplate = widgetCache.GetAssetData(childWidgetTemplateIdentifier), copiedDocument = Move(copiedDocument), &finishedStage,
		&sceneRegistry, softReference](WidgetCache&, const TemplateIdentifier) mutable
		      {
		        if (const Optional<Widget*> pWidget = softReference.Find<Widget>(sceneRegistry))
		        {
		          Serialization::Data topLevelData(copiedDocument);

		          Entity::Data::ExternalScene& externalScene = *pWidget->FindDataComponentOfType<Entity::Data::ExternalScene>(sceneRegistry);
		          externalScene.OnMasterSceneLoaded(*pWidget, *widgetTemplate.m_pWidget, Move(topLevelData), finishedStage);
		        }
		        return EventCallbackResult::Remove;
		      }
		    }
		  );
		  if (cachedWidgetLoadJobBatch.IsValid())
		  {
		    jobBatch.QueueAfterStartStage(cachedWidgetLoadJobBatch);
		  }

		  return jobBatch;
		}*/

		Threading::JobBatch jobBatch;

		Entity::Manager& entityManager = System::Get<Entity::Manager>();
		Entity::ComponentRegistry& componentRegistry = entityManager.GetRegistry();
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		if (const Optional<Serialization::Reader> dataComponentsReader = reader.FindSerializer("data_components"))
		{
			Threading::JobBatch dataComponentsBatch;

			for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
			{
				const Optional<Guid> typeGuid = dataComponentReader.Read<Guid>("typeGuid");
				if (typeGuid.IsInvalid())
				{
					continue;
				}

				const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(typeGuid.Get());
				if (typeIdentifier.IsInvalid())
				{
					continue;
				}

				const Optional<Entity::ComponentTypeInterface*> pComponentTypeInfo = componentRegistry.Get(typeIdentifier);
				if (LIKELY(pComponentTypeInfo.IsValid()))
				{
					Threading::JobBatch dataComponentBatch;
					Widgets::Data::Component::Deserializer deserializer{
						Reflection::TypeDeserializer{dataComponentReader, reflectionRegistry, dataComponentBatch},
						sceneRegistry,
						*this
					};
					Optional<Component*> pComponent =
						pComponentTypeInfo->DeserializeInstanceWithoutChildrenManualOnCreated(deserializer, sceneRegistry);
					if (pComponent.IsValid())
					{
						pComponentTypeInfo->OnComponentDeserialized(*pComponent, dataComponentReader, dataComponentBatch);
					}
					if (pComponent.IsValid())
					{
						pComponentTypeInfo->OnComponentCreated(*pComponent, this, sceneRegistry);
					}

					if (dataComponentBatch.IsValid())
					{
						dataComponentsBatch.QueueAfterStartStage(dataComponentBatch);
					}
				}
				else
				{
					continue;
				}
			}
			jobBatch.QueueAsNewFinishedStage(dataComponentsBatch);
		}

		{
			Threading::JobBatch childrenJobBatch;
			if (const Optional<Serialization::Reader> childrenReader = reader.FindSerializer("children"))
			{
				uint16 childIndex = 0;
				for (const Serialization::Reader childReader : childrenReader->GetArrayView())
				{
					Threading::JobBatch childJobBatch = Deserialize(
						childReader,
						*this,
						sceneRegistry,
						[](const Optional<Widget*>)
						{
						},
						childIndex
					);
					childrenJobBatch.QueueAsNewFinishedStage(childJobBatch);
					childIndex++;
				}
			}
			jobBatch.QueueAsNewFinishedStage(childrenJobBatch);
		}

		return jobBatch;
	}

	bool Widget::SerializeDataComponentsAndChildren(Serialization::Writer writer) const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (!IsScene(sceneRegistry) || writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UndoHistory))
		{
			if (m_flags.IsSet(Flags::IsAssetRootWidget))
			{
				Entity::Data::ExternalScene& externalScene = *FindDataComponentOfType<Entity::Data::ExternalScene>(sceneRegistry);

				const Asset::Guid externalWidgetAssetGuid = externalScene.GetAssetGuid();
				writer.Serialize("guid", externalWidgetAssetGuid);
			}

			return BaseType::SerializeDataComponentsAndChildren(writer);
		}

		Entity::Data::ExternalScene& externalScene = *FindDataComponentOfType<Entity::Data::ExternalScene>(sceneRegistry);

		const Asset::Guid externalWidgetAssetGuid = externalScene.GetAssetGuid();
		if (m_flags.IsSet(Flags::IsAssetRootWidget))
		{
			writer.Serialize("guid", externalWidgetAssetGuid);
		}

		Manager& widgetManager = *System::FindPlugin<Manager>();
		WidgetCache& widgetCache = widgetManager.GetWidgetCache();

		const TemplateIdentifier widgetTemplateIdentifier = widgetCache.FindOrRegister(externalWidgetAssetGuid);

		const Optional<const Widget*> pTemplateComponent = widgetCache.GetAssetData(widgetTemplateIdentifier).m_pWidget;
		if (UNLIKELY(!pTemplateComponent.IsValid() || pTemplateComponent == this))
		{
			return BaseType::SerializeDataComponentsAndChildren(writer);
		}

		const Widget& templateComponent = *pTemplateComponent;

		[[maybe_unused]] const bool wasSerialized = externalScene.SerializeTemplateComponents(writer, *this, templateComponent);
		Assert(wasSerialized);

		writer.Serialize("asset", externalWidgetAssetGuid);

		return true;
	}

	/* static */ Threading::JobBatch Widget::Deserialize(
		const Serialization::Reader reader,
		Widget& parent,
		Entity::SceneRegistry& sceneRegistry,
		DeserializationCallback&& callback,
		const Optional<uint16> parentChildIndex,
		const EnumFlags<Flags> flags
	)
	{
		if (const Optional<Asset::Guid> childWidgetAssetGuid = reader.Read<Asset::Guid>("asset"))
		{
			Entity::ComponentSoftReference parentReference{parent, sceneRegistry};

			Manager& widgetManager = *System::FindPlugin<Manager>();
			WidgetCache& widgetCache = widgetManager.GetWidgetCache();
			const TemplateIdentifier childWidgetTemplateIdentifier = widgetCache.FindOrRegister(*childWidgetAssetGuid);

			Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
			Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
			intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

			Threading::JobBatch cachedWidgetLoadJobBatch = widgetManager.GetWidgetCache().TryLoad(
				childWidgetTemplateIdentifier,
				WidgetCache::LoadListenerData{
					&widgetCache,
					[&widgetCache,
			     childWidgetTemplateIdentifier,
			     callback = Move(callback),
			     topLevelData = Serialization::Data(reader.GetValue()),
			     parentReference,
			     parentChildIndex,
			     &sceneRegistry,
			     &intermediateStage,
			     flags](WidgetCache&, const TemplateIdentifier) mutable
					{
						const Widgets::Template& widgetTemplate = widgetCache.GetAssetData(childWidgetTemplateIdentifier);

						const ConstByteView data{widgetTemplate.m_data.GetView()};
						Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();

						Serialization::MergedReader mergedReader;
						const Serialization::Reader topLevelReader(topLevelData);

						Serialization::RootReader assetReader = Serialization::GetReaderFromBuffer(
							ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
						);
						Assert(assetReader.GetData().IsValid());
						if (LIKELY(assetReader.GetData().IsValid()))
						{
							// Don't inherit instance guids from the template data
							{
								using RemoveInstanceGuids = void (*)(Serialization::Object& object);
								static RemoveInstanceGuids removeInstanceGuids = [](Serialization::Object& object)
								{
									object.RemoveMember("instanceGuid");
									if (const Optional<Serialization::TValue*> children = object.FindMember("children"))
									{
										for (Serialization::TValue& child : *children->AsArray())
										{
											removeInstanceGuids(*child.AsObject());
										}
									}
								};
								removeInstanceGuids(Serialization::Object::GetFromReference((Serialization::TValue&)assetReader.GetData().GetDocument()));
							}

							const Optional<ConstStringView> localStyle = topLevelReader.Read<ConstStringView>("style");
							const Optional<ConstStringView> assetStyle = Serialization::Reader(assetReader).Read<ConstStringView>("style");

							// Check if we need to merge styles
							String combinedStyle;
							if (localStyle.IsValid() && assetStyle.IsValid())
							{
								combinedStyle = String::Merge(*assetStyle, "; ", *localStyle);
							}

							mergedReader = Serialization::MergeSerializers(topLevelReader, Move(assetReader));

							// Apply the merged style
							if (combinedStyle.HasElements())
							{
								Serialization::Writer writer(mergedReader.GetRootReader()->GetData());
								writer.Serialize("style", combinedStyle);
							}
						}
						else
						{
							mergedReader = topLevelReader;
						}

						const Serialization::Reader reader = *mergedReader.GetReader();

						if (const Optional<Widget*> pParent = parentReference.Find<Widget>(sceneRegistry))
						{
							Widgets::Widget* pWidget;

							const Guid widgetTypeGuid = reader.ReadWithDefaultValue<Guid>("typeGuid", Guid(Reflection::GetTypeGuid<Widget>()));
							Entity::Manager& entityManager = System::Get<Entity::Manager>();
							Entity::ComponentRegistry& componentRegistry = entityManager.GetRegistry();
							const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(widgetTypeGuid);
							Assert(typeIdentifier.IsValid());
							if (UNLIKELY_ERROR(typeIdentifier.IsInvalid()))
							{
								LogError("Failed to instantiate widget with invalid type guid!");
								callback(Invalid);
								intermediateStage.SignalExecutionFinishedAndDestroying(thread);
								return EventCallbackResult::Remove;
							}

							Optional<Entity::ComponentTypeInterface*> pComponentType = componentRegistry.Get(typeIdentifier);

							Threading::JobBatch jobBatch;
							{

								const Optional<Entity::ComponentTypeSceneDataInterface*> pTypeSceneData =
									sceneRegistry.GetOrCreateComponentTypeData(typeIdentifier);
								if (UNLIKELY(pTypeSceneData.IsInvalid()))
								{
									LogError("Failed to instantiate widget with type guid {} - Componentt type could not be found!", widgetTypeGuid);
									callback(Invalid);
									intermediateStage.SignalExecutionFinishedAndDestroying(thread);
									return EventCallbackResult::Remove;
								}

								const Optional<Entity::Component*> pComponent = pComponentType->DeserializeInstanceWithChildren(
									Widget::Deserializer{
										Reflection::TypeDeserializer{reader, System::Get<Reflection::Registry>(), jobBatch},
										*pParent,
										sceneRegistry,
										flags,
										parentChildIndex
									},
									sceneRegistry
								);
								if (UNLIKELY(pComponent.IsInvalid()))
								{
									LogError("Failed to instantiate widget with type guid {} - deserialization failed!", widgetTypeGuid);
									callback(Invalid);
									intermediateStage.SignalExecutionFinishedAndDestroying(thread);
									return EventCallbackResult::Remove;
								}

								pWidget = &static_cast<Widgets::Widget&>(*pComponent);
							}

							if (callback.IsValid())
							{
								jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
									[callback = Forward<DeserializationCallback>(callback), pWidget](Threading::JobRunnerThread&)
									{
										callback(pWidget);
									},
									Threading::JobPriority::UserInterfaceLoading
								));
							}
							if (jobBatch.IsValid())
							{
								jobBatch.QueueAsNewFinishedStage(intermediateStage);
								Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
							}
							else
							{
								intermediateStage.SignalExecutionFinishedAndDestroying(thread);
							}
						}
						else
						{
							callback(Invalid);
							intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						}
						return EventCallbackResult::Remove;
					}
				}
			);
			if (cachedWidgetLoadJobBatch.IsValid())
			{
				jobBatch.QueueAfterStartStage(cachedWidgetLoadJobBatch);
			}

			return jobBatch;
		}

		Widgets::Widget* pWidget;

		const Guid widgetTypeGuid = reader.ReadWithDefaultValue<Guid>("typeGuid", Guid(Reflection::GetTypeGuid<Widget>()));
		Entity::Manager& entityManager = System::Get<Entity::Manager>();
		Entity::ComponentRegistry& componentRegistry = entityManager.GetRegistry();
		const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(widgetTypeGuid);
		Assert(typeIdentifier.IsValid());
		if (UNLIKELY_ERROR(typeIdentifier.IsInvalid()))
		{
			LogError("Failed to instantiate widget with invalid type guid!");
			callback(Invalid);
			return Threading::JobBatch{};
		}

		Optional<Entity::ComponentTypeInterface*> pComponentType = componentRegistry.Get(typeIdentifier);

		Threading::JobBatch jobBatch;
		{

			const Optional<Entity::ComponentTypeSceneDataInterface*> pTypeSceneData = sceneRegistry.GetOrCreateComponentTypeData(typeIdentifier);
			if (UNLIKELY(pTypeSceneData.IsInvalid()))
			{
				LogError("Failed to instantiate widget with type guid {} - Componentt type could not be found!", widgetTypeGuid);
				callback(Invalid);
				return Threading::JobBatch{};
			}

			const Optional<Entity::Component*> pComponent = pComponentType->DeserializeInstanceWithoutChildrenManualOnCreated(
				Widget::Deserializer{
					Reflection::TypeDeserializer{reader, System::Get<Reflection::Registry>(), jobBatch},
					parent,
					sceneRegistry,
					flags,
					parentChildIndex
				},
				sceneRegistry
			);
			if (UNLIKELY(pComponent.IsInvalid()))
			{
				LogError("Failed to instantiate widget with type guid {} - deserialization failed!", widgetTypeGuid);
				callback(Invalid);
				Assert(jobBatch.IsInvalid());
				return Threading::JobBatch{};
			}

			pWidget = &static_cast<Widgets::Widget&>(*pComponent);

			pComponentType->OnComponentDeserialized(*pWidget, reader, jobBatch);

			Threading::JobBatch childJobBatch = pWidget->DeserializeDataComponentsAndChildren(reader);
			if (childJobBatch.IsValid())
			{
				jobBatch.QueueAsNewFinishedStage(childJobBatch);
			}

			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[pComponentType, pWidget, &parent, &sceneRegistry](Threading::JobRunnerThread&)
				{
					pComponentType->OnComponentCreated(*pWidget, parent, sceneRegistry);
				},
				Threading::JobPriority::UserInterfaceLoading
			));
		}

		if (callback.IsValid())
		{
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[callback = Forward<DeserializationCallback>(callback), pWidget](Threading::JobRunnerThread&)
				{
					callback(pWidget);
				},
				Threading::JobPriority::UserInterfaceLoading
			));
		}
		return jobBatch;
	}

	/* static */ Threading::JobBatch Widget::Deserialize(
		const Asset::Guid assetGuid,
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Widget*> pParent,
		DeserializationCallback&& callback,
		const Optional<uint16> parentChildIndex,
		const EnumFlags<Flags> flags
	)
	{
		Entity::ComponentSoftReference parentReference = pParent.IsValid() ? Entity::ComponentSoftReference{*pParent, sceneRegistry}
		                                                                   : Entity::ComponentSoftReference{};

		Manager& widgetManager = *System::FindPlugin<Manager>();
		WidgetCache& widgetCache = widgetManager.GetWidgetCache();
		const TemplateIdentifier widgetTemplateIdentifier = widgetCache.FindOrRegister(assetGuid);

		Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
		Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
		intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

		Threading::JobBatch cachedWidgetLoadJobBatch = widgetManager.GetWidgetCache().TryLoad(
			widgetTemplateIdentifier,
			WidgetCache::LoadListenerData{
				&widgetCache,
				[&widgetCache,
		     widgetTemplateIdentifier,
		     parentReference,
		     &sceneRegistry,
		     callback = Forward<decltype(callback)>(callback),
		     parentChildIndex,
		     &intermediateStage,
		     flags](WidgetCache&, const TemplateIdentifier) mutable
				{
					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();

					const Widgets::Template& widgetTemplate = widgetCache.GetAssetData(widgetTemplateIdentifier);

					if (UNLIKELY(widgetTemplate.m_pWidget.IsInvalid()))
					{
						callback(Invalid);
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						return EventCallbackResult::Remove;
					}

					if (const Optional<Widget*> pParent = parentReference.Find<Widget>(sceneRegistry))
					{
						Entity::ComponentTypeInterface& componentTypeInterface = *widgetTemplate.m_pWidget->GetTypeInfo();

						Threading::JobBatch jobBatch;
						Entity::Manager& entityManager = System::Get<Entity::Manager>();
						Optional<Entity::Component*> pComponent = componentTypeInterface.CloneFromTemplateWithChildrenManualOnCreated(
							Guid::Generate(),
							*widgetTemplate.m_pWidget,
							widgetTemplate.m_pWidget->GetParent(),
							*pParent,
							entityManager.GetRegistry(),
							sceneRegistry,
							entityManager.GetComponentTemplateCache().GetTemplateSceneRegistry(),
							jobBatch,
							parentChildIndex
						);

						const Optional<Widget*> pWidget = static_cast<Widget*>(pComponent.Get());
						if (pWidget.IsValid())
						{
							pWidget->m_flags |= flags;
							componentTypeInterface.OnComponentCreated(*pWidget, *pParent, sceneRegistry);
						}

						if (jobBatch.IsValid())
						{
							jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
								[callback = Move(callback), pWidget](Threading::JobRunnerThread&)
								{
									callback(pWidget);
								},
								Threading::JobPriority::UserInterfaceLoading
							));
							jobBatch.QueueAsNewFinishedStage(intermediateStage);
							thread.Queue(jobBatch);
						}
						else
						{
							callback(pWidget);
							intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						}
					}
					else
					{
						callback(Invalid);
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
					}
					return EventCallbackResult::Remove;
				}
			}
		);
		if (cachedWidgetLoadJobBatch.IsValid())
		{
			jobBatch.QueueAfterStartStage(cachedWidgetLoadJobBatch);
		}

		return jobBatch;
	}

	Threading::JobBatch Widget::ReadProperties(const Serialization::Reader reader, Entity::SceneRegistry& sceneRegistry)
	{
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		const Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		Threading::JobBatch jobBatch;

		if (const Optional<Asset::Guid> stylesheetAssetGuid = reader.Read<Asset::Guid>("stylesheet"))
		{
			const StylesheetIdentifier stylesheetIdentifier = stylesheetCache.FindOrRegister(*stylesheetAssetGuid);
			if (const Optional<Data::Stylesheet*> pStylesheetData = FindDataComponentOfType<Data::Stylesheet>(sceneRegistry))
			{
				const Optional<Threading::Job*> pStylesheetLoadJob = pStylesheetData->AddStylesheet(*this, stylesheetIdentifier);
				if (pStylesheetLoadJob.IsValid())
				{
					jobBatch.QueueAfterStartStage(*pStylesheetLoadJob);
				}
			}
			else
			{
				CreateDataComponent<Data::Stylesheet>(
					sceneRegistry,
					Data::Stylesheet::Initializer{Data::Component::Initializer{jobBatch, *this, sceneRegistry}, stylesheetIdentifier}
				);
			}
		}
		else if (const Optional<Serialization::Reader> stylesheetsReader = reader.FindSerializer("stylesheets"))
		{
			Data::Stylesheet::StylesheetIdentifiers stylesheetIdentifiers{Memory::Reserve, (uint32)stylesheetsReader->GetArraySize()};
			for (const Optional<Asset::Guid> assetGuid : stylesheetsReader->GetArrayView<Asset::Guid>())
			{
				stylesheetIdentifiers.EmplaceBack(stylesheetCache.FindOrRegister(*assetGuid));
			}

			if (const Optional<Data::Stylesheet*> pStylesheetData = FindDataComponentOfType<Data::Stylesheet>(sceneRegistry))
			{
				for (const StylesheetIdentifier stylesheetIdentifier : stylesheetIdentifiers)
				{
					const Optional<Threading::Job*> pStylesheetLoadJob = pStylesheetData->AddStylesheet(*this, stylesheetIdentifier);
					if (pStylesheetLoadJob.IsValid())
					{
						jobBatch.QueueAfterStartStage(*pStylesheetLoadJob);
					}
				}
			}
			else
			{
				CreateDataComponent<Data::Stylesheet>(
					sceneRegistry,
					Data::Stylesheet::Initializer{Data::Component::Initializer{jobBatch, *this, sceneRegistry}, Move(stylesheetIdentifiers)}
				);
			}
		}

		if (const Optional<Serialization::Reader> propertySourceReader = reader.FindSerializer("property_source"))
		{
			if (const Optional<Data::PropertySource*> pPropertySourceData = FindDataComponentOfType<Data::PropertySource>(sceneRegistry))
			{
				pPropertySourceData->DeserializeCustomData(*propertySourceReader, *this);
			}
			else
			{
				Threading::JobBatch propertySourceJobBatch;
				DataComponentOwner::CreateDataComponent<Data::PropertySource>(
					sceneRegistry,
					Data::PropertySource::Deserializer{*propertySourceReader, reflectionRegistry, propertySourceJobBatch, *this, sceneRegistry}
				);
				Assert(propertySourceJobBatch.IsInvalid());
			}
		}

		return jobBatch;
	}

	bool Widget::Hide(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsHidden);
		if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
		{
			OnBecomeHiddenInternal(sceneRegistry);
			return true;
		}

		return false;
	}

	bool Widget::Hide()
	{
		return Hide(GetSceneRegistry());
	}

	bool Widget::Ignore(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsIgnored);
		if (previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource))
		{
			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
			{
				OnBecomeHiddenInternal(sceneRegistry);
			}

			OnIgnoredInternal(sceneRegistry, GetOwningWindow());
			return true;
		}

		return false;
	}

	bool Widget::Unignore(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsIgnored);
		if (previousFlags.IsSet(Flags::IsIgnored) & previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource & ~Flags::IsIgnored))
		{
			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsIgnored))
			{
				OnBecomeVisibleInternal(sceneRegistry, GetOwningWindow());
			}

			OnUnignoredInternal(sceneRegistry, GetOwningWindow());
			return true;
		}

		return false;
	}

	void Widget::ToggleIgnore(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchXor(Flags::IsIgnored);
		const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow();
		if (previousFlags.IsSet(Flags::IsIgnored))
		{
			if (previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource & ~Flags::IsIgnored))
			{
				OnUnignoredInternal(sceneRegistry, pWindow);
			}

			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsIgnored))
			{
				OnBecomeVisibleInternal(sceneRegistry, pWindow);
			}
		}
		else if (previousFlags.AreNoneSet(Flags::IsIgnoredFromAnySource))
		{
			OnIgnoredInternal(sceneRegistry, pWindow);

			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
			{
				OnBecomeHiddenInternal(sceneRegistry);
			}
		}
	}

	void Widget::OnIgnoredInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow)
	{
		if (pWindow.IsValid())
		{
			ApplyStyle(sceneRegistry, *pWindow);
		}
	}

	void Widget::OnUnignoredInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow)
	{
		if (pWindow.IsValid())
		{
			ApplyStyle(sceneRegistry, *pWindow);
		}
	}

	void Widget::OnPassedValidation()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::FailedValidation);
		if (previousFlags.IsSet(Flags::FailedValidation) & previousFlags.AreNoneSet(Flags::FailedValidationFromAnySource & ~Flags::FailedValidation))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Valid | Style::Modifier::Invalid);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentPassedValidation();
			}
		}
	}
	void Widget::OnFailedValidation()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::FailedValidation);
		if (previousFlags.AreNoneSet(Flags::FailedValidationFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Valid | Style::Modifier::Invalid);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentFailedValidation();
			}
		}
	}

	void Widget::SetHasRequirements()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::HasRequirements);
		if (previousFlags.AreNoneSet(Flags::HasRequirementsFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Required | Style::Modifier::Optional);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentReceivedRequirements();
			}
		}
	}
	void Widget::ClearHasRequirements()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::HasRequirements);
		if (previousFlags.IsSet(Flags::HasRequirements) & previousFlags.AreNoneSet(Flags::HasRequirementsFromAnySource & ~Flags::HasRequirements))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Required | Style::Modifier::Optional);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentLostRequirements();
			}
		}
	}

	void Widget::OnParentSetToggledOff()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsToggledOffFromParent);
		Assert(!previousFlags.IsSet(Flags::IsToggledOffFromParent));
		if (previousFlags.AreNoneSet(Flags::IsToggledOffFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::ToggledOff);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentSetToggledOff();
			}
		}
	}
	void Widget::OnParentClearedToggledOff()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsToggledOffFromParent);
		Assert(previousFlags.IsSet(Flags::IsToggledOffFromParent));
		if (previousFlags.IsSet(Flags::IsToggledOffFromParent) & previousFlags.AreNoneSet(Flags::IsToggledOffFromAnySource & ~Flags::IsToggledOffFromParent))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::ToggledOff);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentClearedToggledOff();
			}
		}
	}

	void Widget::OnParentReceivedRequirements()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::HasRequirementsFromParent);
		Assert(!previousFlags.IsSet(Flags::HasRequirementsFromParent));
		if (previousFlags.AreNoneSet(Flags::HasRequirementsFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Required | Style::Modifier::Optional);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentReceivedRequirements();
			}
		}
	}
	void Widget::OnParentLostRequirements()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::HasRequirementsFromParent);
		Assert(previousFlags.IsSet(Flags::HasRequirementsFromParent));
		if (previousFlags.IsSet(Flags::HasRequirementsFromParent) & previousFlags.AreNoneSet(Flags::HasRequirementsFromAnySource & ~Flags::HasRequirementsFromParent))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Required | Style::Modifier::Optional);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentLostRequirements();
			}
		}
	}

	void Widget::OnParentPassedValidation()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::FailedValidationFromParent);
		Assert(previousFlags.IsSet(Flags::FailedValidationFromParent));
		if (previousFlags.IsSet(Flags::FailedValidationFromParent) & previousFlags.AreNoneSet(Flags::FailedValidationFromAnySource & ~Flags::FailedValidationFromParent))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Valid | Style::Modifier::Invalid);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentPassedValidation();
			}
		}
	}
	void Widget::OnParentFailedValidation()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::FailedValidationFromParent);
		Assert(!previousFlags.IsSet(Flags::FailedValidationFromParent));
		if (previousFlags.AreNoneSet(Flags::FailedValidationFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::Valid | Style::Modifier::Invalid);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentFailedValidation();
			}
		}
	}

	void Widget::SetToggledOff()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsToggledOff);
		if (previousFlags.AreNoneSet(Flags::IsToggledOffFromAnySource))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::ToggledOff);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentSetToggledOff();
			}
		}
	}
	void Widget::ClearToggledOff()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsToggledOff);
		if (previousFlags.IsSet(Flags::IsToggledOff) & previousFlags.AreNoneSet(Flags::IsToggledOffFromAnySource & ~Flags::IsToggledOff))
		{
			Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::ToggledOff);

			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentClearedToggledOff();
			}
		}
	}
	void Widget::ToggleOff()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchXor(Flags::IsToggledOff);
		Data::Modifiers::OnModifiersChangedInternal(*this, Style::Modifier::ToggledOff);

		if (previousFlags.IsSet(Flags::IsToggledOff))
		{
			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentClearedToggledOff();
			}
		}
		else
		{
			for (Widget& childWidget : GetChildren())
			{
				childWidget.OnParentSetToggledOff();
			}
		}
	}

	bool Widget::MakeVisible(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsHidden);
		if (previousFlags.IsSet(Flags::IsHidden) & previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsHidden))
		{
			OnBecomeVisibleInternal(sceneRegistry, GetOwningWindow());
			return true;
		}

		return false;
	}

	bool Widget::MakeVisible()
	{
		return MakeVisible(GetSceneRegistry());
	}

	void Widget::ToggleVisibility(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchXor(Flags::IsHidden);
		if (previousFlags.IsSet(Flags::IsHidden))
		{
			if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsHidden))
			{
				OnBecomeVisibleInternal(sceneRegistry, GetOwningWindow());
			}
		}
		else if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsHidden))
		{
			OnBecomeHiddenInternal(sceneRegistry);
		}
	}

	void Widget::ToggleVisibility()
	{
		ToggleVisibility(GetSceneRegistry());
	}

	void Widget::OnParentBecomeVisible(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsParentHidden);
		if (previousFlags.IsSet(Flags::IsParentHidden) & previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource & ~Flags::IsParentHidden))
		{
			OnBecomeVisibleInternal(sceneRegistry, GetOwningWindow());
		}
	}

	void Widget::OnParentBecomeHidden(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsParentHidden);
		if (previousFlags.AreNoneSet(Flags::IsHiddenFromAnySource))
		{
			OnBecomeHiddenInternal(sceneRegistry);
		}
	}

	void Widget::OnConstructed()
	{
		BaseType::OnConstructed();

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();

			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
				*sceneRegistry.FindComponentTypeData<Data::ExternalStyle>();
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>(
			);
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

			ApplyInitialStyle(
				sceneRegistry,
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::ImageDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::CircleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::GridDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::LineDrawable>();

			ApplyStyleInternal(
				sceneRegistry,
				*pWindow,
				screenProperties,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}
	}

	void Widget::OnCreated()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		if (!GetRootScene().IsTemplate())
		{
			const bool hasDynamicProperties =
				[&dynamicStyleSceneData, &modifiersSceneData](const Widget& widget, Entity::SceneRegistry& sceneRegistry)
			{
				const Optional<Data::DynamicStyle*> pDynamicStyle = widget.FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData);
				const Optional<Data::Modifiers*> pModifiers = widget.FindDataComponentOfType<Data::Modifiers>(modifiersSceneData);
				if ((pModifiers.IsValid() && pModifiers->HasDynamicProperties()) || pDynamicStyle.IsValid() || widget.m_windowTitlePropertyIdentifier.IsValid() || widget.m_windowUriPropertyIdentifier.IsValid())
				{
					return true;
				}

				if (const Optional<Data::DataSource*> pDataSourceData = widget.FindDataComponentOfType<Data::DataSource>(sceneRegistry))
				{
					if (pDataSourceData->GetDynamicTagQuery().IsValid())
					{
						return true;
					}
				}

				if (widget.HasDataComponentOfType<Data::PropertySource>(sceneRegistry))
				{
					return true;
				}

				if (Optional<Data::EventData*> pEventData = widget.FindDataComponentOfType<Data::EventData>(sceneRegistry);
				    pEventData.IsValid() && pEventData->HasDynamicEvents())
				{
					return true;
				}

				return false;
			}(*this, sceneRegistry);
			if (hasDynamicProperties)
			{
				UpdateFromDataSource(sceneRegistry);
			}
		}

		if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();

			ApplyInitialStyle(
				sceneRegistry,
				screenProperties,
				*pWindow,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData
			);

			// Make sure we automatically apply style again in the future
			m_flags |= Flags::ShouldAutoApplyStyle;

			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::TextDrawable>();
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::ImageDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::RectangleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::CircleDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::GridDrawable>();
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::Primitives::LineDrawable>();

			ApplyStyleInternal(
				sceneRegistry,
				*pWindow,
				screenProperties,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}
	}

	void Widget::OnDeserialized(const Serialization::Reader reader, Threading::JobBatch& jobBatch)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		// Backwards compatibility for "name" field, replace with editor info
		if (reader.HasSerializer("name"))
		{
#if PROFILE_BUILD
			m_debugName = reader.ReadWithDefaultValue<String>("name", {});
#endif

			constexpr Guid editorInfoComponentTypeGuid = "fefa51b8-945d-4f88-8859-356c7546efa7"_guid;
			Entity::ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
			const Entity::ComponentTypeIdentifier typeIdentifier = componentRegistry.FindIdentifier(editorInfoComponentTypeGuid);
			if (typeIdentifier.IsValid())
			{
				Entity::ComponentTypeInterface& editorInfoTypeInterface = *componentRegistry.Get(typeIdentifier);
				Threading::JobBatch editorInfoJobBatch;
				editorInfoTypeInterface.DeserializeInstanceWithoutChildren(
					Entity::Data::Component::Deserializer{
						Reflection::TypeDeserializer{reader, System::Get<Reflection::Registry>(), &editorInfoJobBatch},
						sceneRegistry,
						*this
					},
					sceneRegistry
				);
				Assert(!editorInfoJobBatch.IsValid());
			}
		}

		Threading::JobBatch propertiesJobBatch = ReadProperties(reader, sceneRegistry);
		jobBatch.QueueAsNewFinishedStage(propertiesJobBatch);
	}

	void Widget::OnBecomeVisibleInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow)
	{
		const Math::Rectanglei maskedContentArea = GetMaskedContentArea(sceneRegistry, pWindow);
		if (maskedContentArea.HasSize())
		{
			GetRootScene().GetRootComponent().AddComponent(*this, GetDepthRatio(), (Math::Rectanglef)GetContentArea());
		}

		OnBecomeVisible();

		for (Widget& childWidget : GetChildren())
		{
			childWidget.OnParentBecomeVisible(sceneRegistry);
		}
	}

	void Widget::OnBecomeHiddenInternal(Entity::SceneRegistry& sceneRegistry)
	{
		if (HasDataComponentOfType<Entity::Data::QuadtreeNode>(sceneRegistry))
		{
			GetRootScene().GetRootComponent().RemoveComponent(*this);
		}

		OnBecomeHidden();

		for (Widget& childWidget : GetChildren())
		{
			childWidget.OnParentBecomeHidden(sceneRegistry);
		}
	}

	void Widget::OnContentAreaChangedInternal(Entity::SceneRegistry& sceneRegistry, const EnumFlags<ContentAreaChangeFlags> changeFlags)
	{
		if (Optional<Data::ContentAreaListener*> pListener = FindFirstDataComponentImplementingType<Data::ContentAreaListener>(sceneRegistry))
		{
			pListener->OnContentAreaChanged(changeFlags);
		}

		OnContentAreaChanged(changeFlags);
	}

	void Widget::OnEnable()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		if (style.HasAnyModifiers(Style::Modifier::Disabled))
		{
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}
	}

	void Widget::OnDisable()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (HierarchyComponentBase::GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsDestroying))
		{
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.HasAnyModifiers(Style::Modifier::Disabled))
			{
				OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
			}
		}
	}

	bool Widget::Destroy(Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const Optional<Entity::Data::Flags*> pFlags = flagsSceneData.GetComponentImplementation(GetIdentifier());
		if (LIKELY(pFlags.IsValid()))
		{
			AtomicEnumFlags<Entity::ComponentFlags>& flags = *pFlags;

			// Check if we had focus, can't be checked after the destroy flag is set
			bool hasInputFocus = false;
			bool hasActiveFocus = false;
			bool hasHoverFocus = false;
			if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
			{
				{
					const Optional<Widgets::Widget*> pInputFocusWidget = pWindow->GetInputFocusWidget();
					hasInputFocus = pInputFocusWidget.IsValid() && (pInputFocusWidget == this || pInputFocusWidget->IsChildOfRecursive(*this));
				}
				{
					const Optional<Widgets::Widget*> pActiveFocusWidget = pWindow->GetActiveFocusWidget();
					hasActiveFocus = pActiveFocusWidget.IsValid() && (pActiveFocusWidget == this || pActiveFocusWidget->IsChildOfRecursive(*this));
				}
				{
					const Optional<Widgets::Widget*> pHoverFocusWidget = pWindow->GetHoverFocusWidget();
					hasHoverFocus = pHoverFocusWidget.IsValid() && (pHoverFocusWidget == this || pHoverFocusWidget->IsChildOfRecursive(*this));
				}
			}

			if (flags.TrySetFlags(Entity::ComponentFlags::IsDestroying))
			{
				{
					const EnumFlags<Entity::ComponentFlags> previousFlags = flags.FetchOr(Entity::ComponentFlags::IsDisabledWithChildren);
					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource))
					{
						DisableInternal(sceneRegistry);
					}
				}

				{
					if (const Optional<Widget*> pParent = GetParentSafe(); pParent.IsValid())
					{
						if (flags.IsNotSet(Entity::ComponentFlags::IsDetachedFromTree))
						{
							OnBeforeDetachFromParent();
							pParent->RemoveChildAndClearParent(*this, sceneRegistry);
							pParent->OnChildDetached(*this);
						}
						Assert(!pParent->GetChildren().Contains(*this));
					}

					const EnumFlags<Entity::ComponentFlags> previousFlags = flags.FetchOr(Entity::ComponentFlags::IsDetachedFromTree);
					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDetachedFromTreeFromAnySource))
					{
						Optional<Entity::ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
						pSceneData->DetachInstanceFromTree(*this, GetParentSafe());
					}
				}

				RemoveDataComponentOfType<Data::PropertySource>(sceneRegistry);
				RemoveDataComponentOfType<Data::SubscribedEvents>(sceneRegistry);

				if (const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow())
				{
					if (hasInputFocus)
					{
						pWindow->SetInputFocusWidget(nullptr);
					}
					if (hasActiveFocus)
					{
						pWindow->SetActiveFocusWidget(nullptr);
					}
					if (hasHoverFocus)
					{
						pWindow->SetHoverFocusWidget(nullptr);
					}
				}

				while (HasChildren())
				{
					Widget& child = static_cast<Widget&>(GetChild(0));
					Assert(child.GetParentSafe() == this);
					child.Destroy(sceneRegistry);
				}

				static_cast<RootWidget&>(GetRootWidget()).OnWidgetRemoved(*this);

				Assert(!HasChildren());
				Assert(!GetParentSafe().IsValid());
				GetRootScene().QueueComponentDestruction(*this);
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	void Widget::OnAttachedToTree([[maybe_unused]] const Optional<Widget*> pParent)
	{
		// TODO: Move tree logic in here
		// GetRootSceneComponent().AddComponent(*this, GetDepthRatio(), (Math::Rectanglef)GetContentArea());
	}

	void Widget::OnDetachedFromTree([[maybe_unused]] const Optional<Widget*> pParentnt)
	{
		if (HasDataComponentOfType<Entity::Data::QuadtreeNode>(GetSceneRegistry()))
		{
			GetRootSceneComponent().RemoveComponent(*this);
		}
	}

	void Widget::OnDisplayPropertiesChanged(Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::TextDrawable*> pTextDrawableComponent = FindDataComponentOfType<Data::TextDrawable>(sceneRegistry))
		{
			pTextDrawableComponent->OnDisplayPropertiesChanged(*this);
		}

		for (Widget& childWidget : GetChildren())
		{
			childWidget.OnDisplayPropertiesChanged(sceneRegistry);
		}
	}

	[[nodiscard]] bool ShouldMask(
		const Widget& widget,
		Entity::SceneRegistry& sceneRegistry,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	)
	{
		const Style::CombinedEntry style = widget.GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		const OverflowType overflowType =
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);
		return overflowType != OverflowType::Visible;
	}

	Math::Rectanglei Widget::GetMaskedContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();
		return GetMaskedContentArea(
			sceneRegistry,
			pWindow,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData
		);
	}

	Math::Rectanglei Widget::GetMaskedContentArea(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	) const
	{
		const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		const PositionType positionType =
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);

		const OverflowType overflowType =
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);

		Math::Rectanglei contentArea = GetContentArea(sceneRegistry);
		if (overflowType == OverflowType::Visible && positionType == PositionType::Absolute)
		{
			if (pWindow.IsValid())
			{
				return pWindow->GetLocalClientArea().Mask(contentArea);
			}
			else
			{
				return contentArea;
			}
		}

		for (Widget* pParent = GetParentSafe(); pParent != nullptr; pParent = pParent->GetParentSafe())
		{
			if (ShouldMask(*pParent, sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData))
			{
				contentArea = pParent->GetContentArea(sceneRegistry).Mask(contentArea);
			}
		}

		if (pWindow.IsValid())
		{
			contentArea = pWindow->GetLocalClientArea().Mask(contentArea);
		}
		return contentArea;
	}

	Math::Rectanglei Widget::GetMaskedContentArea(
		Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow, Math::Rectanglei contentArea
	) const
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();
		return GetMaskedContentArea(
			sceneRegistry,
			pWindow,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			contentArea
		);
	}

	Math::Rectanglei Widget::GetMaskedContentArea(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Math::Rectanglei contentArea
	) const
	{
		const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		auto positionType =
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);

		const OverflowType overflowType =
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);

		contentArea = GetContentArea(sceneRegistry).Mask(contentArea);
		if (overflowType == OverflowType::Visible && positionType == PositionType::Absolute)
		{
			if (pWindow.IsValid())
			{
				return pWindow->GetLocalClientArea().Mask(contentArea);
			}
			else
			{
				return contentArea;
			}
		}

		for (Widget* pParent = GetParentSafe(); pParent != nullptr; pParent = pParent->GetParentSafe())
		{
			if (ShouldMask(*pParent, sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData))
			{
				contentArea = pParent->GetContentArea(sceneRegistry).Mask(contentArea);
			}
		}
		if (pWindow.IsValid())
		{
			contentArea = pWindow->GetLocalClientArea().Mask(contentArea);
		}
		return contentArea;
	}

	void Widget::SetContentArea(Math::Rectanglei newContentArea, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& __restrict worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform2D>& __restrict localTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::LocalTransform2D>();
		Entity::ComponentTypeSceneData<Entity::Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Parent>();

		Entity::Data::WorldTransform2D& __restrict worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()
		);
		Entity::Data::LocalTransform2D& __restrict localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()
		);
		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());

		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		EnumFlags<ContentAreaChangeFlags> changeFlags;
		{
			const Math::Rectanglei previousContentArea = {
				(Math::Vector2i)worldTransform.GetLocation(),
				(Math::Vector2i)worldTransform.GetScale()
			};
			Assert(previousContentArea != newContentArea);
			Assert((newContentArea.GetSize() >= Math::Zero).AreAllSet());

			const typename Math::Vector2i::BoolType wasPositionChanged = newContentArea.GetPosition() != previousContentArea.GetPosition();
			changeFlags |= Widget::ContentAreaChangeFlags::PositionXChanged * wasPositionChanged.IsXSet();
			changeFlags |= Widget::ContentAreaChangeFlags::PositionYChanged * wasPositionChanged.IsYSet();

			const typename Math::Vector2i::BoolType wasSizeChanged = newContentArea.GetSize() != previousContentArea.GetSize();
			changeFlags |= Widget::ContentAreaChangeFlags::SizeXChanged * wasSizeChanged.IsXSet();
			changeFlags |= Widget::ContentAreaChangeFlags::SizeYChanged * wasSizeChanged.IsYSet();
		}

		worldTransform.SetLocation((Math::Vector2f)newContentArea.GetPosition());
		worldTransform.SetScale((Math::Vector2f)newContentArea.GetSize());

		localTransform.SetTransform(Math::Transform2Df{
			parentWorldTransform.InverseTransformRotation(worldTransform.GetRotation()),
			parentWorldTransform.InverseTransformLocationWithoutScale(worldTransform.GetLocation())
		});

		using UpdateChildren = void (*)(
			Widget& widget,
			const Optional<ToolWindow*> pWindow,
			const Math::WorldTransform2D newParentTransform,
			Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
			Entity::ComponentTypeSceneData<Entity::Data::LocalTransform2D>& localTransformSceneData,
			Entity::SceneRegistry& sceneRegistry,
			Entity::RootSceneComponent2D& rootSceneComponent
		);
		static UpdateChildren updateChildren = [](
																						 Widget& widget,
																						 const Optional<ToolWindow*> pWindow,
																						 const Math::WorldTransform2D newParentTransform,
																						 Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& worldTransformSceneData,
																						 Entity::ComponentTypeSceneData<Entity::Data::LocalTransform2D>& localTransformSceneData,
																						 Entity::SceneRegistry& sceneRegistry,
																						 Entity::RootSceneComponent2D& rootSceneComponent
																					 )
		{
			Entity::Data::WorldTransform2D& worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
			const Math::Transform2Df localTransform = localTransformSceneData.GetComponentImplementationUnchecked(widget.GetIdentifier());
			const Math::WorldTransform2D newTransform = Math::WorldTransform2D{
				newParentTransform.TransformRotation(localTransform.GetRotation()),
				newParentTransform.TransformLocationWithoutScale(localTransform.GetLocation()),
				worldTransform.GetScale()
			};

			Math::Rectanglei widgetContentArea = {(Math::Vector2i)worldTransform.GetLocation(), (Math::Vector2i)worldTransform.GetScale()};
			bool changed = ((Math::Vector2i)newTransform.GetLocation() != widgetContentArea.GetPosition()).AreAnySet();

			worldTransform = newTransform;
			widgetContentArea.SetPosition((Math::Vector2i)newTransform.GetLocation());

			if (widget.GetMaskedContentArea(sceneRegistry, pWindow).HasSize() && widget.IsVisible())
			{
				if (widget.HasDataComponentOfType<Entity::Data::QuadtreeNode>(sceneRegistry))
				{
					rootSceneComponent
						.OnComponentWorldLocationOrBoundsChanged(widget, widget.GetDepthRatio(), (Math::Rectanglef)widgetContentArea, sceneRegistry);
				}
				else
				{
					rootSceneComponent.AddComponent(widget, widget.GetDepthRatio(), (Math::Rectanglef)widgetContentArea);
				}
				changed = true;
			}
			else if (widget.HasDataComponentOfType<Entity::Data::QuadtreeNode>(sceneRegistry))
			{
				rootSceneComponent.RemoveComponent(widget);
				changed = true;
			}

			if (changed)
			{
				for (Widget& child : widget.GetChildren())
				{
					updateChildren(child, pWindow, newTransform, worldTransformSceneData, localTransformSceneData, sceneRegistry, rootSceneComponent);
				}
			}
		};

		const Optional<ToolWindow*> pWindow = GetOwningWindow();
		Entity::RootSceneComponent2D& rootSceneComponent = GetRootScene().GetRootComponent();
		for (Widget& child : GetChildren())
		{
			updateChildren(child, pWindow, worldTransform, worldTransformSceneData, localTransformSceneData, sceneRegistry, rootSceneComponent);
		}

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		RecalculateHierarchy(sceneRegistry, externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData, modifiersSceneData);
		OnContentAreaChangedInternal(sceneRegistry, changeFlags);
	}

	void Widget::SetContentArea(Math::Rectanglei newContentArea)
	{
		SetContentArea(newContentArea, GetSceneRegistry());
	}

	void Widget::Reposition(const Coordinate newPosition)
	{
		Reposition(newPosition, GetSceneRegistry());
	}

	void Widget::Resize(const Math::Vector2i size)
	{
		Resize(size, GetSceneRegistry());
	}

	void Widget::SetRelativeLocation(const LocalCoordinate coordinate, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform2D>& __restrict worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform2D>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform2D>& __restrict localTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::LocalTransform2D>();
		Entity::ComponentTypeSceneData<Entity::Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Parent>();

		Entity::Data::LocalTransform2D& __restrict localTransform = localTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier()
		);
		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());

		const Math::WorldTransform2D parentWorldTransform = (Math::WorldTransform2D
		)worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		Math::Transform2Df relativeTransform = localTransform;
		relativeTransform.SetLocation((Math::Vector2f)coordinate);

		Reposition((Coordinate)parentWorldTransform.TransformLocationWithoutScale(relativeTransform.GetLocation()), sceneRegistry);
	}

	void Widget::SetRelativeLocation(const LocalCoordinate coordinate)
	{
		SetRelativeLocation(coordinate, GetSceneRegistry());
	}

	void Widget::OnReceivedHoverFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags |= Flags::HasHoverFocus;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Hover))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildReceivedHoverFocusInternal(sceneRegistry);
		}

		RecalculateHierarchy(sceneRegistry);
	}

	void Widget::OnLostHoverFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags &= ~Flags::HasHoverFocus;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Hover))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildLostHoverFocusInternal(sceneRegistry);
		}

		RecalculateHierarchy(sceneRegistry);
	}

	void Widget::OnChildReceivedHoverFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags |= Flags::IsHoverFocusInChildren;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Hover))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildReceivedHoverFocusInternal(sceneRegistry);
		}
	}

	void Widget::OnChildLostHoverFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags &= ~Flags::IsHoverFocusInChildren;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Hover))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(GetSceneRegistry(), GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildLostHoverFocusInternal(sceneRegistry);
		}
	}

	void Widget::SetHasInputFocus()
	{
		m_flags |= Flags::HasInputFocus;

		for (Optional<Widget*> pParent = GetParentSafe(); pParent.IsValid(); pParent = pParent->GetParentSafe())
		{
			pParent->m_flags |= Flags::IsInputFocusInChildren;
		}
	}

	void Widget::ClearHasInputFocus()
	{
		m_flags &= ~Flags::HasInputFocus;

		for (Optional<Widget*> pParent = GetParentSafe(); pParent.IsValid(); pParent = pParent->GetParentSafe())
		{
			pParent->m_flags &= ~Flags::IsInputFocusInChildren;
		}
	}

	void Widget::OnReceivedInputFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnReceivedInputFocus(*this);
		}

		{
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			if (style.HasAnyModifiers(Style::Modifier::Focused))
			{
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
			}
		}

		for (Optional<Widget*> pParent = GetParentSafe(); pParent.IsValid(); pParent = pParent->GetParentSafe())
		{
			const Style::CombinedEntry parentStyle = pParent->GetStyle(sceneRegistry);
			if (parentStyle.HasAnyModifiers(Style::Modifier::Focused))
			{
				const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);
				pParent->OnActiveStyleModifiersChangedInternal(
					sceneRegistry,
					GetOwningWindow(),
					parentStyle,
					parentMatchingModifiers,
					parentStyle.m_valueTypeMask
				);
			}

			if (const Optional<Data::Input*> pInputComponent = pParent->FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				pInputComponent->OnChildReceivedInputFocus(*pParent);
			}
		}

		RecalculateHierarchy(sceneRegistry);
	}

	void Widget::OnLostInputFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnLostInputFocus(*this);
		}

		{
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			if (style.HasAnyModifiers(Style::Modifier::Focused))
			{
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
			}
		}

		for (Optional<Widget*> pParent = GetParentSafe(); pParent.IsValid(); pParent = pParent->GetParentSafe())
		{
			const Style::CombinedEntry parentStyle = pParent->GetStyle(sceneRegistry);
			if (parentStyle.HasAnyModifiers(Style::Modifier::Focused))
			{
				const EnumFlags<Style::Modifier> parentActiveModifiers = pParent->GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);
				pParent->OnActiveStyleModifiersChangedInternal(
					sceneRegistry,
					GetOwningWindow(),
					parentStyle,
					parentMatchingModifiers,
					parentStyle.m_valueTypeMask
				);
			}

			if (const Optional<Data::Input*> pInputComponent = pParent->FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				pInputComponent->OnChildLostInputFocus(*pParent);
			}
		}

		RecalculateHierarchy(sceneRegistry);
	}

	void Widget::OnReceivedActiveFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags |= Flags::HasActiveFocus;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Active))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildReceivedActiveFocusInternal(sceneRegistry);
		}
	}

	void Widget::OnLostActiveFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags &= ~Flags::HasActiveFocus;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Active))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildLostActiveFocusInternal(sceneRegistry);
		}
	}

	void Widget::OnChildReceivedActiveFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags |= Flags::IsActiveFocusInChildren;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Active))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildReceivedActiveFocusInternal(sceneRegistry);
		}
	}

	void Widget::OnChildLostActiveFocusInternal(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags &= ~Flags::IsActiveFocusInChildren;

		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		if (style.HasAnyModifiers(Style::Modifier::Active))
		{
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			OnActiveStyleModifiersChangedInternal(sceneRegistry, GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		if (Optional<Widget*> pParent = GetParentSafe())
		{
			pParent->OnChildLostActiveFocusInternal(sceneRegistry);
		}
	}

	Widget& Widget::GetRootWidget()
	{
		return m_rootWidget;
	}

	Widget& Widget::GetRootAssetWidget()
	{
		Widget* pWidget = this;
		while (!pWidget->m_flags.IsSet(Flags::IsAssetRootWidget) && pWidget->GetParentSafe().IsValid())
		{
			pWidget = pWidget->GetParentSafe();
		}
		return *pWidget;
	}

	DataSource::PropertyValue Widget::DataSourceProperties::GetDataProperty(const ngine::DataSource::PropertyIdentifier propertyIdentifier
	) const
	{
		for (auto it = m_dataSourceEntries.begin(), endIt = m_dataSourceEntries.end(); it != endIt; ++it)
		{
			if (it->m_pDataSourceState.IsValid())
			{
				if (DataSource::PropertyValue propertyValue = it->m_pDataSourceState->GetDataProperty(it->m_data, propertyIdentifier);
				    propertyValue.HasValue())
				{
					return Move(propertyValue);
				}
			}

			if (DataSource::PropertyValue propertyValue = it->m_dataSource.GetDataProperty(it->m_data, propertyIdentifier);
			    propertyValue.HasValue())
			{
				return Move(propertyValue);
			}
		}

		for (auto it = m_propertySources.begin(), endIt = m_propertySources.end(); it != endIt; ++it)
		{
			ngine::PropertySource::Interface& propertySource = *it;
			propertySource.LockRead();
			if (DataSource::PropertyValue propertyValue = propertySource.GetDataProperty(propertyIdentifier); propertyValue.HasValue())
			{
				propertySource.UnlockRead();
				return Move(propertyValue);
			}
			propertySource.UnlockRead();
		}

		return {};
	}

	void Widget::ToggleModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry)
	{
		if (Optional<Data::Modifiers*> pModifiers = FindDataComponentOfType<Data::Modifiers>(sceneRegistry))
		{
			pModifiers->ToggleModifiers(*this, modifiers);
		}
		else
		{
			pModifiers = CreateDataComponent<Data::Modifiers>(sceneRegistry, Data::Modifiers::Initializer{*this, sceneRegistry});
			pModifiers->ToggleModifiers(*this, modifiers);
		}
	}

	void Widget::ToggleModifiers(const EnumFlags<Style::Modifier> modifiers)
	{
		ToggleModifiers(modifiers, GetSceneRegistry());
	}

	void Widget::SetModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry)
	{
		if (Optional<Data::Modifiers*> pModifiers = FindDataComponentOfType<Data::Modifiers>(sceneRegistry))
		{
			pModifiers->SetModifiers(*this, modifiers);
		}
		else
		{
			pModifiers = CreateDataComponent<Data::Modifiers>(sceneRegistry, Data::Modifiers::Initializer{*this, sceneRegistry});
			pModifiers->SetModifiers(*this, modifiers);
		}
	}

	void Widget::SetModifiers(const EnumFlags<Style::Modifier> modifiers)
	{
		SetModifiers(modifiers, GetSceneRegistry());
	}

	void Widget::ClearModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::Modifiers*> pModifiers = FindDataComponentOfType<Data::Modifiers>(sceneRegistry))
		{
			pModifiers->ClearModifiers(*this, modifiers);
		}
	}

	void Widget::ClearModifiers(const EnumFlags<Style::Modifier> modifiers)
	{
		ClearModifiers(modifiers, GetSceneRegistry());
	}

	void Widget::ModifyDynamicStyleEntries(
		Entity::SceneRegistry& sceneRegistry, const Style::ValueTypeIdentifier valueTypeIdentifier, const StyleEntryCallback& callback
	)
	{
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::GridLayout>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData = *sceneRegistry.FindComponentTypeData<Data::DataSource>();
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		ModifyDynamicStyleEntriesInternal(
			sceneRegistry,
			GetOwningWindow(),
			valueTypeIdentifier,
			callback,
			flexLayoutSceneData,
			gridLayoutSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			dataSourceSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);
	}

	void Widget::ModifyDynamicStyleEntriesInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const Style::ValueTypeIdentifier valueTypeIdentifier,
		const StyleEntryCallback& callback,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		// For now only allow setting of dynamic style if it doesn't get supplied by a data source
		const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData);
		if (pDynamicStyle.IsValid() && !HasDataComponentOfType(sceneRegistry, dataSourceSceneData.GetIdentifier()))
		{
			Style::DynamicEntry& __restrict dynamicStyle = pDynamicStyle->GetEntry();
			if (dynamicStyle.m_dynamicEntry.GetDynamicValueTypeMask().IsSet((uint8)valueTypeIdentifier))
			{
				Style::Entry::ModifierValues* pDynamicModifierValues = dynamicStyle.m_dynamicEntry.FindExactModifierMatch(Style::Modifier::None);
				Style::Entry::ModifierValues* pPopulatedModifierValues = dynamicStyle.m_populatedEntry.FindExactModifierMatch(Style::Modifier::None
				);
				if (!pPopulatedModifierValues)
				{
					dynamicStyle.m_populatedEntry = dynamicStyle.m_dynamicEntry;
				}
				pPopulatedModifierValues = dynamicStyle.m_populatedEntry.FindExactModifierMatch(Style::Modifier::None);

				const Style::Entry::ValueView dynamicValues = pDynamicModifierValues->GetValues();
				const Style::Entry::ValueView populatedValues = pPopulatedModifierValues->GetValues();

				if (callback(
							dynamicValues[valueTypeIdentifier]->GetExpected<ngine::DataSource::PropertyIdentifier>(),
							*populatedValues[valueTypeIdentifier]
						))
				{
					dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);

					const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
					const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
					const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

					ChangedStyleValues changedStyleValues;
					changedStyleValues.Set((uint8)valueTypeIdentifier);
					OnStyleChangedInternal(
						sceneRegistry,
						pWindow,
						style,
						matchingModifiers,
						changedStyleValues,
						flexLayoutSceneData,
						gridLayoutSceneData,
						externalStyleSceneData,
						inlineStyleSceneData,
						dynamicStyleSceneData,
						modifiersSceneData,
						textDrawableSceneData,
						imageDrawableSceneData,
						roundedRectangleDrawableSceneData,
						rectangleDrawableSceneData,
						circleDrawableSceneData,
						gridDrawableSceneData,
						lineDrawableSceneData
					);
				}
			}
		}

		for (Widget& child : GetChildren())
		{
			child.ModifyDynamicStyleEntriesInternal(
				sceneRegistry,
				pWindow,
				valueTypeIdentifier,
				callback,
				flexLayoutSceneData,
				gridLayoutSceneData,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				dataSourceSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}
	}

	void Widget::ModifyDynamicStyleEntries(const Style::ValueTypeIdentifier valueTypeIdentifier, const StyleEntryCallback& callback)
	{
		ModifyDynamicStyleEntries(GetSceneRegistry(), valueTypeIdentifier, callback);
	}

	void Widget::UpdateFromDataSource(Entity::SceneRegistry& sceneRegistry, DataSourceEntryInfo&& dataSourceInfo)
	{
		m_flags &= ~Flags::ShouldAutoApplyStyle;

		const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow();

		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::GridLayout>();
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::DataSource>();
		Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::DataSourceEntry>();
		Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::PropertySource>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		DataSourceProperties dataSourceProperties;
		// Top priority: this widget's data source entry
		dataSourceProperties.Emplace(Forward<DataSourceEntryInfo>(dataSourceInfo));

		InlineVector<ReferenceWrapper<ngine::DataSource::Interface>, 6> lockedDataSources;
		// Second priority: This widget's inline data source
		if (Optional<ngine::DataSource::Interface*> pLockedDataSource = ResolveInlineDataSource(dataSourceProperties, dataSourceSceneData))
		{
			lockedDataSources.EmplaceBack(*pLockedDataSource);
		}

		// Third priority: This widget's property source
		ResolvePropertySource(dataSourceProperties, propertySourceSceneData);

		// Now walk the hierarchy and emplace data from parents
		for (Optional<Widget*> pParent = GetParentSafe(); pParent != nullptr; pParent = pParent->GetParentSafe())
		{
			// Top priority: widget's data source entry
			if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = pParent->ResolveDataSourceEntry(dataSourceProperties, flexLayoutSceneData, gridLayoutSceneData, dataSourceSceneData, dataSourceEntrySceneData))
			{
				lockedDataSources.EmplaceBack(*pLockedDataSource);
			}

			// Second priority: widget's inline data source
			if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = pParent->ResolveInlineDataSource(dataSourceProperties, dataSourceSceneData))
			{
				lockedDataSources.EmplaceBack(*pLockedDataSource);
			}

			// Third priority: widget's property source
			pParent->ResolvePropertySource(dataSourceProperties, propertySourceSceneData);
		}

		UpdateFromResolvedDataSourceInternal(
			sceneRegistry,
			pWindow,
			dataSourceProperties,
			flexLayoutSceneData,
			gridLayoutSceneData,
			dataSourceSceneData,
			dataSourceEntrySceneData,
			propertySourceSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);

		for (ngine::DataSource::Interface& lockedDataSource : lockedDataSources)
		{
			lockedDataSource.UnlockRead();
		}
	}

	void Widget::UpdateFromDataSource(Entity::SceneRegistry& sceneRegistry)
	{
		m_flags &= ~Flags::ShouldAutoApplyStyle;

		const Optional<Rendering::ToolWindow*> pWindow = GetOwningWindow();
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::GridLayout>();
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::DataSource>();
		Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::DataSourceEntry>();
		Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::PropertySource>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		DataSourceProperties dataSourceProperties;
		InlineVector<ReferenceWrapper<ngine::DataSource::Interface>, 6> lockedDataSources;
		// Now walk the hierarchy and emplace data from parents
		for (Optional<Widget*> pWidget = this; pWidget != nullptr; pWidget = pWidget->GetParentSafe())
		{
			// Top priority: widget's data source entry
			if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = pWidget->ResolveDataSourceEntry(dataSourceProperties, flexLayoutSceneData, gridLayoutSceneData, dataSourceSceneData, dataSourceEntrySceneData))
			{
				lockedDataSources.EmplaceBack(*pLockedDataSource);
			}

			// Second priority: widget's inline data source
			if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = pWidget->ResolveInlineDataSource(dataSourceProperties, dataSourceSceneData))
			{
				lockedDataSources.EmplaceBack(*pLockedDataSource);
			}

			// Third priority: widget's property source
			pWidget->ResolvePropertySource(dataSourceProperties, propertySourceSceneData);
		}

		UpdateFromResolvedDataSourceInternal(
			sceneRegistry,
			pWindow,
			dataSourceProperties,
			flexLayoutSceneData,
			gridLayoutSceneData,
			dataSourceSceneData,
			dataSourceEntrySceneData,
			propertySourceSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);

		for (ngine::DataSource::Interface& lockedDataSource : lockedDataSources)
		{
			lockedDataSource.UnlockRead();
		}
	}

	void Widget::UpdateDynamicTagQueryFromDataSourceInternal(
		Entity::SceneRegistry& sceneRegistry,
		const DataSourceProperties& dataSourceProperties,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData
	)
	{
		if (const Optional<Data::DataSource*> pDataSourceData = FindDataComponentOfType<Data::DataSource>(dataSourceSceneData))
		{
			if (Data::DataSource::DynamicTagQuery& dynamicTagQueryReference = pDataSourceData->GetDynamicTagQuery();
			    dynamicTagQueryReference.IsValid())
			{
				const Optional<const Tag::Query*> pConstTagQuery = pDataSourceData->GetConstTagQuery();

				Tag::Query& dynamicTagQuery = *dynamicTagQueryReference.m_pTagQuery;
				dynamicTagQuery = pConstTagQuery.IsValid() ? *pConstTagQuery : Tag::Query{};

				if (dynamicTagQueryReference.m_allowedFilterMaskPropertyIdentifier.IsValid())
				{
					const DataSource::PropertyValue dataPropertyValue =
						dataSourceProperties.GetDataProperty(dynamicTagQueryReference.m_allowedFilterMaskPropertyIdentifier);
					if (dataPropertyValue.HasValue())
					{
						if (const Optional<const Tag::Identifier*> tagIdentifier = dataPropertyValue.Get<Tag::Identifier>())
						{
							if (tagIdentifier->IsValid())
							{
								dynamicTagQuery.m_allowedFilterMask.Set(*tagIdentifier);
							}
						}
						else if (const Optional<const Tag::Mask*> tagMask = dataPropertyValue.Get<Tag::Mask>())
						{
							dynamicTagQuery.m_allowedFilterMask |= *tagMask;
						}
					}
				}

				if (dynamicTagQueryReference.m_requiredFilterMaskPropertyIdentifier.IsValid())
				{
					const DataSource::PropertyValue dataPropertyValue =
						dataSourceProperties.GetDataProperty(dynamicTagQueryReference.m_requiredFilterMaskPropertyIdentifier);
					if (dataPropertyValue.HasValue())
					{
						if (const Optional<const Tag::Identifier*> tagIdentifier = dataPropertyValue.Get<Tag::Identifier>())
						{
							if (tagIdentifier->IsValid())
							{
								dynamicTagQuery.m_requiredFilterMask.Set(*tagIdentifier);
							}
						}
						else if (const Optional<const Tag::Mask*> tagMask = dataPropertyValue.Get<Tag::Mask>())
						{
							dynamicTagQuery.m_requiredFilterMask |= *tagMask;
						}
					}
				}

				if (dynamicTagQueryReference.m_disallowedFilterMaskPropertyIdentifier.IsValid())
				{
					const DataSource::PropertyValue dataPropertyValue =
						dataSourceProperties.GetDataProperty(dynamicTagQueryReference.m_disallowedFilterMaskPropertyIdentifier);
					if (dataPropertyValue.HasValue())
					{
						if (const Optional<const Tag::Identifier*> tagIdentifier = dataPropertyValue.Get<Tag::Identifier>())
						{
							if (tagIdentifier->IsValid())
							{
								dynamicTagQuery.m_disallowedFilterMask.Set(*tagIdentifier);
							}
						}
						else if (const Optional<const Tag::Mask*> tagMask = dataPropertyValue.Get<Tag::Mask>())
						{
							dynamicTagQuery.m_disallowedFilterMask |= *tagMask;
						}
					}
				}

				if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData))
				{
					pFlexLayoutComponent->OnDataSourcePropertiesChanged(*this, sceneRegistry);
				}
				else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData))
				{
					pGridLayoutComponent->OnDataSourcePropertiesChanged(*this, sceneRegistry);
				}
			}
		}
	}

	void Widget::UpdateDynamicStyleFromDataSourceInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const DataSourceProperties& dataSourceProperties,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData))
		{
			Style::DynamicEntry& __restrict dynamicStyle = pDynamicStyle->GetEntry();

			Bitset<(uint8)Style::ValueTypeIdentifier::Count> changedStyleValues;
			if (!dynamicStyle.m_populatedEntry.HasElements())
			{
				dynamicStyle.m_populatedEntry = dynamicStyle.m_dynamicEntry;
				changedStyleValues = dynamicStyle.m_dynamicEntry.GetValueTypeMask();
			}
			const Style::Entry::ModifierValues& sourceModifierValues = *dynamicStyle.m_dynamicEntry.FindExactModifierMatch(Style::Modifier::None);
			Style::Entry::ModifierValues& targetModifierValues = *dynamicStyle.m_populatedEntry.FindExactModifierMatch(Style::Modifier::None);

			Asset::Manager& assetManager = System::Get<Asset::Manager>();

			const Style::Entry::ConstValueView sourceValues = sourceModifierValues.GetValues();
			const Style::Entry::ValueView targetValues = targetModifierValues.GetValues();
			for (const uint8 dynamicValueTypeIdentifier : dynamicStyle.m_dynamicEntry.GetDynamicValueTypeMask().GetSetBitsIterator())
			{
				const Style::ValueTypeIdentifier valueTypeIdentifier = (Style::ValueTypeIdentifier)dynamicValueTypeIdentifier;

				const UniquePtr<Style::EntryValue>& pSourceValue = sourceValues[valueTypeIdentifier];
				Assert(pSourceValue.IsValid());
				if (UNLIKELY_ERROR(!pSourceValue.IsValid()))
				{
					continue;
				}
				const Style::Entry::Value& sourceValue = *pSourceValue;

				UniquePtr<Style::EntryValue>& pTargetValue = targetValues[valueTypeIdentifier];
				Assert(pTargetValue.IsValid());
				if (UNLIKELY_ERROR(!pTargetValue.IsValid()))
				{
					continue;
				}
				Style::Entry::Value& targetValue = *pTargetValue;

				if (const Optional<const ngine::DataSource::PropertyIdentifier*> propertyIdentifier =
				      sourceValue.Get<ngine::DataSource::PropertyIdentifier>();
				    propertyIdentifier.IsValid() && propertyIdentifier->IsValid())
				{
					const DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(*propertyIdentifier);
					if (dataPropertyValue.HasValue())
					{
						if (const Optional<const ConstStringView*> stringView = dataPropertyValue.Get<ConstStringView>(); stringView.IsValid())
						{
							UnicodeString unicodeString(*stringView);
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != unicodeString)
							{
								targetValue = Move(unicodeString);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const ConstUnicodeStringView*> unicodeStringView = dataPropertyValue.Get<ConstUnicodeStringView>();
						         unicodeStringView.IsValid())
						{
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != *unicodeStringView)
							{
								targetValue = UnicodeString(*unicodeStringView);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const String*> string = dataPropertyValue.Get<String>(); string.IsValid())
						{
							UnicodeString unicodeString(string->GetView());
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != unicodeString)
							{
								targetValue = Move(unicodeString);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const IO::Path*> path = dataPropertyValue.Get<IO::Path>(); path.IsValid())
						{
							UnicodeString unicodeString(path->GetView().GetStringView());
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != unicodeString)
							{
								targetValue = Move(unicodeString);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const IO::PathView*> pathView = dataPropertyValue.Get<IO::PathView>(); pathView.IsValid())
						{
							UnicodeString unicodeString(pathView->GetStringView());
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != unicodeString)
							{
								targetValue = Move(unicodeString);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const UnicodeString*> unicodeString = dataPropertyValue.Get<UnicodeString>(); unicodeString.IsValid())
						{
							if (!targetValue.Is<UnicodeString>() || *targetValue.Get<UnicodeString>() != *unicodeString)
							{
								targetValue = UnicodeString(*unicodeString);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const Math::Color*> color = dataPropertyValue.Get<Math::Color>(); color.IsValid())
						{
							if (!targetValue.Is<Math::Color>() || *targetValue.Get<Math::Color>() != *color)
							{
								targetValue = Math::Color(*color);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const Math::LinearGradient*> gradient = dataPropertyValue.Get<Math::LinearGradient>())
						{
							if (!targetValue.Is<Math::LinearGradient>() || *targetValue.Get<Math::LinearGradient>() != *gradient)
							{
								targetValue = Math::LinearGradient(*gradient);
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const Asset::Guid*> assetGuid = dataPropertyValue.Get<Asset::Guid>();
						         assetGuid.IsValid() && assetGuid->IsValid())
						{

							if (!targetValue.Is<Asset::Guid>() || *targetValue.Get<Asset::Guid>() != *assetGuid)
							{
								if (assetManager.HasAsset(*assetGuid) || assetManager.GetAssetLibrary().HasAsset(*assetGuid))
								{
									targetValue = Asset::Guid(*assetGuid);
									changedStyleValues.Set((uint8)valueTypeIdentifier);
									dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
									targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
								}
								else if (targetValue.HasValue())
								{
									targetValue = {};
									changedStyleValues.Set((uint8)valueTypeIdentifier);
									dynamicStyle.m_populatedEntry.OnValueTypeRemoved(valueTypeIdentifier);
									targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
								}
							}
						}
						else if (const Optional<const Entity::ComponentSoftReference*> componentReference3D =
						           dataPropertyValue.Get<Entity::ComponentSoftReference>();
						         componentReference3D.IsValid() && componentReference3D->IsPotentiallyValid())
						{
							if (!targetValue.Is<Entity::ComponentSoftReference>() || *targetValue.Get<Entity::ComponentSoftReference>() != *componentReference3D)
							{
								targetValue = *componentReference3D;
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (const Optional<const Entity::ComponentSoftReference*> componentReference2D =
						           dataPropertyValue.Get<Entity::ComponentSoftReference>();
						         componentReference2D.IsValid() && componentReference2D->IsPotentiallyValid())
						{
							if (!targetValue.Is<Entity::ComponentSoftReference>() || *targetValue.Get<Entity::ComponentSoftReference>() != *componentReference2D)
							{
								targetValue = *componentReference2D;
								changedStyleValues.Set((uint8)valueTypeIdentifier);
								dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
								targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
							}
						}
						else if (targetValue.HasValue())
						{
							targetValue = {};
							changedStyleValues.Set((uint8)valueTypeIdentifier);
							dynamicStyle.m_populatedEntry.OnValueTypeRemoved(valueTypeIdentifier);
							targetModifierValues.OnValueTypeRemoved(valueTypeIdentifier);
						}
					}
					else if (targetValue.HasValue())
					{
						targetValue = {};
						changedStyleValues.Set((uint8)valueTypeIdentifier);
						dynamicStyle.m_populatedEntry.OnValueTypeRemoved(valueTypeIdentifier);
						targetModifierValues.OnValueTypeRemoved(valueTypeIdentifier);
					}
				}
				else
				{
					static auto resolveSizeAxisExpression = [](
																										const Style::SizeAxisExpression& sourceSizeAxisExpression,
																										Style::SizeAxisExpression& targetSizeAxisExpression,
																										const DataSourceProperties dataSourceProperties
																									) -> bool
					{
						bool changedAny = false;
						for (const Style::SizeAxisOperation& sourceSizeAxisOperation : sourceSizeAxisExpression.m_operations)
						{
							const Optional<const Style::SizeAxis*> pSourceSizeAxis = sourceSizeAxisOperation.Get<Style::SizeAxis>();
							if (pSourceSizeAxis.IsInvalid())
							{
								continue;
							}

							const Optional<const ngine::DataSource::PropertyIdentifier*> propertyIdentifier =
								pSourceSizeAxis->m_value.Get<ngine::DataSource::PropertyIdentifier>();
							if (propertyIdentifier.IsInvalid())
							{
								continue;
							}

							const DataSource::PropertyValue dataPropertyValue = dataSourceProperties.GetDataProperty(*propertyIdentifier);
							if (!dataPropertyValue.HasValue())
							{
								continue;
							}

							Style::SizeAxisOperation& targetSizeAxisOperation =
								targetSizeAxisExpression.m_operations[sourceSizeAxisExpression.m_operations.GetIteratorIndex(&sourceSizeAxisOperation)];

							if (const Optional<const float*> floatValue = dataPropertyValue.Get<float>())
							{
								if (targetSizeAxisOperation != Style::SizeAxis(*floatValue))
								{
									targetSizeAxisOperation = Style::SizeAxis(*floatValue);
									changedAny = true;
								}
							}
							else if (const Optional<const uint32*> uint32Value = dataPropertyValue.Get<uint32>())
							{
								if (targetSizeAxisOperation != Style::SizeAxis(PixelValue{(int32)*uint32Value}))
								{
									targetSizeAxisOperation = Style::SizeAxis(PixelValue{(int32)*uint32Value});
									changedAny = true;
								}
							}
							else if (const Optional<const int32*> int32Value = dataPropertyValue.Get<int32>())
							{
								if (targetSizeAxisOperation != Style::SizeAxis(PixelValue{*int32Value}))
								{
									targetSizeAxisOperation = Style::SizeAxis(PixelValue{*int32Value});
									changedAny = true;
								}
							}
						}
						return changedAny;
					};

					if (const Optional<const Style::SizeAxisExpression*> sourceSizeAxisExpression = sourceValue.Get<Style::SizeAxisExpression>())
					{
						const Optional<Style::SizeAxisExpression*> targetSizeAxisExpression = targetValue.Get<Style::SizeAxisExpression>();
						if (resolveSizeAxisExpression(*sourceSizeAxisExpression, *targetSizeAxisExpression, dataSourceProperties))
						{
							changedStyleValues.Set((uint8)valueTypeIdentifier);
							dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
							targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
						}
					}
					else if (const Optional<const Style::Size*> sourceSize = sourceValue.Get<Style::Size>())
					{
						const Optional<Style::Size*> targetSize = targetValue.Get<Style::Size>();
						if (resolveSizeAxisExpression(sourceSize->x, targetSize->x, dataSourceProperties) | resolveSizeAxisExpression(sourceSize->y, targetSize->y, dataSourceProperties))
						{
							changedStyleValues.Set((uint8)valueTypeIdentifier);
							dynamicStyle.m_populatedEntry.OnValueTypeAdded(valueTypeIdentifier);
							targetModifierValues.OnValueTypeAdded(valueTypeIdentifier);
						}
					}
				}
			}

			if constexpr (ENABLE_ASSERTS)
			{
				for ([[maybe_unused]] UniquePtr<Style::Entry::Value>& storedValue : targetModifierValues.GetValues())
				{
					Assert(
						storedValue.IsInvalid() || !storedValue->Is<ngine::DataSource::PropertyIdentifier>(),
						"Failed to convert dynamic property!"
					);
				}
			}

			if (changedStyleValues.AreAnySet())
			{
				const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				OnStyleChangedInternal(
					sceneRegistry,
					pWindow,
					style,
					matchingModifiers,
					changedStyleValues,
					flexLayoutSceneData,
					gridLayoutSceneData,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData,
					textDrawableSceneData,
					imageDrawableSceneData,
					roundedRectangleDrawableSceneData,
					rectangleDrawableSceneData,
					circleDrawableSceneData,
					gridDrawableSceneData,
					lineDrawableSceneData
				);
			}
		}
	}

	bool Widget::RequiresResolvedDataSource(
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData,
		Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData
	) const
	{
		if (const Optional<Data::DataSource*> pDataSourceData = FindDataComponentOfType<Data::DataSource>(dataSourceSceneData))
		{
			if (pDataSourceData->IsInline())
			{
				return true;
			}
		}

		if (const Optional<Data::PropertySource*> pPropertySource = FindDataComponentOfType<Data::PropertySource>(propertySourceSceneData))
		{
			return true;
		}

		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			if (const Optional<Data::DataSourceEntry*> pDataSourceEntry = FindDataComponentOfType<Data::DataSourceEntry>(dataSourceEntrySceneData))
			{
				if (const Optional<Data::DataSource*> pParentDataSource = pParent->FindDataComponentOfType<Data::DataSource>(dataSourceSceneData))
				{
					Optional<Data::Layout*> pParentLayout = pParent->FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
					if (pParentLayout.IsInvalid())
					{
						pParentLayout = pParent->FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
					}

					const Optional<ngine::DataSource::GenericDataIndex> dataIndex = pDataSourceEntry->GetDataIndex();
					return pParentLayout.IsValid() && dataIndex.IsValid();
				}
			}
		}

		return false;
	}

	Optional<ngine::DataSource::Interface*> Widget::ResolveInlineDataSource(
		DataSourceProperties& dataSourceProperties, Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData
	)
	{
		if (const Optional<Data::DataSource*> pDataSourceData = FindDataComponentOfType<Data::DataSource>(dataSourceSceneData))
		{
			if (pDataSourceData->IsInline())
			{
				Assert(pDataSourceData->GetMaximumDataCount() == 1, "Inline data sources are only relevant with one possible element");
				ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
				if (Optional<ngine::DataSource::Interface*> pLockedDataSource = dataSourceCache.Get(pDataSourceData->GetDataSourceIdentifier()))
				{
					const ngine::DataSource::StateIdentifier dataSourceStateIdentifier = pDataSourceData->GetDataSourceStateIdentifier();
					const Optional<ngine::DataSource::State*> pState = dataSourceStateIdentifier.IsValid()
					                                                     ? dataSourceCache.GetState(dataSourceStateIdentifier)
					                                                     : Optional<ngine::DataSource::State*>{};

					const Optional<const Tag::Query*> pTagQuery = pDataSourceData->GetConstTagQuery();
					pLockedDataSource->LockRead();

					ngine::DataSource::CachedQuery cachedQuery;
					pLockedDataSource->CacheQuery(pTagQuery.IsValid() ? *pTagQuery : ngine::DataSource::Interface::Query{}, cachedQuery);

					if (cachedQuery.AreAnySet())
					{
						pLockedDataSource->IterateData(
							cachedQuery,
							[&dataSourceProperties, &dataSource = *pLockedDataSource, pState](const ngine::DataSource::Data data)
							{
								dataSourceProperties.Emplace(Widget::DataSourceEntryInfo{dataSource, data, pState});
							},
							Math::Range<ngine::DataSource::GenericDataIndex>::Make(0, 1)
						);
					}
					return pLockedDataSource;
				}
			}
		}
		return {};
	}

	void Widget::ResolvePropertySource(
		DataSourceProperties& dataSourceProperties, Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData
	)
	{
		if (const Optional<Data::PropertySource*> pPropertySource = FindDataComponentOfType<Data::PropertySource>(propertySourceSceneData))
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (const Optional<ngine::PropertySource::Interface*> pPropertySourceInterface = dataSourceCache.GetPropertySourceCache().Get(pPropertySource->GetPropertySourceIdentifier()))
			{
				dataSourceProperties.Emplace(*pPropertySourceInterface);
			}
		}
	}

	Optional<ngine::DataSource::Interface*> Widget::ResolveDataSourceEntry(
		DataSourceProperties& dataSourceProperties,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData
	)
	{
		Optional<Widget*> pParent = GetParentSafe();
		if (pParent.IsValid())
		{
			if (const Optional<Data::DataSourceEntry*> pDataSourceEntry = FindDataComponentOfType<Data::DataSourceEntry>(dataSourceEntrySceneData))
			{
				if (const Optional<Data::DataSource*> pParentDataSource = pParent->FindDataComponentOfType<Data::DataSource>(dataSourceSceneData))
				{
					ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
					Optional<ngine::DataSource::Interface*> pLockedDataSource = dataSourceCache.Get(pParentDataSource->GetDataSourceIdentifier());

					Optional<Data::Layout*> pParentLayout = pParent->FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData);
					if (pParentLayout.IsInvalid())
					{
						pParentLayout = pParent->FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData);
					}

					if (pLockedDataSource.IsValid())
					{
						pLockedDataSource->LockRead();
					}

					const Optional<ngine::DataSource::GenericDataIndex> dataIndex = pDataSourceEntry->GetDataIndex();

					if (pLockedDataSource.IsValid() && pParentLayout.IsValid() && dataIndex.IsValid())
					{
						const ngine::DataSource::StateIdentifier dataSourceStateIdentifier = pParentDataSource->GetDataSourceStateIdentifier();
						const Optional<ngine::DataSource::State*> pState = dataSourceStateIdentifier.IsValid()
						                                                     ? dataSourceCache.GetState(dataSourceStateIdentifier)
						                                                     : Optional<ngine::DataSource::State*>{};

						if (const ngine::DataSource::PropertyIdentifier sortedPropertyIdentifier = pParentDataSource->GetSortedPropertyIdentifier();
						    sortedPropertyIdentifier.IsValid())
						{
							pLockedDataSource->IterateData(
								*pParentLayout->GetCachedDataSortingQuery(),
								[&dataSourceProperties, &dataSource = *pLockedDataSource, pState](const ngine::DataSource::Data data)
								{
									dataSourceProperties.Emplace(Widget::DataSourceEntryInfo{dataSource, data, pState});
								},
								Math::Range<ngine::DataSource::GenericDataIndex>::Make(*dataIndex, 1)
							);
						}
						else
						{
							pLockedDataSource->IterateData(
								*pParentLayout->GetCachedDataQuery(),
								[&dataSourceProperties, &dataSource = *pLockedDataSource, pState](const ngine::DataSource::Data data)
								{
									dataSourceProperties.Emplace(Widget::DataSourceEntryInfo{dataSource, data, pState});
								},
								Math::Range<ngine::DataSource::GenericDataIndex>::Make(*dataIndex, 1)
							);
						}
					}

					return pLockedDataSource;
				}
			}
		}

		return {};
	}

	void Widget::UpdateFromResolvedDataSourceInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const DataSourceProperties& dataSourceProperties,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
		Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData,
		Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		if (const Optional<Data::Modifiers*> pModifiers = FindDataComponentOfType<Data::Modifiers>(modifiersSceneData);
		    pModifiers.IsValid() && pModifiers->HasDynamicProperties())
		{
			pModifiers->UpdateFromDataSourceInternal(*this, dataSourceProperties);
		}
		UpdateDynamicTagQueryFromDataSourceInternal(
			sceneRegistry,
			dataSourceProperties,
			flexLayoutSceneData,
			gridLayoutSceneData,
			dataSourceSceneData
		);
		UpdateDynamicStyleFromDataSourceInternal(
			sceneRegistry,
			pWindow,
			dataSourceProperties,
			flexLayoutSceneData,
			gridLayoutSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);

		if (const Optional<Data::EventData*> pEventData = FindDataComponentOfType<Data::EventData>(sceneRegistry))
		{
			pEventData->UpdateFromDataSource(*this, dataSourceProperties);
		}

		if (const Optional<Data::ContextMenuEntries*> pContextMenuEntries = FindDataComponentOfType<Data::ContextMenuEntries>(sceneRegistry))
		{
			pContextMenuEntries->UpdateFromDataSource(*this, dataSourceProperties);
		}

		if (m_windowTitlePropertyIdentifier.IsValid() && pWindow.IsValid())
		{
			const DataSource::PropertyValue windowTitleValue = dataSourceProperties.GetDataProperty(m_windowTitlePropertyIdentifier);
			if (windowTitleValue.HasValue())
			{
				if (const Optional<const UnicodeString*> windowTitle = windowTitleValue.Get<UnicodeString>())
				{
					pWindow->SetTitle(NativeString(*windowTitle));
				}
			}
		}

		if (m_windowUriPropertyIdentifier.IsValid() && pWindow.IsValid())
		{
			const DataSource::PropertyValue windowUriValue = dataSourceProperties.GetDataProperty(m_windowUriPropertyIdentifier);
			if (windowUriValue.HasValue())
			{
				if (const Optional<const IO::URI*> windowUri = windowUriValue.Get<IO::URI>())
				{
					pWindow->SetUri(*windowUri);
				}
			}
		}

		const uint16 childCount = GetChildCount();
		for (uint16 childIndex = 0; childIndex < childCount; ++childIndex)
		{
			Optional<Widget*> pChildWidget;
			{
				const Widget::ChildView children = GetChildren();
				if (childIndex < children.GetSize())
				{
					pChildWidget = children[childIndex];
				}
				else
				{
					break;
				}
			}
			Widget& child = *pChildWidget;
			if (child.RequiresResolvedDataSource(
						flexLayoutSceneData,
						gridLayoutSceneData,
						dataSourceSceneData,
						dataSourceEntrySceneData,
						propertySourceSceneData
					))
			{
				DataSourceProperties childDataSourceProperties;
				InlineVector<ReferenceWrapper<ngine::DataSource::Interface>, 6> lockedDataSources;

				// Top priority: this widget's data source entry
				if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = child.ResolveDataSourceEntry(childDataSourceProperties, flexLayoutSceneData, gridLayoutSceneData, dataSourceSceneData, dataSourceEntrySceneData))
				{
					lockedDataSources.EmplaceBack(*pLockedDataSource);
				}

				// Second priority: widget's inline data source
				if (const Optional<ngine::DataSource::Interface*> pLockedDataSource = child.ResolveInlineDataSource(childDataSourceProperties, dataSourceSceneData))
				{
					lockedDataSources.EmplaceBack(*pLockedDataSource);
				}

				// Third priority: widget's property source
				child.ResolvePropertySource(childDataSourceProperties, propertySourceSceneData);

				childDataSourceProperties.CopyFrom(dataSourceProperties);

				child.UpdateFromResolvedDataSourceInternal(
					sceneRegistry,
					pWindow,
					childDataSourceProperties,
					flexLayoutSceneData,
					gridLayoutSceneData,
					dataSourceSceneData,
					dataSourceEntrySceneData,
					propertySourceSceneData,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData,
					textDrawableSceneData,
					imageDrawableSceneData,
					roundedRectangleDrawableSceneData,
					rectangleDrawableSceneData,
					circleDrawableSceneData,
					gridDrawableSceneData,
					lineDrawableSceneData
				);

				for (ngine::DataSource::Interface& lockedDataSource : lockedDataSources)
				{
					lockedDataSource.UnlockRead();
				}
			}
			else
			{
				child.UpdateFromResolvedDataSourceInternal(
					sceneRegistry,
					pWindow,
					dataSourceProperties,
					flexLayoutSceneData,
					gridLayoutSceneData,
					dataSourceSceneData,
					dataSourceEntrySceneData,
					propertySourceSceneData,
					externalStyleSceneData,
					inlineStyleSceneData,
					dynamicStyleSceneData,
					modifiersSceneData,
					textDrawableSceneData,
					imageDrawableSceneData,
					roundedRectangleDrawableSceneData,
					rectangleDrawableSceneData,
					circleDrawableSceneData,
					gridDrawableSceneData,
					lineDrawableSceneData
				);
			}
		}
	}

	Memory::CallbackResult Widget::IterateAttachedItems(
		const LocalWidgetCoordinate coordinate,
		const ArrayView<const Reflection::TypeDefinition> allowedTypes,
		CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
	) const
	{
		auto checkAttachedItem = [](
															 const Optional<const Style::EntryValue*> attachedItem,
															 const ArrayView<const Reflection::TypeDefinition> allowedTypes,
															 CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
														 ) -> Optional<Memory::CallbackResult>
		{
			if (attachedItem.IsValid())
			{
				if (const Optional<const Asset::Guid*> assetGuid = attachedItem->Get<Asset::Guid>())
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
					{
						{
							const Guid assetTypeGuid = assetManager.GetAssetTypeGuid(*assetGuid);
							if (assetTypeGuid.IsValid())
							{
								Asset::Reference assetReference{*assetGuid, assetTypeGuid};
								return callback(assetReference);
							}
						}

						{
							const Guid assetTypeGuid = assetManager.GetAssetLibrary().GetAssetTypeGuid(*assetGuid);
							if (assetTypeGuid.IsValid())
							{
								Asset::LibraryReference assetReference{*assetGuid, assetTypeGuid};
								return callback(assetReference);
							}
						}
					}
				}
				else if (const Optional<const Entity::ComponentSoftReference*> componentSoftReference = attachedItem->Get<Entity::ComponentSoftReference>())
				{
					if (allowedTypes.Contains(Reflection::TypeDefinition::Get<Entity::ComponentSoftReference>()))
					{
						return callback(*componentSoftReference);
					}
				}
				return Memory::CallbackResult::Break;
			}
			return Invalid;
		};

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		Optional<Memory::CallbackResult> callbackResult;
		if (callbackResult = checkAttachedItem(
					style.Find(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers),
					allowedTypes,
					Forward<decltype(callback)>(callback)
				);
		    callbackResult.IsValid())
		{
			return *callbackResult;
		}
		else if (callbackResult = checkAttachedItem(
							 style.Find(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers),
							 allowedTypes,
							 Forward<decltype(callback)>(callback)
						 );
		         callbackResult.IsValid())
		{
			return *callbackResult;
		}
		else if (callbackResult = checkAttachedItem(
							 style.Find(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers),
							 allowedTypes,
							 Forward<decltype(callback)>(callback)
						 );
		         callbackResult.IsValid())
		{
			return *callbackResult;
		}
		else if (callbackResult = checkAttachedItem(
							 style.Find(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers),
							 allowedTypes,
							 Forward<decltype(callback)>(callback)
						 );
		         callbackResult.IsValid())
		{
			return *callbackResult;
		}
		else
		{
			Optional<Widget*> pParent = GetParentSafe();
			if (pParent != nullptr)
			{
				return pParent->IterateAttachedItems(
					pParent->ConvertWindowToLocalCoordinates(ConvertLocalToWindowCoordinates(coordinate)),
					allowedTypes,
					Forward<decltype(callback)>(callback)
				);
			}

			return Memory::CallbackResult::Continue;
		}
	}

	Memory::CallbackResult Widget::IterateAttachedItemsAsync(
		const LocalWidgetCoordinate coordinate,
		const ArrayView<const Reflection::TypeDefinition> allowedTypes,
		CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
	) const
	{
		Optional<Widget*> pParent = GetParentSafe();
		if (pParent != nullptr)
		{
			return pParent->IterateAttachedItemsAsync(
				pParent->ConvertWindowToLocalCoordinates(ConvertLocalToWindowCoordinates(coordinate)),
				allowedTypes,
				Forward<decltype(callback)>(callback)
			);
		}

		return Memory::CallbackResult::Continue;
	}

	bool Widget::LoadResources(Entity::SceneRegistry& sceneRegistry, const Optional<Threading::JobBatch*> pJobBatchOut)
	{
		if (!m_flags.FetchOr(Flags::HasStartedLoadingResources).IsSet(Flags::HasStartedLoadingResources))
		{
			LoadResourcesResult result = TryLoadResources(sceneRegistry);
			switch (result.m_status)
			{
				case LoadResourcesResult::Status::FailedRetryLater:
				{
					m_flags &= ~Flags::HasStartedLoadingResources;
					return false;
				}
				case LoadResourcesResult::Status::Invalid:
				{
					return false;
				}
				case LoadResourcesResult::Status::QueueJobsAndTryAgain:
				{
					result.m_jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
						[this](Threading::JobRunnerThread&)
						{
							m_flags |= Flags::HasLoadedResources | Flags::AreResourcesUpToDate;
						},
						Threading::JobPriority::UserInterfaceLoading
					));
					if (pJobBatchOut.IsValid())
					{
						*pJobBatchOut = result.m_jobBatch;
					}
					else
					{
						System::Get<Threading::JobManager>().Queue(result.m_jobBatch, Threading::JobPriority::UserInterfaceAction);
					}
					return false;
				}
				case LoadResourcesResult::Status::Valid:
					m_flags |= Flags::HasLoadedResources | Flags::AreResourcesUpToDate;
					return true;
			}
		}

		return false;
	}

	bool Widget::TryReloadResources(Entity::SceneRegistry& sceneRegistry)
	{
		const EnumFlags<Flags> previousFlags =
			m_flags.FetchAnd(~(Flags::HasLoadedResources | Flags::AreResourcesUpToDate | Flags::HasStartedLoadingResources));
		if (previousFlags.AreAnySet(Flags::HasLoadedResources | Flags::AreResourcesUpToDate))
		{
			return LoadResources(sceneRegistry);
		}
		return false;
	}

	Optional<Input::Monitor*> Widget::GetFocusedInputMonitorAtCoordinates(const WindowCoordinate coordinate)
	{
		if (m_flags.IsSet(Flags::BlockPointerInputs))
		{
			return Invalid;
		}

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::EventData*> pEventData = FindDataComponentOfType<Data::EventData>(sceneRegistry);
		    pEventData.IsValid() && pEventData->HasEvents(EventType::Tap))
		{
			return Invalid;
		}

		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			return pParent->GetFocusedInputMonitorAtCoordinates(coordinate);
		}

		return Invalid;
	}

	bool Widget::NotifyEvent(Entity::SceneRegistry& sceneRegistry, const EventType eventType)
	{
		if (Optional<Data::EventData*> pEventData = FindDataComponentOfType<Data::EventData>(sceneRegistry);
		    pEventData.IsValid() && pEventData->HasEvents(eventType))
		{
			Assert(Threading::JobRunnerThread::GetCurrent().IsValid(), "Should be executed on job runner");
			pEventData->NotifyAll(*this, eventType);
			return true;
		}
		else
		{
			return false;
		}
	}

	CursorResult
	Widget::HandleStartTap(const Input::DeviceIdentifier deviceIdentifier, const uint8 fingerCount, const LocalWidgetCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnStartTap(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult result = OnStartTap(deviceIdentifier, fingerCount, coordinate);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}

		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleStartTap(deviceIdentifier, fingerCount, ConvertLocalToParentCoordinates(coordinate));
			}
		}

		return {};
	}

	CursorResult
	Widget::HandleEndTap(const Input::DeviceIdentifier deviceIdentifier, const uint8 fingerCount, const LocalWidgetCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			const bool notifiedEvent = NotifyEvent(sceneRegistry, EventType::Tap);

			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnEndTap(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult result = OnEndTap(deviceIdentifier, fingerCount, coordinate);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (!notifiedEvent)
			{
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry style = GetStyle(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				Optional<const Style::EntryValue*> attachedItem = style.Find(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers);
				if (attachedItem.IsInvalid())
				{
					attachedItem = style.Find(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers);
				}

				if (attachedItem.IsValid())
				{
					if (const Optional<const Asset::Guid*> assetGuid = attachedItem->Get<Asset::Guid>())
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						bool isValid = assetManager.HasAsset(*assetGuid);
						if (!isValid)
						{
							// TODO: Change attached items from Asset::Guid to Asset::Reference and LibraryReference
							const Asset::Identifier importedAssetIdentifier =
								assetManager.Import(Asset::LibraryReference{*assetGuid, assetManager.GetAssetLibrary().GetAssetTypeGuid(*assetGuid)});
							isValid = importedAssetIdentifier.IsValid();
						}

						Assert(isValid);
						if (LIKELY(isValid))
						{
							Assert(Threading::JobRunnerThread::GetCurrent().IsValid(), "Should be executed on job runner");

							Asset::Mask assetsMask;
							assetsMask.Set(assetManager.GetAssetIdentifier(*assetGuid));

							Context::EventManager eventManager(*this, sceneRegistry);
							eventManager.Notify(
								"6615f19d-3697-463e-a12d-27dd4f65f84e"_asset,
								Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Asset::Mask>(assetsMask))
							);
						}
					}
					else if (const Optional<const Entity::ComponentSoftReference*> componentSoftReference = attachedItem->Get<Entity::ComponentSoftReference>())
					{
						Assert(Threading::JobRunnerThread::GetCurrent().IsValid(), "Should be executed on job runner");
						Entity::ComponentSoftReferences componentSoftReferences;
						componentSoftReferences.Add(*componentSoftReference);

						Context::EventManager eventManager(*this, sceneRegistry);
						eventManager.Notify(
							"5AEDA997-6A7E-4FBD-B736-E3C959508017"_asset,
							Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Entity::ComponentSoftReferences>(componentSoftReferences))
						);
					}
				}
			}

			if (notifiedEvent)
			{
				return CursorResult{*this};
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleEndTap(deviceIdentifier, fingerCount, ConvertLocalToParentCoordinates(coordinate));
			}
		}

		return {};
	}

	CursorResult Widget::HandleCancelTap(const Input::DeviceIdentifier deviceIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnCancelTap(*this, deviceIdentifier);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult result = OnCancelTap(deviceIdentifier);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleCancelTap(deviceIdentifier);
			}
		}

		return {};
	}

	CursorResult Widget::HandleDoubleTap(const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			const bool notifiedEvent = NotifyEvent(sceneRegistry, EventType::DoubleTap);

			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnDoubleTap(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult cursorResult = OnDoubleTap(deviceIdentifier, coordinate);
			if (cursorResult.pHandledWidget != nullptr)
			{
				return cursorResult;
			}

			if (!notifiedEvent)
			{
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry style = GetStyle(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				Optional<const Style::EntryValue*> attachedItem = style.Find(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers);
				if (attachedItem.IsInvalid())
				{
					attachedItem = style.Find(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers);
				}

				if (attachedItem.IsValid())
				{
					if (const Optional<const Asset::Guid*> assetGuid = attachedItem->Get<Asset::Guid>())
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						bool isValid = assetManager.HasAsset(*assetGuid);
						if (!isValid)
						{
							// TODO: Change attached items from Asset::Guid to Asset::Reference and LibraryReference
							const Asset::Identifier importedAssetIdentifier =
								assetManager.Import(Asset::LibraryReference{*assetGuid, assetManager.GetAssetLibrary().GetAssetTypeGuid(*assetGuid)});
							isValid = importedAssetIdentifier.IsValid();
						}

						Assert(isValid);
						if (LIKELY(isValid))
						{
							Assert(Threading::JobRunnerThread::GetCurrent().IsValid(), "Should be executed on job runner");

							Asset::Mask assetsMask;
							assetsMask.Set(assetManager.GetAssetIdentifier(*assetGuid));

							Context::EventManager eventManager(*this, sceneRegistry);
							eventManager.Notify(
								"a0b620ee-564b-4e18-9a14-76204bb56ee5"_asset,
								Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Asset::Mask>(assetsMask))
							);
						}
					}
					else if (const Optional<const Entity::ComponentSoftReference*> componentSoftReference = attachedItem->Get<Entity::ComponentSoftReference>())
					{
						Assert(Threading::JobRunnerThread::GetCurrent().IsValid(), "Should be executed on job runner");
						Entity::ComponentSoftReferences componentSoftReferences;
						componentSoftReferences.Add(*componentSoftReference);

						Context::EventManager eventManager(*this, sceneRegistry);
						eventManager.Notify(
							"7005CBEA-F3E0-4A02-B110-D3F1B1CE29B1"_asset,
							Scripting::VM::DynamicInvoke::LoadArgument(ReferenceWrapper<const Entity::ComponentSoftReferences>(componentSoftReferences))
						);
					}
				}
			}

			if (notifiedEvent)
			{
				return CursorResult{*this};
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleDoubleTap(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate));
			}
		}

		return {};
	}

	PanResult Widget::HandleStartLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		const LocalWidgetCoordinate coordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (fingerCount == 1)
			{
				Input::Manager& inputManager = System::Get<Input::Manager>();
				const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
				if (deviceTypeIdentifier == inputManager.GetTouchscreenDeviceTypeIdentifier())
				{
					if (HandleSpawnContextMenu(coordinate, sceneRegistry))
					{
						return PanResult{*this};
					}
				}
				else if (HasContextMenu(sceneRegistry))
				{
					return PanResult{*this};
				}
			}

			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				PanResult result = pInputComponent->OnStartLongPress(*this, deviceIdentifier, coordinate, fingerCount, touchRadius);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const PanResult result = OnStartLongPress(deviceIdentifier, coordinate, fingerCount, touchRadius);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleStartLongPress(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), fingerCount, touchRadius);
			}
		}

		return {};
	}

	CursorResult Widget::HandleMoveLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		const LocalWidgetCoordinate coordinate,
		const uint8 fingerCount,
		const Optional<uint16> touchRadius
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnMoveLongPress(*this, deviceIdentifier, coordinate, fingerCount, touchRadius);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult cursorResult = OnMoveLongPress(deviceIdentifier, coordinate, fingerCount, touchRadius);
			if (cursorResult.pHandledWidget != nullptr)
			{
				return cursorResult;
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleMoveLongPress(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), fingerCount, touchRadius);
			}
		}

		return {};
	}

	CursorResult Widget::HandleEndLongPress(const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
			if (deviceTypeIdentifier == inputManager.GetMouseDeviceTypeIdentifier())
			{
				if (HandleSpawnContextMenu(coordinate, sceneRegistry))
				{
					return CursorResult{*this};
				}
			}

			const bool notifiedEvent = NotifyEvent(sceneRegistry, EventType::LongPress);

			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnEndLongPress(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult cursorResult = OnEndLongPress(deviceIdentifier, coordinate);
			if (cursorResult.pHandledWidget != nullptr)
			{
				return cursorResult;
			}

			if (notifiedEvent)
			{
				return CursorResult{*this};
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleEndLongPress(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate));
			}
		}

		return {};
	}

	CursorResult Widget::HandleCancelLongPress(const Input::DeviceIdentifier deviceIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnCancelLongPress(*this, deviceIdentifier);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult cursorResult = OnCancelLongPress(deviceIdentifier);
			if (cursorResult.pHandledWidget != nullptr)
			{
				return cursorResult;
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleCancelLongPress(deviceIdentifier);
			}
		}

		return {};
	}

	PanResult Widget::HandleStartPan(
		const Input::DeviceIdentifier deviceIdentifier,
		const LocalWidgetCoordinate coordinate,
		const Math::Vector2f velocity,
		const Optional<uint16> touchRadius,
		SetCursorCallback& pointer
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				PanResult result = pInputComponent->OnStartPan(*this, deviceIdentifier, coordinate, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			PanResult result = OnStartPan(deviceIdentifier, coordinate, velocity, touchRadius, pointer);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnStartPan(*this, deviceIdentifier, coordinate, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnStartPan(*this, deviceIdentifier, coordinate, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleStartPan(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), velocity, touchRadius, pointer);
			}
		}

		return {};
	}

	CursorResult Widget::HandleMovePan(
		const Input::DeviceIdentifier deviceIdentifier,
		const LocalWidgetCoordinate coordinate,
		const Math::Vector2i delta,
		const Math::Vector2f velocity,
		[[maybe_unused]] const Optional<uint16> touchRadius,
		SetCursorCallback& pointer
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnMovePan(*this, deviceIdentifier, coordinate, delta, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnMovePan(deviceIdentifier, coordinate, delta, velocity, touchRadius, pointer);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnMovePan(*this, deviceIdentifier, coordinate, delta, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnMovePan(*this, deviceIdentifier, coordinate, delta, velocity, touchRadius, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleMovePan(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), delta, velocity, touchRadius, pointer);
			}
		}
		return {};
	}

	CursorResult Widget::HandleEndPan(
		const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2f velocity
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnEndPan(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnEndPan(deviceIdentifier, coordinate, velocity);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnEndPan(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnEndPan(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleEndPan(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), velocity);
			}
		}

		return {};
	}

	CursorResult Widget::HandleCancelPan(const Input::DeviceIdentifier deviceIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnCancelPan(*this, deviceIdentifier);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult result = OnCancelPan(deviceIdentifier);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleCancelPan(deviceIdentifier);
			}
		}
		return {};
	}

	CursorResult Widget::HandleHover(
		const Input::DeviceIdentifier deviceIdentifier,
		const LocalWidgetCoordinate coordinate,
		const Math::Vector2i delta,
		SetCursorCallback& pointer
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnHover(*this, deviceIdentifier, coordinate, delta, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			const CursorResult cursorResult = OnHover(deviceIdentifier, coordinate, delta, pointer);
			if (cursorResult.pHandledWidget != nullptr)
			{
				return cursorResult;
			}

			if (const Optional<Data::EventData*> pEventData = FindDataComponentOfType<Data::EventData>(sceneRegistry);
			    pEventData.IsValid() && pEventData->HasEvents(EventType::Tap))
			{
				// This widget can be tapped, so show the correct cursor
				pointer.SetCursor(Rendering::CursorType::Hand);
				return {*this};
			}

			if (GetStyle(sceneRegistry).HasAnyModifiers(Style::Modifier::Hover))
			{
				return {*this};
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleHover(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), delta, pointer);
			}
		}
		return {};
	}

	CursorResult Widget::HandleStartScroll(
		const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2i delta
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnStartScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnStartScroll(deviceIdentifier, coordinate, delta);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnStartScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnStartScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleStartScroll(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), delta);
			}
		}

		return {};
	}

	CursorResult
	Widget::HandleScroll(const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2i delta)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnScroll(deviceIdentifier, coordinate, delta);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnScroll(*this, deviceIdentifier, coordinate, delta);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleScroll(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), delta);
			}
		}

		return {};
	}

	CursorResult Widget::HandleEndScroll(
		const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate, const Math::Vector2f velocity
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnEndScroll(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnEndScroll(deviceIdentifier, coordinate, velocity);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnEndScroll(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnEndScroll(*this, deviceIdentifier, coordinate, velocity);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleEndScroll(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate), velocity);
			}
		}

		return {};
	}

	CursorResult Widget::HandleCancelScroll(const Input::DeviceIdentifier deviceIdentifier, const LocalWidgetCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				CursorResult result = pInputComponent->OnCancelScroll(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			CursorResult result = OnCancelScroll(deviceIdentifier, coordinate);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}

			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				result = pFlexLayoutComponent->OnCancelScroll(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				result = pGridLayoutComponent->OnCancelScroll(*this, deviceIdentifier, coordinate);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}
		}
		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->HandleCancelScroll(deviceIdentifier, ConvertLocalToParentCoordinates(coordinate));
			}
		}

		return {};
	}

	DragAndDropResult
	Widget::HandleStartDragWidgetOverThis(const LocalWidgetCoordinate coordinate, const Widget& otherWidget, SetCursorCallback& pointer)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				DragAndDropResult result = pInputComponent->OnStartDragWidgetOverThis(*this, coordinate, otherWidget, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			DragAndDropResult result = OnStartDragWidgetOverThis(coordinate, otherWidget, pointer);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (Optional<Widget*> pWidget = GetParentSafe())
		{
			return pWidget->HandleStartDragWidgetOverThis(ConvertLocalToParentCoordinates(coordinate), otherWidget, pointer);
		}
		return {};
	}

	void Widget::HandleCancelDragWidgetOverThis()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				DragAndDropResult result = pInputComponent->OnCancelDragWidgetOverThis(*this);
				if (result.pHandledWidget != nullptr)
				{
					return;
				}
			}

			DragAndDropResult result = OnCancelDragWidgetOverThis();
			if (result.pHandledWidget != nullptr)
			{
				return;
			}
		}
		if (Optional<Widget*> pWidget = GetParentSafe())
		{
			return pWidget->HandleCancelDragWidgetOverThis();
		}
	}

	DragAndDropResult
	Widget::HandleMoveDragWidgetOverThis(const LocalWidgetCoordinate coordinate, const Widget& otherWidget, SetCursorCallback& pointer)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				DragAndDropResult result = pInputComponent->OnMoveDragWidgetOverThis(*this, coordinate, otherWidget, pointer);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			DragAndDropResult result = OnMoveDragWidgetOverThis(coordinate, otherWidget, pointer);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (Optional<Widget*> pWidget = GetParentSafe())
		{
			return pWidget->HandleMoveDragWidgetOverThis(ConvertLocalToParentCoordinates(coordinate), otherWidget, pointer);
		}
		return {};
	}

	DragAndDropResult Widget::HandleReleaseDragWidgetOverThis(const LocalWidgetCoordinate coordinate, const Widget& otherWidget)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (IsEnabled(sceneRegistry))
		{
			if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
			{
				DragAndDropResult result = pInputComponent->OnReleaseDragWidgetOverThis(*this, coordinate, otherWidget);
				if (result.pHandledWidget != nullptr)
				{
					return result;
				}
			}

			DragAndDropResult result = OnReleaseDragWidgetOverThis(coordinate, otherWidget);
			if (result.pHandledWidget != nullptr)
			{
				return result;
			}
		}
		if (Optional<Widget*> pWidget = GetParentSafe())
		{
			return pWidget->HandleReleaseDragWidgetOverThis(ConvertLocalToParentCoordinates(coordinate), otherWidget);
		}
		return {};
	}

	void Widget::HandleTextInput(const ConstUnicodeStringView input)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnTextInput(*this, input);
		}
		OnTextInput(input);
	}

	void Widget::HandleCopyToPasteboard()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnCopyToPasteboard(*this);
		}
	}

	void Widget::HandlePasteFromPasteboard()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnPasteFromPasteboard(*this);
		}
	}

	void Widget::HandleMoveTextCursor(const EnumFlags<ngine::Input::MoveTextCursorFlags> flags)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnMoveTextCursor(*this, flags);
		}
		OnMoveTextCursor(flags);
	}

	void Widget::HandleApplyTextInput()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnApplyTextInput(*this);
		}
		OnApplyTextInput();
	}

	void Widget::HandleAbortTextInput()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnAbortTextInput(*this);
		}
		OnAbortTextInput();
	}

	void Widget::HandleDeleteTextInput(const ngine::Input::DeleteTextType type)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnDeleteTextInput(*this, type);
		}
		OnDeleteTextInput(type);
	}

	void Widget::HandleTabForward()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnTabForward(*this);
		}
	}

	void Widget::HandleTabBack()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			pInputComponent->OnTabBack(*this);
		}
	}

	bool Widget::HasEditableText() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			return pInputComponent->HasEditableText(*this);
		}
		return false;
	}

	Style::CombinedEntry Widget::GetStyle() const
	{
		return GetStyle(GetSceneRegistry());
	}

	Style::CombinedEntry Widget::GetStyle(Entity::SceneRegistry& sceneRegistry) const
	{
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::ExternalStyle>();
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Data::DynamicStyle>();

		return GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
	}

	Style::CombinedEntry Widget::GetStyle(
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData
	) const
	{
		const Optional<Data::ExternalStyle*> pExternalStyle = FindDataComponentOfType<Data::ExternalStyle>(externalStyleSceneData);
		const Optional<const Widgets::Style::Entry*> pExternalStyleEntry = pExternalStyle.IsValid() ? pExternalStyle->GetEntry()
		                                                                                            : Optional<const Widgets::Style::Entry*>{};
		const Style::Entry& externalStyleEntry = pExternalStyleEntry.IsValid() ? *pExternalStyleEntry : Style::Entry::GetDummy();

		if (const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(inlineStyleSceneData))
		{
			if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData))
			{
				return {pDynamicStyle->GetEntry().m_populatedEntry, pInlineStyle->GetEntry(), externalStyleEntry};
			}
			else
			{
				return {pInlineStyle->GetEntry(), externalStyleEntry};
			}
		}
		else if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(dynamicStyleSceneData))
		{
			return {pDynamicStyle->GetEntry().m_populatedEntry, externalStyleEntry};
		}
		else
		{
			return externalStyleEntry;
		}
	}

	void Widget::ChangeInlineStyle(Style::Entry&& inlineStyle, Entity::SceneRegistry& sceneRegistry)
	{
		ChangedStyleValues changedStyleValues = inlineStyle.GetValueTypeMask();

		if (const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(sceneRegistry))
		{
			pInlineStyle->SetEntry(*this, Forward<Style::Entry>(inlineStyle));
		}
		else
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::InlineStyle>(
				sceneRegistry,
				Data::InlineStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, sceneRegistry},
					UniquePtr<Style::Entry>::Make(Forward<Style::Entry>(inlineStyle))
				}
			);
			Assert(jobBatch.IsInvalid());

			const Style::CombinedEntry combinedStyle = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
			OnStyleChangedInternal(sceneRegistry, GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
		}
	}

	void Widget::ChangeInlineStyle(Style::Entry&& inlineStyle)
	{
		ChangeInlineStyle(Forward<Style::Entry>(inlineStyle), GetSceneRegistry());
	}

	void Widget::EmplaceInlineStyle(Style::Value&& value, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers)
	{
		if (const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(sceneRegistry))
		{
			pInlineStyle->EmplaceValue(*this, Forward<Style::Value>(value), modifiers);
		}
		else
		{
			Widget::ChangedStyleValues changedStyleValues;
			changedStyleValues.Set((uint8)value.GetTypeIdentifier());

			UniquePtr<Style::Entry> pEntry = UniquePtr<Style::Entry>::Make();
			Style::Entry::ModifierValues& modifier = pEntry->FindOrEmplaceExactModifierMatch(modifiers);
			modifier.Emplace(Forward<Style::Value>(value));
			pEntry->OnValueTypeAdded(value.GetTypeIdentifier());

			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::InlineStyle>(
				sceneRegistry,
				Data::InlineStyle::Initializer{Data::Component::Initializer{jobBatch, *this, sceneRegistry}, Move(pEntry)}
			);
			Assert(jobBatch.IsInvalid());

			const Style::CombinedEntry combinedStyle = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
			OnStyleChangedInternal(sceneRegistry, GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
		}
	}

	void Widget::EmplaceInlineStyle(Style::Value&& value, const EnumFlags<Style::Modifier> modifiers)
	{
		EmplaceInlineStyle(Forward<Style::Value>(value), GetSceneRegistry(), modifiers);
	}

	void Widget::ClearInlineStyle(
		const Style::ValueTypeIdentifier valueType, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers
	)
	{
		if (const Optional<Data::InlineStyle*> pInlineStyle = FindDataComponentOfType<Data::InlineStyle>(sceneRegistry))
		{
			pInlineStyle->ClearValue(*this, valueType, modifiers);
		}
	}

	void Widget::ClearInlineStyle(const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers)
	{
		ClearInlineStyle(valueType, GetSceneRegistry(), modifiers);
	}

	void Widget::ChangeDynamicStyle(Style::Entry&& entry, Entity::SceneRegistry& sceneRegistry)
	{
		ChangedStyleValues changedStyleValues = entry.GetValueTypeMask();

		if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(sceneRegistry))
		{
			pDynamicStyle->SetEntry(*this, Forward<Style::Entry>(entry));
		}
		else
		{
			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::DynamicStyle>(
				sceneRegistry,
				Data::DynamicStyle::Initializer{
					Data::Component::Initializer{jobBatch, *this, sceneRegistry},
					UniquePtr<Style::DynamicEntry>::Make(Style::DynamicEntry{Forward<Style::Entry>(entry)})
				}
			);
			Assert(jobBatch.IsInvalid());

			const Style::CombinedEntry combinedStyle = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
			OnStyleChangedInternal(sceneRegistry, GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
		}
	}

	void Widget::ChangeDynamicStyle(Style::Entry&& entry)
	{
		ChangeDynamicStyle(Forward<Style::Entry>(entry), GetSceneRegistry());
	}

	void Widget::EmplaceDynamicStyle(Style::Value&& value, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers)
	{
		if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(sceneRegistry))
		{
			pDynamicStyle->EmplaceValue(*this, Forward<Style::Value>(value), modifiers);
		}
		else
		{
			Widget::ChangedStyleValues changedStyleValues;
			changedStyleValues.Set((uint8)value.GetTypeIdentifier());

			UniquePtr<Style::DynamicEntry> pEntry = UniquePtr<Style::DynamicEntry>::Make();
			Style::Entry::ModifierValues& modifier = pEntry->m_dynamicEntry.FindOrEmplaceExactModifierMatch(modifiers);
			modifier.Emplace(Forward<Style::Value>(value));
			pEntry->m_dynamicEntry.OnDynamicValueTypeAdded(value.GetTypeIdentifier());
			pEntry->m_dynamicEntry.OnValueTypeAdded(value.GetTypeIdentifier());

			Threading::JobBatch jobBatch;
			CreateDataComponent<Data::DynamicStyle>(
				sceneRegistry,
				Data::DynamicStyle::Initializer{Data::Component::Initializer{jobBatch, *this, sceneRegistry}, Move(pEntry)}
			);
			Assert(jobBatch.IsInvalid());

			const Style::CombinedEntry combinedStyle = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
			OnStyleChangedInternal(sceneRegistry, GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
		}
	}

	void Widget::EmplaceDynamicStyle(Style::Value&& value, const EnumFlags<Style::Modifier> modifiers)
	{
		EmplaceDynamicStyle(Forward<Style::Value>(value), GetSceneRegistry(), modifiers);
	}

	void Widget::ClearDynamicStyle(
		const Style::ValueTypeIdentifier valueType, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers
	)
	{
		if (const Optional<Data::DynamicStyle*> pDynamicStyle = FindDataComponentOfType<Data::DynamicStyle>(sceneRegistry))
		{
			pDynamicStyle->ClearValue(*this, valueType, modifiers);
		}
	}

	void Widget::ClearDynamicStyle(const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers)
	{
		ClearDynamicStyle(valueType, GetSceneRegistry(), modifiers);
	}

	void Widget::OnStyleChangedInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues
	)
	{
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::GridLayout>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		OnStyleChangedInternal(
			sceneRegistry,
			pWindow,
			style,
			matchingModifiers,
			changedStyleValues,
			flexLayoutSceneData,
			gridLayoutSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);
	}

	void Widget::OnStyleChangedInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedStyleValues,
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
	)
	{
		const EnumFlags<DepthInfo::ChangeFlags> depthInfoChangeFlags = m_depthInfo.OnStyleChanged(style, matchingModifiers);
		if (depthInfoChangeFlags.AreAnySet(DepthInfo::ChangeFlags::ShouldRecalculateSubsequentDepthInfo))
		{
			RecalculateSubsequentDepthInfo();
		}

		if (pWindow.IsValid())
		{
			const Rendering::ScreenProperties screenProperties = pWindow->GetCurrentScreenProperties();
			ApplyStyleInternal(
				sceneRegistry,
				*pWindow,
				screenProperties,
				externalStyleSceneData,
				inlineStyleSceneData,
				dynamicStyleSceneData,
				modifiersSceneData,
				textDrawableSceneData,
				imageDrawableSceneData,
				roundedRectangleDrawableSceneData,
				rectangleDrawableSceneData,
				circleDrawableSceneData,
				gridDrawableSceneData,
				lineDrawableSceneData
			);
		}

		OnStyleChanged(style, matchingModifiers, changedStyleValues);

		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = FindDataComponentOfType<Data::FlexLayout>(flexLayoutSceneData))
		{
			if (pFlexLayoutComponent->OnStyleChanged(*this, sceneRegistry, style, matchingModifiers, changedStyleValues))
			{
				RecalculateHierarchy(sceneRegistry);
			}
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = FindDataComponentOfType<Data::GridLayout>(gridLayoutSceneData))
		{
			if (pGridLayoutComponent->OnStyleChanged(*this, sceneRegistry, style, matchingModifiers, changedStyleValues))
			{
				RecalculateHierarchy(sceneRegistry);
			}
		}

		if (const Optional<Data::Drawable*> pDrawableComponent = FindDrawable(textDrawableSceneData, imageDrawableSceneData, roundedRectangleDrawableSceneData, rectangleDrawableSceneData, circleDrawableSceneData, gridDrawableSceneData, lineDrawableSceneData))
		{
			pDrawableComponent->OnStyleChanged(*this, style, matchingModifiers, changedStyleValues);
		}

		if (Entity::HierarchyComponentBase::GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsConstructing) && changedStyleValues.IsSet((uint8)Style::ValueTypeIdentifier::AttachedDocumentAsset) && Implements<Document::Widget>(sceneRegistry))
		{
			if (Optional<const Style::EntryValue*> pAttachedAsset = style.Find(Style::ValueTypeIdentifier::AttachedDocumentAsset, matchingModifiers))
			{
				if (const Optional<const Asset::Guid*> assetGuid = pAttachedAsset->Get<Asset::Guid>())
				{
					[[maybe_unused]] Threading::JobBatch jobBatch = static_cast<Document::Window&>(*pWindow).OpenDocuments(
						Array{DocumentData{*assetGuid}},
						OpenDocumentFlags{},
						static_cast<Document::Widget&>(*this)
					);
					if (jobBatch.IsValid())
					{
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
				}
			}
		}
	}

	void
	Widget::OnStyleChanged(const Style::CombinedEntry&, const Style::CombinedMatchingEntryModifiersView, const ConstChangedStyleValuesView)
	{
	}

	EnumFlags<Style::Modifier> Widget::GetActiveModifiers() const
	{
		return GetActiveModifiers(GetSceneRegistry());
	}

	EnumFlags<Style::Modifier> Widget::GetActiveModifiers(Entity::SceneRegistry& sceneRegistry) const
	{
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Data::Modifiers>();
		return GetActiveModifiers(sceneRegistry, modifiersSceneData);
	}

	EnumFlags<Style::Modifier> Widget::GetActiveModifiers(
		Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	) const
	{
		EnumFlags<Style::Modifier> modifiers;
		if (const Optional<Data::Modifiers*> pModifiers = FindDataComponentOfType<Data::Modifiers>(modifiersSceneData))
		{
			modifiers = pModifiers->GetModifiers();
		}

		const EnumFlags<Flags> flags = m_flags.GetFlags();

		Entity::ComponentTypeSceneData<Entity::Data::Flags>& componentFlagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const Optional<Entity::Data::Flags*> pComponentFlags = componentFlagsSceneData.GetComponentImplementation(GetIdentifier());
		const EnumFlags<Entity::ComponentFlags> componentFlags = pComponentFlags != nullptr
		                                                           ? (EnumFlags<Entity::ComponentFlags>)*pComponentFlags
		                                                           : EnumFlags<Entity::ComponentFlags>{};

		modifiers |= Style::Modifier::ToggledOff * flags.AreAnySet(Flags::IsToggledOff | Flags::IsToggledOffFromParent);
		modifiers |= Style::Modifier::Disabled * componentFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource);

		const bool failedValidation = flags.AreAnySet(Flags::FailedValidation | Flags::FailedValidationFromParent);
		modifiers |= Style::Modifier::Valid * !failedValidation;
		modifiers |= Style::Modifier::Invalid * failedValidation;

		const bool hasRequirements = flags.AreAnySet(Flags::HasRequirements | Flags::HasRequirementsFromParent);
		modifiers |= Style::Modifier::Required * hasRequirements;
		modifiers |= Style::Modifier::Optional * !hasRequirements;

		if (modifiers.AreNoneSet(Style::Modifier::Disabled))
		{
			modifiers |= Style::Modifier::Hover * flags.AreAnySet(Flags::HasHoverFocus | Flags::IsHoverFocusInChildren);
			modifiers |= Style::Modifier::Focused * flags.AreAnySet(Flags::HasInputFocus | Flags::IsInputFocusInChildren);
			modifiers |= Style::Modifier::Active * flags.AreAnySet(Flags::HasActiveFocus | Flags::IsActiveFocusInChildren);
		}
		return modifiers;
	}

	void Widget::OnActiveStyleModifiersChangedInternal(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Rendering::ToolWindow*> pWindow,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		const ConstChangedStyleValuesView changedValues
	)
	{
		Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::FlexLayout>();
		Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData = *sceneRegistry.FindComponentTypeData<Data::GridLayout>();

		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::ExternalStyle>(
		);
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::InlineStyle>();
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData = *sceneRegistry.FindComponentTypeData<Data::DynamicStyle>();
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData = *sceneRegistry.FindComponentTypeData<Data::Modifiers>();

		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::TextDrawable>();
		Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData = *sceneRegistry.FindComponentTypeData<Data::ImageDrawable>(
		);
		Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RoundedRectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::RectangleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::CircleDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::GridDrawable>();
		Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData =
			*sceneRegistry.FindComponentTypeData<Data::Primitives::LineDrawable>();

		OnStyleChangedInternal(
			sceneRegistry,
			pWindow,
			style,
			matchingModifiers,
			changedValues,
			flexLayoutSceneData,
			gridLayoutSceneData,
			externalStyleSceneData,
			inlineStyleSceneData,
			dynamicStyleSceneData,
			modifiersSceneData,
			textDrawableSceneData,
			imageDrawableSceneData,
			roundedRectangleDrawableSceneData,
			rectangleDrawableSceneData,
			circleDrawableSceneData,
			gridDrawableSceneData,
			lineDrawableSceneData
		);
	}

	Style::Size Widget::GetStylePreferredSize(
		Entity::SceneRegistry& sceneRegistry,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
	) const
	{
		const Style::CombinedEntry style = GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry, modifiersSceneData);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		switch (GetLayoutFromStyle(style, matchingModifiers, *this))
		{
			case LayoutType::None:
				return Style::Size(Math::Zero);
			case LayoutType::Block:
			case LayoutType::Flex:
			case LayoutType::Grid:
			{
				Style::Size preferredSize = style.GetSize(Style::ValueTypeIdentifier::PreferredSize, Style::Size{Style::Auto}, matchingModifiers);

				if (preferredSize.x.Is<Style::AutoType>())
				{
					preferredSize.x = 100_percent;
				}
				if (preferredSize.y.Is<Style::AutoType>())
				{
					const int position = GetPosition(sceneRegistry).y;
					int maximumEndPositionY = position;
					for (const Widget& widget : GetChildren())
					{
						maximumEndPositionY = Math::Max(maximumEndPositionY, widget.GetContentArea().GetEndPosition().y);
					}
					preferredSize.y = PixelValue{maximumEndPositionY - position};
				}

				return preferredSize;
			}
		}
		ExpectUnreachable();
	}

	Style::Size Widget::GetPreferredSize(
		Entity::SceneRegistry& sceneRegistry,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
		Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
		Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
		Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData
	) const
	{
		Style::Size preferredSize;
		if (const Optional<const Style::EntryValue*> pPreferredSize = style.Find(Style::ValueTypeIdentifier::PreferredSize, matchingModifiers))
		{
			preferredSize = *pPreferredSize->Get<Style::Size>();
		}
		else
		{
			preferredSize = Style::Size{Style::Auto};
		}

		if (preferredSize.x.Is<Style::AutoType>() || preferredSize.y.Is<Style::AutoType>())
		{
			if (const Optional<Data::TextDrawable*> pTextDrawableComponent = FindDataComponentOfType<Data::TextDrawable>(textDrawableSceneData))
			{
				Widget& parent = GetParent();
				const Style::CombinedEntry& parentStyle = parent.GetStyle(externalStyleSceneData, inlineStyleSceneData, dynamicStyleSceneData);
				const EnumFlags<Style::Modifier> parentActiveModifiers = parent.GetActiveModifiers(sceneRegistry, modifiersSceneData);
				const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(parentActiveModifiers);

				const LayoutType parentLayoutType =
					parentStyle.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, parentMatchingModifiers);
				const Orientation parentOrientation = parentStyle.GetWithDefault<Orientation>(
					Style::ValueTypeIdentifier::Orientation,
					GetDefaultOrientation(parentLayoutType),
					parentMatchingModifiers
				);
				const Alignment parentSecondaryAlignment = parentStyle.GetWithDefault<Alignment>(
					Style::ValueTypeIdentifier::SecondaryDirectionAlignment,
					Alignment::Stretch,
					parentMatchingModifiers
				);

				if (preferredSize.x.Is<Style::AutoType>())
				{
					if (parentSecondaryAlignment == Alignment::Stretch && parentOrientation == Orientation::Vertical && parentLayoutType != LayoutType::Block)
					{
						// Keep auto layout, let stretch handle logic
					}
					else if (HasLoadedResources() || pTextDrawableComponent->GetFontAtlas().IsValid())
					{
						preferredSize.x = PixelValue(pTextDrawableComponent->CalculateTotalWidth(*this));
					}
					else
					{
						// Specify a size to make sure we trigger a font atlas load
						preferredSize.x = PixelValue(1);
					}
				}
				if (preferredSize.y.Is<Style::AutoType>())
				{
					if (parentSecondaryAlignment == Alignment::Stretch && parentOrientation == Orientation::Horizontal && parentLayoutType != LayoutType::Block)
					{
						// Keep auto layout, let stretch handle logic
					}
					else if (HasLoadedResources() || pTextDrawableComponent->GetFontAtlas().IsValid())
					{
						preferredSize.y = PixelValue(pTextDrawableComponent->CalculateTotalLineHeight(*this));
					}
					else
					{
						// Specify a size to make sure we trigger a font atlas load
						preferredSize.y = PixelValue(1);
					}
				}
			}
		}
		return preferredSize;
	}

	Style::Size Widget::GetMaximumSize(
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers,
		Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData
	) const
	{
		Style::Size maximumSize;
		if (const Optional<const Style::EntryValue*> pMaximumSize = style.Find(Style::ValueTypeIdentifier::MaximumSize, matchingModifiers))
		{
			maximumSize = *pMaximumSize->Get<Style::Size>();
		}
		else
		{
			maximumSize = Style::Size{Style::SizeAxis{Style::NoMaximum}, Style::SizeAxis{Style::NoMaximum}};
		}

		if (maximumSize.x.Is<Style::AutoType>() | maximumSize.x.Is<Math::Ratiof>() | maximumSize.y.Is<Style::AutoType>() | maximumSize.y.Is<Math::Ratiof>())
		{
			if (const Optional<Data::TextDrawable*> pTextDrawableComponent = FindDataComponentOfType<Data::TextDrawable>(textDrawableSceneData))
			{
				if (maximumSize.x.Is<Style::AutoType>() | maximumSize.x.Is<Math::Ratiof>())
				{
					const uint32 totalTextWidth = pTextDrawableComponent->CalculateTotalWidth(*this);
					if (totalTextWidth == 0u)
					{
						maximumSize.x = Style::SizeAxis(Style::NoMaximum);
					}
					else
					{
						maximumSize.x = Widgets::PixelValue(totalTextWidth);
					}
				}

				if (maximumSize.y.Is<Style::AutoType>() | maximumSize.y.Is<Math::Ratiof>())
				{
					const uint32 totalTextHeight = pTextDrawableComponent->CalculateTotalLineHeight(*this);
					if (totalTextHeight == 0u)
					{
						maximumSize.y = Style::SizeAxis(Style::NoMaximum);
					}
					else
					{
						maximumSize.y = Widgets::PixelValue(totalTextHeight);
					}
				}
			}
		}
		return maximumSize;
	}

	void Widget::NotifyChildMaximumSizeChanged(Widget& widget, Entity::SceneRegistry& sceneRegistry)
	{
		widget.RecalculateHierarchy(sceneRegistry);
	}

	void Widget::NotifyOnChildPreferredSizeChanged(Widget& widget, Entity::SceneRegistry& sceneRegistry)
	{
		widget.RecalculateHierarchy(sceneRegistry);
	}

	CursorResult Widget::OnStartTap(
		const Input::DeviceIdentifier, [[maybe_unused]] const uint8 fingerCount, [[maybe_unused]] const LocalWidgetCoordinate coordinate
	)
	{
		return {};
	}
	CursorResult Widget::OnEndTap(
		const Input::DeviceIdentifier, [[maybe_unused]] const uint8 fingerCount, [[maybe_unused]] const LocalWidgetCoordinate coordinate
	)
	{
		return {};
	}
	CursorResult Widget::OnCancelTap(const Input::DeviceIdentifier)
	{
		return {};
	}

	CursorResult Widget::OnDoubleTap(const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
	{
		return {};
	}

	PanResult Widget::OnStartLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		[[maybe_unused]] const LocalWidgetCoordinate coordinate,
		[[maybe_unused]] const uint8 fingerCount,
		[[maybe_unused]] const Optional<uint16> touchRadius
	)
	{
		if constexpr (!PLATFORM_APPLE_VISIONOS)
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
			if (deviceTypeIdentifier == inputManager.GetTouchscreenDeviceTypeIdentifier())
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				const Style::CombinedEntry style = GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
				{
					return PanResult{this, PanType::DragWidget};
				}
			}
		}

		return {};
	}
	CursorResult Widget::OnMoveLongPress(
		const Input::DeviceIdentifier deviceIdentifier,
		[[maybe_unused]] const LocalWidgetCoordinate coordinate,
		[[maybe_unused]] const uint8 fingerCount,
		[[maybe_unused]] const Optional<uint16> touchRadius
	)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetTouchscreenDeviceTypeIdentifier())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				return CursorResult{this};
			}
		}

		return {};
	}
	CursorResult
	Widget::OnEndLongPress(const Input::DeviceIdentifier deviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetTouchscreenDeviceTypeIdentifier())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				return CursorResult{this};
			}
		}

		return {};
	}
	CursorResult Widget::OnCancelLongPress(const Input::DeviceIdentifier deviceIdentifier)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetTouchscreenDeviceTypeIdentifier())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				return CursorResult{this};
			}
		}

		return {};
	}

	void ContextMenuBuilder::AddEntry(const Asset::Guid iconAssetGuid, const ConstUnicodeStringView title, const Guid eventGuid)
	{
		ngine::DataSource::Dynamic& dataSource = GetOrCreateDataSource();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSource.LockWrite();
		const ngine::DataSource::GenericDataIdentifier dataIdentifier = dataSource.CreateDataIdentifier();
		dataSource.AddDataProperty(dataIdentifier, dataSourceCache.FindOrRegisterPropertyIdentifier("dropdown_entry_icon"), iconAssetGuid);
		dataSource.AddDataProperty(dataIdentifier, dataSourceCache.FindOrRegisterPropertyIdentifier("dropdown_entry_title"), title);
		dataSource.AddDataProperty(dataIdentifier, dataSourceCache.FindOrRegisterPropertyIdentifier("dropdown_entry_event_guid"), eventGuid);
		dataSource.UnlockWrite();
	}
	void ContextMenuBuilder::AddEntry(const ConstUnicodeStringView title, const Guid eventGuid)
	{
		ngine::DataSource::Dynamic& dataSource = GetOrCreateDataSource();
		dataSource.LockWrite();
		const ngine::DataSource::GenericDataIdentifier dataIdentifier = dataSource.CreateDataIdentifier();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		dataSource.AddDataProperty(dataIdentifier, dataSourceCache.FindOrRegisterPropertyIdentifier("dropdown_entry_title"), title);
		dataSource.AddDataProperty(dataIdentifier, dataSourceCache.FindOrRegisterPropertyIdentifier("dropdown_entry_event_guid"), eventGuid);
		dataSource.UnlockWrite();
	}

	ngine::DataSource::Dynamic& ContextMenuBuilder::GetOrCreateDataSource()
	{
		if (m_pDataSource.IsInvalid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			const ngine::DataSource::Identifier dataSourceIdentifier = dataSourceCache.Register(Guid::Generate());
			m_pDataSource = new ngine::DataSource::Dynamic(dataSourceIdentifier);
		}
		return *m_pDataSource;
	}

	void ContextMenuBuilder::SetAsset(const Asset::Guid assetGuid)
	{
		ngine::PropertySource::Dynamic& propertySource = GetOrCreatePropertySource();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		propertySource.LockWrite();
		propertySource.AddDataProperty(dataSourceCache.FindOrRegisterPropertyIdentifier("asset_guid"), assetGuid);
		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		UnicodeString assetName(assetManager.GetAssetName(assetGuid));
		propertySource.AddDataProperty(dataSourceCache.FindOrRegisterPropertyIdentifier("asset_name"), Move(assetName));
		propertySource.UnlockWrite();
	}
	void ContextMenuBuilder::SetComponent(const Entity::ComponentSoftReference component)
	{
		ngine::PropertySource::Dynamic& propertySource = GetOrCreatePropertySource();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		propertySource.LockWrite();
		propertySource.AddDataProperty(dataSourceCache.FindOrRegisterPropertyIdentifier("component"), component);
		propertySource.UnlockWrite();
	}

	ngine::PropertySource::Dynamic& ContextMenuBuilder::GetOrCreatePropertySource()
	{
		if (m_pPropertySource.IsInvalid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			ngine::PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
			const ngine::PropertySource::Identifier propertySourceIdentifier = propertySourceCache.Register(Guid::Generate());
			m_pPropertySource = new ngine::PropertySource::Dynamic(propertySourceIdentifier);
		}
		return *m_pPropertySource;
	}

	bool Widget::HasContextMenu(Entity::SceneRegistry& sceneRegistry) const
	{
		if (HasDataComponentOfType<Data::ContextMenuEntries>(sceneRegistry))
		{
			return true;
		}

		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);

		if (style.Contains(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers))
		{
			return true;
		}

		return false;
	}

	bool Widget::HandleSpawnContextMenu(const LocalWidgetCoordinate coordinate, Entity::SceneRegistry& sceneRegistry)
	{
		ContextMenuBuilder contextMenuBuilder;

		const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
		{
			Optional<const Style::EntryValue*> pAttachedAsset = style.Find(Style::ValueTypeIdentifier::AttachedAsset, matchingModifiers);
			if (pAttachedAsset.IsValid())
			{
				if (const Optional<const Asset::Guid*> assetGuid = pAttachedAsset->Get<Asset::Guid>())
				{
					contextMenuBuilder.SetAsset(*assetGuid);

					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					Guid assetTypeGuid = assetManager.GetAssetTypeGuid(*assetGuid);
					if (assetTypeGuid.IsInvalid())
					{
						assetTypeGuid = assetManager.GetAssetLibrary().GetAssetTypeGuid(*assetGuid);
					}
					Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
					if (const Optional<const Reflection::TypeInterface*> pTypeInterface = reflectionRegistry.FindTypeInterface(assetTypeGuid))
					{
						if (const Optional<const Asset::EditorTypeExtension*> pAssetTypeEditorExtension = pTypeInterface->FindExtension<Asset::EditorTypeExtension>())
						{
							contextMenuBuilder.SetContextMenuAsset(pAssetTypeEditorExtension->m_documentContextMenuGuid);
							contextMenuBuilder.SetContextMenuEntryAsset(pAssetTypeEditorExtension->m_documentContextMenuEntryGuid);
						}
					}
				}
			}
		}
		{
			Optional<const Style::EntryValue*> pAttachedComponent = style.Find(Style::ValueTypeIdentifier::AttachedComponent, matchingModifiers);
			if (pAttachedComponent.IsValid())
			{
				if (const Optional<const Entity::ComponentSoftReference*> componentReference = pAttachedComponent->Get<Entity::ComponentSoftReference>())
				{
					contextMenuBuilder.SetComponent(*componentReference);
				}
			}
		}

		if (Optional<Data::ContextMenuEntries*> pContextMenuEntries = FindDataComponentOfType<Data::ContextMenuEntries>(sceneRegistry))
		{
			Context::EventManager eventManager(*this, sceneRegistry);
			for (const Data::ContextMenuEntries::Entry& entry : pContextMenuEntries->GetEntries())
			{
				const Guid eventGuid = Guid::Generate();
				struct EventData
				{
					Widget& widget;
					const Data::ContextMenuEntries::Entry& entry;
				};
				eventManager.Subscribe(
					eventGuid,
					this,
					[pEventData = new EventData{*this, entry}]()
					{
						System::Get<Input::Manager>().GetFeedback().TriggerImpact();
						pEventData->entry.NotifyAll(pEventData->widget);
					}
				);

				contextMenuBuilder.AddEntry(entry.m_iconAssetGuid, entry.m_title, eventGuid);
			}
		}
		else if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			contextMenuBuilder = pInputComponent->OnSpawnContextMenu(*this, coordinate);
		}

		if (!contextMenuBuilder.IsValid())
		{
			contextMenuBuilder = OnSpawnContextMenu(coordinate);
		}

		if (contextMenuBuilder.IsValid())
		{
			if (!contextMenuBuilder.IsDeferred())
			{
				SpawnContextMenu(contextMenuBuilder, coordinate, sceneRegistry);
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void Widget::SpawnContextMenu(
		const ContextMenuBuilder contextMenuBuilder, const LocalCoordinate coordinate, Entity::SceneRegistry& sceneRegistry
	)
	{
		const Optional<Data::ContextMenu*> pContextMenu = CreateDataComponent<Data::ContextMenu>(
			sceneRegistry,
			Data::ContextMenu::Initializer{
				Data::ModalTrigger::Initializer{
					Data::Component::Initializer{*this, sceneRegistry},
					contextMenuBuilder.GetContextMenuAsset(),
					Data::ContextMenu::Flags::SpawnAtCursorLocation | Data::ContextMenu::Flags::CloseOnEvent
				},
				contextMenuBuilder.GetDataSource(),
				contextMenuBuilder.GetPropertySource(),
				contextMenuBuilder.GetContextMenuEntryAsset()
			}
		);
		Assert(pContextMenu.IsValid());
		if (LIKELY(pContextMenu.IsValid()))
		{
			pContextMenu->SpawnModal(*this, ConvertLocalToWindowCoordinates(coordinate));
		}
	}

	ContextMenuBuilder Widget::OnSpawnContextMenu([[maybe_unused]] const LocalWidgetCoordinate coordinate)
	{
		return {};
	}

	PanResult Widget::OnStartPan(
		const Input::DeviceIdentifier deviceIdentifier,
		[[maybe_unused]] const LocalWidgetCoordinate coordinate,
		[[maybe_unused]] const Math::Vector2f velocity,
		[[maybe_unused]] const Optional<uint16> touchRadius,
		SetCursorCallback& pointer
	)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetMouseDeviceTypeIdentifier() || PLATFORM_APPLE_VISIONOS)
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				pointer.SetCursor(Rendering::CursorType::Hand);
				return PanResult{this, PanType::DragWidget};
			}
		}

		return {};
	}

	CursorResult Widget::
		OnMovePan(const Input::DeviceIdentifier deviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetMouseDeviceTypeIdentifier())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				return CursorResult{this};
			}
		}

		return {};
	}

	CursorResult Widget::OnEndPan(
		const Input::DeviceIdentifier deviceIdentifier,
		[[maybe_unused]] const LocalWidgetCoordinate coordinate,
		[[maybe_unused]] const Math::Vector2f velocity
	)
	{
		if (deviceIdentifier.IsValid())
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
			if (deviceTypeIdentifier == inputManager.GetMouseDeviceTypeIdentifier())
			{
				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				const Style::CombinedEntry style = GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
				{
					return CursorResult{this};
				}
			}
		}

		return {};
	}

	CursorResult Widget::OnCancelPan(const Input::DeviceIdentifier deviceIdentifier)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceInstance(deviceIdentifier).GetTypeIdentifier();
		if (deviceTypeIdentifier == inputManager.GetMouseDeviceTypeIdentifier())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			const Style::CombinedEntry style = GetStyle(sceneRegistry);
			const EnumFlags<Style::Modifier> activeModifiers = GetActiveModifiers(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			if (style.Contains(Style::ValueTypeIdentifier::DraggableAsset, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::DraggableComponent, matchingModifiers))
			{
				return CursorResult{this};
			}
		}

		return {};
	}

	CursorResult Widget::
		OnHover(const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, SetCursorCallback&)
	{
		return {};
	}

	CursorResult Widget::OnStartScroll(
		const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta
	)
	{
		return {};
	}
	CursorResult Widget::OnScroll(
		const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta
	)
	{
		return {};
	}
	CursorResult Widget::OnEndScroll(
		const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity
	)
	{
		return {};
	}
	CursorResult Widget::OnCancelScroll(const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
	{
		return {};
	}

	DragAndDropResult Widget::
		OnStartDragWidgetOverThis([[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&)
	{
		return {};
	}

	DragAndDropResult Widget::OnCancelDragWidgetOverThis()
	{
		return {};
	}

	DragAndDropResult Widget::
		OnMoveDragWidgetOverThis([[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&)
	{
		return {};
	}

	DragAndDropResult
	Widget::OnReleaseDragWidgetOverThis([[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget)
	{
		return {};
	}

	bool Widget::CanApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate2D coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	) const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			return pInputComponent->CanApplyAtPoint(*this, applicableData, ConvertWindowToLocalCoordinates((Coordinate)coordinate), applyFlags);
		}

		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->CanApplyAtPoint(applicableData, coordinate, applyFlags);
			}
		}

		return false;
	}

	bool Widget::ApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate2D coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (const Optional<Data::Input*> pInputComponent = FindFirstDataComponentImplementingType<Data::Input>(sceneRegistry))
		{
			return pInputComponent->ApplyAtPoint(*this, applicableData, ConvertWindowToLocalCoordinates((Coordinate)coordinate), applyFlags);
		}

		if (m_flags.IsNotSet(Flags::BlockPointerInputs))
		{
			if (Optional<Widget*> pWidget = GetParentSafe())
			{
				return pWidget->ApplyAtPoint(applicableData, coordinate, applyFlags);
			}
		}

		return false;
	}

	void Widget::SetPositionFromProperty(WidgetPositionInfo widgetPositionInfo)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);

		const Style::SizeAxisEdges currentPosition =
			style.GetWithDefault<Style::SizeAxisEdges>(Style::ValueTypeIdentifier::Position, Style::SizeAxisEdges{Invalid}, matchingModifiers);
		Style::SizeAxisEdges newPosition{widgetPositionInfo.position[0], widgetPositionInfo.position[1], {}, {}};
		if (currentPosition != newPosition)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Position, Move(newPosition)});
		}

		const PositionType currentPositionType =
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);
		if (currentPositionType != widgetPositionInfo.positionType)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::PositionType, widgetPositionInfo.positionType});
		}
	}

	WidgetPositionInfo Widget::GetPositionFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);

		const Style::SizeAxisEdges currentPosition =
			style.GetWithDefault<Style::SizeAxisEdges>(Style::ValueTypeIdentifier::Position, Style::SizeAxisEdges{Invalid}, matchingModifiers);

		return WidgetPositionInfo{
			{currentPosition.m_left, currentPosition.m_top},
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers)
		};
	}

	void Widget::SetRotationFromProperty(Math::Anglef rotation)
	{
		SetWorldRotation(rotation, GetSceneRegistry());
	}

	Math::Anglef Widget::GetRotationFromProperty() const
	{
		return GetWorldTransform(GetSceneRegistry()).GetRotation();
	}

	void Widget::SetSizeFromProperty(WidgetSizeInfo widgetSizeInfo)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Style::Size currentSize = style.GetSize(
			Style::ValueTypeIdentifier::PreferredSize,
			Style::Size{(Math::Ratiof)100_percent, (Math::Ratiof)0_percent},
			matchingModifiers
		);
		if (currentSize != widgetSizeInfo.size)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::PreferredSize, Move(widgetSizeInfo.size)});
		}

		const float currentGrowthFactor = style.GetWithDefault(Style::ValueTypeIdentifier::ChildGrowthFactor, 0.f, matchingModifiers);
		if (currentGrowthFactor != widgetSizeInfo.growthFactor)
		{
			if (widgetSizeInfo.growthFactor == 0.f)
			{
				ClearInlineStyle(Style::ValueTypeIdentifier::ChildGrowthFactor);
			}
			else
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::ChildGrowthFactor, widgetSizeInfo.growthFactor});
			}
			RecalculateHierarchy(sceneRegistry);
		}
	}

	WidgetSizeInfo Widget::GetSizeFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);

		WidgetSizeInfo::GrowthDirection growthDirection = WidgetSizeInfo::GrowthDirection::None;
		if (const Optional<Widget*> pParent = GetParentSafe())
		{
			const Style::CombinedEntry parentStyle = pParent->GetStyle(sceneRegistry);
			const Style::CombinedEntry::MatchingModifiers parentMatchingModifiers = parentStyle.GetMatchingModifiers(Style::Modifier::None);
			const LayoutType layoutType =
				parentStyle.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, parentMatchingModifiers);
			switch (layoutType)
			{
				case LayoutType::None:
				case LayoutType::Block:
					break;
				case LayoutType::Flex:
				case LayoutType::Grid:
					switch (
						parentStyle.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), parentMatchingModifiers)
					)
					{
						case Orientation::Horizontal:
							growthDirection = WidgetSizeInfo::GrowthDirection::Horizontal;
							break;
						case Orientation::Vertical:
							growthDirection = WidgetSizeInfo::GrowthDirection::Vertical;
							break;
					}
					break;
			}
		}

		return WidgetSizeInfo{
			style.GetSize(
				Style::ValueTypeIdentifier::PreferredSize,
				Style::Size{(Math::Ratiof)100_percent, (Math::Ratiof)0_percent},
				matchingModifiers
			),
			style.GetWithDefault(Style::ValueTypeIdentifier::ChildGrowthFactor, 0.f, matchingModifiers),
			growthDirection
		};
	}

	void Widget::SetLayoutFromProperty(const LayoutType newLayoutType)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const LayoutType layoutType =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		if (layoutType != newLayoutType)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::LayoutType, newLayoutType});
			RecalculateHierarchy(sceneRegistry);
		}
	}

	LayoutType Widget::GetLayoutFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
	}

	void Widget::SetPaddingFromProperty(Style::SizeAxisEdges newSizeEdges)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Style::SizeAxisEdges sizeEdges = GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Padding, matchingModifiers.GetView());
		if (sizeEdges != newSizeEdges)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Padding, Move(newSizeEdges)});
			// UI editing forces use of box-sizing: border-box
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::ElementSizingType, ElementSizingType::BorderBox});
			RecalculateHierarchy(sceneRegistry);
		}
	}

	Style::SizeAxisEdges Widget::GetPaddingFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return GetEdgesFromStyle(style, Style::ValueTypeIdentifier::Padding, matchingModifiers.GetView());
	}

	void Widget::SetOrientationFromProperty(const Orientation newOrientation)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const LayoutType layoutType =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);
		if (orientation != newOrientation)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Orientation, newOrientation});
			RecalculateHierarchy(sceneRegistry);
		}
	}

	Orientation Widget::GetOrientationFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const LayoutType layoutType =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		return style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);
	}

	void Widget::SetAlignmentFromProperty(const WidgetAlignmentInfo newAlignment)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const LayoutType layoutType =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);
		const Alignment primaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::PrimaryDirectionAlignment, Alignment::Start, matchingModifiers);
		const Alignment secondaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::SecondaryDirectionAlignment, Alignment::Stretch, matchingModifiers);

		if (orientation == Orientation::Horizontal)
		{
			if (primaryAlignment != newAlignment.x)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::PrimaryDirectionAlignment, newAlignment.x});
			}
			if (secondaryAlignment != newAlignment.y)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::SecondaryDirectionAlignment, newAlignment.y});
			}
		}
		else
		{
			if (primaryAlignment != newAlignment.y)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::PrimaryDirectionAlignment, newAlignment.y});
			}
			if (secondaryAlignment != newAlignment.x)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::SecondaryDirectionAlignment, newAlignment.x});
			}
		}
		RecalculateHierarchy(sceneRegistry);
	}

	WidgetAlignmentInfo Widget::GetAlignmentFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const LayoutType layoutType =
			style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
		const Orientation orientation =
			style.GetWithDefault(Style::ValueTypeIdentifier::Orientation, GetDefaultOrientation(layoutType), matchingModifiers);
		const Alignment primaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::PrimaryDirectionAlignment, Alignment::Start, matchingModifiers);
		const Alignment secondaryAlignment =
			style.GetWithDefault<Alignment>(Style::ValueTypeIdentifier::SecondaryDirectionAlignment, Alignment::Stretch, matchingModifiers);
		if (orientation == Orientation::Horizontal)
		{
			return {primaryAlignment, secondaryAlignment};
		}
		else
		{
			return {secondaryAlignment, primaryAlignment};
		}
	}

	void Widget::SetGapFromProperty(Style::SizeAxisExpression newChildOffset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Style::Size childOffset = style.GetSize(Style::ValueTypeIdentifier::ChildOffset, 0_px, matchingModifiers);
		if (childOffset != newChildOffset)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::ChildOffset, Style::Size{Move(newChildOffset)}});
			RecalculateHierarchy(sceneRegistry);
		}
	}

	Style::SizeAxisExpression Widget::GetGapFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style.GetSize(Style::ValueTypeIdentifier::ChildOffset, 0_px, matchingModifiers)[0];
	}

	void Widget::SetOverflowTypeFromProperty(const OverflowType newOverflow)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const OverflowType overflowType =
			style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);
		if (overflowType != newOverflow)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::OverflowType, newOverflow});
			RecalculateHierarchy(sceneRegistry);
		}
	}

	OverflowType Widget::GetOverflowTypeFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style.GetWithDefault(Style::ValueTypeIdentifier::OverflowType, OverflowType(DefaultOverflowType), matchingModifiers);
	}

	void Widget::SetBackgroundAssetFromProperty(Asset::Picker newBackgroundAsset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Asset::Guid assetGuid = style.GetWithDefault<Asset::Guid>(Style::ValueTypeIdentifier::AssetIdentifier, {}, matchingModifiers);
		if (newBackgroundAsset.IsValid())
		{
			if (assetGuid != newBackgroundAsset.GetAssetGuid())
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::AssetIdentifier, newBackgroundAsset.GetAssetGuid()});
			}
		}
		else if (assetGuid.IsValid())
		{
			ClearInlineStyle(Style::ValueTypeIdentifier::AssetIdentifier);
		}
	}
	Asset::Picker Widget::GetBackgroundAssetFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return {
			style.GetWithDefault<Asset::Guid>(Style::ValueTypeIdentifier::AssetIdentifier, {}, matchingModifiers),
			TextureAssetType::AssetFormat.assetTypeGuid
		};
	}

	void Widget::SetBackgroundColorFromProperty(Math::Color newColor)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		if (style.Contains(Style::ValueTypeIdentifier::AssetIdentifier, matchingModifiers) || style.Contains(Style::ValueTypeIdentifier::Text, matchingModifiers))
		{
			// Color field tweaks background asset or text when available
			const Math::Color color = style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::Color, "#00000000"_colorf, matchingModifiers);
			if (color != newColor)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Color, newColor});
			}
		}
		else
		{
			const Math::Color backgroundColor =
				style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
			if (backgroundColor != newColor)
			{
				EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::BackgroundColor, newColor});
			}
		}
	}
	Math::Color Widget::GetBackgroundColorFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		if (style.Contains(Style::ValueTypeIdentifier::AssetIdentifier, matchingModifiers))
		{
			// Color field tweaks background asset when there's a texture attached
			return style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::Color, "#00000000"_colorf, matchingModifiers);
		}
		else
		{
			return style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BackgroundColor, "#00000000"_colorf, matchingModifiers);
		}
	}

	void Widget::SetOpacityFromProperty(Math::Ratiof newOpacity)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Math::Ratiof opacity = style.GetWithDefault<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, 100_percent, matchingModifiers);
		if (opacity != newOpacity)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::Opacity, newOpacity});
		}
	}
	Math::Ratiof Widget::GetOpacityFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style.GetWithDefault<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, 100_percent, matchingModifiers);
	}

	void Widget::SetBackgroundRoundingRadiusFromProperty(Style::SizeAxisExpression newRadius)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Style::SizeAxisCorners currentCorners =
			style.GetWithDefault<Style::SizeAxisCorners>(Style::ValueTypeIdentifier::RoundingRadius, Style::SizeAxisCorners{}, matchingModifiers);
		if (currentCorners != newRadius)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::RoundingRadius, Style::SizeAxisCorners{Move(newRadius)}});
		}
	}
	Style::SizeAxisExpression Widget::GetBackgroundRoundingRadiusFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style
		  .GetWithDefault<Style::SizeAxisCorners>(Style::ValueTypeIdentifier::RoundingRadius, Style::SizeAxisCorners{}, matchingModifiers)[0];
	}

	void Widget::SetBorderFromProperty(Math::Color newColor)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Math::Color borderColor =
			style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BorderColor, "#00000000"_colorf, matchingModifiers);
		if (borderColor != newColor)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::BorderColor, newColor});
		}
	}
	Math::Color Widget::GetBorderFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style.GetWithDefault<Math::Color>(Style::ValueTypeIdentifier::BorderColor, "#00000000"_colorf, matchingModifiers);
	}

	void Widget::SetBorderWidthFromProperty(Style::SizeAxisExpression newWidth)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		const Style::SizeAxisEdges currentWidth = style.GetWithDefault<Style::SizeAxisEdges>(
			Style::ValueTypeIdentifier::BorderThickness,
			Style::SizeAxisEdges{0_px},
			matchingModifiers
		);
		if (currentWidth != newWidth)
		{
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::BorderThickness, Style::SizeAxisEdges{Move(newWidth)}});
			// UI editing forces use of box-sizing: border-box
			EmplaceInlineStyle(Style::Value{Style::ValueTypeIdentifier::ElementSizingType, ElementSizingType::BorderBox});
		}
	}
	Style::SizeAxisExpression Widget::GetBorderWidthFromProperty() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Style::CombinedEntry style = GetStyle(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(Style::Modifier::None);
		return style
		  .GetWithDefault<Style::SizeAxisEdges>(Style::ValueTypeIdentifier::BorderThickness, Style::SizeAxisEdges{0_px}, matchingModifiers)[0];
	}

	bool WidgetPositionInfo::Serialize(const Serialization::Reader)
	{
		return false;
	}
	bool WidgetPositionInfo::Serialize(Serialization::Writer) const
	{
		return false;
	}

	bool WidgetSizeInfo::Serialize(const Serialization::Reader)
	{
		return false;
	}
	bool WidgetSizeInfo::Serialize(Serialization::Writer) const
	{
		return false;
	}

	bool WidgetAlignmentInfo::Serialize(const Serialization::Reader)
	{
		return false;
	}
	bool WidgetAlignmentInfo::Serialize(Serialization::Writer) const
	{
		return false;
	}

	[[maybe_unused]] const bool wasWidgetTypeRegistered = Reflection::Registry::RegisterType<Widget>();
	[[maybe_unused]] const bool wasWidgetComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widget>>::Make());
	[[maybe_unused]] const bool wasPositionTypeRegistered = Reflection::Registry::RegisterType<PositionType>();
	[[maybe_unused]] const bool wasLayoutTypeRegistered = Reflection::Registry::RegisterType<LayoutType>();
	[[maybe_unused]] const bool wasOrientationTypeRegistered = Reflection::Registry::RegisterType<Orientation>();
	[[maybe_unused]] const bool wasAlignmentTypeRegistered = Reflection::Registry::RegisterType<Alignment>();
	[[maybe_unused]] const bool wasOverflowTypeTypeRegistered = Reflection::Registry::RegisterType<OverflowType>();
}
