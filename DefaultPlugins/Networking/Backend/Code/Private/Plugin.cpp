#include "Plugin.h"
#include "Common.h"
#include "LoadAssetFromBackendJob.h"

#include <Http/Plugin.h>
#include <Http/Worker.h>

#include <Common/System/Query.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Engine.h>

#include <Http/LoadAssetFromNetworkJob.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#include <Renderer/Window/iOS/AppDelegate.h>
#include <Renderer/Window/iOS/ViewController.h>

#import <StoreKit/SKProduct.h>
#endif

#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Texture/TextureAsset.h>

#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Format/Guid.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/IO/Format/Path.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Version.h>
#include <Common/Serialization/Reader.h>
#include <Common/Platform/Environment.h>
#include <Common/Memory/AddressOf.h>
#include <Common/IO/FileIterator.h>
#include <Common/IO/Log.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Project System/PluginAssetFormat.h>
#include <Common/Project System/ProjectDatabase.h>

#include <Http/FileCache.h>
#include <Common/Time/Stopwatch.h>

namespace ngine::Networking::Backend
{
	void ParseBackendAssetEntry(
		AssetEntry& assetEntry, const Serialization::Reader reader, const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags
	)
	{
		if (const Optional<Serialization::Reader> storageReader = reader.FindSerializer("storage"))
		{
			assetEntry.m_keyValueStorage.Reserve((uint32)storageReader->GetArraySize());
			assetEntry.m_keyValueStorage.Clear();
			for (const Serialization::Reader entryReader : storageReader->GetArrayView())
			{
				assetEntry.m_keyValueStorage.Emplace(Move(*entryReader.Read<String>("key")), Move(*entryReader.Read<String>("value")));
			}
		}

		if (const Optional<Serialization::Reader> filesReader = reader.FindSerializer("files");
		    filesReader.IsValid() && filesReader->GetArraySize() > 0)
		{
			for (const Serialization::Reader fileReader : filesReader->GetArrayView())
			{
				const IO::URIView urlView = *fileReader.Read<IO::URIView::ConstStringViewType>("url");

				IO::URI::StringType uriString(urlView.GetStringView());
				constexpr bool UseCDN = true;
				if constexpr (UseCDN)
				{
					// Change from blob storage paths to our CDN
					uriString.ReplaceFirstOccurrence(
						MAKE_URI_LITERAL("sceneriassetstorage.blob.core.windows.net"),
						MAKE_URI_LITERAL("cdnsceneri.azureedge.net")
					);

					uriString
						.ReplaceFirstOccurrence(MAKE_URI_LITERAL("storage.googleapis.com/sceneri-asset-storage"), MAKE_URI_LITERAL("cdn.sceneri.com"));
				}

				const IO::URIView urlExtension = urlView.GetRightMostExtension();
				// Check whether this file is the "main" (aka metadata) file
				const bool isMainAssetFile = urlExtension == MAKE_URI(".nasset") || urlExtension == MAKE_URI(".nproject") ||
				                             urlExtension == MAKE_URI(".nplugin") || urlExtension == MAKE_URI(".nassetdb");

				if (const Optional<ConstStringView> entityTag = fileReader.Read<ConstStringView>("entity_tag"); entityTag.IsValid())
				{
					if constexpr (UseCDN)
					{
						if (isMainAssetFile)
						{
							assetEntry.m_mainFileEtag = *entityTag;
						}
					}

					if (parsingFlags.IsNotSet(AssetDatabase::ParsingFlags::FromCache))
					{
						IO::ConstURIView correctedUrlView{uriString.GetView().GetData(), uriString.GetView().GetSize()};

						using Networking::HTTP::FileCache;
						FileCache& networkFileCache = FileCache::GetInstance();
						networkFileCache.ValidateRemoteFileTag(
							assetEntry.m_guid,
							correctedUrlView,
							HTTP::FilterEntityTag(*entityTag),
							IO::Path(IO::Path::StringType(correctedUrlView.GetAllExtensions().GetStringView()))
						);
					}
				}

				if (isMainAssetFile)
				{
					assetEntry.m_mainFileURI = IO::URI(Move(uriString));
				}
			}
		}

		constexpr bool needsExternalIdentifiers = PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS;
		if constexpr (needsExternalIdentifiers)
		{
			if (const Optional<Serialization::Reader> externalIdentifierReader = reader.FindSerializer("external_identifiers"))
			{
				if (externalIdentifierReader->GetValue().IsObject())
				{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
					if (const Optional<Serialization::Reader> appleDataReader = externalIdentifierReader->FindSerializer("apple_app_store"))
					{
						assetEntry.m_appStoreProductIdentifier = *appleDataReader->Read<String>("product_id");
					}
#endif
				}
			}
		}
	}

	void ParseAssetLibraryEntry(
		Asset::Manager& assetManager,
		const Asset::Identifier libraryAssetIdentifier,
		const Asset::Guid assetGuid,
		const Guid assetTypeGuid,
		const Asset::Guid thumbnailAssetGuid,
		const AssetEntry& assetEntry,
		const Serialization::Reader reader,
		const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags,
		Game& gameAPI
	)
	{
		const Asset::Identifier assetIdentifier = assetManager.GetAssetIdentifier(assetGuid);
		const bool wasImported = assetIdentifier.IsValid();
		if (assetEntry.m_mainFileURI.HasElements() && !wasImported)
		{
			assetManager.RemoveAsyncLoadCallback(assetGuid);
			assetManager.RegisterAsyncLoadCallback(
				assetGuid,
				[&gameAPI](
					const Asset::Guid assetGuid,
					const IO::PathView path,
					Threading::JobPriority priority,
					IO::AsyncLoadCallback&& callback,
					const ByteView target,
					const Math::Range<size> dataRange
				) -> Optional<Threading::Job*>
				{
					return new LoadAssetJob(
						gameAPI,
						assetGuid,
						IO::Path(path),
						priority,
						Forward<IO::AsyncLoadCallback>(callback),
						target,
						dataRange
					);
				}
			);
		}

		{
			Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
			const Asset::Identifier assetsCacheFolderAssetIdentifier =
				assetLibrary.FindOrRegisterFolder(Networking::HTTP::FileCache::GetAssetsDirectory().GetParentPath(), Asset::Identifier{});
			if (thumbnailAssetGuid.IsValid())
			{
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				const Tag::Identifier assetThumbnailTagIdentifier = tagRegistry.FindOrRegister(Asset::Library::ThumbnailAssetTagGuid);

				if (!assetLibrary.HasAsset(thumbnailAssetGuid))
				{
					assetLibrary.RegisterAsset(
						thumbnailAssetGuid,
						Asset::DatabaseEntry{
							TextureAssetType::AssetFormat.assetTypeGuid,
							Guid{},
							Networking::HTTP::FileCache::GetAssetPath(thumbnailAssetGuid, TextureAssetType::AssetFormat.metadataFileExtension)
						},
						assetsCacheFolderAssetIdentifier,
						Array<Tag::Identifier, 1>{assetThumbnailTagIdentifier}
					);
				}
				else
				{
					assetLibrary.SetTagAsset(assetThumbnailTagIdentifier, assetLibrary.GetAssetIdentifier(thumbnailAssetGuid));
				}

				if (!assetManager.HasAsset(thumbnailAssetGuid) && !assetManager.HasAsyncLoadCallback(thumbnailAssetGuid))
				{
					assetManager.RegisterAsyncLoadCallback(
						thumbnailAssetGuid,
						[&gameAPI](
							const Asset::Guid assetGuid,
							const IO::PathView path,
							Threading::JobPriority priority,
							IO::AsyncLoadCallback&& callback,
							const ByteView target,
							const Math::Range<size> dataRange
						) -> Optional<Threading::Job*>
						{
							return new LoadAssetJob(
								gameAPI,
								assetGuid,
								IO::Path(path),
								priority,
								Forward<IO::AsyncLoadCallback>(callback),
								target,
								dataRange
							);
						}
					);
				}
			}

			if (const Optional<ConstStringView> assetDatabaseGuidString = assetEntry.FindStorageValue("AssetDatabaseGuid"))
			{
				const Guid assetDatabaseGuid = Guid::TryParse(*assetDatabaseGuidString);
				if(assetDatabaseGuid.IsValid() && !assetManager.HasAsset(assetDatabaseGuid) && !assetManager.GetAssetLibrary().HasAsset(assetDatabaseGuid) && !assetManager.HasAsset(assetDatabaseGuid) && !assetManager.GetAssetLibrary().HasAsset(assetDatabaseGuid))
				{
					assetManager.GetAssetLibrary().RegisterAsset(
						assetDatabaseGuid,
						Asset::DatabaseEntry{
							Asset::Database::AssetFormat.assetTypeGuid,
							Guid{},
							Networking::HTTP::FileCache::GetAssetPath(assetDatabaseGuid, Asset::Database::AssetFormat.metadataFileExtension)
						},
						assetsCacheFolderAssetIdentifier
					);

					assetManager.RegisterAsyncLoadCallback(
						assetDatabaseGuid,
						[&gameAPI](
							const Asset::Guid assetGuid,
							const IO::PathView path,
							Threading::JobPriority priority,
							IO::AsyncLoadCallback&& callback,
							const ByteView target,
							const Math::Range<size> dataRange
						) -> Optional<Threading::Job*>
						{
							return new LoadAssetJob(
								gameAPI,
								assetGuid,
								IO::Path(path),
								priority,
								Forward<IO::AsyncLoadCallback>(callback),
								target,
								dataRange
							);
						}
					);
				}
			}
		}

		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		const auto setTag =
			[libraryAssetIdentifier, assetIdentifier, wasImported, &assetManager, &assetLibrary](const Tag::Identifier tagIdentifier)
		{
			assetLibrary.SetTagAsset(tagIdentifier, libraryAssetIdentifier);

			if (wasImported)
			{
				assetManager.SetTagAsset(tagIdentifier, assetIdentifier);
			}
		};

		setTag(tagRegistry.FindOrRegister(assetTypeGuid));
		setTag(tagRegistry.FindOrRegister(Asset::Library::CloudAssetTagGuid));
		if (parsingFlags.IsNotSet(AssetDatabase::ParsingFlags::FromCache) && reader.ReadWithDefaultValue<bool>("featured", false))
		{
			setTag(tagRegistry.FindOrRegister(Asset::Library::FeaturedAssetTagGuid));
		}
		if (parsingFlags.IsSet(AssetDatabase::ParsingFlags::Inventory))
		{
			setTag(tagRegistry.FindOrRegister(Asset::Library::OwnedAssetTagGuid));
		}
		if (parsingFlags.IsSet(AssetDatabase::ParsingFlags::Avatar))
		{
			setTag(tagRegistry.FindOrRegister(Asset::Library::AvatarAssetTagGuid));
		}

		assetLibrary.OnDataChanged();

		if (wasImported)
		{
			assetManager.OnDataChanged();
		}
	}

	AssetDatabase::AssetDatabase(Game& game)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		const DataSource::PropertyIdentifier likesCountPropertyIdentifier = dataSourceCache.RegisterProperty("asset_likes_count");
		const DataSource::PropertyIdentifier wasAssetLikedPropertyIdentifier = dataSourceCache.RegisterProperty("asset_liked");
		const DataSource::PropertyIdentifier isAssetInCartPropertyIdentifier = dataSourceCache.RegisterProperty("asset_in_cart");
		const DataSource::PropertyIdentifier playsCountPropertyIdentifier = dataSourceCache.RegisterProperty("asset_plays_count");
		const DataSource::PropertyIdentifier remixCountPropertyIdentifier = dataSourceCache.RegisterProperty("asset_remix_count");
		const DataSource::PropertyIdentifier assetEditTitlePropertyIdentifier = dataSourceCache.RegisterProperty("asset_edit_title");
		const DataSource::PropertyIdentifier assetEditIconPropertyIdentifier = dataSourceCache.RegisterProperty("asset_edit_icon");
		const DataSource::PropertyIdentifier pricePropertyIdentifier = dataSourceCache.RegisterProperty("asset_price");
		const DataSource::PropertyIdentifier assetIsDraftPropertyIdentifier = dataSourceCache.RegisterProperty("asset_is_draft");
		const DataSource::PropertyIdentifier assetIsPublishedPropertyIdentifier = dataSourceCache.RegisterProperty("asset_is_published");
		const DataSource::PropertyIdentifier assetTimeSinceEditPropertyIdentifier = dataSourceCache.RegisterProperty("asset_time_since_edit");
		const DataSource::PropertyIdentifier assetTimeSincePlayPropertyIdentifier = dataSourceCache.RegisterProperty("asset_time_since_play");

		const DataSource::PropertyIdentifier creatorTagPropertyIdentifier = dataSourceCache.RegisterProperty("asset_creator_tag");
		const DataSource::PropertyIdentifier creatorNamePropertyIdentifier = dataSourceCache.RegisterProperty("asset_creator_name");

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
		assetManager.RegisterDataPropertyCallback(
			likesCountPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_likesCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			pricePropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_price);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetLibrary.RegisterDataPropertyCallback(
			pricePropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_price);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertySortingCallback(
			likesCountPropertyIdentifier,
			[this, &assetManager](
				const DataSource::CachedQuery& cachedQuery,
				const DataSource::SortingOrder order,
				DataSource::SortedQueryIndices& cachedSortedQueryOut
			)
			{
				const Asset::Mask& __restrict assetMask = reinterpret_cast<const Asset::Mask&>(cachedQuery);
				cachedSortedQueryOut.Clear();
				cachedSortedQueryOut.Reserve(assetMask.GetNumberOfSetBits());

				for (Asset::Identifier::IndexType assetIdentifierIndex : assetMask.GetSetBitsIterator())
				{
					cachedSortedQueryOut.EmplaceBack(assetIdentifierIndex);
				}

				Algorithms::Sort(
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.begin(),
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.end(),
					[this, &assetManager, order](const Asset::Identifier::IndexType leftIndex, const Asset::Identifier::IndexType rightIndex)
					{
						const Asset::Guid leftAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(leftIndex));
						const Asset::Guid rightAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(rightIndex));

						const auto getLikeCount = [this](const Asset::Guid assetGuid) -> uint64
						{
							return VisitEntry(
								assetGuid,
								[](const Optional<const AssetEntry*> pAssetEntry) -> uint64
								{
									if (pAssetEntry.IsValid())
									{
										return pAssetEntry->m_likesCount;
									}
									else
									{
										return 0;
									}
								}
							);
						};

						const uint64 leftLikesCount = getLikeCount(leftAssetGuid);
						const uint64 rightLikesCount = getLikeCount(rightAssetGuid);

						if (order == DataSource::SortingOrder::Descending)
						{
							return leftLikesCount < rightLikesCount;
						}
						else
						{
							return rightLikesCount < leftLikesCount;
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			likesCountPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_likesCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			playsCountPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_playsCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);
		assetManager.RegisterDataPropertySortingCallback(
			playsCountPropertyIdentifier,
			[this, &assetManager](
				const DataSource::CachedQuery& cachedQuery,
				const DataSource::SortingOrder order,
				DataSource::SortedQueryIndices& cachedSortedQueryOut
			)
			{
				const Asset::Mask& __restrict assetMask = reinterpret_cast<const Asset::Mask&>(cachedQuery);
				cachedSortedQueryOut.Clear();
				cachedSortedQueryOut.Reserve(assetMask.GetNumberOfSetBits());

				for (Asset::Identifier::IndexType assetIdentifierIndex : assetMask.GetSetBitsIterator())
				{
					cachedSortedQueryOut.EmplaceBack(assetIdentifierIndex);
				}

				Threading::SharedLock lock(m_assetsMutex);
				Algorithms::Sort(
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.begin(),
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.end(),
					[this,
			     &assetManager,
			     order,
			     endIt = m_assets.end()](const Asset::Identifier::IndexType leftIndex, const Asset::Identifier::IndexType rightIndex)
					{
						const Asset::Guid leftAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(leftIndex));
						const Asset::Guid rightAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(rightIndex));

						const auto leftIt = m_assets.Find(leftAssetGuid);
						const auto rightIt = m_assets.Find(rightAssetGuid);
						const Optional<const AssetEntry*> pLeftAssetEntry = leftIt != endIt ? &leftIt->second : nullptr;
						const Optional<const AssetEntry*> pRightAssetEntry = rightIt != endIt ? &rightIt->second : nullptr;

						const uint64 leftCount = pLeftAssetEntry.IsValid() ? pLeftAssetEntry->m_playsCount : 0;
						const uint64 rightCount = pRightAssetEntry.IsValid() ? pRightAssetEntry->m_playsCount : 0;

						if (order == DataSource::SortingOrder::Descending)
						{
							return leftCount < rightCount;
						}
						else
						{
							return rightCount < leftCount;
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			playsCountPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_playsCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			remixCountPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_remixCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);
		assetManager.RegisterDataPropertySortingCallback(
			remixCountPropertyIdentifier,
			[this, &assetManager](
				const DataSource::CachedQuery& cachedQuery,
				const DataSource::SortingOrder order,
				DataSource::SortedQueryIndices& cachedSortedQueryOut
			)
			{
				const Asset::Mask& __restrict assetMask = reinterpret_cast<const Asset::Mask&>(cachedQuery);
				cachedSortedQueryOut.Clear();
				cachedSortedQueryOut.Reserve(assetMask.GetNumberOfSetBits());

				for (Asset::Identifier::IndexType assetIdentifierIndex : assetMask.GetSetBitsIterator())
				{
					cachedSortedQueryOut.EmplaceBack(assetIdentifierIndex);
				}

				Threading::SharedLock lock(m_assetsMutex);
				Algorithms::Sort(
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.begin(),
					(Asset::Identifier::IndexType*)cachedSortedQueryOut.end(),
					[this,
			     &assetManager,
			     order,
			     endIt = m_assets.end()](const Asset::Identifier::IndexType leftIndex, const Asset::Identifier::IndexType rightIndex)
					{
						const Asset::Guid leftAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(leftIndex));
						const Asset::Guid rightAssetGuid = assetManager.GetAssetGuid(Asset::Identifier::MakeFromValidIndex(rightIndex));

						const auto leftIt = m_assets.Find(leftAssetGuid);
						const auto rightIt = m_assets.Find(rightAssetGuid);
						const Optional<const AssetEntry*> pLeftAssetEntry = leftIt != endIt ? &leftIt->second : nullptr;
						const Optional<const AssetEntry*> pRightAssetEntry = rightIt != endIt ? &rightIt->second : nullptr;

						const uint64 leftCount = pLeftAssetEntry.IsValid() ? pLeftAssetEntry->m_remixCount : 0;
						const uint64 rightCount = pRightAssetEntry.IsValid() ? pRightAssetEntry->m_remixCount : 0;

						if (order == DataSource::SortingOrder::Descending)
						{
							return leftCount < rightCount;
						}
						else
						{
							return rightCount < leftCount;
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			remixCountPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							UnicodeString string;
							string.Format("{}", pAssetEntry->m_remixCount);
							return Move(string);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		const Tag::Identifier editableLocalProjectTagIdentifier = tagRegistry.FindOrRegister(Tags::EditableLocalProjectTagGuid);
		const Tag::Identifier localCreatorTagIdentifier = tagRegistry.FindOrRegister(DataSource::Players::LocalPlayerTagGuid);

		assetManager.RegisterDataPropertyCallback(
			assetEditTitlePropertyIdentifier,
			[editableLocalProjectTagIdentifier, localCreatorTagIdentifier, &assetManager](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const bool isExistingLocalProject = assetManager.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
				const bool isLocalCreatorProject = assetManager.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

				if (isExistingLocalProject | isLocalCreatorProject)
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Edit"));
				}
				else
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Remix"));
				}
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetEditTitlePropertyIdentifier,
			[editableLocalProjectTagIdentifier, localCreatorTagIdentifier, &assetLibrary](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const bool isExistingLocalProject = assetLibrary.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
				const bool isLocalCreatorProject = assetLibrary.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

				if (isExistingLocalProject | isLocalCreatorProject)
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Edit"));
				}
				else
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Remix"));
				}
			}
		);

		assetManager.RegisterDataPropertyCallback(
			assetEditIconPropertyIdentifier,
			[editableLocalProjectTagIdentifier, localCreatorTagIdentifier, &assetManager](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const bool isExistingLocalProject = assetManager.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
				const bool isLocalCreatorProject = assetManager.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

				if (isExistingLocalProject | isLocalCreatorProject)
				{
					// Edit
					return "c5bd0ad1-b27a-17e0-782c-68bbe391d890"_asset;
				}
				else
				{
					// Remix
					return "da1db5c7-8409-8da0-72ea-a7e840842bc8"_asset;
				}
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetEditIconPropertyIdentifier,
			[editableLocalProjectTagIdentifier, localCreatorTagIdentifier, &assetLibrary](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const bool isExistingLocalProject = assetLibrary.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
				const bool isLocalCreatorProject = assetLibrary.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

				if (isExistingLocalProject | isLocalCreatorProject)
				{
					// Edit
					return "c5bd0ad1-b27a-17e0-782c-68bbe391d890"_asset;
				}
				else
				{
					// Remix
					return "da1db5c7-8409-8da0-72ea-a7e840842bc8"_asset;
				}
			}
		);

		assetManager.RegisterDataPropertyCallback(
			wasAssetLikedPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							return pAssetEntry->m_flags.IsSet(AssetFlags::Liked);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			wasAssetLikedPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							return pAssetEntry->m_flags.IsSet(AssetFlags::Liked);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			isAssetInCartPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							if (pAssetEntry->m_flags.IsSet(AssetFlags::IsInCart))
							{
								return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Remove from cart"));
							}
							else
							{
								return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Add to cart"));
							}
						}
						else
						{
							return {};
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			isAssetInCartPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid())
						{
							if (pAssetEntry->m_flags.IsSet(AssetFlags::IsInCart))
							{
								return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Remove from cart"));
							}
							else
							{
								return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Add to cart"));
							}
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			creatorTagPropertyIdentifier,
			[this, &assetManager, &tagRegistry](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[&tagRegistry](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid() && pAssetEntry->m_creatorTagGuid.IsValid())
						{
							return tagRegistry.FindOrRegister(pAssetEntry->m_creatorTagGuid);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			creatorTagPropertyIdentifier,
			[this, &assetLibrary, &tagRegistry](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[&tagRegistry](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid() && pAssetEntry->m_creatorTagGuid.IsValid())
						{
							return tagRegistry.FindOrRegister(pAssetEntry->m_creatorTagGuid);
						}
						else
						{
							return {};
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			creatorNamePropertyIdentifier,
			[this, &assetManager, &playersDataSource = game.GetPlayersDataSource()](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[&playersDataSource](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid() && pAssetEntry->m_creatorId.IsValid())
						{
							return playersDataSource.VisitPlayerInfo(
								pAssetEntry->m_creatorId,
								[](const Optional<const DataSource::Players::PlayerInfo*> pPlayerInfo) -> UnicodeString
								{
									if (pPlayerInfo.IsValid())
									{
										return pPlayerInfo->username;
									}
									else
									{
										return UnicodeString(MAKE_UNICODE_LITERAL("Sceneri"));
									}
								}
							);
						}
						else
						{
							return UnicodeString(MAKE_UNICODE_LITERAL("Sceneri"));
						}
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			creatorNamePropertyIdentifier,
			[this, &assetLibrary, &playersDataSource = game.GetPlayersDataSource()](const Asset::Identifier assetIdentifier
		  ) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[&playersDataSource](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						if (pAssetEntry.IsValid() && pAssetEntry->m_creatorTagGuid.IsValid())
						{
							return playersDataSource.VisitPlayerInfo(
								pAssetEntry->m_creatorId,
								[](const Optional<const DataSource::Players::PlayerInfo*> pPlayerInfo) -> UnicodeString
								{
									if (pPlayerInfo.IsValid())
									{
										return pPlayerInfo->username;
									}
									else
									{
										return UnicodeString(MAKE_UNICODE_LITERAL("Sceneri"));
									}
								}
							);
						}
						else
						{
							return UnicodeString(MAKE_UNICODE_LITERAL("Sceneri"));
						}
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			assetIsDraftPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						return pAssetEntry.IsInvalid();
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetIsDraftPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						return pAssetEntry.IsInvalid();
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			assetIsPublishedPropertyIdentifier,
			[this, &assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						return pAssetEntry.IsValid();
					}
				);
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetIsPublishedPropertyIdentifier,
			[this, &assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				return VisitEntry(
					assetGuid,
					[](const Optional<const AssetEntry*> pAssetEntry) -> DataSource::PropertyValue
					{
						return pAssetEntry.IsValid();
					}
				);
			}
		);

		assetManager.RegisterDataPropertyCallback(
			assetTimeSinceEditPropertyIdentifier,
			[&assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				ProjectDatabase projectDatabase = EditableProjectDatabase();

				const Time::Timestamp lastUsageTime = Time::Timestamp::GetCurrent() - projectDatabase.FindProjectLastUsageTime(assetGuid);
				if (lastUsageTime.IsValid())
				{
					return lastUsageTime.ToRelativeString();
				}
				else
				{
					return String{""};
				}
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetTimeSinceEditPropertyIdentifier,
			[&assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				ProjectDatabase projectDatabase = EditableProjectDatabase();

				const Time::Timestamp lastUsageTime = Time::Timestamp::GetCurrent() - projectDatabase.FindProjectLastUsageTime(assetGuid);
				if (lastUsageTime.IsValid())
				{
					return lastUsageTime.ToRelativeString();
				}
				else
				{
					return String{""};
				}
			}
		);

		assetManager.RegisterDataPropertyCallback(
			assetTimeSincePlayPropertyIdentifier,
			[&assetManager](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
				ProjectDatabase projectDatabase = PlayableProjectDatabase();

				const Time::Timestamp lastUsageTime = Time::Timestamp::GetCurrent() - projectDatabase.FindProjectLastUsageTime(assetGuid);
				if (lastUsageTime.IsValid())
				{
					return lastUsageTime.ToRelativeString();
				}
				else
				{
					return String{""};
				}
			}
		);
		assetLibrary.RegisterDataPropertyCallback(
			assetTimeSincePlayPropertyIdentifier,
			[&assetLibrary](const Asset::Identifier assetIdentifier) -> DataSource::PropertyValue
			{
				const Asset::Guid assetGuid = assetLibrary.GetAssetGuid(assetIdentifier);
				ProjectDatabase projectDatabase = PlayableProjectDatabase();

				const Time::Timestamp lastUsageTime = Time::Timestamp::GetCurrent() - projectDatabase.FindProjectLastUsageTime(assetGuid);
				if (lastUsageTime.IsValid())
				{
					return lastUsageTime.ToRelativeString();
				}
				else
				{
					return String{""};
				}
			}
		);
	}

	AssetDatabase::~AssetDatabase()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_likes_count"), "asset_likes_count");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_liked"), "asset_liked");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_in_cart"), "asset_in_cart");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_price"), "asset_price");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_plays_count"), "asset_plays_count");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_creator_tag"), "asset_creator_tag");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_creator_name"), "asset_creator_name");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_edit_title"), "asset_edit_title");
		dataSourceCache.DeregisterProperty(dataSourceCache.FindPropertyIdentifier("asset_edit_icon"), "asset_edit_icon");
	}

	void AssetDatabase::ReserveAssets(const uint32 assetCount)
	{
		{
			Threading::UniqueLock lock(m_assetsMutex);
			m_requestedAssetCapacity += assetCount;
			m_assets.Reserve(m_requestedAssetCapacity);
		}

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
		assetLibrary.Reserve(assetCount);
	}

	Optional<AssetEntry*> AssetDatabase::ParseAsset(
		const Serialization::Reader reader,
		const Asset::Guid assetGuid,
		Asset::Manager& assetManager,
		const EnumFlags<ParsingFlags> parsingFlags,
		Game& gameAPI,
		const Optional<Guid> tag
	)
	{
		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();

		// TODO: Require that filters are GUIDs, and then those can be tags too.
		// Also add tags for owned

		struct EntryResult
		{
			AssetEntry& assetEntry;
			Threading::SharedLock<Threading::SharedMutex> lock;
		};

		auto getOrEmplaceAssetEntry = [this, assetGuid, reader, parsingFlags]() -> EntryResult
		{
			auto findAsset = [this](const Asset::Guid assetGuid) -> Optional<AssetEntry*>
			{
				if (auto it = m_assets.Find(assetGuid); it != m_assets.end())
				{
					return it->second;
				}
				else
				{
					return Invalid;
				}
			};

			auto parseAsset = [](AssetEntry& assetEntry, const Serialization::Reader reader, const EnumFlags<ParsingFlags> parsingFlags)
			{
				assetEntry.m_id = *reader.Read<uint32>("id");
				Assert(assetEntry.m_id > 0);
				assetEntry.m_name = *reader.Read<UnicodeString>("name");
				assetEntry.m_description = reader.ReadWithDefaultValue<UnicodeString>("description", {});
				assetEntry.m_defaultVariationId = reader.ReadWithDefaultValue<uint32>("default_variation_id", 0u);
				assetEntry.m_flags |= AssetFlags::UpToDate * !parsingFlags.IsSet(ParsingFlags::FromCache);
				assetEntry.m_flags |= AssetFlags::Active;
				assetEntry.m_creatorId = PersistentUserIdentifier{reader.ReadWithDefaultValue<PersistentUserIdentifier::Type>("creator_id", 0u)};
				assetEntry.m_creatorTagGuid = reader.ReadWithDefaultValue<Guid>("profile_uuid", Guid(DataSource::Players::AdminTagGuid));
				assetEntry.m_likesCount = reader.ReadWithDefaultValue<uint32>("number_likes", 0u);
				assetEntry.m_playsCount = reader.ReadWithDefaultValue<uint32>("number_plays", 0u);
			};

			{
				Threading::SharedLock sharedLock(m_assetsMutex);
				if (const Optional<AssetEntry*> pExistingEntry = findAsset(assetGuid))
				{
					parseAsset(*pExistingEntry, reader, parsingFlags);
					return {*pExistingEntry, Move(sharedLock)};
				}
			}

			Threading::UniqueLock uniqueLock(m_assetsMutex);
			if (Optional<AssetEntry*> pExistingEntry = findAsset(assetGuid))
			{
				uniqueLock.Unlock();
				Threading::SharedLock sharedLock(m_assetsMutex);
				pExistingEntry = findAsset(assetGuid);
				parseAsset(*pExistingEntry, reader, parsingFlags);
				return {*pExistingEntry, Move(sharedLock)};
			}
			else
			{
				m_requestedAssetCapacity++;
				m_assets.Emplace(
					Guid(assetGuid),
					AssetEntry{
						assetGuid,
						*reader.Read<uint32>("id"),
						AssetFlags::Active | AssetFlags::UpToDate * !parsingFlags.IsSet(ParsingFlags::FromCache),
						*reader.Read<UnicodeString>("name"),
						reader.ReadWithDefaultValue<UnicodeString>("description", {}),
						0,
						0,
						0,
						IO::URI{},
						String{},
						reader.ReadWithDefaultValue<uint32>("price", 0u),
						reader.ReadWithDefaultValue<uint32>("default_variation_id", 0u),
						reader.ReadWithDefaultValue<uint32>("creator_id", 0u),
						reader.ReadWithDefaultValue<Guid>("profile_uuid", Guid(DataSource::Players::AdminTagGuid))
					}
				);
				uniqueLock.Unlock();

				Threading::SharedLock sharedLock(m_assetsMutex);
				auto it = m_assets.Find(assetGuid);
				AssetEntry& assetEntry = it->second;
				return {assetEntry, Move(sharedLock)};
			}
		};

		EntryResult entryResult = getOrEmplaceAssetEntry();
		AssetEntry& assetEntry = entryResult.assetEntry;

		ParseBackendAssetEntry(assetEntry, reader, parsingFlags);

		const Guid assetTypeGuid = *reader.Read<Guid>("context_uuid");

		Guid componentTypeGuid;
		if (const Optional<ConstStringView> componentTypeGuidString = assetEntry.FindStorageValue("ComponentTypeGuid"))
		{
			componentTypeGuid = Guid::TryParse(*componentTypeGuidString);
		}

		Guid thumbnailAssetGuid;
		if (const Optional<ConstStringView> thumbnailAssetGuidString = assetEntry.FindStorageValue("ThumbnailAssetGuid"))
		{
			thumbnailAssetGuid = Guid::TryParse(*thumbnailAssetGuidString);
		}

		const ConstUnicodeStringView backendAssetName = assetEntry.m_name;
		IO::Path assetPathName;
		if (assetEntry.m_mainFileURI.HasElements())
		{
			assetPathName = IO::Path::Combine(backendAssetName, assetEntry.m_mainFileURI.GetAllExtensions().GetStringView());
		}
		else
		{
			assetPathName = IO::Path::Combine(backendAssetName, Asset::Asset::FileExtension);
		}

		IO::Path assetLibraryPath;
		if (Optional<ConstStringView> assetPathStringView = assetEntry.FindStorageValue("Path");
		    assetPathStringView.IsValid() && assetPathStringView->HasElements())
		{
			*assetPathStringView += assetPathStringView->StartsWith("/");
			IO::Path::StringType assetPathString{*assetPathStringView};

			IO::Path assetPath{Move(assetPathString)};
			assetPath.MakeNativeSlashes();

			assetLibraryPath = Networking::HTTP::FileCache::GetAssetPath(assetGuid, assetPath, assetPathName.GetAllExtensions());
		}
		else
		{
			assetLibraryPath = Networking::HTTP::FileCache::GetAssetPath(assetGuid, assetPathName.GetAllExtensions());
		}

		Asset::DatabaseEntry::DependenciesStorage dependencies;
		Asset::DatabaseEntry::TagsStorage tags;
		if (const Optional<Serialization::Reader> dataEntitiesReader = reader.FindSerializer("data_entities"))
		{
			for (const Serialization::Reader entityReader : dataEntitiesReader->GetArrayView())
			{
				const ConstStringView dataEntityName = *entityReader.Read<ConstStringView>("name");
				if (dataEntityName == "Dependencies")
				{
					Serialization::Data dependenciesData(*entityReader.Read<ConstStringView>("data"));
					Assert(dependenciesData.IsValid());
					if (LIKELY(dependenciesData.IsValid()))
					{
						Serialization::Reader dependenciesReader(dependenciesData);
						dependenciesReader.SerializeInPlace(dependencies);
					}
				}
				else if (dataEntityName == "Tags")
				{
					Serialization::Data tagsData(*entityReader.Read<ConstStringView>("data"));
					Assert(tagsData.IsValid());
					if (LIKELY(tagsData.IsValid()))
					{
						Serialization::Reader tagsReader(tagsData);
						tagsReader.SerializeInPlace(tags);
					}
				}
			}
		}

		tags.EmplaceBack(assetEntry.m_creatorTagGuid);

		if (assetEntry.m_creatorId == gameAPI.GetLocalPlayerInternalIdentifier())
		{
			tags.EmplaceBack(DataSource::Players::LocalPlayerTagGuid);
		}

		if (tag.IsValid())
		{
			tags.EmplaceBack(*tag);
		}

		Asset::DatabaseEntry::ContainerContentsStorage containerStorage;
		if (const Optional<Serialization::Reader> containerContentsReader = reader.FindSerializer("package_contents");
		    containerContentsReader.IsValid() && !containerContentsReader->GetValue().IsNull())
		{
			containerStorage.Reserve((uint32)containerContentsReader->GetArraySize());
			for (const Serialization::Reader packageAssetReader : containerContentsReader->GetArrayView())
			{
				containerStorage.EmplaceBack(*packageAssetReader.Read<Guid>("asset_uuid"));
			}
		}

		{
			const Asset::Identifier assetIdentifier = assetLibrary.VisitAssetEntry(
				assetGuid,
				[&assetLibrary,
			   assetGuid,
			   assetTypeGuid,
			   componentTypeGuid,
			   &assetLibraryPath,
			   &assetEntry,
			   thumbnailAssetGuid,
			   &tags,
			   &dependencies,
			   &containerStorage,
			   reader,
			   parsingFlags](const Optional<Asset::DatabaseEntry*> pAssetLibraryEntry) mutable
				{
					if (pAssetLibraryEntry.IsValid())
					{
						const Asset::Identifier assetIdentifier = assetLibrary.GetAssetIdentifier(assetGuid);

						pAssetLibraryEntry->m_assetTypeGuid = assetTypeGuid;
						pAssetLibraryEntry->m_componentTypeGuid = componentTypeGuid;
						pAssetLibraryEntry->m_path = Move(assetLibraryPath);
						pAssetLibraryEntry->SetName(UnicodeString(assetEntry.m_name));
						pAssetLibraryEntry->m_description = assetEntry.m_description;
						pAssetLibraryEntry->m_thumbnailGuid = thumbnailAssetGuid;
						pAssetLibraryEntry->m_tags = Move(tags);
						pAssetLibraryEntry->m_dependencies = Move(dependencies);
						pAssetLibraryEntry->m_containerContents = Move(containerStorage);

						return assetIdentifier;
					}
					else
					{
						return Asset::Identifier{};
					}
				}
			);
			if (assetIdentifier.IsValid())
			{
				ParseAssetLibraryEntry(
					assetManager,
					assetIdentifier,
					assetGuid,
					assetTypeGuid,
					thumbnailAssetGuid,
					assetEntry,
					reader,
					parsingFlags,
					gameAPI
				);
				assetLibrary.OnDataChanged();
				return Invalid;
			}
		}

		const Asset::Identifier assetsCacheFolderAssetIdentifier =
			assetLibrary.FindOrRegisterFolder(Networking::HTTP::FileCache::GetAssetsDirectory().GetParentPath(), Asset::Identifier{});
		const Asset::Identifier assetIdentifier = assetLibrary.RegisterAsset(
			assetGuid,
			Asset::DatabaseEntry{
				assetTypeGuid,
				componentTypeGuid,
				Move(assetLibraryPath),
				UnicodeString(assetEntry.m_name),
				UnicodeString(assetEntry.m_description),
				thumbnailAssetGuid,
				Move(tags),
				Move(dependencies),
				Move(containerStorage)
			},
			assetsCacheFolderAssetIdentifier
		);
		if (LIKELY(assetIdentifier.IsValid()))
		{
			ParseAssetLibraryEntry(
				assetManager,
				assetIdentifier,
				assetGuid,
				assetTypeGuid,
				thumbnailAssetGuid,
				assetEntry,
				reader,
				parsingFlags,
				gameAPI
			);

			OnAssetAdded(assetEntry);
			assetLibrary.OnDataChanged();
		}

		return assetEntry;
	}

	void AssetDatabase::DeactivateAsset(const Asset::Guid assetGuid, Asset::Manager& assetManager)
	{
		{
			Threading::SharedLock lock(m_assetsMutex);
			const auto it = m_assets.Find(assetGuid);
			if (it != m_assets.end())
			{
				AssetEntry& assetEntry = it->second;
				assetEntry.m_flags.Clear(AssetFlags::Active);
			}
		}

		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
		if (const Asset::Identifier assetIdentifier = assetLibrary.GetAssetIdentifier(assetGuid))
		{
			UNUSED(assetIdentifier);
			// TODO: Consider changing asset library tags
		}
	}

	void AssetDatabase::MarkAllAssetsUpToDate(const Guid assetTypeGuid)
	{
		Asset::Library& assetLibrary = System::Get<Asset::Manager>().GetAssetLibrary();

		Threading::SharedLock lock(m_assetsMutex);
		for (decltype(m_assets)::PairType& assetPair : m_assets)
		{
			AssetEntry& assetEntry = assetPair.second;
			if (assetEntry.m_flags.IsNotSet(AssetFlags::UpToDate))
			{
				if (assetLibrary.GetAssetTypeGuid(assetPair.first) == assetTypeGuid)
				{
					assetEntry.m_flags |= AssetFlags::UpToDate;
				}
			}
		}
	}

	void Plugin::OnLoaded(Application&)
	{
		const Optional<Engine*> pEngine = System::Find<Engine>();

		if (pEngine.IsValid())
		{
			// TODO: Register only when necessary
			m_server.RegisterSession(*pEngine);
		}
	}

	void Plugin::OnUnloaded(Application&)
	{
	}

	void Plugin::QueueRequestInternal(
		const HTTP::RequestType requestType,
		const IO::URIView endpointName,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		HTTP::RequestData&& requestData,
		RequestCallback&& responseCallback
	)
	{
		if (m_pHttpPlugin == nullptr)
		{
			m_pHttpPlugin = System::FindPlugin<HTTP::Plugin>();
		}

		const Environment& environment = GetEnvironment();
		m_pHttpPlugin->GetHighPriorityWorker().QueueRequest(
			Networking::HTTP::Worker::Request{
				requestType,
				IO::URI::Combine(environment.m_backendURI, endpointName),
				headers,
				Forward<HTTP::RequestData>(requestData),
				[responseCallback = Move(responseCallback)](
					[[maybe_unused]] const Networking::HTTP::Worker::RequestInfo& requestInfo,
					const Networking::HTTP::Worker::ResponseData& responseData
				)
				{
					Serialization::Data responseSerializedData(responseData.m_responseBody);
					Serialization::Reader responseReader(responseSerializedData);
					bool success = responseData.m_responseCode.IsSuccessful();
					if (responseSerializedData.IsValid() && !responseSerializedData.GetDocument().IsArray())
					{
						success &= responseReader.ReadWithDefaultValue<bool>("success", true);
					}
					Assert(!responseData.HasHeader("X-Endpoint-Deprecated"), "Calling deprecated LootLocker end-point");

#if DEBUG_BUILD
					if (UNLIKELY(!success))
					{
						LogWarning(
							"Backend request of type {} to uri {} failed with response code {}!",
							HTTP::GetRequestTypeString(requestInfo.m_requestType),
							String(requestInfo.m_url.GetView().GetStringView()),
							responseData.m_responseCode.m_code
						);
						if (const Optional<const HTTP::RequestBody*> pRequestBody = requestInfo.m_requestData.Get<HTTP::RequestBody>())
						{
							LogMessage("Request body: {}", *pRequestBody);
						}
						else if (const Optional<const HTTP::FormData*> pFormData = requestInfo.m_requestData.Get<HTTP::FormData>())
						{
							LogMessage("With form data parts:");
							for (const HTTP::FormDataPart& formDataPart : pFormData->m_parts)
							{
								LogMessage(
									"Part: {} (file name {}, content type {})",
									formDataPart.m_name,
									formDataPart.m_fileName,
									formDataPart.m_contentType
								);
							}
						}
						LogMessage("Response Headers:");
						for (const ConstStringView header : requestInfo.m_requestHeaders)
						{
							LogMessage(header);
						}
						if (const Optional<const HTTP::RequestBody*> pRequestBody = requestInfo.m_requestData.Get<HTTP::RequestBody>();
				        pRequestBody.IsValid() && pRequestBody->HasElements())
						{
							LogMessage("Response Body: {}", *pRequestBody);
						}
					}
#endif

					responseCallback(success, responseData.m_responseCode, responseReader);
				}
			},
			Threading::JobPriority::HighPriorityBackendNetworking
		);
	}

	void Plugin::QueueGameRequest(
		const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback
	)
	{
		Assert(m_game.m_signInFlags.IsSet(SignInFlags::HasSessionToken));
		m_game.QueueRequest(requestType, endpointName, Forward<String>(requestBody), Forward<RequestCallback>(callback));
	}

	void Plugin::QueueServerRequest(
		const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback
	)
	{
		Assert(m_server.m_hasSessionToken);
		m_server.QueueRequest(requestType, endpointName, Forward<String>(requestBody), Forward<RequestCallback>(callback));
	}

	void StreamedRequest::Parse(String& responseBody)
	{
		const ConstStringView fullResponseBody = responseBody;
		Assert(fullResponseBody.IsEmpty() || fullResponseBody[0] == '{');
		ConstStringView checkedBody = fullResponseBody.GetSubstringFrom(checkedOffset);
		uint32 nextObjectStartOffset = 0;

		for (auto it = checkedBody.begin(), endIt = checkedBody.end(); it != endIt;)
		{
			const char& element = *it;

			isInString ^= (element == '"') && ((*(it - 1)) != '\\');
			if (isInString)
			{
				++it;
				continue;
			}

			indendationCount += (element == '{') | (element == '[');
			indendationCount -= (element == '}') | (element == ']');
			Assert(indendationCount >= 0);

			if (indendationCount == 0)
			{
				// complete object
				Assert(fullResponseBody[0] == '{');
				const uint32 objectEndIndex = fullResponseBody.GetIteratorIndex(Memory::GetAddressOf(element)) + 1;
				const ConstStringView jsonObject = fullResponseBody.GetSubstring(nextObjectStartOffset, objectEndIndex - nextObjectStartOffset);
				Serialization::Data objectData(jsonObject);
				Assert(objectData.IsValid());
				if (LIKELY(objectData.IsValid()))
				{
					Serialization::Reader objectReader(objectData);

					callback(objectReader, jsonObject);
				}
				nextObjectStartOffset = objectEndIndex;
				checkedOffset = objectEndIndex;
				checkedBody = fullResponseBody.GetSubstringFrom(checkedOffset);
				Assert(checkedBody.IsEmpty() || checkedBody[0] == '{');

				it = checkedBody.begin();
				endIt = checkedBody.end();
			}
			else
			{
				++it;
			}
		}
		// Remove fully parsed objects
		responseBody.Remove(ConstStringView{fullResponseBody.begin(), nextObjectStartOffset});
		Assert(responseBody.IsEmpty() || responseBody[0] == '{');

		checkedOffset = responseBody.GetSize();
	}

	void Plugin::QueueStreamedRequestInternal(
		const HTTP::RequestType requestType,
		const EnumFlags<RequestFlags> requestFlags,
		const IO::URIView endpointName,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		HTTP::RequestData&& requestData,
		StreamResultCallback&& callback,
		StreamingFinishedCallback&& finishedCallback
	)
	{
		if (m_pHttpPlugin == nullptr)
		{
			m_pHttpPlugin = System::FindPlugin<HTTP::Plugin>();
		}

		HTTP::Worker& worker = requestFlags.IsSet(RequestFlags::HighPriority) ? m_pHttpPlugin->GetHighPriorityWorker()
		                                                                      : m_pHttpPlugin->GetLowPriorityWorker();

		const Environment& environment = GetEnvironment();
		worker.QueueRequest(
			Networking::HTTP::Worker::Request{
				requestType,
				IO::URI::Combine(environment.m_phpBackendURI, endpointName),
				headers,
				Forward<HTTP::RequestData>(requestData),
				[finishedCallback = Move(finishedCallback
		     )](const Networking::HTTP::Worker::RequestInfo&, const Networking::HTTP::Worker::ResponseData& responseData)
				{
					const bool success = responseData.m_responseCode.IsSuccessful();
					finishedCallback(success);
				},
				[streamInfo = StreamedRequest{0u, 0u, false, Forward<StreamResultCallback>(callback)}](
					const Networking::HTTP::Worker::RequestInfo&,
					String& responseBody
				) mutable
				{
					streamInfo.Parse(responseBody);
				}
			},
			requestFlags.IsSet(RequestFlags::HighPriority) ? Threading::JobPriority::HighPriorityBackendNetworking
																										 : Threading::JobPriority::LowPriorityBackendNetworking
		);
	}

	void Plugin::PopulateAsset(
		const Asset::Guid assetGuid,
		const Asset::DatabaseEntry& assetEntry,
		const Asset::Database& assetDatabase,
		const IO::PathView assetsFolderPath,
		Serialization::Writer assetWriter
	)
	{
		Assert(assetEntry.IsValid());

		assetWriter.Serialize("name", assetEntry.GetName());
		assetWriter.Serialize("description", assetEntry.m_description);
		assetWriter.Serialize("context_uuid", assetEntry.m_assetTypeGuid);
		assetWriter.Serialize("uuid", assetGuid);
		// Don't activate until we receive a successful response
		assetWriter.Serialize("active", false);

		{
			AssetEntry::KeyValueStorage keyValueStorage;
			keyValueStorage.Reserve(3);
			if (assetEntry.m_componentTypeGuid.IsValid())
			{
				keyValueStorage.Emplace(ConstStringView("ComponentTypeGuid"), String(assetEntry.m_componentTypeGuid.ToString()));
			}
			if (assetEntry.m_thumbnailGuid.IsValid())
			{
				keyValueStorage.Emplace(ConstStringView("ThumbnailAssetGuid"), String(assetEntry.m_thumbnailGuid.ToString()));
			}

			if (assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid || assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid)
			{
				keyValueStorage.Emplace(ConstStringView("AssetDatabaseGuid"), String(assetDatabase.GetGuid().ToString()));
			}

			if (assetEntry.m_path.IsRelativeTo(assetsFolderPath))
			{
				keyValueStorage.Emplace(ConstStringView("Path"), String(assetEntry.m_path.GetRelativeToParent(assetsFolderPath).GetStringView()));
			}

			assetWriter.Serialize("storage", keyValueStorage);
		}

		if (assetEntry.m_dependencies.HasElements() || assetEntry.m_tags.HasElements())
		{

			assetWriter.SerializeArrayWithCallback(
				"data_entities",
				[&assetEntry](Serialization::Writer dataEntitySerializer, [[maybe_unused]] const uint32 index)
				{
					switch (index)
					{
						// Dependencies
						case 0:
						{
							if (assetEntry.m_dependencies.HasElements())
							{
								Serialization::Data dependencyData(rapidjson::Type::kArrayType);
								Serialization::Writer dependenciesWriter(dependencyData);
								dependenciesWriter.SerializeInPlace(assetEntry.m_dependencies.GetView());

								dataEntitySerializer.GetValue() = Serialization::Value(rapidjson::kObjectType);
								dataEntitySerializer.Serialize("name", ConstStringView("Dependencies"));
								dataEntitySerializer.Serialize("data", dependencyData.SaveToBuffer<String>(Serialization::SavingFlags{}));
								return true;
							}
							else
							{
								return false;
							}
						}
						// Tags
						case 1:
						{
							if (assetEntry.m_tags.HasElements())
							{
								Serialization::Data tagsData(rapidjson::Type::kArrayType);
								Serialization::Writer tagsWriter(tagsData);
								tagsWriter.SerializeInPlace(assetEntry.m_tags.GetView());

								dataEntitySerializer.GetValue() = Serialization::Value(rapidjson::kObjectType);
								dataEntitySerializer.Serialize("name", ConstStringView("Tags"));
								dataEntitySerializer.Serialize("data", tagsData.SaveToBuffer<String>(Serialization::SavingFlags{}));
								return true;
							}
							else
							{
								return false;
							}
						}
						default:
							ExpectUnreachable();
					}
				},
				2
			);
		}

		if (assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid || assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid)
		{
			Serialization::Data dependencyData(rapidjson::Type::kArrayType);
			Serialization::Writer dependencyWriter(dependencyData);

			Assert(assetDatabase.HasAsset(assetDatabase.GetGuid()));

			assetDatabase.IterateAssets(
				[dependencyWriter](const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
				{
					if (assetEntry.m_assetTypeGuid != PluginAssetFormat.assetTypeGuid && assetEntry.m_assetTypeGuid != ProjectAssetFormat.assetTypeGuid)
					{
						Serialization::Writer childAssetWriter = dependencyWriter.EmplaceArrayElement();
						childAssetWriter.GetValue() = Serialization::Value(rapidjson::kObjectType);
						childAssetWriter.Serialize("asset_uuid", assetGuid);
					}
					return Memory::CallbackResult::Continue;
				}
			);

			assetWriter.AddMember("package_contents", Move(dependencyData.GetDocument()));
		}
		else if (assetEntry.m_assetTypeGuid == MeshSceneAssetType::AssetFormat.assetTypeGuid)
		{
			Serialization::Data dependencyData(rapidjson::Type::kArrayType);
			Serialization::Writer dependencyWriter(dependencyData);

			const IO::PathView meshSceneDirectoryPath = assetEntry.m_path.GetParentPath();

			assetDatabase.IterateAssets(
				[dependencyWriter, meshSceneDirectoryPath](const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
				{
					if (assetEntry.m_assetTypeGuid == MeshPartAssetType::AssetFormat.assetTypeGuid && assetEntry.m_path.GetParentPath() == meshSceneDirectoryPath)
					{
						Serialization::Writer childAssetWriter = dependencyWriter.EmplaceArrayElement();
						childAssetWriter.GetValue() = Serialization::Value(rapidjson::kObjectType);
						childAssetWriter.Serialize("asset_uuid", assetGuid);
					}
					return Memory::CallbackResult::Continue;
				}
			);

			assetWriter.AddMember("package_contents", Move(dependencyData.GetDocument()));
		}
	}

	struct AssetCreationInfo
	{
		ngine::Guid guid;
		bool isPackage;
		Optional<const Asset::DatabaseEntry*> pAssetEntry;
		uint32 depth;

		[[nodiscard]] uint32 GetDepth() const
		{
			return depth - isPackage;
		}

		[[nodiscard]] bool operator==(const AssetCreationInfo& __restrict other) const
		{
			return guid == other.guid;
		}

		[[nodiscard]] bool operator<(const AssetCreationInfo& __restrict other) const
		{
			return GetDepth() > other.GetDepth();
		}
	};

	FixedCapacityVector<AssetCreationInfo> Plugin::PopulateAssetBatch(Asset::Database& assetDatabase)
	{
		struct Comparator
		{
			[[nodiscard]] bool operator()(const AssetCreationInfo& __restrict left, const AssetCreationInfo& __restrict right) const
			{
				return left < right;
			}
		};
		FixedCapacityVector<AssetCreationInfo> assets(Memory::Reserve, assetDatabase.GetAssetCount());

		assetDatabase.IterateAssets(
			[&assetDatabase, &assets](const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
			{
				Assert(assetGuid.IsValid());
				Assert(assetEntry.m_assetTypeGuid.IsValid());
				if (assetGuid.IsValid() & assetEntry.m_assetTypeGuid.IsValid())
				{
					if (!assets.Contains(AssetCreationInfo{assetGuid}))
					{
						assets.EmplaceBack(AssetCreationInfo{
							assetGuid,
							assetEntry.m_assetTypeGuid == MeshSceneAssetType::AssetFormat.assetTypeGuid ||
								assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid ||
								assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid,
							assetEntry,
							assetEntry.m_path.GetView().GetDepth()
						});
					}

					if (assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid || assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid)
					{
						assetDatabase.IterateAssets(
							[&assets](const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
							{
								if (assetEntry.m_assetTypeGuid != PluginAssetFormat.assetTypeGuid && assetEntry.m_assetTypeGuid != ProjectAssetFormat.assetTypeGuid)
								{
									if (!assets.Contains(AssetCreationInfo{assetGuid}))
									{
										assets.EmplaceBack(AssetCreationInfo{
											assetGuid,
											assetEntry.m_assetTypeGuid == MeshSceneAssetType::AssetFormat.assetTypeGuid ||
												assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid ||
												assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid,
											assetEntry,
											assetEntry.m_path.GetView().GetDepth()
										});
									}
								}
								return Memory::CallbackResult::Continue;
							}
						);
					}
					else if (assetEntry.m_assetTypeGuid == MeshSceneAssetType::AssetFormat.assetTypeGuid)
					{
						const IO::PathView meshSceneDirectoryPath = assetEntry.m_path.GetParentPath();

						assetDatabase.IterateAssets(
							[meshSceneDirectoryPath, &assets](const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
							{
								if (assetEntry.m_assetTypeGuid == MeshPartAssetType::AssetFormat.assetTypeGuid && assetEntry.m_path.GetParentPath() == meshSceneDirectoryPath)
								{
									if (!assets.Contains(AssetCreationInfo{assetGuid}))
									{
										assets.EmplaceBack(AssetCreationInfo{
											assetGuid,
											assetEntry.m_assetTypeGuid == MeshSceneAssetType::AssetFormat.assetTypeGuid ||
												assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid ||
												assetEntry.m_assetTypeGuid == PluginAssetFormat.assetTypeGuid,
											assetEntry,
											assetEntry.m_path.GetView().GetDepth()
										});
									}
								}
								return Memory::CallbackResult::Continue;
							}
						);
					}
					return Memory::CallbackResult::Continue;
				}
				else
				{
					LogMessage("Failed to validate asset with guid {} at path {}", assetGuid, assetEntry.m_path);
					assets.Clear();
					return Memory::CallbackResult::Break;
				}
			}
		);

		Algorithms::Sort(&*assets.begin(), &*assets.end(), Comparator());
		return assets;
	}

	Serialization::Data Plugin::PopulateAssetBatchData(
		const ArrayView<const AssetCreationInfo> assets, Asset::Database& assetDatabase, const IO::PathView assetsFolderPath
	)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		const bool serializedAssets = writer.SerializeObjectWithCallback(
			"assets",
			[this, &assets, &assetDatabase, assetsFolderPath](Serialization::Writer assetsWriter) mutable
			{
				assetsWriter.GetValue() = Serialization::Value(rapidjson::kArrayType);

				for (auto it = assets.begin(), endIt = assets.end(); it != endIt; ++it)
				{
					Serialization::Writer assetWriter = assetsWriter.EmplaceArrayElement();
					assetWriter.GetValue() = Serialization::Value(rapidjson::kObjectType);

					const ngine::Guid assetGuid = it->guid;

#if DEVELOPMENT_BUILD
					LogMessage("Preparing to register asset {}", assetGuid);
#endif

					const Asset::DatabaseEntry& assetEntry = *it->pAssetEntry;
					Assert(assetEntry.IsValid());
					if (assetEntry.IsValid())
					{
						PopulateAsset(assetGuid, assetEntry, assetDatabase, assetsFolderPath, assetWriter);
					}
					else
					{
						LogError("Failed to register invalid asset {}", assetGuid);
						return false;
					}
				}
				return true;
			}
		);
		if (serializedAssets)
		{
			return serializedData;
		}
		else
		{
			return {};
		}
	}

	void Plugin::CreateAsset(
		IO::URI&& endpointURI,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		AssetInfo&& assetInfo,
		CreateAssetCallback&& callback
	)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("name", assetInfo.assetName);
		writer.Serialize("description", assetInfo.assetDescription);
		writer.Serialize("context_uuid", assetInfo.contextGuid);
		writer.Serialize("uuid", assetInfo.guid);
		writer.Serialize("storage", assetInfo.keyValueStorage);

		if (assetInfo.dependencies.HasElements())
		{
			writer.SerializeArrayWithCallback(
				"data_entities",
				[dependencies = (ConstStringView
			   )assetInfo.dependencies.GetView()](Serialization::Writer dataEntitySerializer, [[maybe_unused]] const uint32 index)
				{
					dataEntitySerializer.GetValue() = Serialization::Value(rapidjson::kObjectType);
					dataEntitySerializer.Serialize("name", ConstStringView("Dependencies"));
					dataEntitySerializer.Serialize("data", dependencies);
					return true;
				},
				1
			);
		}

		QueueRequestInternal(
			HTTP::RequestType::Post,
			Forward<IO::URI>(endpointURI),
			headers,
			writer.SaveToBuffer<String>(),
			[callback = Forward<CreateAssetCallback>(callback
		   )](const bool success, const HTTP::ResponseCode, const Serialization::Reader responseReader)
			{
				if (success)
				{
					if (const Optional<Serialization::Reader> entryReader = responseReader.FindSerializer("asset"))
					{
						callback(*entryReader->Read<uint32>("id"), *entryReader->Read<ngine::Guid>("uuid"));
					}
					else
					{
						callback(Invalid, {});
					}
				}
				else
				{
					callback(Invalid, {});
				}
			}
		);
	}

	void Plugin::CreateAsset(
		IO::URI&& endpointURI,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		const Asset::Guid assetGuid,
		const Asset::DatabaseEntry& assetEntry,
		const IO::PathView assetsFolderPath,
		UploadAssetFilesCallback&& uploadCallback,
		AssetProgressCallback&& progressCallback
	)
	{
		AssetEntry::KeyValueStorage keyValueStorage;
		keyValueStorage.Reserve(2);
		if (assetEntry.m_componentTypeGuid.IsValid())
		{
			keyValueStorage.Emplace(ConstStringView("ComponentTypeGuid"), String(assetEntry.m_componentTypeGuid.ToString()));
		}
		if (assetEntry.m_thumbnailGuid.IsValid())
		{
			keyValueStorage.Emplace(ConstStringView("ThumbnailAssetGuid"), String(assetEntry.m_thumbnailGuid.ToString()));
		}

		if (assetEntry.m_path.IsRelativeTo(assetsFolderPath))
		{
			keyValueStorage.Emplace(ConstStringView("Path"), String(assetEntry.m_path.GetRelativeToParent(assetsFolderPath).GetStringView()));
		}

		String dataEntities;
		if (assetEntry.m_dependencies.HasElements())
		{
			Serialization::Data dependencyData(rapidjson::Type::kArrayType);
			Serialization::Writer writer(dependencyData);
			writer.SerializeInPlace(assetEntry.m_dependencies.GetView());
			dataEntities = dependencyData.SaveToBuffer<String>(Serialization::SavingFlags{});
		}

		progressCallback(true, 1);
		CreateAsset(
			Forward<IO::URI>(endpointURI),
			headers,
			AssetInfo{
				String(assetEntry.GetName()),
				String(assetEntry.m_description),
				assetEntry.m_assetTypeGuid,
				assetGuid,
				Move(keyValueStorage),
				Move(dataEntities)
			},
			[assetEntry,
		   uploadCallback = Forward<UploadAssetFilesCallback>(uploadCallback),
		   progressCallback = Forward<AssetProgressCallback>(progressCallback
		   )](const Optional<uint32> assetIdentifier, [[maybe_unused]] const ngine::Guid createdAssetGuid)
			{
				Assert(assetIdentifier.IsValid());
				if (assetIdentifier.IsValid())
				{
					uploadCallback(*assetIdentifier, assetEntry, progressCallback);
				}
			}
		);
	}

	void Plugin::CreateAssets(
		IO::URI&& endpointURI,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		IO::Path&& assetDatabasePath,
		Asset::Database&& assetDatabase,
		UploadAssetFilesCallback&& uploadCallback,
		AssetProgressCallback&& progressCallback
	)
	{
		// Make sure the database contains itself
		if (!assetDatabase.HasAsset(assetDatabase.GetGuid()))
		{
			assetDatabase.RegisterAsset(
				assetDatabase.GetGuid(),
				Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, {}, Move(assetDatabasePath)},
				{}
			);
		}

		progressCallback(true, assetDatabase.GetAssetCount());

		LogMessage("Populating asset batch");
		FixedCapacityVector<AssetCreationInfo> assets = PopulateAssetBatch(assetDatabase);
		Assert(assets.HasElements());
		if (assets.HasElements())
		{
			CreateAssetsInternal(
				Forward<IO::URI>(endpointURI),
				headers,
				Forward<Asset::Database>(assetDatabase),
				IO::Path(assetDatabasePath.GetWithoutExtensions()),
				Forward<UploadAssetFilesCallback>(uploadCallback),
				Forward<AssetProgressCallback>(progressCallback),
				Move(assets)
			);
		}
		else
		{
			progressCallback(false, assetDatabase.GetAssetCount());
		}
	}

	void Plugin::CreateAssetsInternal(
		IO::URI&& endpointURI,
		const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
		Asset::Database&& assetDatabase,
		IO::Path&& assetsFolderPath,
		UploadAssetFilesCallback&& uploadCallback,
		AssetProgressCallback&& progressCallback,
		FixedCapacityVector<AssetCreationInfo>&& assets
	)
	{
		constexpr int maximumAssetCountPerWriter = 20;
		const ArrayView<const AssetCreationInfo> createdAssets = assets.GetSubView(0, maximumAssetCountPerWriter);
		LogMessage("Populating {} assets ({} remaining)", createdAssets.GetSize(), assets.GetSize() - createdAssets.GetSize());

		Serialization::Data assetBatchData = PopulateAssetBatchData(createdAssets, assetDatabase, assetsFolderPath);
		if (assetBatchData.IsValid())
		{
			assets.Remove(createdAssets);

			LogMessage("Queuing batch asset craetion request");
			QueueStreamedRequestInternal(
				HTTP::RequestType::Post,
				RequestFlags::HighPriority,
				IO::URI(endpointURI),
				headers,
				assetBatchData.SaveToBuffer<String>(Serialization::SavingFlags{}),
				[this,
			   endpointURI,
			   assets = Move(assets),
			   assetsFolderPath = Move(assetsFolderPath),
			   headers = FixedSizeVector<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>>(headers),
			   assetDatabase = Forward<Asset::Database>(assetDatabase),
			   uploadCallback = Forward<UploadAssetFilesCallback>(uploadCallback),
			   progressCallback = Forward<AssetProgressCallback>(progressCallback
			   )](const Serialization::Reader assetResultReader, [[maybe_unused]] const ConstStringView objectJson) mutable
				{
					const Optional<Serialization::Reader> streamedObjectCountReader = assetResultReader.FindSerializer("streamedObjectCount");
					if (streamedObjectCountReader.IsInvalid())
					{
						const ngine::Guid assetGuid = assetResultReader.ReadWithDefaultValue<ngine::Guid>("asset_uuid", ngine::Guid{});
						Assert(assetGuid.IsValid());
						const Optional<Serialization::Reader> failureMessageReader = assetResultReader.FindSerializer("failure_message");
						if (failureMessageReader.IsInvalid())
						{
							// Upload asset
							const Optional<Asset::DatabaseEntry*> pAssetEntry = assetDatabase.GetAssetEntry(assetGuid);
							Assert(pAssetEntry.IsValid());
							if (LIKELY(pAssetEntry.IsValid()))
							{
								LogMessage("Successfully registered asset {}, uploading files", assetGuid);
								const uint32 assetIdentifier = *assetResultReader.Read<uint32>("asset_id");
								uploadCallback(assetIdentifier, *pAssetEntry, progressCallback);
								progressCallback(true, -1);
							}
							else
							{
								LogError("Failed finding asset {} after creation", assetGuid);
								progressCallback(false, -1);
							}
						}
						else
						{
							LogError("Failed creating asset {}: {}", assetGuid, *failureMessageReader->ReadInPlace<ConstStringView>());
							progressCallback(false, -1);
						}
					}
					else
					{
						LogMessage("Finished registering batch, created {} assets", *streamedObjectCountReader->ReadInPlace<uint32>());
						// Start creating remaining assets if any
						if (assets.HasElements())
						{
							CreateAssetsInternal(
								Move(endpointURI),
								headers,
								Move(assetDatabase),
								Move(assetsFolderPath),
								Move(uploadCallback),
								Move(progressCallback),
								Move(assets)
							);
						}
					}
				},
				[](const bool success)
				{
					if (success)
					{
						LogMessage("Finished backend asset creation (uploading in progress)");
					}
					else
					{
						LogError("Failed backend asset creation!");
					}
				}
			);
		}
		else
		{
			LogError("Failed to populate asset batch!");
			progressCallback(false, createdAssets.GetSize());
		}
	}

	void Plugin::UploadAssetFiles(
		UploadAssetFileCallback&& callback, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback
	)
	{
		Vector<IO::Path> uploadedAssets;
		{
			// This asset could have multiple (binary) files, e.g. '.tex.bc' and '.tex.astc'
			const IO::PathView binaryFilePath = assetEntry.GetBinaryFilePath();
			if (binaryFilePath.HasElements())
			{
				IO::FileIterator::TraverseDirectory(
					IO::Path(binaryFilePath.GetParentPath()),
					[name = String(assetEntry.GetName()),
				   binaryFilePath,
				   &callback = callback,
				   &progressCallback = progressCallback,
				   &uploadedAssets](IO::Path&& filePath) -> IO::FileIterator::TraversalResult
					{
						if (filePath.GetView().StartsWith(binaryFilePath) && filePath.StartsWithExtensions(binaryFilePath.GetAllExtensions()))
						{
							IO::File file(filePath, IO::AccessModeFlags::ReadBinary);
							Assert(file.IsValid());
							if (LIKELY(file.IsValid()))
							{
								HTTP::FormDataPart::StoredData data(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)file.GetSize());
								const bool wasRead = file.ReadIntoView(data.GetView());
								Assert(wasRead);
								if (LIKELY(wasRead))
								{
									callback(
										HTTP::FormDataPart{
											String(name),
											String(filePath.GetFileName().GetStringView()),
											"application/octet-stream",
											HTTP::FormDataPart::Data{Move(data)}
										},
										AssetProgressCallback(progressCallback)
									);
									uploadedAssets.EmplaceBack(Move(filePath));
								}
								else
								{
									LogError("Couldn't read asset entry with name {} and path {}", String(name), String(filePath.GetView().GetStringView()));
								}
							}
							else
							{
								LogError("Invalid asset entry with name {} and path {}", String(name), String(filePath.GetView().GetStringView()));
							}
						}
						return IO::FileIterator::TraversalResult::Continue;
					}
				);
			}
		}

		if (!uploadedAssets.Contains(assetEntry.m_path))
		{
			IO::File file(assetEntry.m_path, IO::AccessModeFlags::ReadBinary);
			Assert(file.IsValid());
			if (LIKELY(file.IsValid()))
			{
				HTTP::FormDataPart::StoredData data(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)file.GetSize());
				const bool wasRead = file.ReadIntoView(data.GetView());
				Assert(wasRead);
				if (LIKELY(wasRead))
				{
					callback(
						HTTP::FormDataPart{
							String(assetEntry.GetName()),
							String(assetEntry.m_path.GetFileName().GetStringView()),
							"application/octet-stream",
							HTTP::FormDataPart::Data{Move(data)}
						},
						AssetProgressCallback(progressCallback)
					);
				}
				else
				{
					LogError(
						"Couldn't read asset entry with name {} and path {}",
						String(assetEntry.GetName()),
						String(assetEntry.m_path.GetZeroTerminated().GetView())
					);
				}
			}
			else
			{
				LogError(
					"Invalid asset entry with name {} and path {}",
					String(assetEntry.GetName()),
					String(assetEntry.m_path.GetZeroTerminated().GetView())
				);
			}
		}
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Networking::Backend::Plugin>();
#else
extern "C" HTTP_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Networking::Backend::Plugin(application);
}
#endif
