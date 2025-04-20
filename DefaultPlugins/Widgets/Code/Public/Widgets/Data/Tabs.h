#pragma once

#include <Widgets/Data/Input.h>

#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/PropertySourceInterface.h>

namespace ngine::DataSource
{
	struct Cache;
}

namespace ngine::Widgets::Data
{
	struct Tabs final : public Widgets::Data::Input, public ngine::DataSource::Interface, public ngine::PropertySource::Interface
	{
		using BaseType = Widgets::Data::Input;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		struct Initializer : public Tabs::BaseType::Initializer
		{
			using BaseType = Tabs::BaseType::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, const Guid dataSourceGuid, const Guid propertySourceGuid)
				: BaseType(Forward<BaseType>(initializer))
				, m_dataSourceGuid(dataSourceGuid)
				, m_propertySourceGuid(propertySourceGuid)
			{
			}

			Guid m_dataSourceGuid;
			Guid m_propertySourceGuid;
		};

		Tabs(Initializer&& initializer);
		Tabs(const Deserializer& deserializer);
		Tabs(const Tabs& templateComponent, const Cloner& cloner);
		Tabs(const Tabs&) = delete;
		Tabs(Tabs&&) = delete;
		Tabs& operator=(const Tabs&) = delete;
		Tabs& operator=(Tabs&&) = delete;
		virtual ~Tabs();

		void OnCreated(Widget& parent);
		void OnDestroying(Widget& parent);

		// Data::Input
		[[nodiscard]] virtual PanResult
		OnStartPan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
			override;
		[[nodiscard]] virtual CursorResult
		OnMovePan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate, [[maybe_unused]] const Math::Vector2i delta, [[maybe_unused]] const Math::Vector2f velocity, [[maybe_unused]] const Optional<uint16> touchRadius, SetCursorCallback&)
			override;
		[[nodiscard]] virtual CursorResult OnEndPan(
			[[maybe_unused]] Widget& owner,
			const Input::DeviceIdentifier,
			[[maybe_unused]] const LocalWidgetCoordinate coordinate,
			[[maybe_unused]] const Math::Vector2f velocity
		) override;
		[[nodiscard]] virtual CursorResult OnCancelPan([[maybe_unused]] Widget& owner, const Input::DeviceIdentifier) override;

		virtual void OnTabForward(Widget& owner) override;
		virtual void OnTabBack(Widget& owner) override;

		virtual void OnChildAdded(Widget& owner, Widget& child, const uint32 childIndex, const Optional<uint16> preferredChildIndex) override;
		virtual void OnChildRemoved(Widget& owner, Widget& child, [[maybe_unused]] const uint32 previousChildIndex) override;
		// ~Data::Input

		// DataSource::Interface
		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override final;
		virtual void IterateData(
			const CachedQuery& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual void IterateData(
			const SortedQueryIndices& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual ngine::DataSource::PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const override final;
		// ~DataSource::Interface

		// PropertySource::Interface
		[[nodiscard]] virtual ngine::DataSource::PropertyValue GetDataProperty(const PropertyIdentifier identifier) const override final;
		// ~PropertySource::Interface

		[[nodiscard]] Optional<Widget*> GetActiveTabWidget() const;
		inline static constexpr uint16 InvalidTabIndex = Math::NumericLimits<uint16>::Max;
		[[nodiscard]] uint16 GetActiveTabIndex() const
		{
			return m_activeTabIndex;
		}

		enum class TabCycleMode : uint8
		{
			Next,
			Previous
		};

		[[nodiscard]] uint16 GetTabIndex(Widget& ownerWidget, Widget& widget);
		void SetActiveTab(Widget& ownerWidget, const uint16 index);
		void CloseTab(Widget& ownerWidget, const uint16 index);
		void CycleTab(Widget& ownerWidget, TabCycleMode cycleMode);

		void DetachGrabbedTab();

		enum class Flags : uint8
		{
			//! Disallow not having an active tab if any exist
			RequiresActiveTab = 1 << 0,
			AutoActivateTab = 1 << 1,
			IsDraggingTab = 1 << 2,
			Default = RequiresActiveTab | AutoActivateTab
		};

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		Tabs(
			Widget& owner,
			ngine::DataSource::Cache& dataSourceCache,
			const Guid dataSourceGuid,
			const Guid propertySourceGuid,
			const Guid deactivateEventGuid,
			const Guid becomeActiveEventGuid,
			const Guid becomeInactiveEventGuid,
			const bool applyContext
		);
	protected:
		Widget& m_owner;

		Guid m_dataSourceGuid;
		Guid m_propertySourceGuid;

		Guid m_deactivateEventGuid;
		Guid m_becomeActiveEventGuid;
		Guid m_becomeInactiveEventGuid;

		uint16 m_activeTabIndex = InvalidTabIndex;

		EnumFlags<Flags> m_flags{Flags::Default};

		const ngine::DataSource::PropertyIdentifier m_tabContentWidgetPropertyIdentifier;
		const ngine::DataSource::PropertyIdentifier m_tabActivePropertyIdentifier;
		const ngine::DataSource::PropertyIdentifier m_hasNoActiveTabPropertyIdentifier;

		Vector<Guid, uint16> m_tabAssetGuids;
	};

	ENUM_FLAG_OPERATORS(Tabs::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Tabs>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Tabs>(
			"{7F27C7C9-4B67-4D50-8969-A8014A69FF2F}"_guid,
			MAKE_UNICODE_LITERAL("Tabs"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation
		);
	};
}
