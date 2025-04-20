#include <Widgets/Data/ModalTrigger.h>

#include <Widgets/ToolWindow.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/DefaultStyles.h>
#include <Widgets/Widget.inl>
#include <Widgets/RootWidget.h>
#include <Widgets/Data/PropertySource.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Manager.h>

#include <Engine/Input/InputManager.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Context/EventManager.inl>
#include <Engine/Context/Utils.h>
#include <Engine/Context/Reference.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/HierarchyComponent.inl>

#include <Common/System/Query.h>
#include <Common/Math/Vector2/Select.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	struct ModalFocusListener final : public Widgets::Data::Input
	{
		using BaseType = Widgets::Data::Input;
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		struct Initializer : public Widgets::Data::Input::Initializer
		{
			using BaseType = Widgets::Data::Input::Initializer;

			Initializer(BaseType&& baseInitializer, Widget& modalTriggerOwner, ModalTrigger& modalTrigger)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_modalTriggerOwner(modalTriggerOwner)
				, m_modalTrigger(modalTrigger)
			{
			}

			Widget& m_modalTriggerOwner;
			ModalTrigger& m_modalTrigger;
		};

		ModalFocusListener(Initializer&& initializer)
			: m_modalTriggerOwner(initializer.m_modalTriggerOwner)
			, m_modalTrigger(initializer.m_modalTrigger)
		{
		}
		ModalFocusListener(const ModalTrigger&) = delete;
		ModalFocusListener(ModalTrigger&&) = delete;
		ModalFocusListener& operator=(const ModalFocusListener&) = delete;
		ModalFocusListener& operator=(ModalFocusListener&&) = delete;

		// Data::Input
		virtual void OnLostInputFocus(Widget& owner) override
		{
			if (!owner.IsInputFocusInside())
			{
				m_modalTrigger.OnModalLostFocus(m_modalTriggerOwner);
			}
		}
		virtual void OnChildLostInputFocus(Widget& owner) override
		{
			if (!owner.IsInputFocusInside())
			{
				m_modalTrigger.OnModalLostFocus(m_modalTriggerOwner);
			}
		}
		// ~Data::Input
	protected:
		Widget& m_modalTriggerOwner;
		ModalTrigger& m_modalTrigger;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ModalFocusListener>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ModalFocusListener>(
			"E0353E1E-D7E2-4DFC-BDB8-406740101CC4"_guid,
			MAKE_UNICODE_LITERAL("Modal Focus Listener"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}

namespace ngine::Widgets::Data
{
	ModalTrigger::ModalTrigger(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_flags(initializer.m_flags)
		, m_modalWidgetAssetGuid(initializer.m_modalWidgetAssetGuid)
	{
		initializer.GetParent().EnableInput();
	}

	ModalTrigger::ModalTrigger(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_flags(
				Flags::SpawnOnTap |
				Flags::SpawnAtCursorLocation * deserializer.m_reader.ReadWithDefaultValue<bool>("spawn_at_cursor_location", false) |
				Flags::CloseOnEvent * deserializer.m_reader.ReadWithDefaultValue<bool>("close_on_event", false)
			)
		, m_modalWidgetAssetGuid(deserializer.m_reader.ReadWithDefaultValue<Guid>("modal_asset", Guid{}))
	{
		deserializer.GetParent().EnableInput();
	}

	ModalTrigger::ModalTrigger(const ModalTrigger& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_flags(templateComponent.m_flags)
		, m_modalWidgetAssetGuid(templateComponent.m_modalWidgetAssetGuid)
	{
		cloner.GetParent().EnableInput();
	}

	void ModalTrigger::OnCreated(Widget&)
	{
	}

	void ModalTrigger::OnDestroying(Widget& owner)
	{
		if (m_pSpawnedWidget.IsValid())
		{
			Close(owner);
		}
	}

	Widgets::CursorResult ModalTrigger::OnStartTap(
		[[maybe_unused]] Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate
	)
	{
		if (m_flags.IsSet(Flags::SpawnOnTap))
		{
			return CursorResult{owner};
		}
		else
		{
			return CursorResult{};
		}
	}

	[[nodiscard]] Widgets::CursorResult
	ModalTrigger::OnEndTap(Widget& owner, const Input::DeviceIdentifier, [[maybe_unused]] const LocalWidgetCoordinate coordinate)
	{
		if (m_flags.IsSet(Flags::SpawnOnTap))
		{
			if (m_pSpawnedWidget.IsInvalid())
			{
				const WindowCoordinate clickLocation = owner.ConvertLocalToWindowCoordinates(coordinate);
				System::Get<Threading::JobManager>().QueueCallback(
					[this, &owner, clickLocation](Threading::JobRunnerThread&)
					{
						SpawnModal(owner, clickLocation);
					},
					Threading::JobPriority::UserInterfaceAction
				);
			}
			else
			{
				Close(owner);
			}
			return CursorResult{owner};
		}
		else
		{
			return CursorResult{};
		}
	}

	bool ModalTrigger::SpawnModal(Widget& owner, const WindowCoordinate clickLocation)
	{
		if (m_pSpawnedWidget.IsValid())
		{
			return false;
		}

		Rendering::ToolWindow& owningWindow = *owner.GetOwningWindow();
		Entity::SceneRegistry& sceneRegistry = owningWindow.GetEntitySceneRegistry();

		UniquePtr<Widgets::Style::Entry> pStyle = []()
		{
			UniquePtr<Widgets::Style::Entry> pStyle{Memory::ConstructInPlace};
			Widgets::Style::Entry::ModifierValues& modifier = pStyle->EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
			modifier.ParseFromCSS("position: dynamic; width: 100%; height: 100%;");
			pStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
			pStyle->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());
			return pStyle;
		}();

		Optional<Widgets::Widget*> pModalParent = owningWindow.GetRootWidget();
		if (const Optional<Widgets::Widget*> pCurrentFocusWidget = owningWindow.GetInputFocusWidget())
		{
			if (const Entity::DataComponentResult<ModalFocusListener> modalFocusListener = pCurrentFocusWidget->FindFirstDataComponentOfTypeInParents<ModalFocusListener>(sceneRegistry))
			{
				// The current focus was a modal, spawn inside that
				pModalParent = modalFocusListener.m_pDataComponentOwner;
			}
		}

		Optional<Widgets::Widget*> pModalWidget = sceneRegistry.GetOrCreateComponentTypeData<Widgets::Widget>()->CreateInstance(
			Widgets::Widget::Initializer{*pModalParent, Widget::Flags::IsHidden | Widget::Flags::IsInputDisabled, Move(pStyle)}
		);

		if (pModalWidget)
		{
			if (!pModalWidget->HasDataComponentOfType<Context::Data::Component>(sceneRegistry))
			{
				// Create a reference to the context of the triggering component if present
				// This ensures that local event identifiers are still accessible from the popup (which can be spawned on the window root or
				// even from a separate window)
				if (Entity::DataComponentResult<Context::Data::Component> pResult = Context::Utils::FindContext(owner, sceneRegistry))
				{
					pModalWidget->CreateDataComponent<Context::Data::Reference>(
						sceneRegistry,
						Context::Data::Reference::Initializer{
							Entity::Data::Component::Initializer{},
							pResult.m_pDataComponentOwner ? pResult.m_pDataComponentOwner->GetIdentifier() : owner.GetIdentifier()
						}
					);
				}
			}
		}

		Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
			m_modalWidgetAssetGuid,
			sceneRegistry,
			pModalWidget,
			[ownerReference = Entity::ComponentSoftReference{owner, sceneRegistry}, &sceneRegistry, clickLocation, pModalWidget](
				const Optional<Widgets::Widget*> pWidget
			)
			{
				Assert(pWidget.IsValid());
				if (pWidget.IsValid())
				{
					const Optional<Widgets::Widget*> pOwner = ownerReference.Find<Widgets::Widget>(sceneRegistry);
					if (pOwner.IsInvalid())
					{
						return;
					}

					const Optional<ModalTrigger*> pModalTrigger = pOwner->FindFirstDataComponentImplementingType<ModalTrigger>(sceneRegistry);
					if (pModalTrigger.IsInvalid())
					{
						return;
					}

					Assert(pModalTrigger->m_pSpawnedWidget.IsInvalid());

					Rendering::ToolWindow& owningWindow = *pOwner->GetOwningWindow();

					pWidget->CreateDataComponent<ModalFocusListener>(
						sceneRegistry,
						ModalFocusListener::Initializer{Widgets::Data::Component::Initializer{*pWidget, sceneRegistry}, *pOwner, *pModalTrigger}
					);

					// Clone the triggering component's data source if this widget didn't have one
					if (!pWidget->HasDataComponentOfType<Data::DataSource>(sceneRegistry))
					{
						if (const Optional<const Data::DataSource*> pTemplateDataSource = pOwner->FindDataComponentOfType<Data::DataSource>(sceneRegistry))
						{
							Threading::JobBatch jobBatch;
							pWidget->CreateDataComponent<Data::DataSource>(
								sceneRegistry,
								*pTemplateDataSource,
								Data::DataSource::Cloner{jobBatch, *pWidget, *pOwner, sceneRegistry, sceneRegistry}
							);
							Assert(jobBatch.IsInvalid());
						}
					}

					// Clone the triggering component's property source if this widget didn't have one
					if (!pWidget->HasDataComponentOfType<Data::PropertySource>(sceneRegistry))
					{
						if (const Optional<const Data::PropertySource*> pTemplatePropertySource = pOwner->FindDataComponentOfType<Data::PropertySource>(sceneRegistry))
						{
							Threading::JobBatch jobBatch;
							pWidget->CreateDataComponent<Data::PropertySource>(
								sceneRegistry,
								*pTemplatePropertySource,
								Data::PropertySource::Cloner{jobBatch, *pWidget, *pOwner, sceneRegistry, sceneRegistry}
							);
							Assert(jobBatch.IsInvalid());
						}
					}

					pModalTrigger->m_pSpawnedWidget = pWidget;

					pModalTrigger->OnSpawnedModal(*pOwner, *pWidget);

					pOwner->RecalculateHierarchy();

					Math::Vector2i newWidgetLocation;
					const Math::Vector2i modalSize = pWidget->GetSize();
					const Math::Vector2i clientAreaSize = (Math::Vector2i)owningWindow.GetClientAreaSize();
					const Math::Vector2i padding = Widgets::Style::Size{4_px, 4_px}.Get(clientAreaSize, owningWindow.GetCurrentScreenProperties());
					const Math::Rectanglei spawnableArea{Math::Vector2i{padding}, clientAreaSize - padding * 2};
					if (pModalTrigger->m_flags.IsSet(Flags::SpawnAtCursorLocation))
					{
						newWidgetLocation = (Math::Vector2i)clickLocation;
						const Math::Vector2i newWidgetEndLocation = newWidgetLocation + modalSize;

						// Spawn widget on the left / top of the cursor if it would exceed window bounds
						newWidgetLocation -= Math::Select(newWidgetEndLocation > spawnableArea.GetEndPosition(), modalSize, Math::Vector2i{Math::Zero});
					}
					else
					{
						// Spawn the widget at our parent widget's lower left position by default
						const Math::Rectanglei ownerContentArea = pOwner->GetContentArea();
						newWidgetLocation = ownerContentArea.GetPosition();
						newWidgetLocation.y += ownerContentArea.GetSize().y;

						const Math::Vector2i newWidgetEndLocation = newWidgetLocation + modalSize;

						// Spawn widget on left / top of the cursor if it would exceed window bounds
						newWidgetLocation -= Math::Select(
							newWidgetEndLocation > spawnableArea.GetEndPosition(),
							modalSize - Math::Vector2i{ownerContentArea.GetSize().x, 0},
							Math::Vector2i{Math::Zero}
						);
					}
					newWidgetLocation = Math::Max(newWidgetLocation, spawnableArea.GetPosition());

					pModalWidget->Reposition(newWidgetLocation, sceneRegistry);
					pModalWidget->MakeVisible();
					owningWindow.SetInputFocusWidget(pWidget);

					if (pModalTrigger->m_flags.IsSet(Flags::CloseOnEvent))
					{
						Events::Manager& eventManager = System::Get<Events::Manager>();
						const Events::Identifier eventIdentifier = eventManager.FindOrRegisterEvent(pWidget->GetInstanceGuid());
						eventManager.Subscribe<&ModalTrigger::OnClose>(eventIdentifier, *pModalTrigger);
					}
				}
			}
		);
		if (jobBatch.IsValid())
		{
			System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::UserInterfaceAction);
		}
		return true;
	}

	bool ModalTrigger::Close(Widget& owner)
	{
		if (m_pSpawnedWidget.IsValid())
		{
			Widget* pWidget = m_pSpawnedWidget;
			m_pSpawnedWidget = nullptr;
			OnModalClosed(owner, *pWidget);

			if (m_flags.IsSet(Flags::CloseOnEvent))
			{
				// In case this comes from an event we need to unsubscribe outside of it
				System::Get<Threading::JobManager>().QueueCallback(
					[this, instanceGuid = pWidget->GetInstanceGuid()](Threading::JobRunnerThread&)
					{
						Events::Manager& eventManager = System::Get<Events::Manager>();
						const Events::Identifier eventIdentifier = eventManager.FindEvent(instanceGuid);
						eventManager.Unsubscribe(eventIdentifier, *this);
					},
					Threading::JobPriority::UserInterfaceAction
				);
			}

			Widgets::Widget& rootWidget = pWidget->GetRootWidget();
			pWidget->GetParent().Destroy(owner.GetSceneRegistry());

			rootWidget.RecalculateHierarchy();
			return true;
		}
		else
		{
			return false;
		}
	}

	void ModalTrigger::OnModalLostFocus(Widget& owner)
	{
		Close(owner);
	}

	[[maybe_unused]] const bool wasModalTriggerTypeRegistered = Reflection::Registry::RegisterType<Widgets::Data::ModalTrigger>();
	[[maybe_unused]] const bool wasModalTriggerComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widgets::Data::ModalTrigger>>::Make());
	[[maybe_unused]] const bool wasModalFocusListenerTypeRegistered = Reflection::Registry::RegisterType<Widgets::Data::ModalFocusListener>();
	[[maybe_unused]] const bool wasModalFocusListenerComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Widgets::Data::ModalFocusListener>>::Make());
}
