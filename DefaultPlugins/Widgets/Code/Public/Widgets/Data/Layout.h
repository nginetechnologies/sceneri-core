#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/Orientation.h>
#include <Widgets/OverflowType.h>
#include <Widgets/Style/Size.h>
#include <Widgets/Style/SizeEdges.h>
#include <Widgets/Style/Modifier.h>
#include <Widgets/Style/ValueTypeIdentifier.h>
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>
#include <Widgets/ContentAreaChangeFlags.h>

#include <Engine/Input/DeviceIdentifier.h>

#include <Engine/DataSource/CachedQuery.h>

#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Time/Stopwatch.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::DataSource
{
	struct Interface;
}

namespace ngine::Widgets
{
	struct CursorResult;
	struct PanResult;
	struct SetCursorCallback;

	namespace Style
	{
		struct CombinedEntry;
	}
}

namespace ngine::Widgets::Data
{
	struct DataSource;

	struct Layout : public Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;
		using BaseType = Component;

		Layout(Initializer&& initializer);
		virtual ~Layout();

		using ConstChangedStyleValuesView = const Bitset<(uint8)Style::ValueTypeIdentifier::Count>&;
		//! Returns true if any of our properties were changed
		[[nodiscard]] virtual bool OnStyleChanged(
			Widget& owner,
			Entity::SceneRegistry& sceneRegistry,
			const Style::CombinedEntry& combinedEntry,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedStyleValues
		);

		virtual CursorResult OnStartScroll(Widget& owner, const Input::DeviceIdentifier, const Math::Vector2i, const Math::Vector2i delta);
		virtual CursorResult OnScroll(Widget& owner, const Input::DeviceIdentifier, const Math::Vector2i, const Math::Vector2i delta);
		virtual CursorResult OnEndScroll(Widget& owner, const Input::DeviceIdentifier, const Math::Vector2i, const Math::Vector2f velocity);
		virtual CursorResult OnCancelScroll(Widget& owner, const Input::DeviceIdentifier, const Math::Vector2i);
		virtual PanResult OnStartPan(
			Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const Math::Vector2i coordinate,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const Optional<uint16> touchRadius,
			SetCursorCallback& pointer
		);
		virtual CursorResult OnMovePan(
			Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const Math::Vector2i coordinate,
			[[maybe_unused]] const Math::Vector2i delta,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const Optional<uint16> touchRadius,
			SetCursorCallback& pointer
		);
		virtual CursorResult
		OnEndPan(Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const Math::Vector2i coordinate, const Math::Vector2f velocity);

		[[nodiscard]] Math::Rectanglei GetAvailableChildContentArea(const Widget&, Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Math::Vector2i GetNextAutoChildPosition(const Widget& owner, Entity::SceneRegistry& sceneRegistry) const;

		[[nodiscard]] Style::SizeAxisEdges GetPadding() const
		{
			return m_padding;
		}
		[[nodiscard]] Style::Size GetChildOffset() const
		{
			return m_childOffset;
		}
		[[nodiscard]] Orientation GetChildOrientation() const
		{
			return m_childOrientation;
		}
		void SetChildOrientation(Widget&, const Orientation orientation)
		{
			m_childOrientation = orientation;
		}

		[[nodiscard]] int32 GetScrollPosition() const
		{
			return -(int32)Math::Floor(m_scrollPosition);
		}

		void SetScrollPosition(float position, Entity::SceneRegistry& sceneRegistry)
		{
			SetVirtualScrollPosition(m_owner, sceneRegistry, position);
		}

		void SetMaximumScrollPosition(Widget&, const float position)
		{
			m_maximumScrollPosition = position;
		}

		[[nodiscard]] virtual Math::Vector2i GetInitialChildLocation(
			const Widget& owner, Entity::SceneRegistry& sceneRegistry, const Widget& childWidget, const Math::Vector2i size
		) const = 0;

		enum class Flags : uint8
		{
			HasWidgetData = 1 << 0,
			AwaitingWidgetData = 1 << 1
		};
		virtual void PopulateItemWidgets(Widget& owner, Entity::SceneRegistry& sceneRegistry) = 0;
		bool UpdateVirtualItemVisibility(Widget& owner, Entity::SceneRegistry& sceneRegistry);

		void OnDataSourceChanged(Widget& owner);
		[[nodiscard]] bool HasDataSource(Widget& owner, Entity::SceneRegistry& sceneRegistry) const;

		void OnDataSourcePropertiesChanged(Widget& owner, Entity::SceneRegistry& sceneRegistry);

		void Update();
		void OnDestroying();
		[[nodiscard]] uint32 GetFilteredDataCount(Widget& owner, Entity::SceneRegistry& sceneRegistry) const;
		void CacheOutOfDateDataSourceQuery(Widget& owner, Entity::SceneRegistry& sceneRegistry) const;

		void UpdateViewToShowItemAtIndex(Widget& owner, Entity::SceneRegistry& sceneRegistry, const uint32 index);
		void UpdateViewToShowVirtualItemAtIndex(Widget& owner, Entity::SceneRegistry& sceneRegistry, const uint32 index);

		[[nodiscard]] Optional<ngine::DataSource::CachedQuery*> GetCachedDataQuery() const LIFETIME_BOUND
		{
			return m_pCachedDataQuery.Get();
		}
		[[nodiscard]] Optional<ngine::DataSource::SortedQueryIndices*> GetCachedDataSortingQuery() const LIFETIME_BOUND
		{
			return m_pCachedDataSortingQuery.Get();
		}
	protected:
		void SetVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, const float position);
		//! Called from a direct or indrect parent when we are a fit-content layout that is contained inside an infinite scroll view
		virtual void SetInlineVirtualScrollPosition(
			Widget& owner, Entity::SceneRegistry& sceneRegistry, const float previousParentPosition, const float newParentPosition
		);
		virtual float CalculateNewScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, float position) = 0;
		virtual bool UpdateVirtualItemVisibilityInternal(Widget& owner, Entity::SceneRegistry& sceneRegistry) = 0;

		[[nodiscard]] virtual float GetMaximumVirtualScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry) const;

		virtual void EnableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) = 0;
		virtual void DisableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) = 0;

		void ResetCachedDataSourceView()
		{
			// Force data index update
			m_lastDataIndexStart = Math::NumericLimits<ngine::DataSource::GenericDataIdentifier::IndexType>::Max;
			m_lastDataIndexEnd = Math::NumericLimits<ngine::DataSource::GenericDataIdentifier::IndexType>::Max;
		}

		void RequestMoreData(Widget& owner, Entity::SceneRegistry& sceneRegistry);
	protected:
		friend struct Reflection::ReflectedType<Layout>;

		Widget& m_owner;
		EnumFlags<Flags> m_flags;

		mutable UniquePtr<ngine::DataSource::CachedQuery> m_pCachedDataQuery;
		mutable UniquePtr<ngine::DataSource::SortedQueryIndices> m_pCachedDataSortingQuery;

		Style::SizeAxisEdges m_margin;
		Style::SizeAxisEdges m_padding;
		Style::Size m_childOffset;
		Orientation m_childOrientation = Orientation::Horizontal;
		OverflowType m_overflowType = DefaultOverflowType;

		float m_scrollPosition = 0.f;
		float m_virtualScrollPosition = 0.f;
		float m_maximumScrollPosition = 0.f;
		float m_startPanCoordinate;
		float m_startPanScrollPosition;
		float m_requestedPanAcceleration = 0.f;
		Threading::Atomic<bool> m_shouldResetPanVelocity = false;
		float m_panVelocity = 0.f;
		Time::Stopwatch m_stopwatch;

		ngine::DataSource::GenericDataIndex m_lastDataIndexStart{Math::NumericLimits<ngine::DataSource::GenericDataIdentifier::IndexType>::Max};
		ngine::DataSource::GenericDataIndex m_lastDataIndexEnd{Math::NumericLimits<ngine::DataSource::GenericDataIdentifier::IndexType>::Max};
		ngine::DataSource::GenericDataIndex m_lastActiveIndex{Math::NumericLimits<ngine::DataSource::GenericDataIdentifier::IndexType>::Max};
	};

	ENUM_FLAG_OPERATORS(Layout::Flags);

	struct FlexLayout final : public Layout
	{
		using BaseType = Layout;
		using Layout::Layout;

		virtual void EnableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;
		virtual void DisableUpdate(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;

		virtual void PopulateItemWidgets(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;
		virtual bool UpdateVirtualItemVisibilityInternal(Widget& owner, Entity::SceneRegistry& sceneRegistry) override;
		virtual float CalculateNewScrollPosition(Widget& owner, Entity::SceneRegistry& sceneRegistry, float position) override;
		[[nodiscard]] virtual Math::Vector2i GetInitialChildLocation(
			const Widget& owner, Entity::SceneRegistry& sceneRegistry, const Widget& childWidget, const Math::Vector2i size
		) const override;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Layout>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Layout>(
			"{DC18D979-9AAF-4B86-AB25-928A76753F98}"_guid,
			MAKE_UNICODE_LITERAL("Widget Layout"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::IsAbstract
		);
	};

	template<>
	struct ReflectedType<Widgets::Data::FlexLayout>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::FlexLayout>(
			"872AA2F8-A0E0-4996-B2B8-5B08A31532B8"_guid,
			MAKE_UNICODE_LITERAL("Widget Flex Layout"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk
		);
	};
}
