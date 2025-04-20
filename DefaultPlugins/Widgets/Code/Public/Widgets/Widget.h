#pragma once

#include <Engine/Asset/Identifier.h>
#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/WindowCoordinate.h>
#include <Engine/Input/Actions/DeleteTextType.h>
#include <Engine/Input/Actions/MoveTextCursorFlags.h>
#include <Engine/Entity/Component2D.h>
#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Event/Identifier.h>
#include <Engine/DataSource/Data.h>
#include <Engine/DataSource/PropertySourceInterface.h>
#include <Engine/DataSource/PropertySourceIdentifier.h>

#include <Widgets/WidgetFlags.h>
#include <Widgets/LayoutType.h>
#include <Widgets/PositionType.h>
#include <Widgets/Orientation.h>
#include <Widgets/OverflowType.h>
#include <Widgets/Style/Size.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Style/Modifier.h>
#include <Widgets/Style/ValueTypeIdentifier.h>
#include <Widgets/Style/ForwardDeclarations/Value.h>
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>
#include <Widgets/ContentAreaChangeFlags.h>
#include <Widgets/SetCursorCallback.h>
#include <Widgets/Data/Component.h>
#include <Widgets/EventType.h>
#include <Widgets/EventInfo.h>
#include <Widgets/DepthInfo.h>
#include <Widgets/InputResults.h>
#include <Widgets/LocalWidgetCoordinate.h>
#include <Widgets/ContextMenuBuilder.h>
#include <Widgets/WidgetPositionInfo.h>
#include <Widgets/WidgetSizeInfo.h>
#include <Widgets/WidgetAlignmentInfo.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/SignNonZero.h>
#include <Common/Math/Color.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/ForwardDeclarations/Any.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Function/ForwardDeclarations/CopyableFunction.h>
#include <Common/EnumFlags.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Reflection/Type.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/Register.h>
#include <Common/IO/URI.h>
#include <Common/Asset/Picker.h>

namespace ngine
{
	namespace Reflection
	{
		struct Registry;
	}

	namespace Asset
	{
		struct Manager;
	}

	namespace Entity
	{
		struct ComponentRegistry;
		struct SceneRegistry;
		struct ComponentTypeSceneDataInterface;
		struct ComponentSoftReference;

		template<typename ComponentType>
		struct ComponentTypeSceneData;
	}

	namespace Events
	{
		struct Manager;
	}

	namespace DataSource
	{
		struct Interface;
		struct Dynamic;
		struct State;
	}

	namespace PropertySource
	{
		struct Interface;
		struct Dynamic;
	}

	namespace Threading
	{
		struct JobBatch;
	}
}

namespace ngine::Input
{
	struct Monitor;
}

namespace ngine::Rendering
{
	struct ToolWindow;
	struct PointerDevice;
	struct RenderPassView;
	struct FramebufferView;
	struct RenderCommandEncoderView;
	struct FramegraphBuilder;
}

namespace ngine::Widgets
{
	namespace Style
	{
		struct Entry;
		struct Value;
		struct CombinedEntry;
		struct StylesheetCache;
		struct DynamicEntry;
		struct ComputedStylesheet;
	}

	namespace Data
	{
		struct FlexLayout;
		struct GridLayout;
		struct DataSource;
		struct DataSourceEntry;
		struct PropertySource;
		struct Drawable;
		struct TextDrawable;
		struct ImageDrawable;

		namespace Primitives
		{
			struct RoundedRectangleDrawable;
			struct RectangleDrawable;
			struct CircleDrawable;
			struct GridDrawable;
			struct LineDrawable;
		}

		struct ExternalStyle;
		struct InlineStyle;
		struct DynamicStyle;
		struct Modifiers;
	}

	struct Widget;
	struct RootWidget;
	struct SetCursorCallback;
	struct LoadResourcesResult;
	struct EventData;
	struct UIViewMode;

	struct DataSourceEntryInfo
	{
		const ngine::DataSource::Interface& m_dataSource;
		Any m_data;
		Optional<ngine::DataSource::State*> m_pDataSourceState;
	};

	struct DataSourceProperties
	{
		[[nodiscard]] DataSource::PropertyValue GetDataProperty(const ngine::DataSource::PropertyIdentifier) const;

		void CopyFrom(const DataSourceProperties& other)
		{
			m_propertySources.CopyEmplaceRange(m_propertySources.end(), Memory::DefaultConstruct, other.m_propertySources.GetView());
			m_dataSourceEntries.CopyEmplaceRange(m_dataSourceEntries.end(), Memory::DefaultConstruct, other.m_dataSourceEntries.GetView());
		}

		void Emplace(ngine::PropertySource::Interface& interface)
		{
			m_propertySources.EmplaceBack(interface);
		}
		[[nodiscard]] bool HasPropertySources() const
		{
			return m_propertySources.HasElements();
		}

		void Emplace(DataSourceEntryInfo&& dataSourceInfo)
		{
			m_dataSourceEntries.EmplaceBack(Forward<DataSourceEntryInfo>(dataSourceInfo));
		}
	protected:
		InlineVector<ReferenceWrapper<ngine::PropertySource::Interface>, 1> m_propertySources;
		InlineVector<DataSourceEntryInfo, 1> m_dataSourceEntries;
	};

	struct Widget : public Entity::Component2D
	{
		using InstanceIdentifier = TIdentifier<uint32, 16>;

		inline static constexpr uint8 MaximumWidgetCountPerHolder = 255;
		using SizeType = Memory::NumericSize<MaximumWidgetCountPerHolder>;

		using ToolWindow = Rendering::ToolWindow;
		using ParentType = Widget;

		using Flags = WidgetFlags;

		using BaseType = Component2D;
		using RootParentType = RootWidget;

		using LocalCoordinate = LocalWidgetCoordinate;
		using Coordinate = WindowCoordinate;

		struct Initializer : public Entity::Component2D::Initializer
		{
			using BaseType = Entity::Component2D::Initializer;

			Initializer(Widget& parent, const EnumFlags<Flags> flags = {}, UniquePtr<Style::Entry>&& pInlineStyle = {});
			Initializer(DynamicInitializer&& dynamicInitializer);
			Initializer(const Initializer&) = delete;
			Initializer& operator=(const Initializer&) = delete;
			Initializer(Initializer&& other);
			Initializer& operator=(Initializer&& other) = delete;
			~Initializer();

			[[nodiscard]] Initializer operator|(const EnumFlags<Flags> flags)
			{
				Initializer initializer = Move(*this);
				initializer.m_widgetFlags |= flags;
				return initializer;
			}

			[[nodiscard]] Optional<Widget*> GetParent() const
			{
				return static_cast<Widget*>(BaseType::GetParent().Get());
			}

			EnumFlags<Flags> m_widgetFlags;
			UniquePtr<Style::Entry> m_pInlineStyle;
		};
		// This Initializer is the initializer needed to instantiate transformed components at runtime when type info is unknown
		using DynamicInitializer = BaseType::DynamicInitializer;

		struct Deserializer : public Entity::Component2D::Deserializer
		{
			using BaseType = Entity::Component2D::Deserializer;

			Deserializer(
				Reflection::TypeDeserializer&& deserializer,
				Widget& parent,
				Entity::SceneRegistry& sceneRegistry,
				const EnumFlags<Flags> flags = {},
				const Optional<ChildIndex> preferredChildIndex = {}
			)
				: BaseType{Forward<Reflection::TypeDeserializer>(deserializer), sceneRegistry, parent}
				, m_widgetFlags(flags)
				, m_preferredChildIndex(preferredChildIndex)
			{
			}

			[[nodiscard]] Deserializer operator|(const EnumFlags<Flags> flags) const
			{
				Deserializer initializer = *this;
				initializer.m_widgetFlags |= flags;
				return initializer;
			}

			[[nodiscard]] Deserializer operator|(const EnumFlags<Entity::ComponentFlags> flags) const
			{
				Deserializer initializer = *this;
				initializer.m_flags |= flags;
				return initializer;
			}

			[[nodiscard]] Optional<Widget*> GetParent() const
			{
				return static_cast<Widget*>(BaseType::GetParent().Get());
			}

			EnumFlags<Flags> m_widgetFlags;
			Optional<ChildIndex> m_preferredChildIndex;
		};
		struct Cloner : public Entity::Component2D::Cloner
		{
			using BaseType = Entity::Component2D::Cloner;

			Cloner(
				Threading::JobBatch& jobBatch,
				Widget& parent,
				Entity::SceneRegistry& sceneRegistry,
				const Entity::SceneRegistry& templateSceneRegistry,
				const Guid instanceGuid = Guid::Generate(),
				const Optional<ChildIndex> preferredChildIndex = Invalid
			)
				: BaseType{jobBatch, parent, sceneRegistry, templateSceneRegistry, instanceGuid, preferredChildIndex}
			{
			}

			[[nodiscard]] Optional<Widget*> GetParent() const
			{
				return static_cast<Widget*>(BaseType::GetParent().Get());
			}
		};

		Widget(Initializer&& initializer);
		Widget(
			Widget& parent,
			const Math::Rectanglei contentArea,
			const EnumFlags<Flags> flags = Flags(),
			UniquePtr<Style::Entry>&& pInlineStyle = {}
		);
		Widget(Widget& parent, const Coordinate position, const EnumFlags<Flags> flags = Flags(), UniquePtr<Style::Entry>&& pInlineStyle = {});
		Widget(
			Entity::SceneRegistry& sceneRegistry,
			RootWidget&,
			const Math::Vector2ui size,
			const Optional<Rendering::ToolWindow*> pOwningWindow,
			Component2D& parent,
			const EnumFlags<Flags> flags = Flags(),
			UniquePtr<Style::Entry>&& pInlineStyle = {}
		);
		Widget(const Deserializer& deserializer);
		Widget(const Widget& templateWidget, const Cloner& cloner);
		Widget(const Widget& other) = delete;
		Widget(Widget&& other) = delete;
		Widget& operator=(const Widget&) = delete;
		Widget& operator=(Widget&&) = delete;
		virtual ~Widget();

		void OnEnable();
		void OnDisable();

		[[nodiscard]] bool HasHoverFocus() const
		{
			return m_flags.IsSet(Flags::HasHoverFocus);
		}
		[[nodiscard]] bool IsHoverFocusInside() const
		{
			return m_flags.AreAnySet(Flags::HasHoverFocus | Flags::IsHoverFocusInChildren);
		}
		[[nodiscard]] bool HasInputFocus() const
		{
			return m_flags.IsSet(Flags::HasInputFocus);
		}
		[[nodiscard]] bool IsInputFocusInside() const
		{
			return m_flags.AreAnySet(Flags::HasInputFocus | Flags::IsInputFocusInChildren);
		}
		[[nodiscard]] bool HasActiveFocus() const
		{
			return m_flags.IsSet(Flags::HasActiveFocus);
		}
		[[nodiscard]] bool IsActiveFocusInside() const
		{
			return m_flags.AreAnySet(Flags::HasActiveFocus | Flags::IsActiveFocusInChildren);
		}

		[[nodiscard]] Optional<Rendering::ToolWindow*> GetOwningWindow() const;

		[[nodiscard]] Widget& GetParent() const LIFETIME_BOUND
		{
			Assert(!IsRootWidget());
			return static_cast<Widget&>(HierarchyComponent::GetParent());
		}
		[[nodiscard]] Optional<Widget*> GetParentSafe() const LIFETIME_BOUND
		{
			if (!IsRootWidget())
			{
				return static_cast<Widget*>(HierarchyComponent::GetParentSafe().Get());
			}
			else
			{
				return Invalid;
			}
		}
		[[nodiscard]] bool IsRootWidget() const;
		[[nodiscard]] Widget& GetRootWidget();
		[[nodiscard]] Widget& GetRootAssetWidget();

		[[nodiscard]] Optional<Widget*> GetFirstChild() const;
		[[nodiscard]] Optional<Widget*> GetLastChild() const;
		[[nodiscard]] Optional<Widget*> GetSubsequentSibling() const;
		[[nodiscard]] Optional<Widget*> GetSubsequentSiblingInHierarchy() const;
		[[nodiscard]] Optional<Widget*> GetSubsequentWidgetInHierarchy() const;
		[[nodiscard]] Optional<Widget*> GetPreviousSibling() const;
		[[nodiscard]] Optional<Widget*> GetPreviousSiblingInHierarchy() const;
		[[nodiscard]] Optional<Widget*> GetPreviousWidgetInHierarchy() const;

		struct ChildView : private Entity::Component2D::ChildView
		{
			using BaseType = Entity::Component2D::ChildView;
			using BaseType::ConstViewType;

			ChildView(BaseType&& view)
				: BaseType(Forward<BaseType>(view))
			{
			}
			using BaseType::BaseType;
			using BaseType::operator=;

			using BaseType::Contains;
			using BaseType::ContainsIf;
			using BaseType::GetSize;
			using BaseType::IsEmpty;
			using BaseType::HasElements;
			using BaseType::Find;
			using BaseType::FindIf;
			using BaseType::GetIteratorIndex;
			using BaseType::ContainsAny;
			using BaseType::All;
			using BaseType::Any;
			using BaseType::IsValidIndex;
			using BaseType::operator++;
			using BaseType::operator--;

			using IteratorType = Iterator<const ReferenceWrapper<Widget>>;
			[[nodiscard]] IteratorType begin() const noexcept
			{
				BaseType::IteratorType baseBegin = BaseType::begin();
				const ReferenceWrapper<Entity::Component2D>* pBaseValue = baseBegin.Get();
				return IteratorType(reinterpret_cast<const ReferenceWrapper<Widget>*>(pBaseValue));
			}
			[[nodiscard]] IteratorType end() const noexcept
			{
				BaseType::IteratorType baseEnd = BaseType::end();
				const ReferenceWrapper<Entity::Component2D>* pBaseValue = baseEnd.Get();
				return IteratorType(reinterpret_cast<const ReferenceWrapper<Widget>*>(pBaseValue));
			}

			[[nodiscard]] PURE_STATICS constexpr Widget& operator[](const IndexType index) const noexcept
			{
				return static_cast<Widget&>(BaseType::operator[](index));
			}
			[[nodiscard]] PURE_STATICS Widget& GetLastElement() const noexcept
			{
				return static_cast<Widget&>(BaseType::GetLastElement());
			}

			[[nodiscard]] PURE_STATICS IndexType GetIteratorIndex(const ReferenceWrapper<Widget>* it) const noexcept
			{
				return BaseType::GetIteratorIndex(reinterpret_cast<const ReferenceWrapper<HierarchyComponentBase>*>(it));
			}
		};
		[[nodiscard]] ChildView GetChildren() const LIFETIME_BOUND
		{
			return ChildView(BaseType::GetChildren());
		}

		[[nodiscard]] Math::Rectanglei GetContentArea(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Rectanglei GetContentArea() const;
		[[nodiscard]] Math::Rectanglei GetMaskedContentArea(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
		) const;
		[[nodiscard]] Math::Rectanglei
		GetMaskedContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const;
		[[nodiscard]] Math::Rectanglei GetMaskedContentArea(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
			Math::Rectanglei contentArea
		) const;
		[[nodiscard]] Math::Rectanglei GetMaskedContentArea(
			Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow, Math::Rectanglei contentArea
		) const;
		void SetContentArea(const Math::Rectanglei contentArea, Entity::SceneRegistry& sceneRegistry);
		void SetContentArea(const Math::Rectanglei contentArea);
		void Reposition(const Coordinate newPosition, Entity::SceneRegistry& sceneRegistry)
		{
			Assert((newPosition != GetPosition(sceneRegistry)).AreAnySet());
			const Math::Rectanglei newContentArea = {(Math::Vector2i)newPosition, GetSize(sceneRegistry)};
			SetContentArea(newContentArea, sceneRegistry);
		}
		void Reposition(const Coordinate newPosition);
		void Resize(const Math::Vector2i size, Entity::SceneRegistry& sceneRegistry)
		{
			Assert((size != GetSize(sceneRegistry)).AreAnySet());
			const Math::Rectanglei newContentArea = {(Math::Vector2i)GetPosition(sceneRegistry), size};
			SetContentArea(newContentArea, sceneRegistry);
		}
		void Resize(const Math::Vector2i size);
		[[nodiscard]] PURE_STATICS Coordinate GetPosition(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Coordinate GetPosition() const;
		[[nodiscard]] PURE_STATICS LocalCoordinate GetRelativeLocation(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS LocalCoordinate GetRelativeLocation() const;
		void SetRelativeLocation(const LocalCoordinate coordinate, Entity::SceneRegistry& sceneRegistry);
		void SetRelativeLocation(const LocalCoordinate coordinate);
		[[nodiscard]] PURE_STATICS Math::Vector2i GetSize(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] PURE_STATICS Math::Vector2i GetSize() const;
		[[nodiscard]] PURE_STATICS Math::Vector2i
		GetParentSize(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const;
		[[nodiscard]] PURE_STATICS Math::Vector2i GetParentSize() const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei
		GetParentContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei GetParentContentArea() const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei
		GetParentAvailableChildContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei GetParentAvailableChildContentArea() const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei
		GetAvailableChildContentArea(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow) const;
		[[nodiscard]] PURE_STATICS Math::Rectanglei GetAvailableChildContentArea() const;

		[[nodiscard]] float GetDepthRatio() const
		{
			constexpr double MinimumDepth = 65537;
			constexpr double MaximumDepth = 10000000.0;
			return m_depthInfo.GetDepthRatio(MinimumDepth, MaximumDepth);
		}
		[[nodiscard]] DepthInfo GetDepthInfo() const
		{
			return m_depthInfo;
		}
		[[nodiscard]] DepthInfo GetLastNestedChildOrOwnDepthInfo() const;

		[[nodiscard]] bool IsVisible() const
		{
			return !IsHidden();
		}
		[[nodiscard]] bool IsHidden() const
		{
			return m_flags.AreAnySet(Flags::IsHiddenFromAnySource);
		}
		[[nodiscard]] bool IsIgnored() const
		{
			return m_flags.AreAnySet(Flags::IsIgnoredFromAnySource);
		}

		[[nodiscard]] bool IsInputDisabled() const
		{
			return m_flags.AreAnySet(Flags::IsInputDisabled | Flags::IsIgnoredFromAnySource);
		}
		void DisableInput()
		{
			m_flags |= Flags::IsInputDisabled;
		}
		void EnableInput()
		{
			m_flags &= ~Flags::IsInputDisabled;
		}
		void BlockPointerInputs()
		{
			m_flags |= Flags::BlockPointerInputs;
			m_flags &= ~Flags::IsInputDisabled;
		}
		[[nodiscard]] bool IsPointerInputBlocked() const
		{
			return m_flags.IsSet(Flags::BlockPointerInputs);
		}

		[[nodiscard]] bool CanReceiveInputFocus(Entity::SceneRegistry& sceneRegistry) const;

		using DataSourceEntryInfo = Widgets::DataSourceEntryInfo;
		using DataSourceProperties = Widgets::DataSourceProperties;
		void UpdateFromDataSource(Entity::SceneRegistry& sceneRegistry, DataSourceEntryInfo&&);
		void UpdateFromDataSource(Entity::SceneRegistry& sceneRegistry);

		bool TryReloadResources(Entity::SceneRegistry& sceneRegistry);
		void ResetLoadedResources()
		{
			m_flags &= ~(Flags::HasLoadedResources | Flags::AreResourcesUpToDate | Flags::HasStartedLoadingResources);
		}
		void InvalidateLoadedResources()
		{
			m_flags &= ~(Flags::AreResourcesUpToDate | Flags::HasStartedLoadingResources);
		}
		void OnLoadedResources()
		{
			m_flags |= Flags::HasLoadedResources | Flags::AreResourcesUpToDate;
		}
		bool LoadResources(Entity::SceneRegistry& sceneRegistry, const Optional<Threading::JobBatch*> pJobBatchOut = Invalid);
		[[nodiscard]] bool HasLoadedResources() const
		{
			return m_flags.IsSet(Flags::HasLoadedResources);
		}
		[[nodiscard]] bool AreResourcesUpToDate() const
		{
			return m_flags.IsSet(Flags::AreResourcesUpToDate);
		}
		[[nodiscard]] bool HasStartedLoadingResources() const
		{
			return m_flags.IsSet(Flags::HasStartedLoadingResources);
		}

		[[nodiscard]] EnumFlags<Flags> GetFlags() const
		{
			return m_flags.GetFlags();
		}

		template<typename Type>
		Optional<Type*> EmplaceChildWidget(typename Type::Initializer&& initializer);

		bool Hide(Entity::SceneRegistry& sceneRegistry);
		bool Hide();
		bool MakeVisible(Entity::SceneRegistry& sceneRegistry);
		bool MakeVisible();
		void ToggleVisibility(Entity::SceneRegistry& sceneRegistry);
		void ToggleVisibility();

		bool Destroy(Entity::SceneRegistry& sceneRegistry);
		void OnAttachedToTree(const Optional<Widget*> pParent);
		void OnDetachedFromTree(const Optional<Widget*> pParent);

		bool Ignore(Entity::SceneRegistry& sceneRegistry);
		bool Unignore(Entity::SceneRegistry& sceneRegistry);
		void ToggleIgnore(Entity::SceneRegistry& sceneRegistry);

		void SetToggledOff();
		void ClearToggledOff();
		void ToggleOff();
		[[nodiscard]] bool IsToggledOff() const
		{
			return m_flags.IsSet(Flags::IsToggledOff);
		}

		void OnPassedValidation();
		void OnFailedValidation();
		[[nodiscard]] bool HasFailedValidation() const
		{
			return m_flags.IsSet(Flags::FailedValidation);
		}
		void SetHasRequirements();
		void ClearHasRequirements();
		[[nodiscard]] bool HasRequirements() const
		{
			return m_flags.IsSet(Flags::HasRequirements);
		}

		[[nodiscard]] virtual Optional<Input::Monitor*> GetFocusedInputMonitorAtCoordinates([[maybe_unused]] const Coordinate coordinates);
		[[nodiscard]] virtual Optional<Input::Monitor*> GetFocusedInputMonitor()
		{
			return nullptr;
		}

		void RotateChildren(const ChildIndex offset);

		[[nodiscard]] LocalCoordinate ConvertWindowToLocalCoordinates(const Coordinate coordinate) const
		{
			return (LocalCoordinate)(coordinate - GetPosition());
		}
		[[nodiscard]] LocalCoordinate ConvertLocalToParentCoordinates(const LocalCoordinate coordinates) const
		{
			const LocalCoordinate delta{GetPosition() - GetParent().GetPosition()};
			return LocalCoordinate{coordinates + delta};
		}
		[[nodiscard]] Coordinate ConvertLocalToWindowCoordinates(const LocalCoordinate coordinates) const
		{
			return Coordinate{(Coordinate)coordinates + GetPosition()};
		}

		using CursorResult = Widgets::CursorResult;
		using PanResult = Widgets::PanResult;
		using PanType = Widgets::PanType;
		using LocalWidgetCoordinate = Widgets::LocalWidgetCoordinate;

		using SetCursorCallback = Widgets::SetCursorCallback;

		[[nodiscard]] CursorResult
		HandleStartTap(const Input::DeviceIdentifier, const uint8 fingerCount, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] CursorResult
		HandleEndTap(const Input::DeviceIdentifier, const uint8 fingerCount, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] CursorResult HandleCancelTap(const Input::DeviceIdentifier);
		[[nodiscard]] CursorResult HandleDoubleTap(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] PanResult HandleStartLongPress(
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalCoordinate coordinate,
			const uint8 fingerCount,
			const Optional<uint16> touchRadius
		);
		[[nodiscard]] CursorResult HandleMoveLongPress(
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalCoordinate coordinate,
			const uint8 fingerCount,
			const Optional<uint16> touchRadius
		);
		[[nodiscard]] CursorResult HandleEndLongPress(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] CursorResult HandleCancelLongPress(const Input::DeviceIdentifier);
		[[nodiscard]] PanResult
		HandleStartPan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&);
		[[nodiscard]] CursorResult
		HandleMovePan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&);
		[[nodiscard]] CursorResult
		HandleEndPan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, const Math::Vector2f velocity);
		[[nodiscard]] CursorResult HandleCancelPan(const Input::DeviceIdentifier);
		[[nodiscard]] CursorResult
		HandleHover(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, SetCursorCallback&);
		[[nodiscard]] CursorResult HandleStartScroll(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta
		);
		[[nodiscard]] CursorResult HandleScroll(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta
		);
		[[nodiscard]] CursorResult HandleEndScroll(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity
		);
		[[nodiscard]] CursorResult HandleCancelScroll(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);

		[[nodiscard]] bool HasContextMenu(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] bool HandleSpawnContextMenu(const LocalCoordinate coordinate, Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] DragAndDropResult
		HandleStartDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&);
		void HandleCancelDragWidgetOverThis();
		[[nodiscard]] DragAndDropResult
		HandleMoveDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&);
		[[nodiscard]] DragAndDropResult
		HandleReleaseDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget);
		void HandleTextInput(const ConstUnicodeStringView input);
		void HandleCopyToPasteboard();
		void HandlePasteFromPasteboard();
		void HandleMoveTextCursor(const EnumFlags<ngine::Input::MoveTextCursorFlags>);
		void HandleApplyTextInput();
		void HandleAbortTextInput();
		void HandleDeleteTextInput(const ngine::Input::DeleteTextType);
		void HandleTabForward();
		void HandleTabBack();

		[[nodiscard]] virtual CursorResult
		OnStartTap(const Input::DeviceIdentifier, const uint8 fingerCount, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] virtual CursorResult
		OnEndTap(const Input::DeviceIdentifier, const uint8 fingerCount, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] virtual CursorResult OnCancelTap(const Input::DeviceIdentifier);
		[[nodiscard]] virtual CursorResult OnDoubleTap(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] virtual PanResult OnStartLongPress(
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			const Optional<uint16> touchRadius
		);
		[[nodiscard]] virtual CursorResult OnMoveLongPress(
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalCoordinate coordinate,
			[[maybe_unused]] const uint8 fingerCount,
			const Optional<uint16> touchRadius
		);
		[[nodiscard]] virtual CursorResult OnEndLongPress(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);
		[[nodiscard]] virtual CursorResult OnCancelLongPress(const Input::DeviceIdentifier);
		[[nodiscard]] virtual PanResult
		OnStartPan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&);
		[[nodiscard]] virtual CursorResult
		OnMovePan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, const Math::Vector2f velocity, const Optional<uint16> touchRadius, SetCursorCallback&);
		[[nodiscard]] virtual CursorResult
		OnEndPan(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, const Math::Vector2f velocity);
		[[nodiscard]] virtual CursorResult OnCancelPan(const Input::DeviceIdentifier);
		[[nodiscard]] virtual CursorResult
		OnHover(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, SetCursorCallback&);
		[[nodiscard]] virtual CursorResult OnStartScroll(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta
		);
		[[nodiscard]] virtual CursorResult
		OnScroll(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta);
		[[nodiscard]] virtual CursorResult OnEndScroll(
			const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity
		);
		[[nodiscard]] virtual CursorResult OnCancelScroll(const Input::DeviceIdentifier, [[maybe_unused]] const LocalCoordinate coordinate);

		[[nodiscard]] virtual ContextMenuBuilder OnSpawnContextMenu(const LocalCoordinate coordinate);

		[[nodiscard]] virtual DragAndDropResult
		OnStartDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&);
		[[nodiscard]] virtual DragAndDropResult OnCancelDragWidgetOverThis();
		[[nodiscard]] virtual DragAndDropResult
		OnMoveDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget, SetCursorCallback&);
		[[nodiscard]] virtual DragAndDropResult
		OnReleaseDragWidgetOverThis([[maybe_unused]] const LocalCoordinate coordinate, [[maybe_unused]] const Widget& otherWidget);
		virtual void OnTextInput(const ConstUnicodeStringView)
		{
		}
		virtual void OnMoveTextCursor([[maybe_unused]] const EnumFlags<ngine::Input::MoveTextCursorFlags>)
		{
		}
		virtual void OnApplyTextInput()
		{
		}
		virtual void OnAbortTextInput()
		{
		}
		virtual void OnDeleteTextInput(const ngine::Input::DeleteTextType)
		{
		}
		bool HasEditableText() const;

		[[nodiscard]] EnumFlags<Style::Modifier>
		GetActiveModifiers(Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData) const;
		[[nodiscard]] EnumFlags<Style::Modifier> GetActiveModifiers(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] EnumFlags<Style::Modifier> GetActiveModifiers() const;

		using ChangedStyleValues = Bitset<(uint8)Style::ValueTypeIdentifier::Count>;
		using ConstChangedStyleValuesView = const Bitset<(uint8)Style::ValueTypeIdentifier::Count>&;

		[[nodiscard]] Style::CombinedEntry GetStyle(
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData
		) const;
		[[nodiscard]] Style::CombinedEntry GetStyle(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Style::CombinedEntry GetStyle() const;
		void ChangeInlineStyle(Style::Entry&& entry, Entity::SceneRegistry& sceneRegistry);
		void ChangeInlineStyle(Style::Entry&& entry);
		void EmplaceInlineStyle(
			Style::Value&& value, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None
		);
		void EmplaceInlineStyle(Style::Value&& value, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None);
		void ClearInlineStyle(
			const Style::ValueTypeIdentifier valueType,
			Entity::SceneRegistry& sceneRegistry,
			const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None
		);
		void ClearInlineStyle(const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None);
		void ChangeDynamicStyle(Style::Entry&& entry, Entity::SceneRegistry& sceneRegistry);
		void ChangeDynamicStyle(Style::Entry&& entry);
		void EmplaceDynamicStyle(
			Style::Value&& value, Entity::SceneRegistry& sceneRegistry, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None
		);
		void EmplaceDynamicStyle(Style::Value&& value, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None);
		void ClearDynamicStyle(
			const Style::ValueTypeIdentifier valueType,
			Entity::SceneRegistry& sceneRegistry,
			const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None
		);
		void ClearDynamicStyle(const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers = Style::Modifier::None);
		void OnActiveStyleModifiersChangedInternal(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedValues
		);
		[[nodiscard]] Style::Size GetStylePreferredSize(
			Entity::SceneRegistry& sceneRegistry,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
		) const;
		[[nodiscard]] Style::Size GetPreferredSize(
			Entity::SceneRegistry& sceneRegistry,
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData,
			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData
		) const;
		[[nodiscard]] Style::Size GetMaximumSize(
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData
		) const;

		void NotifyChildMaximumSizeChanged(Widget&, Entity::SceneRegistry& sceneRegistry);
		void NotifyOnChildPreferredSizeChanged(Widget&, Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] virtual Memory::CallbackResult IterateAttachedItemsAsync(
			const LocalCoordinate coordinate,
			const ArrayView<const Reflection::TypeDefinition> allowedTypes,
			CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
		) const;
		[[nodiscard]] virtual Memory::CallbackResult IterateAttachedItems(
			const LocalCoordinate coordinate,
			const ArrayView<const Reflection::TypeDefinition> allowedTypes,
			CopyableFunction<Memory::CallbackResult(ConstAnyView), 36>&& callback
		) const;

		using DeserializationCallback = Function<void(const Optional<Widget*> pWidget), 24>;
		[[nodiscard]] static Threading::JobBatch Deserialize(
			const Serialization::Reader reader,
			Widget& parent,
			Entity::SceneRegistry& sceneRegistry,
			DeserializationCallback&& callback,
			const Optional<ChildIndex> parentChildIndex = Invalid,
			const EnumFlags<Flags> flags = {}
		);
		[[nodiscard]] static Threading::JobBatch Deserialize(
			const Asset::Guid assetGuid,
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Widget*> pParent,
			DeserializationCallback&& callback,
			const Optional<ChildIndex> parentChildIndex = Invalid,
			const EnumFlags<Flags> flags = {}
		);

		using ContentAreaChangeFlags = Widgets::ContentAreaChangeFlags;

		void OnConstructed();
		void OnCreated();
		void OnDeserialized(const Serialization::Reader reader, Threading::JobBatch& jobBatch);
		Threading::JobBatch DeserializeDataComponentsAndChildren(Serialization::Reader reader);
		bool SerializeDataComponentsAndChildren(Serialization::Writer writer) const;
		void RecalculateHierarchy(Entity::SceneRegistry& sceneRegistry);
		void RecalculateHierarchy(
			Entity::SceneRegistry& sceneRegistry,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
		);
		void RecalculateHierarchy();

		void ToggleModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry);
		void ToggleModifiers(const EnumFlags<Style::Modifier> modifiers);
		void SetModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry);
		void SetModifiers(const EnumFlags<Style::Modifier> modifiers);
		void ClearModifiers(const EnumFlags<Style::Modifier> modifiers, Entity::SceneRegistry& sceneRegistry);
		void ClearModifiers(const EnumFlags<Style::Modifier> modifiers);

		//! Iterate dynamic style entries for which there is no data source
		using StyleEntryCallback = Function<bool(const ngine::DataSource::PropertyIdentifier, Style::EntryValue&), 36>;
		void
		ModifyDynamicStyleEntries(Entity::SceneRegistry& sceneRegistry, const Style::ValueTypeIdentifier, const StyleEntryCallback& callback);
		void ModifyDynamicStyleEntries(const Style::ValueTypeIdentifier, const StyleEntryCallback& callback);

		[[nodiscard]] static EventInfo DeserializeEventInfo(const ConstStringView eventString);
		[[nodiscard]] static EventInfo DeserializeEventInfo(const Serialization::Reader reader);
		[[nodiscard]] static String GetEventInfoKey(const EventInfo& eventInfo);

		void SpawnContextMenu(const ContextMenuBuilder builder, const LocalCoordinate coordinate, Entity::SceneRegistry& sceneRegistry);

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate2D, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		[[nodiscard]] virtual bool ApplyAtPoint(
			const Entity::ApplicableData&, const Math::WorldCoordinate2D, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) override;

#if PROFILE_BUILD
		void SetDebugName(String&& name)
		{
			m_debugName = Forward<String>(name);
		}
		[[nodiscard]] ConstStringView GetDebugName() const
		{
			return m_debugName;
		}
#endif
	protected:
		friend RootWidget;
		friend UIViewMode;
		friend Rendering::ToolWindow;
		friend Rendering::PointerDevice;
		friend Data::ExternalStyle;
		friend Data::InlineStyle;
		friend Data::DynamicStyle;
		friend Data::Modifiers;

		[[nodiscard]] Optional<Data::Drawable*> FindDrawable(
			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
		) const;

		void RecalculateHierarchyInternal(Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& window);
		void ApplyInitialStyle(
			Entity::SceneRegistry& sceneRegistry,
			const Rendering::ScreenProperties screenProperties,
			Rendering::ToolWindow& window,
			Entity::ComponentTypeSceneData<Data::ExternalStyle>& externalStyleSceneData,
			Entity::ComponentTypeSceneData<Data::InlineStyle>& inlineStyleSceneData,
			Entity::ComponentTypeSceneData<Data::DynamicStyle>& dynamicStyleSceneData,
			Entity::ComponentTypeSceneData<Data::Modifiers>& modifiersSceneData
		);
		void ApplyStyle(Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& window);
		void ApplyStyleInternal(
			Entity::SceneRegistry& sceneRegistry, Rendering::ToolWindow& window, const Rendering::ScreenProperties screenProperties
		);
		void ApplyStyleInternal(
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
		);
		void ApplyDrawablePrimitive(
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
		);
		void OnDrawableAdded(Entity::SceneRegistry& sceneRegistry);
		bool RemoveDrawable(Entity::SceneRegistry& sceneRegistry);
		bool RemoveDrawable(
			Entity::SceneRegistry& sceneRegistry,
			Entity::ComponentTypeSceneData<Data::TextDrawable>& textDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::ImageDrawable>& imageDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::RoundedRectangleDrawable>& roundedRectangleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::RectangleDrawable>& rectangleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::CircleDrawable>& circleDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::GridDrawable>& gridDrawableSceneData,
			Entity::ComponentTypeSceneData<Data::Primitives::LineDrawable>& lineDrawableSceneData
		);

		void UpdateFromResolvedDataSourceInternal(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			const DataSourceProperties&,
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
		);
		[[nodiscard]] bool RequiresResolvedDataSource(
			Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
			Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData,
			Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData
		) const;
		Optional<ngine::DataSource::Interface*> ResolveInlineDataSource(
			DataSourceProperties& ResolvePropertySource, Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData
		);
		void ResolvePropertySource(
			DataSourceProperties& ResolvePropertySource, Entity::ComponentTypeSceneData<Data::PropertySource>& propertySourceSceneData
		);
		Optional<ngine::DataSource::Interface*> ResolveDataSourceEntry(
			DataSourceProperties& ResolvePropertySource,
			Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData,
			Entity::ComponentTypeSceneData<Data::DataSourceEntry>& dataSourceEntrySceneData
		);
		void UpdateDynamicTagQueryFromDataSourceInternal(
			Entity::SceneRegistry& sceneRegistry,
			const DataSourceProperties& dataSourceProperties,
			Entity::ComponentTypeSceneData<Data::FlexLayout>& flexLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::GridLayout>& gridLayoutSceneData,
			Entity::ComponentTypeSceneData<Data::DataSource>& dataSourceSceneData
		);
		void UpdateDynamicStyleFromDataSourceInternal(
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
		);

		virtual LoadResourcesResult TryLoadResources(Entity::SceneRegistry& sceneRegistry);
		[[nodiscard]] Threading::JobBatch ReadProperties(const Serialization::Reader reader, Entity::SceneRegistry& sceneRegistry);

		virtual void OnBeforeContentAreaChanged(const EnumFlags<ContentAreaChangeFlags>)
		{
		}
		virtual void OnContentAreaChanged(const EnumFlags<ContentAreaChangeFlags>)
		{
		}

		virtual void OnFramegraphInvalidated()
		{
		}
		virtual void BuildFramegraph(Rendering::FramegraphBuilder&)
		{
		}
		virtual void OnFramegraphBuilt()
		{
		}
		virtual void OnEnableFramegraph()
		{
		}
		virtual void OnDisableFramegraph()
		{
		}

		virtual void OnAttachedToNewParent() override;
		virtual void OnChildAttached(
			[[maybe_unused]] HierarchyComponentBase& newChildComponent, const ChildIndex index, const Optional<ChildIndex> preferredChildIndex
		) override;

		void RemoveChildInternal(Widget&, Entity::SceneRegistry& sceneRegistry);
		void OnParentSetToggledOff();
		void OnParentClearedToggledOff();
		void OnParentReceivedRequirements();
		void OnParentLostRequirements();
		void OnParentPassedValidation();
		void OnParentFailedValidation();

		void OnReceivedHoverFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnLostHoverFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnChildReceivedHoverFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnChildLostHoverFocusInternal(Entity::SceneRegistry& sceneRegistry);

		void SetHasInputFocus();
		void ClearHasInputFocus();

		void OnReceivedInputFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnLostInputFocusInternal(Entity::SceneRegistry& sceneRegistry);

		void OnReceivedActiveFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnLostActiveFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnChildReceivedActiveFocusInternal(Entity::SceneRegistry& sceneRegistry);
		void OnChildLostActiveFocusInternal(Entity::SceneRegistry& sceneRegistry);

		void OnParentBecomeVisible(Entity::SceneRegistry& sceneRegistry);
		void OnParentBecomeHidden(Entity::SceneRegistry& sceneRegistry);

		void RecalculateSubsequentDepthInfo();

		virtual void OnBecomeVisible()
		{
		}
		virtual void OnBecomeHidden()
		{
		}

		virtual void OnSwitchToForeground()
		{
		}
		virtual void OnSwitchToBackground()
		{
		}

		void OnDisplayPropertiesChanged(Entity::SceneRegistry& sceneRegistry);
	private:
		void OnBecomeVisibleInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow);
		void OnBecomeHiddenInternal(Entity::SceneRegistry& sceneRegistry);
		void OnIgnoredInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow);
		void OnUnignoredInternal(Entity::SceneRegistry& sceneRegistry, const Optional<Rendering::ToolWindow*> pWindow);
		void OnContentAreaChangedInternal(Entity::SceneRegistry& sceneRegistry, const EnumFlags<ContentAreaChangeFlags>);

		void OnStyleChangedInternal(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedValues
		);
		void OnStyleChangedInternal(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedValues,
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
		);
		virtual void OnStyleChanged(
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedValues
		);

		void ModifyDynamicStyleEntriesInternal(
			Entity::SceneRegistry& sceneRegistry,
			const Optional<Rendering::ToolWindow*> pWindow,
			const Style::ValueTypeIdentifier,
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
		);

		friend struct Reflection::ReflectedType<Widget>;

		void SetPositionFromProperty(WidgetPositionInfo transform);
		[[nodiscard]] WidgetPositionInfo GetPositionFromProperty() const;
		void SetRotationFromProperty(Math::Anglef);
		[[nodiscard]] Math::Anglef GetRotationFromProperty() const;
		void SetSizeFromProperty(WidgetSizeInfo transform);
		[[nodiscard]] WidgetSizeInfo GetSizeFromProperty() const;
		void SetLayoutFromProperty(LayoutType layoutType);
		[[nodiscard]] LayoutType GetLayoutFromProperty() const;
		void SetPaddingFromProperty(Style::SizeAxisEdges);
		[[nodiscard]] Style::SizeAxisEdges GetPaddingFromProperty() const;
		void SetOrientationFromProperty(Orientation orientation);
		[[nodiscard]] Orientation GetOrientationFromProperty() const;
		void SetAlignmentFromProperty(WidgetAlignmentInfo alignment);
		[[nodiscard]] WidgetAlignmentInfo GetAlignmentFromProperty() const;
		void SetGapFromProperty(Style::SizeAxisExpression);
		[[nodiscard]] Style::SizeAxisExpression GetGapFromProperty() const;
		void SetOverflowTypeFromProperty(OverflowType orientation);
		[[nodiscard]] OverflowType GetOverflowTypeFromProperty() const;

		void SetBackgroundAssetFromProperty(Asset::Picker backgroundAsset);
		[[nodiscard]] Asset::Picker GetBackgroundAssetFromProperty() const;
		void SetBackgroundColorFromProperty(Math::Color color);
		[[nodiscard]] Math::Color GetBackgroundColorFromProperty() const;
		void SetOpacityFromProperty(Math::Ratiof ratio);
		[[nodiscard]] Math::Ratiof GetOpacityFromProperty() const;
		void SetBackgroundRoundingRadiusFromProperty(Style::SizeAxisExpression);
		[[nodiscard]] Style::SizeAxisExpression GetBackgroundRoundingRadiusFromProperty() const;

		void SetBorderFromProperty(Math::Color color);
		[[nodiscard]] Math::Color GetBorderFromProperty() const;
		void SetBorderWidthFromProperty(Style::SizeAxisExpression);
		[[nodiscard]] Style::SizeAxisExpression GetBorderWidthFromProperty() const;
	private:
#if PROFILE_BUILD
		String m_debugName;
#endif

		ReferenceWrapper<RootWidget> m_rootWidget;

		AtomicEnumFlags<Flags> m_flags;

		ngine::DataSource::PropertyIdentifier m_windowTitlePropertyIdentifier;
		ngine::DataSource::PropertyIdentifier m_windowUriPropertyIdentifier;

		DepthInfo m_depthInfo;

		[[nodiscard]] bool NotifyEvent(Entity::SceneRegistry& sceneRegistry, const EventType eventType);

		Threading::SharedMutex m_deferredEventMutex;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Widget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Widget>(
			"c6866385-3b27-4853-8f59-ce098fe20273"_guid,
			MAKE_UNICODE_LITERAL("Widget"),
			TypeFlags{},
			Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Position"),
					"position",
					"60f158ef-6d62-42a4-bbc9-be8d50d17f00"_guid,
					MAKE_UNICODE_LITERAL("Transform"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetPositionFromProperty,
					&Widgets::Widget::GetPositionFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Rotation"),
					"rotation",
					"11e0aca4-b8af-43e4-8ba1-af6739e6f116"_guid,
					MAKE_UNICODE_LITERAL("Transform"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetRotationFromProperty,
					&Widgets::Widget::GetRotationFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Size"),
					"size",
					"f854abbb-23a9-4053-8580-fa2bdb187aed"_guid,
					MAKE_UNICODE_LITERAL("Transform"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetSizeFromProperty,
					&Widgets::Widget::GetSizeFromProperty
				),

				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Type"),
					"layoutType",
					"138ef080-0e5e-4c22-bbb9-673cca62dca0"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetLayoutFromProperty,
					&Widgets::Widget::GetLayoutFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Padding"),
					"padding",
					"fb5e914e-74e4-4427-b0fa-718ed524e1c1"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetPaddingFromProperty,
					&Widgets::Widget::GetPaddingFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Direction"),
					"direction",
					"c5259a15-3c8a-4885-b014-cda646bd378b"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetOrientationFromProperty,
					&Widgets::Widget::GetOrientationFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Alignment"),
					"alignment",
					"8ee45404-1c95-4eb2-b92e-39029e4a7c6b"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetAlignmentFromProperty,
					&Widgets::Widget::GetAlignmentFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Gap"),
					"gap",
					"89a81cdb-92ff-4273-b72f-badf146301e3"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetGapFromProperty,
					&Widgets::Widget::GetGapFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Type"),
					"overflowType",
					"e463cc4a-1e5e-45ae-a370-c8635695d342"_guid,
					MAKE_UNICODE_LITERAL("Layout"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetOverflowTypeFromProperty,
					&Widgets::Widget::GetOverflowTypeFromProperty
				),

				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Image"),
					"image",
					"284d3772-7fd0-479a-b2c4-6dda8e299036"_guid,
					MAKE_UNICODE_LITERAL("Background"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetBackgroundAssetFromProperty,
					&Widgets::Widget::GetBackgroundAssetFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"2e476e2a-db83-4aa4-9f89-85ad5d8fa092"_guid,
					MAKE_UNICODE_LITERAL("Background"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetBackgroundColorFromProperty,
					&Widgets::Widget::GetBackgroundColorFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Opacity"),
					"opacity",
					"df5357ad-b5ec-4bda-9408-f3a0bf72659b"_guid,
					MAKE_UNICODE_LITERAL("Background"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetOpacityFromProperty,
					&Widgets::Widget::GetOpacityFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"rounding",
					"de3e9e16-03f1-4056-a2ab-c0be72cb786c"_guid,
					MAKE_UNICODE_LITERAL("Background"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetBackgroundRoundingRadiusFromProperty,
					&Widgets::Widget::GetBackgroundRoundingRadiusFromProperty
				),

				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"borderColor",
					"565683ed-3e05-442f-9c13-8cda38eec793"_guid,
					MAKE_UNICODE_LITERAL("Border"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetBorderFromProperty,
					&Widgets::Widget::GetBorderFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Width"),
					"borderWidth",
					"a1a4c3d5-2b5c-4dc7-ab85-cd52af4c53fe"_guid,
					MAKE_UNICODE_LITERAL("Border"),
					Reflection::PropertyFlags::Transient,
					&Widgets::Widget::SetBorderWidthFromProperty,
					&Widgets::Widget::GetBorderWidthFromProperty
				),
			}
		);
	};
}
