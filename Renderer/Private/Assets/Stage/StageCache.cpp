#include <Renderer/Assets/Stage/StageCache.h>

#include <Engine/Asset/AssetType.inl>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Renderer/Stages/Stage.h>
#include <Renderer/Renderer.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/System/Query.h>
#include <Common/Asset/VirtualAsset.h>

namespace ngine::Rendering
{
	[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& StageCache::GetRenderer()
	{
		return Memory::GetOwnerFromMember(*this, &Renderer::m_stageCache);
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& StageCache::GetRenderer() const
	{
		return Memory::GetConstOwnerFromMember(*this, &Renderer::m_stageCache);
	}

	StageCache::StageCache()
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_stageGenericIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("render_stage_generic_id");
		m_stageNamePropertyIdentifier = dataSourceCache.RegisterProperty("render_stage_name");
		m_stageGuidPropertyIdentifier = dataSourceCache.RegisterProperty("render_stage_guid");
	}
	StageCache::~StageCache()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));

		dataSourceCache.DeregisterProperty(m_stageGenericIdentifierPropertyIdentifier, "render_stage_generic_id");
		dataSourceCache.DeregisterProperty(m_stageNamePropertyIdentifier, "render_stage_name");
		dataSourceCache.DeregisterProperty(m_stageGuidPropertyIdentifier, "render_stage_guid");
	}

	SceneRenderStageIdentifier StageCache::RegisterAsset(const Guid guid, const EnumFlags<StageFlags> flags)
	{
		Assert(flags.IsSet(StageFlags::Hidden), "Visible tags must provide name!");
		return BaseType::RegisterAsset(
			guid,
			[flags](const SceneRenderStageIdentifier, const Asset::Guid) mutable
			{
				return StageInfo(flags);
			}
		);
	}

	SceneRenderStageIdentifier StageCache::FindOrRegisterAsset(const Guid guid, const EnumFlags<StageFlags> flags)
	{
		Assert(flags.IsSet(StageFlags::Hidden), "Visible tags must provide name!");
		const SceneRenderStageIdentifier identifier = BaseType::FindOrRegisterAsset(
			guid,
			[flags](const SceneRenderStageIdentifier, const Asset::Guid) mutable
			{
				return StageInfo(flags.GetFlags());
			}
		);
		return identifier;
	}

	SceneRenderStageIdentifier
	StageCache::FindOrRegisterAsset(const Guid guid, const ConstUnicodeStringView name, const EnumFlags<StageFlags> flags)
	{
		const SceneRenderStageIdentifier identifier = BaseType::FindOrRegisterAsset(
			guid,
			[flags](const SceneRenderStageIdentifier, const Asset::Guid) mutable
			{
				return StageInfo(flags.GetFlags());
			}
		);
		StageInfo& stageInfo = GetAssetData(identifier);

		if (stageInfo.m_flags.IsNotSet(StageFlags::Hidden))
		{
			RegisterVirtualAsset(identifier, name);
		}
		return identifier;
	}

	void StageCache::RegisterVirtualAsset(const SceneRenderStageIdentifier identifier, const ConstUnicodeStringView name)
	{
		Assert(GetAssetData(identifier).m_flags.IsNotSet(StageFlags::Hidden));

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		const Asset::Guid assetGuid = GetAssetGuid(identifier);
		if (!assetManager.HasAsset(assetGuid))
		{
			assetManager.RegisterAsset(
				assetGuid,
				Asset::DatabaseEntry{
					Stage::TypeGuid,
					{},
					IO::Path::Merge(assetGuid.ToString().GetView(), MAKE_PATH(".stage"), Asset::VirtualAsset::FileExtension),
					UnicodeString(name)
				},
				Asset::Identifier{}
			);
		}
	}

	DataSource::GenericDataIndex StageCache::GetDataCount() const
	{
		DataSource::GenericDataIndex count{0};
		IterateElements(
			[&count](const StageInfo& stageInfo)
			{
				count += stageInfo.m_flags.IsNotSet(StageFlags::Hidden);
			}
		);
		return count;
	}

	void StageCache::IterateData(const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		uint32 count = 0;
		const CachedQuery::SetBitsIterator iterator = query.GetSetBitsIterator(0, GetMaximumUsedIdentifierCount());
		for (auto it : iterator)
		{
			if (count >= offset.GetMinimum())
			{
				GenericDataIdentifier stageIdentifier = GenericDataIdentifier::MakeFromValidIndex(it);
				callback(stageIdentifier);
				if (count >= offset.GetMaximum())
				{
					break;
				}
			}
			++count;
		}
	}

	void StageCache::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			GenericDataIdentifier stageIdentifier = GenericDataIdentifier::MakeFromValidIndex(identifierIndex);
			callback(stageIdentifier);
		}
	}

	DataSource::PropertyValue StageCache::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		GenericDataIdentifier genericIdentifier = data.GetExpected<GenericDataIdentifier>();
		if (identifier == m_stageNamePropertyIdentifier)
		{
			const SceneRenderStageIdentifier stageIdentifier =
				SceneRenderStageIdentifier::MakeFromValidIndex((SceneRenderStageIdentifier::IndexType)genericIdentifier.GetIndex());
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			return UnicodeString(assetManager.GetAssetName(GetAssetGuid(stageIdentifier)));
		}
		else if (identifier == m_stageGuidPropertyIdentifier)
		{
			const SceneRenderStageIdentifier stageIdentifier =
				SceneRenderStageIdentifier::MakeFromValidIndex((SceneRenderStageIdentifier::IndexType)genericIdentifier.GetIndex());
			return GetAssetGuid(stageIdentifier);
		}
		else if (identifier == m_stageGenericIdentifierPropertyIdentifier)
		{
			return genericIdentifier;
		}
		return {};
	}

	void StageCache::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
	{
		cachedQueryOut.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			cachedQueryOut = *query.m_allowedItems;
		}

		/*const Tag::AtomicMaskContainer<ComponentTypeIdentifier>::ConstRestrictedView tags = m_tags.GetView();
		if (query.m_allowedFilterMask.AreAnySet())
		{
		  ComponentTypeMask allowedFilterMask;
		  for (const Tag::Identifier::IndexType categoryIndex : query.m_allowedFilterMask.GetSetBitsIterator())
		  {
		    const Tag::Identifier categoryIdentifier = Tag::Identifier::MakeFromValidIndex(categoryIndex);
		    const Threading::Atomic<Threading::AtomicIdentifierMask<ComponentTypeIdentifier>*> pTagCategories = tags[categoryIdentifier];
		    if (pTagCategories != nullptr)
		    {
		      allowedFilterMask |= *pTagCategories;
		    }
		  }

		  if (!query.m_allowedItems.IsValid())
		  {
		    selectedComponentTypes |= allowedFilterMask;
		  }
		  else
		  {
		    selectedComponentTypes &= allowedFilterMask;
		  }
		}*/

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			IterateElements(
				[elements = GetValidElementView(), &cachedQueryOut](const StageInfo& stageInfo)
				{
					if (stageInfo.m_flags.IsNotSet(StageFlags::Hidden))
					{
						const SceneRenderStageIdentifier identifier =
							SceneRenderStageIdentifier::MakeFromValidIndex(elements.GetIteratorIndex(&stageInfo));
						cachedQueryOut.Set(GenericDataIdentifier::MakeFromValidIndex(identifier.GetFirstValidIndex()));
					}
				}
			);
		}

		/*if (query.m_allowedItems.IsValid())
		{
		  ComponentTypeMask filteredComonentTypes = selectedComponentTypes;
		  for (const ComponentIdentifier::IndexType componentIndex :
		       selectedComponentTypes.GetSetBitsIterator(0, m_componentTypeIdentifiers.GetMaximumUsedElementCount()))
		  {
		    const ComponentTypeIdentifier componentIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(componentIndex);
		    if (!query.m_allowedItems->IsSet(DataSource::GenericDataIdentifier::MakeFromValidIndex(componentIndex)))
		    {
		      filteredComonentTypes.Clear(componentIdentifier);
		    }
		  }
		  selectedComponentTypes = filteredComonentTypes;
		}*/
	}
}
