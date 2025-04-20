#include <Widgets/Data/Stylesheet.h>
#include <Widgets/Data/ExternalStyle.h>
#include <Widgets/Data/InlineStyle.h>
#include <Widgets/Data/DynamicStyle.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Widgets/Widget.h>
#include <Widgets/Manager.h>
#include <Widgets/WidgetScene.h>
#include <Widgets/Style/StylesheetCache.h>
#include <Widgets/Style/ComputedStylesheet.h>
#include <Widgets/Style/CombinedEntry.h>
#include <Widgets/Style/DynamicEntry.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>

namespace ngine::Widgets::Data
{
	Stylesheet::Stylesheet(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_stylesheetIdentifiers{Move(initializer.m_stylesheetIdentifiers)}
	{
		Assert(m_stylesheetIdentifiers.HasElements());
		if (!initializer.GetParent().GetRootScene().IsTemplate())
		{
			for (const StylesheetIdentifier stylesheetIdentifier : m_stylesheetIdentifiers)
			{
				const Optional<Threading::Job*> pStylesheetLoadJob =
					LoadStylesheet(stylesheetIdentifier, initializer.GetParent(), initializer.GetSceneRegistry());
				if (pStylesheetLoadJob.IsValid())
				{
					initializer.m_pJobBatch->QueueAfterStartStage(*pStylesheetLoadJob);
				}
			}
		}
	}

	Stylesheet::Stylesheet(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		if (const Optional<Asset::Guid> assetGuid = deserializer.m_reader.Read<Asset::Guid>("stylesheet"))
		{
			Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();

			const StylesheetIdentifier stylesheetIdentifier = stylesheetCache.FindOrRegister(*assetGuid);
			m_stylesheetIdentifiers.EmplaceBack(stylesheetIdentifier);

			if (!deserializer.GetParent().GetRootScene().IsTemplate())
			{
				const Optional<Threading::Job*> pStylesheetLoadJob =
					LoadStylesheet(stylesheetIdentifier, deserializer.GetParent(), deserializer.GetSceneRegistry());
				if (pStylesheetLoadJob.IsValid())
				{
					deserializer.m_pJobBatch->QueueAfterStartStage(*pStylesheetLoadJob);
				}
			}
		}
		else if (const Optional<Serialization::Reader> stylesheetsReader = deserializer.m_reader.FindSerializer("stylesheets"))
		{
			Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();

			m_stylesheetIdentifiers.Reserve((uint32)stylesheetsReader->GetArraySize());
			for (const Optional<Asset::Guid> stylesheetAssetGuid : stylesheetsReader->GetArrayView<Asset::Guid>())
			{
				const StylesheetIdentifier stylesheetIdentifier = stylesheetCache.FindOrRegister(*stylesheetAssetGuid);
				m_stylesheetIdentifiers.EmplaceBack(stylesheetIdentifier);

				if (!deserializer.GetParent().GetRootScene().IsTemplate())
				{
					const Optional<Threading::Job*> pStylesheetLoadJob =
						LoadStylesheet(stylesheetIdentifier, deserializer.GetParent(), deserializer.GetSceneRegistry());
					if (pStylesheetLoadJob.IsValid())
					{
						deserializer.m_pJobBatch->QueueAfterStartStage(*pStylesheetLoadJob);
					}
				}
			}
		}
		else
		{
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData = *deserializer.GetSceneRegistry().FindComponentTypeData<Stylesheet>();
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData =
				*deserializer.GetSceneRegistry().FindComponentTypeData<ExternalStyle>();
			ApplyParentStylesheets(deserializer.GetParent(), typeSceneData, externalStyleTypeSceneData);
		}
	}

	Stylesheet::Stylesheet(const Stylesheet& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_stylesheetIdentifiers(templateComponent.m_stylesheetIdentifiers)
	{
		if (m_stylesheetIdentifiers.HasElements())
		{
			if (!cloner.GetParent().GetRootScene().IsTemplate())
			{
				Entity::ComponentSoftReference widgetReference{cloner.GetParent(), cloner.GetSceneRegistry()};
				for (const StylesheetIdentifier stylesheetIdentifier : m_stylesheetIdentifiers)
				{
					const Optional<Threading::Job*> pStylesheetLoadJob =
						LoadStylesheet(stylesheetIdentifier, cloner.GetParent(), cloner.GetSceneRegistry());
					if (pStylesheetLoadJob.IsValid())
					{
						cloner.m_pJobBatch->QueueAfterStartStage(*pStylesheetLoadJob);
					}
				}
			}
		}
		else
		{
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData = *cloner.GetSceneRegistry().FindComponentTypeData<Stylesheet>();
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData =
				*cloner.GetSceneRegistry().FindComponentTypeData<ExternalStyle>();
			ApplyParentStylesheets(cloner.GetParent(), typeSceneData, externalStyleTypeSceneData);
		}
	}

	Stylesheet::~Stylesheet() = default;

	Optional<Threading::Job*>
	Stylesheet::LoadStylesheet(const StylesheetIdentifier stylesheetIdentifier, Widget& widget, Entity::SceneRegistry& sceneRegistry)
	{
		Assert(!widget.GetRootScene().IsTemplate());
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		Entity::ComponentSoftReference widgetReference{widget, sceneRegistry};
		return stylesheetCache.FindOrLoad(
			stylesheetIdentifier,
			Style::StylesheetCache::StylesheetRequesters::ListenerData{
				*this,
				[widgetReference, &sceneRegistry](Stylesheet&, [[maybe_unused]] const Optional<const Style::ComputedStylesheet*> pStylesheet)
				{
					if (const Optional<Widget*> pWidget = widgetReference.Find<Widget>(sceneRegistry))
					{
						Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData = *sceneRegistry.FindComponentTypeData<Stylesheet>();
						if (const Optional<Stylesheet*> pStylesheetData = pWidget->FindDataComponentOfType<Stylesheet>(typeSceneData))
						{
							Assert(pStylesheet.IsValid(), "Failed to load stylesheet");
							Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData =
								*sceneRegistry.FindComponentTypeData<ExternalStyle>();

							const Optional<ExternalStyle*> pExternalStyle = pWidget->FindDataComponentOfType<ExternalStyle>(externalStyleTypeSceneData);
							pStylesheetData->ApplyStylesheetsRecursiveInternal(*pWidget, pExternalStyle, typeSceneData, externalStyleTypeSceneData);
						}
					}
					return EventCallbackResult::Remove;
				}
			}
		);
	}

	Optional<Threading::Job*> Stylesheet::AddStylesheet(Widget& widget, const StylesheetIdentifier stylesheetIdentifier)
	{
		if (!m_stylesheetIdentifiers.EmplaceBackUnique(StylesheetIdentifier{stylesheetIdentifier}))
		{
			return Invalid;
		}

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		if (stylesheetIdentifier.IsValid())
		{
			return LoadStylesheet(stylesheetIdentifier, widget, sceneRegistry);
		}
		else
		{
			Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData = *sceneRegistry.FindComponentTypeData<Stylesheet>();
			Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData = *sceneRegistry.FindComponentTypeData<ExternalStyle>();
			ApplyParentStylesheets(widget, typeSceneData, externalStyleTypeSceneData);
			return Invalid;
		}
	}

	/* static */ void Stylesheet::GetParentStylesheets(
		ParentStylesheets& stylesheets,
		Style::StylesheetCache& stylesheetCache,
		Widget& widget,
		Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData
	)
	{
		for (Optional<Widget*> pParent = widget.GetParentSafe(); pParent != nullptr; pParent = pParent->GetParentSafe())
		{
			if (const Optional<Stylesheet*> pStylesheetData = pParent->FindDataComponentOfType<Stylesheet>(typeSceneData))
			{
				for (const StylesheetIdentifier stylesheetIdentifier : pStylesheetData->m_stylesheetIdentifiers)
				{
					if (const Optional<const Style::ComputedStylesheet*> pStylesheet = stylesheetCache.GetAssetData(stylesheetIdentifier).pStylesheet.Get())
					{
						if (!stylesheets.Contains(*pStylesheet))
						{
							stylesheets
								.Emplace(stylesheets.begin(), Memory::Uninitialized, ReferenceWrapper<const Style::ComputedStylesheet>{*pStylesheet});
						}
					}
				}
			}
		}
	}

	/* static */ Stylesheet::ParentStylesheets Stylesheet::GetStylesheets(
		Style::StylesheetCache& stylesheetCache,
		Widget& widget,
		const Optional<Stylesheet*> pStylesheetData,
		Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData
	)
	{
		ParentStylesheets stylesheets{Memory::Reserve, pStylesheetData.IsValid() ? pStylesheetData->m_stylesheetIdentifiers.GetSize() : 0};
		GetParentStylesheets(stylesheets, stylesheetCache, widget, typeSceneData);

		if (pStylesheetData.IsValid())
		{
			for (const StylesheetIdentifier stylesheetIdentifier : pStylesheetData->m_stylesheetIdentifiers)
			{
				if (const Optional<const Style::ComputedStylesheet*> pStylesheet = stylesheetCache.GetAssetData(stylesheetIdentifier).pStylesheet.Get())
				{
					stylesheets.EmplaceBackUnique(*pStylesheet);
				}
			}
		}

		return stylesheets;
	}

	void Stylesheet::ApplyStylesheetsRecursiveInternal(
		Widget& widget,
		const Optional<ExternalStyle*> pExternalStyle,
		Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
		Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
	)
	{
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		ParentStylesheets stylesheets = GetStylesheets(stylesheetCache, widget, this, typeSceneData);
		ApplyStylesheetRecursiveInternal(widget, stylesheets, pExternalStyle, typeSceneData, externalStyleTypeSceneData);
	}

	/* static */ void Stylesheet::ApplyStylesheetRecursiveInternal(
		Widget& widget,
		const ArrayView<const ReferenceWrapper<const Style::ComputedStylesheet>> stylesheets,
		const Optional<ExternalStyle*> pExternalStyle,
		Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
		Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
	)
	{
		if (pExternalStyle.IsValid())
		{
			pExternalStyle->ApplyStyleEntry(widget, stylesheets);
		}

		for (Widget& child : widget.GetChildren())
		{
			if (const Optional<Stylesheet*> pChildStylesheet = child.FindDataComponentOfType<Stylesheet>(typeSceneData);
			    pChildStylesheet.IsValid() && pChildStylesheet->m_stylesheetIdentifiers.HasElements())
			{
				Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
				Vector<ReferenceWrapper<const Style::ComputedStylesheet>> childStylesheets{
					Memory::Reserve,
					stylesheets.GetSize() + pChildStylesheet->m_stylesheetIdentifiers.GetSize()
				};

				for (const StylesheetIdentifier stylesheetIdentifier : pChildStylesheet->m_stylesheetIdentifiers)
				{
					if (const Optional<const Style::ComputedStylesheet*> pStylesheet = stylesheetCache.GetAssetData(stylesheetIdentifier).pStylesheet.Get())
					{
						childStylesheets.EmplaceBack(*pStylesheet);
					}
				}

				ApplyStylesheetRecursiveInternal(
					child,
					childStylesheets,
					child.FindDataComponentOfType<ExternalStyle>(externalStyleTypeSceneData),
					typeSceneData,
					externalStyleTypeSceneData
				);
			}
			else
			{
				ApplyStylesheetRecursiveInternal(
					child,
					stylesheets,
					child.FindDataComponentOfType<ExternalStyle>(externalStyleTypeSceneData),
					typeSceneData,
					externalStyleTypeSceneData
				);
			}
		}
	}

	/* static */ void Stylesheet::ApplyParentStylesheets(
		Widget& widget,
		Entity::ComponentTypeSceneData<Stylesheet>& typeSceneData,
		Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData
	)
	{
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		ParentStylesheets stylesheets;
		GetParentStylesheets(stylesheets, stylesheetCache, widget, typeSceneData);

		if (stylesheets.HasElements())
		{
			const Optional<ExternalStyle*> pExternalStyle = widget.FindDataComponentOfType<ExternalStyle>(externalStyleTypeSceneData);
			ApplyStylesheetRecursiveInternal(widget, stylesheets, pExternalStyle, typeSceneData, externalStyleTypeSceneData);
		}
		else if (const Optional<ExternalStyle*> pExternalStyle = widget.FindDataComponentOfType<ExternalStyle>(externalStyleTypeSceneData);
		         pExternalStyle.IsValid() && pExternalStyle->GetEntry().IsValid())
		{
			ApplyStylesheetRecursiveInternal(widget, {}, pExternalStyle, typeSceneData, externalStyleTypeSceneData);
		}
	}

	bool Stylesheet::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		if (m_stylesheetIdentifiers.HasElements())
		{
			Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();

			InlineVector<Guid, 1> stylesheetAssetGuids;
			for (const StylesheetIdentifier stylesheetIdentifier : m_stylesheetIdentifiers)
			{
				stylesheetAssetGuids.EmplaceBack(stylesheetCache.GetAssetGuid(stylesheetIdentifier));
			}
			writer.Serialize("stylesheets", stylesheetAssetGuids.GetView());
		}

		return true;
	}

	void Stylesheet::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent)
	{
		if (pReader.IsValid())
		{
			Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();
			if (const Optional<Asset::Guid> assetGuid = pReader->Read<Asset::Guid>("stylesheet"))
			{
				Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();

				const StylesheetIdentifier stylesheetIdentifier = stylesheetCache.FindOrRegister(*assetGuid);
				if (m_stylesheetIdentifiers.EmplaceBackUnique(StylesheetIdentifier{stylesheetIdentifier}))
				{
					if (!parent.GetRootScene().IsTemplate())
					{
						const Optional<Threading::Job*> pStylesheetLoadJob = LoadStylesheet(stylesheetIdentifier, parent, sceneRegistry);
						if (pStylesheetLoadJob.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(*pStylesheetLoadJob);
						}
					}
				}
			}
			else if (const Optional<Serialization::Reader> stylesheetsReader = pReader->FindSerializer("stylesheets"))
			{
				Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();

				m_stylesheetIdentifiers.ReserveAdditionalCapacity((uint32)stylesheetsReader->GetArraySize());
				for (const Optional<Asset::Guid> stylesheetAssetGuid : stylesheetsReader->GetArrayView<Asset::Guid>())
				{
					const StylesheetIdentifier stylesheetIdentifier = stylesheetCache.FindOrRegister(*stylesheetAssetGuid);
					if (m_stylesheetIdentifiers.EmplaceBackUnique(StylesheetIdentifier{stylesheetIdentifier}))
					{
						if (!parent.GetRootScene().IsTemplate())
						{
							const Optional<Threading::Job*> pStylesheetLoadJob = LoadStylesheet(stylesheetIdentifier, parent, sceneRegistry);
							if (pStylesheetLoadJob.IsValid())
							{
								Threading::JobRunnerThread::GetCurrent()->Queue(*pStylesheetLoadJob);
							}
						}
					}
				}
			}
			else
			{
				Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData = *sceneRegistry.FindComponentTypeData<Stylesheet>();
				Entity::ComponentTypeSceneData<ExternalStyle>& externalStyleTypeSceneData = *sceneRegistry.FindComponentTypeData<ExternalStyle>();
				ApplyParentStylesheets(parent, stylesheetTypeSceneData, externalStyleTypeSceneData);
			}
		}
	}

	ExternalStyle::ExternalStyle(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_stylesheetEntryGuid(initializer.m_stylesheetEntryGuid)
	{
		Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData = *initializer.GetSceneRegistry().FindComponentTypeData<Stylesheet>(
		);

		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		Stylesheet::ParentStylesheets stylesheets = Stylesheet::GetStylesheets(
			stylesheetCache,
			initializer.GetParent(),
			initializer.GetParent().FindDataComponentOfType<Stylesheet>(stylesheetTypeSceneData),
			stylesheetTypeSceneData
		);
		if (stylesheets.HasElements())
		{
			ApplyStyleEntry(initializer.GetParent(), stylesheets.GetView());
		}
	}

	ExternalStyle::ExternalStyle(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		if (const Optional<Guid> stylesheetEntryGuid = deserializer.m_reader.Read<Guid>("entry"))
		{
			m_stylesheetEntryGuid = *stylesheetEntryGuid;
		}

		Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Stylesheet>();
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		Stylesheet::ParentStylesheets stylesheets = Stylesheet::GetStylesheets(
			stylesheetCache,
			deserializer.GetParent(),
			deserializer.GetParent().FindDataComponentOfType<Stylesheet>(stylesheetTypeSceneData),
			stylesheetTypeSceneData
		);
		if (stylesheets.HasElements())
		{
			ApplyStyleEntry(deserializer.GetParent(), stylesheets.GetView());
		}
	}

	ExternalStyle::ExternalStyle(const ExternalStyle& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_stylesheetEntryGuid(templateComponent.m_stylesheetEntryGuid)
	{
		Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData = *cloner.GetSceneRegistry().FindComponentTypeData<Stylesheet>();
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		Stylesheet::ParentStylesheets stylesheets = Stylesheet::GetStylesheets(
			stylesheetCache,
			cloner.GetParent(),
			cloner.GetParent().FindDataComponentOfType<Stylesheet>(stylesheetTypeSceneData),
			stylesheetTypeSceneData
		);
		if (stylesheets.HasElements())
		{
			ApplyStyleEntry(cloner.GetParent(), stylesheets.GetView());
		}
	}

	ExternalStyle::~ExternalStyle() = default;

	void ExternalStyle::SetEntry(Widget& widget, const Asset::Guid stylesheetEntryGuid)
	{
		Assert(m_stylesheetEntryGuid != stylesheetEntryGuid);
		m_stylesheetEntryGuid = stylesheetEntryGuid;

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();

		Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData = *sceneRegistry.FindComponentTypeData<Stylesheet>();
		Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
		Stylesheet::ParentStylesheets stylesheets = Stylesheet::GetStylesheets(
			stylesheetCache,
			widget,
			widget.FindDataComponentOfType<Stylesheet>(stylesheetTypeSceneData),
			stylesheetTypeSceneData
		);
		if (stylesheets.HasElements())
		{
			ApplyStyleEntry(widget, stylesheets.GetView());
		}
	}

	void ExternalStyle::ApplyStyleEntry(Widget& widget, const ArrayView<const ReferenceWrapper<const Style::ComputedStylesheet>> stylesheets)
	{
		if (stylesheets.HasElements())
		{
			if (m_stylesheetEntryGuid.IsValid())
			{
				// Find the first valid entry in the provided stylesheets
				for (const Style::ComputedStylesheet& stylesheet : stylesheets)
				{
					if (const Optional<const Style::Entry*> pStyleEntry = stylesheet.FindEntry(m_stylesheetEntryGuid))
					{
						Widget::ChangedStyleValues changedStyleValues = pStyleEntry->GetValueTypeMask();
						if (m_pStyle.IsValid())
						{
							changedStyleValues |= m_pStyle->GetValueTypeMask();
						}

						m_pStyle = pStyleEntry;
						if (changedStyleValues.AreAnySet() && !widget.GetRootScene().IsTemplate())
						{
							Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
							const Style::CombinedEntry style = widget.GetStyle(sceneRegistry);
							const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
							const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
							widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), style, matchingModifiers, changedStyleValues);
						}
						break;
					}
				}
			}
		}
		else if (m_pStyle.IsValid())
		{
			const Widget::ChangedStyleValues changedStyleValues = m_pStyle->GetValueTypeMask();
			m_pStyle = {};
			if (changedStyleValues.AreAnySet() && !widget.GetRootScene().IsTemplate())
			{
				Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
				const Style::CombinedEntry style = widget.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = style.GetMatchingModifiers(activeModifiers);
				widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), style, matchingModifiers, changedStyleValues);
			}
		}
	}

	bool ExternalStyle::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		if (m_stylesheetEntryGuid.IsValid())
		{
			writer.Serialize("entry", m_stylesheetEntryGuid);
		}

		return true;
	}

	void ExternalStyle::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent)
	{
		if (pReader.IsValid())
		{
			if (const Optional<Guid> stylesheetEntryGuid = pReader->Read<Guid>("entry"))
			{
				m_stylesheetEntryGuid = *stylesheetEntryGuid;
			}

			Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();

			Entity::ComponentTypeSceneData<Stylesheet>& stylesheetTypeSceneData = *sceneRegistry.FindComponentTypeData<Stylesheet>();
			Style::StylesheetCache& stylesheetCache = System::FindPlugin<Widgets::Manager>()->GetStylesheetCache();
			Stylesheet::ParentStylesheets stylesheets = Stylesheet::GetStylesheets(
				stylesheetCache,
				parent,
				parent.FindDataComponentOfType<Stylesheet>(stylesheetTypeSceneData),
				stylesheetTypeSceneData
			);
			if (stylesheets.HasElements())
			{
				ApplyStyleEntry(parent, stylesheets.GetView());
			}
		}
	}

	InlineStyle::InlineStyle(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_pEntry(Forward<UniquePtr<Style::Entry>>(initializer.m_pEntry))
	{
		Assert(m_pEntry.IsValid());
	}

	InlineStyle::InlineStyle(const Deserializer& deserializer)
		: BaseType(deserializer)
		, m_pEntry{Memory::ConstructInPlace}
	{
		const ConstStringView inlineDynamicStyle = *deserializer.m_reader.Read<ConstStringView>("style");

		Style::Entry::ModifierValues& modifier = m_pEntry->EmplaceExactModifierMatch(Style::Modifier::None);
		modifier.ParseFromCSS(inlineDynamicStyle);
		m_pEntry->OnValueTypesAdded(modifier.GetValueTypeMask());
		m_pEntry->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());
	}

	InlineStyle::InlineStyle(const InlineStyle& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_pEntry(UniquePtr<Style::Entry>::Make(*templateComponent.m_pEntry))
	{
	}

	InlineStyle::~InlineStyle() = default;

	void InlineStyle::SetEntry(Widget& widget, Style::Entry&& entry)
	{
		Widget::ChangedStyleValues changedStyleValues = entry.GetValueTypeMask();

		if (m_pEntry.IsValid())
		{
			changedStyleValues |= m_pEntry->GetValueTypeMask();

			*m_pEntry = Forward<Style::Entry>(entry);
		}
		else
		{
			m_pEntry.CreateInPlace(Forward<Style::Entry>(entry));
		}

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
		widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
	}

	void InlineStyle::ParseEntry(Widget& widget, const ConstStringView string)
	{
		Style::Entry::ModifierValues& modifier = m_pEntry->FindOrEmplaceExactModifierMatch(Style::Modifier::None);
		Widget::ChangedStyleValues changedStyleValues = modifier.GetValueTypeMask();

		modifier.ParseFromCSS(string);

		changedStyleValues |= modifier.GetValueTypeMask();

		m_pEntry->OnValueTypesAdded(changedStyleValues);

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
		widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
	}

	void InlineStyle::EmplaceValue(Widget& widget, Style::Value&& value, const EnumFlags<Style::Modifier> modifiers)
	{
		Widget::ChangedStyleValues changedStyleValues;
		changedStyleValues.Set((uint8)value.GetTypeIdentifier());

		Style::Entry::ModifierValues& modifier = m_pEntry->FindOrEmplaceExactModifierMatch(modifiers);
		modifier.Emplace(Forward<Style::Value>(value));
		m_pEntry->OnValueTypeAdded(value.GetTypeIdentifier());

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
		widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
	}

	void InlineStyle::ClearValue(Widget& widget, const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers)
	{
		if (const Optional<Style::Entry::ModifierValues*> pModifier = m_pEntry->FindExactModifierMatch(modifiers))
		{
			if (pModifier->Clear(valueType))
			{
				m_pEntry->OnValueTypeRemoved(valueType);

				Widget::ChangedStyleValues changedStyleValues;
				changedStyleValues.Set((uint8)valueType);

				Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
				const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
				widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
			}
		}
	}

	bool InlineStyle::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		const Style::Entry::ModifierValues& modifierValues = *m_pEntry->FindExactModifierMatch(Widgets::Style::Modifier::None);
		writer.Serialize("style", modifierValues.ToString());
		return true;
	}

	void InlineStyle::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent)
	{
		if (pReader.IsValid())
		{
			const ConstStringView inlineDynamicStyle = *pReader->Read<ConstStringView>("style");

			Style::Entry::ModifierValues& modifier = m_pEntry->FindOrEmplaceExactModifierMatch(Style::Modifier::None);

			modifier.ParseFromCSS(inlineDynamicStyle);
			m_pEntry->OnValueTypesAdded(modifier.GetValueTypeMask());
			m_pEntry->OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());

			if (!parent.GetRootScene().IsTemplate())
			{
				Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();
				const Style::CombinedEntry combinedStyle = parent.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = parent.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
				parent
					.OnStyleChangedInternal(sceneRegistry, parent.GetOwningWindow(), combinedStyle, matchingModifiers, modifier.GetValueTypeMask());
			}
		}
	}

	DynamicStyle::DynamicStyle(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_pEntry(Forward<UniquePtr<Style::DynamicEntry>>(initializer.m_pEntry))
	{
	}

	DynamicStyle::DynamicStyle(const Deserializer& deserializer)
		: BaseType(deserializer)
		, m_pEntry{Memory::ConstructInPlace}
	{
		const ConstStringView inlineDynamicStyle = *deserializer.m_reader.Read<ConstStringView>("style");

		Style::Entry::ModifierValues& modifier = m_pEntry->m_dynamicEntry.EmplaceExactModifierMatch(Style::Modifier::None);
		modifier.ParseFromCSS(inlineDynamicStyle);
		m_pEntry->m_dynamicEntry.OnValueTypesAdded(modifier.GetValueTypeMask());
		m_pEntry->m_dynamicEntry.OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());
	}

	DynamicStyle::DynamicStyle(const DynamicStyle& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_pEntry(UniquePtr<Style::DynamicEntry>::Make(*templateComponent.m_pEntry))
	{
	}

	DynamicStyle::~DynamicStyle() = default;

	bool DynamicStyle::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		const Style::Entry::ModifierValues& modifierValues = *m_pEntry->m_dynamicEntry.FindExactModifierMatch(Widgets::Style::Modifier::None);
		writer.Serialize("style", modifierValues.ToString());
		return true;
	}

	void DynamicStyle::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent)
	{
		if (pReader.IsValid())
		{
			const ConstStringView inlineDynamicStyle = *pReader->Read<ConstStringView>("style");

			Style::Entry::ModifierValues& modifier = m_pEntry->m_dynamicEntry.FindOrEmplaceExactModifierMatch(Style::Modifier::None);

			modifier.ParseFromCSS(inlineDynamicStyle);
			m_pEntry->m_dynamicEntry.OnValueTypesAdded(modifier.GetValueTypeMask());
			m_pEntry->m_dynamicEntry.OnDynamicValueTypesAdded(modifier.GetDynamicValueTypeMask());

			if (!parent.GetRootScene().IsTemplate())
			{
				Entity::SceneRegistry& sceneRegistry = parent.GetSceneRegistry();
				const Style::CombinedEntry combinedStyle = parent.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = parent.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
				parent
					.OnStyleChangedInternal(sceneRegistry, parent.GetOwningWindow(), combinedStyle, matchingModifiers, modifier.GetValueTypeMask());
			}
		}
	}

	void DynamicStyle::SetEntry(Widget& widget, Style::Entry&& entry)
	{
		Widget::ChangedStyleValues changedStyleValues = entry.GetValueTypeMask();

		if (m_pEntry.IsValid())
		{
			changedStyleValues |= m_pEntry->m_dynamicEntry.GetValueTypeMask();

			m_pEntry->m_dynamicEntry = Forward<Style::Entry>(entry);
		}
		else
		{
			m_pEntry.CreateInPlace(Style::DynamicEntry{Forward<Style::Entry>(entry)});
		}

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
		widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
	}

	void DynamicStyle::EmplaceValue(Widget& widget, Style::Value&& value, const EnumFlags<Style::Modifier> modifiers)
	{
		Widget::ChangedStyleValues changedStyleValues;
		changedStyleValues.Set((uint8)value.GetTypeIdentifier());

		Style::Entry::ModifierValues& modifier = m_pEntry->m_dynamicEntry.FindOrEmplaceExactModifierMatch(modifiers);
		modifier.OnDynamicValueTypeAdded(value.GetTypeIdentifier());
		modifier.Emplace(Forward<Style::Value>(value));
		m_pEntry->m_dynamicEntry.OnValueTypeAdded(value.GetTypeIdentifier());

		Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
		const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
		const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
		const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
		widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
	}

	void DynamicStyle::ClearValue(Widget& widget, const Style::ValueTypeIdentifier valueType, const EnumFlags<Style::Modifier> modifiers)
	{
		if (const Optional<Style::Entry::ModifierValues*> pModifier = m_pEntry->m_dynamicEntry.FindExactModifierMatch(modifiers))
		{
			if (pModifier->Clear(valueType))
			{
				m_pEntry->m_dynamicEntry.OnValueTypeRemoved(valueType);
				m_pEntry->m_dynamicEntry.OnDynamicValueTypeRemoved(valueType);

				Widget::ChangedStyleValues changedStyleValues;
				changedStyleValues.Set((uint8)valueType);

				Entity::SceneRegistry& sceneRegistry = widget.GetSceneRegistry();
				const Style::CombinedEntry combinedStyle = widget.GetStyle(sceneRegistry);
				const EnumFlags<Style::Modifier> activeModifiers = widget.GetActiveModifiers(sceneRegistry);
				const Style::CombinedEntry::MatchingModifiers matchingModifiers = combinedStyle.GetMatchingModifiers(activeModifiers);
				widget.OnStyleChangedInternal(sceneRegistry, widget.GetOwningWindow(), combinedStyle, matchingModifiers, changedStyleValues);
			}
		}
	}

	[[maybe_unused]] const bool wasExternalStyleTypeRegistered = Reflection::Registry::RegisterType<Data::ExternalStyle>();
	[[maybe_unused]] const bool wasExternalStyleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::ExternalStyle>>::Make());
	[[maybe_unused]] const bool waStylesheetTypeRegistered = Reflection::Registry::RegisterType<Data::Stylesheet>();
	[[maybe_unused]] const bool waStylesheetComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::Stylesheet>>::Make());
	[[maybe_unused]] const bool wasInlineStyleTypeRegistered = Reflection::Registry::RegisterType<Data::InlineStyle>();
	[[maybe_unused]] const bool wasInlineStyleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::InlineStyle>>::Make());
	[[maybe_unused]] const bool wasDynamicStyleTypeRegistered = Reflection::Registry::RegisterType<Data::DynamicStyle>();
	[[maybe_unused]] const bool wasDynamicStyleComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::DynamicStyle>>::Make());
}
