#include <Widgets/Data/Modifiers.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Widgets/Widget.h>
#include <Widgets/Style/CombinedEntry.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Widgets::Data
{
	Modifiers::Modifiers(Initializer&& initializer)
		: BaseType(Forward<BaseType::Initializer>(initializer))
	{
	}

	Modifiers::Modifiers(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		ModifierProperties modifierProperties;

		Serialization::Reader reader = deserializer.m_reader;
		Widget& owner = deserializer.GetParent();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

		if (reader.ReadWithDefaultValue<bool>("block_pointer_inputs", false))
		{
			owner.BlockPointerInputs();
		}

		if (const Optional<Serialization::Reader> disabledReader = deserializer.m_reader.FindSerializer("disabled"))
		{
			if (!disabledReader->IsString())
			{
				Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData =
					deserializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::Flags>();
				AtomicEnumFlags<Entity::ComponentFlags>& componentFlags = flagsSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				if (disabledReader->ReadInPlaceWithDefaultValue<bool>(false))
				{
					componentFlags |= Entity::ComponentFlags::IsDisabledWithChildren;
				}
				else
				{
					componentFlags &= ~Entity::ComponentFlags::IsDisabledWithChildren;
				}
			}
			else
			{
				modifierProperties.SetPropertyIdentifier(
					ModifierProperty::Disabled,
					disabledReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
				);
			}
		}

		if (const Optional<Serialization::Reader> hiddenReader = reader.FindSerializer("hidden"))
		{
			if (!hiddenReader->IsString())
			{
				owner.m_flags |= WidgetFlags::IsHidden * hiddenReader->ReadInPlaceWithDefaultValue<bool>(false);
			}
			else
			{
				modifierProperties.SetPropertyIdentifier(
					ModifierProperty::Hidden,
					hiddenReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
				);
			}
		}

		{
			if (const Optional<Serialization::Reader> activeReader = reader.FindSerializer("active"))
			{
				if (!activeReader->IsString())
				{
					m_modifiers |= Style::Modifier::Active * activeReader->ReadInPlaceWithDefaultValue<bool>(false);
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Active,
						activeReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			if (const Optional<Serialization::Reader> toggledReader = reader.FindSerializer("toggled"))
			{
				if (!toggledReader->IsString())
				{
					owner.m_flags |= WidgetFlags::IsToggledOff * !toggledReader->ReadInPlaceWithDefaultValue<bool>(true);
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Toggled,
						toggledReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			if (const Optional<Serialization::Reader> ignoredReader = reader.FindSerializer("ignored"))
			{
				if (!ignoredReader->IsString())
				{
					owner.m_flags |= WidgetFlags::IsIgnored * ignoredReader->ReadInPlaceWithDefaultValue<bool>(false);
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Ignored,
						ignoredReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			if (const Optional<Serialization::Reader> requiredReader = reader.FindSerializer("required"))
			{
				if (!requiredReader->IsString())
				{
					owner.m_flags |= WidgetFlags::HasRequirements * requiredReader->ReadInPlaceWithDefaultValue<bool>(false);
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Required,
						requiredReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			if (const Optional<Serialization::Reader> validReader = reader.FindSerializer("valid"))
			{
				if (!validReader->IsString())
				{
					if (validReader->ReadInPlaceWithDefaultValue<bool>(true))
					{
						owner.m_flags |= WidgetFlags::FailedValidation;
					}
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Valid,
						validReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}
		}
		if (modifierProperties.AreAnyValid())
		{
			m_pModifierProperties.CreateInPlace(modifierProperties);
		}
	}

	Modifiers::Modifiers(const Modifiers& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_modifiers(templateComponent.m_modifiers)
		, m_pModifierProperties(
				templateComponent.m_pModifierProperties.IsValid() ? UniquePtr<ModifierProperties>::Make(*templateComponent.m_pModifierProperties)
																													: UniquePtr<ModifierProperties>{}
			)
	{
	}

	Modifiers::~Modifiers() = default;

	bool Modifiers::SerializeCustomData(Serialization::Writer writer, const Widget& owner) const
	{
		if (owner.IsPointerInputBlocked())
		{
			writer.Serialize("block_pointer_inputs", true);
		}

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		{
			Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
			AtomicEnumFlags<Entity::ComponentFlags> componentFlags = flagsSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Disabled))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Disabled)));
				writer.Serialize("disabled", format);
			}
			else if (componentFlags.IsSet(Entity::ComponentFlags::IsDisabledWithChildren))
			{
				writer.Serialize("disabled", true);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Hidden))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Hidden)));
				writer.Serialize("hidden", format);
			}
			else if (owner.m_flags.IsSet(WidgetFlags::IsHidden))
			{
				writer.Serialize("hidden", true);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Active))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Active)));
				writer.Serialize("active", format);
			}
			else if (m_modifiers.IsSet(Style::Modifier::Active))
			{
				writer.Serialize("active", true);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Toggled))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Toggled)));
				writer.Serialize("toggled", format);
			}
			else if (owner.m_flags.IsSet(WidgetFlags::IsToggledOff))
			{
				writer.Serialize("toggled", false);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Ignored))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Ignored)));
				writer.Serialize("ignored", format);
			}
			else if (owner.m_flags.IsSet(WidgetFlags::IsIgnored))
			{
				writer.Serialize("ignored", true);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Required))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Required)));
				writer.Serialize("required", format);
			}
			else if (owner.m_flags.IsSet(WidgetFlags::HasRequirements))
			{
				writer.Serialize("required", true);
			}

			if (m_pModifierProperties.IsValid() && m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Valid))
			{
				String format;
				format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Valid)));
				writer.Serialize("valid", format);
			}
			else if (owner.m_flags.IsSet(WidgetFlags::FailedValidation))
			{
				writer.Serialize("valid", false);
			}
		}

		return true;
	}

	void Modifiers::DeserializeCustomData(const Optional<Serialization::Reader> reader, Widget& owner)
	{
		if (reader.IsValid())
		{
			ModifierProperties modifierProperties;

			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

			if (reader->ReadWithDefaultValue<bool>("block_pointer_inputs", false))
			{
				owner.BlockPointerInputs();
			}

			if (const Optional<Serialization::Reader> disabledReader = reader->FindSerializer("disabled"))
			{
				if (!disabledReader->IsString())
				{
					if (disabledReader->ReadInPlaceWithDefaultValue<bool>(false))
					{
						owner.DisableWithChildren();
					}
					else
					{
						owner.EnableWithChildren();
					}
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Disabled,
						disabledReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			if (const Optional<Serialization::Reader> hiddenReader = reader->FindSerializer("hidden"))
			{
				if (!hiddenReader->IsString())
				{
					if (hiddenReader->ReadInPlaceWithDefaultValue<bool>(false))
					{
						owner.Hide();
					}
					else
					{
						owner.MakeVisible();
					}
				}
				else
				{
					modifierProperties.SetPropertyIdentifier(
						ModifierProperty::Hidden,
						hiddenReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
					);
				}
			}

			{
				if (const Optional<Serialization::Reader> activeReader = reader->FindSerializer("active"))
				{
					if (!activeReader->IsString())
					{
						m_modifiers |= Style::Modifier::Active * activeReader->ReadInPlaceWithDefaultValue<bool>(false);
					}
					else
					{
						modifierProperties.SetPropertyIdentifier(
							ModifierProperty::Active,
							activeReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
						);
					}
				}

				if (const Optional<Serialization::Reader> toggledReader = reader->FindSerializer("toggled"))
				{
					if (!toggledReader->IsString())
					{
						if (toggledReader->ReadInPlaceWithDefaultValue<bool>(true))
						{
							owner.ClearToggledOff();
						}
						else
						{
							owner.ToggleOff();
						}
					}
					else
					{
						modifierProperties.SetPropertyIdentifier(
							ModifierProperty::Toggled,
							toggledReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
						);
					}
				}

				if (const Optional<Serialization::Reader> ignoredReader = reader->FindSerializer("ignored"))
				{
					if (!ignoredReader->IsString())
					{
						Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
						if (ignoredReader->ReadInPlaceWithDefaultValue<bool>(false))
						{
							owner.Ignore(sceneRegistry);
						}
						else
						{
							owner.Unignore(sceneRegistry);
						}
					}
					else
					{
						modifierProperties.SetPropertyIdentifier(
							ModifierProperty::Ignored,
							ignoredReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
						);
					}
				}

				if (const Optional<Serialization::Reader> requiredReader = reader->FindSerializer("required"))
				{
					if (!requiredReader->IsString())
					{
						if (requiredReader->ReadWithDefaultValue<bool>("required", false))
						{
							owner.SetHasRequirements();
						}
						else
						{
							owner.ClearHasRequirements();
						}
					}
					else
					{
						modifierProperties.SetPropertyIdentifier(
							ModifierProperty::Required,
							requiredReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
						);
					}
				}

				if (const Optional<Serialization::Reader> validReader = reader->FindSerializer("valid"))
				{
					if (!validReader->IsString())
					{
						if (validReader->ReadWithDefaultValue<bool>("valid", true))
						{
							owner.OnPassedValidation();
						}
						else
						{
							owner.OnFailedValidation();
						}
					}
					else
					{
						modifierProperties.SetPropertyIdentifier(
							ModifierProperty::Valid,
							validReader->ReadInPlaceWithDefaultValue<ngine::DataSource::PropertyIdentifier>({}, dataSourceCache)
						);
					}
				}
			}
			if (modifierProperties.AreAnyValid())
			{
				m_pModifierProperties.CreateInPlace(modifierProperties);
			}
		}
	}

	/* static */ void Modifiers::OnModifiersChangedInternal(Widget& owner, const EnumFlags<Style::Modifier> modifiers)
	{
		const Style::CombinedEntry style = owner.GetStyle();
		if (style.HasAnyModifiers(modifiers))
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			const EnumFlags<Style::Modifier> activeModifiers = owner.GetActiveModifiers();
			const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
			owner.OnActiveStyleModifiersChangedInternal(sceneRegistry, owner.GetOwningWindow(), style, matchingModifiers, style.m_valueTypeMask);
		}

		for (Widget& child : owner.GetChildren())
		{
			OnModifiersChangedInternal(child, modifiers);
		}
	}

	void Modifiers::UpdateFromDataSourceInternal(Widget& owner, const DataSourceProperties& dataSourceProperties)
	{
		if (HasDynamicProperties())
		{
			bool hasModifierChanged = false;
			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Disabled);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue disabledValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (disabledValue.HasValue())
				{
					if (const Optional<const bool*> isDisabled = disabledValue.Get<bool>())
					{
						if (owner.IsDisabled() != *isDisabled)
						{
							if (*isDisabled)
							{
								owner.DisableWithChildren();
							}
							else
							{
								owner.EnableWithChildren();
							}
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Hidden);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue hiddenValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (hiddenValue.HasValue())
				{
					if (const Optional<const bool*> isHidden = hiddenValue.Get<bool>())
					{
						if (owner.IsHidden() != *isHidden)
						{
							if (*isHidden)
							{
								owner.Hide();
							}
							else
							{
								owner.MakeVisible();
							}
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Active);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue activeValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (activeValue.HasValue())
				{
					if (const Optional<const bool*> isActive = activeValue.Get<bool>())
					{
						if (m_modifiers.IsSet(Style::Modifier::Active) != *isActive)
						{
							owner.ToggleModifiers(Style::Modifier::Active);
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Toggled);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue toggledValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (toggledValue.HasValue())
				{
					if (const Optional<const bool*> isToggled = toggledValue.Get<bool>())
					{
						if (owner.IsToggledOff() != *isToggled)
						{
							owner.ToggleOff();
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Ignored);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue ignoredValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (ignoredValue.HasValue())
				{
					if (const Optional<const bool*> isIgnored = ignoredValue.Get<bool>())
					{
						if (owner.IsIgnored() != *isIgnored)
						{
							Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
							owner.ToggleIgnore(sceneRegistry);
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Required);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue requiredValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (requiredValue.HasValue())
				{
					if (const Optional<const bool*> isRequired = requiredValue.Get<bool>())
					{
						if (owner.HasRequirements() != *isRequired)
						{
							if (*isRequired)
							{
								owner.SetHasRequirements();
							}
							else
							{
								owner.ClearHasRequirements();
							}
							hasModifierChanged = true;
						}
					}
				}
			}

			if (const ngine::DataSource::PropertyIdentifier propertyIdentifier =
			      m_pModifierProperties->GetPropertyIdentifier(ModifierProperty::Valid);
			    propertyIdentifier.IsValid())
			{
				const ngine::DataSource::PropertyValue validValue = dataSourceProperties.GetDataProperty(propertyIdentifier);
				if (validValue.HasValue())
				{
					if (const Optional<const bool*> isValid = validValue.Get<bool>())
					{
						if (owner.HasFailedValidation() == *isValid)
						{
							if (*isValid)
							{
								owner.OnPassedValidation();
							}
							else
							{
								owner.OnFailedValidation();
							}
							hasModifierChanged = true;
						}
					}
				}
			}

			if (hasModifierChanged)
			{
				// For now force whole hierarchy to recalculate because of an issue with children sizing.
				owner.GetRootWidget().RecalculateHierarchy();
			}
		}
	}

	[[maybe_unused]] const bool wasModifiersTypeRegistered = Reflection::Registry::RegisterType<Data::Modifiers>();
	[[maybe_unused]] const bool wasModifiersComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::Modifiers>>::Make());
}
