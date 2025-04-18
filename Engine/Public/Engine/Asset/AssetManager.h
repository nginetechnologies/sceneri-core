#pragma once

#include "AssetDatabase.h"
#include "AssetLibrary.h"
#include "ImportingFlags.h"

#include <Engine/Asset/Identifier.h>
#include <Engine/Asset/Mask.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/Tag/TagContainer.h>

#include <Common/Asset/Guid.h>
#include <Common/Asset/AssetDatabase.h>
#include <Common/Storage/IdentifierArray.h>

#include <Common/Function/Event.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Range.h>
#include <Common/Math/NumericLimits.h>
#include <Common/Time/Timestamp.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/IO/AsyncLoadCallback.h>
#include <Common/Threading/Jobs/JobPriority.h>
#include <Common/Threading/Jobs/TimerHandle.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicPtr.h>

#include <Common/System/SystemType.h>

namespace ngine
{
	struct Engine;

	namespace Threading
	{
		struct Job;
		struct JobBatch;
	}

	extern template struct UnorderedMap<Guid, Time::Timestamp, Guid::Hash>;
}

namespace ngine::Asset
{
	struct LibraryReference;

#define TRACK_ASSET_FILE_CHANGES PLATFORM_DESKTOP
#if TRACK_ASSET_FILE_CHANGES
	struct FileChangeDetection;
#endif

	struct Manager : public EngineAssetDatabase
	{
		inline static constexpr System::Type SystemType = System::Type::AssetManager;
		inline static constexpr Guid DataSourceGuid = "{D37F02E5-8502-4EE7-8D83-E70FCF16433D}"_guid;

		Manager();
		virtual ~Manager();

		[[nodiscard]] bool Load(
			const IO::PathView databaseFilePath,
			const Serialization::Reader databaseReader,
			const IO::PathView databaseRootDirectory,
			const Identifier rootFolderAssetIdentifier,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {},
			const ArrayView<const Tag::Identifier, uint8> importedTagIdentifiers = {}
		);

		[[nodiscard]] Library& GetAssetLibrary()
		{
			return m_assetLibrary;
		}
		[[nodiscard]] const Library& GetAssetLibrary() const
		{
			return m_assetLibrary;
		}

		[[nodiscard]] Identifier GetAssetIdentifier(const Guid assetGuid) const
		{
			Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
			auto it = m_assetIdentifierLookupMap.Find(assetGuid);
			if (LIKELY(it != m_assetIdentifierLookupMap.end()))
			{
				return it->second;
			}
			return {};
		}

		using AsyncLoaderCallback = Function<
			Optional<Threading::Job*>(
				const Guid assetGuid,
				const IO::PathView path,
				Threading::JobPriority priority,
				IO::AsyncLoadCallback&& callback,
				const ByteView target,
				const Math::Range<size> dataRange
			),
			24>;
		void RegisterAsyncLoadCallback(const Guid assetGuid, AsyncLoaderCallback&& callback)
		{
			Threading::UniqueLock lock(m_assetLoaderMapMutex);
			Assert(!m_assetLoaderMap.Contains(assetGuid));
			m_assetLoaderMap.Emplace(Guid(assetGuid), Forward<AsyncLoaderCallback>(callback));
		}
		//! Registers an async load callback that when invokes asynchronously copies the data from sourceAssetGuid
		void RegisterCopyFromSourceAsyncLoadCallback(const Guid sourceAssetGuid, const Guid assetGuid);
		[[nodiscard]] bool HasAsyncLoadCallback(const Guid assetGuid) const
		{
			Threading::UniqueLock lock(m_assetLoaderMapMutex);
			return m_assetLoaderMap.Contains(assetGuid);
		}
		bool RemoveAsyncLoadCallback(const Guid assetGuid)
		{
			Threading::UniqueLock lock(m_assetLoaderMapMutex);
			auto it = m_assetLoaderMap.Find(assetGuid);
			if (it != m_assetLoaderMap.end())
			{
				m_assetLoaderMap.Remove(it);
				return true;
			}
			return false;
		}

		Identifier Import(
			const LibraryReference libraryAssetReference,
			const EnumFlags<ImportingFlags> flags = ImportingFlags::SaveToDisk,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
		Identifier Import(
			const LibraryReference libraryAssetReference,
			const DatabaseEntry& libraryAssetEntry,
			const EnumFlags<ImportingFlags> flags = ImportingFlags::SaveToDisk,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
		void RemoveAsset(const Guid assetGuid);

		Event<void(void*, const Mask& assetsMask, const EnumFlags<ImportingFlags> flags), 24> OnAssetsImported;

		[[nodiscard]] Optional<Threading::Job*>
		RequestAsyncLoadAssetMetadata(const Guid assetGuid, Threading::JobPriority priority, IO::AsyncLoadCallback&& callback) const;
		[[nodiscard]] Optional<Threading::Job*> RequestAsyncLoadAssetBinary(
			const Guid assetGuid,
			Threading::JobPriority priority,
			IO::AsyncLoadCallback&& callback,
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		) const;
		[[nodiscard]] Optional<Threading::Job*> RequestAsyncLoadAssetBinary(
			const Guid assetGuid, Threading::JobPriority priority, IO::AsyncLoadCallback&& callback, const size readOffset, const ByteView target
		) const;
		[[nodiscard]] Optional<Threading::Job*> RequestAsyncLoadAssetPath(
			const Guid assetGuid,
			const IO::PathView path,
			Threading::JobPriority priority,
			IO::AsyncLoadCallback&& callback,
			const ByteView target = {},
			const Math::Range<size> dataRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		) const;

		Event<void(void*, const Serialization::Reader), 24> OnLoadAssetDatabase;

#if TRACK_ASSET_FILE_CHANGES
		using AssetModifiedFunction = Function<void(const Guid assetGuid, const IO::PathView filePath), 24>;
		void RegisterAssetModifiedCallback(AssetModifiedFunction&& callback)
		{
			m_assetModifiedCallbacks.EmplaceBack(Forward<AssetModifiedFunction>(callback));
		}
#endif

		bool MoveAsset(const IO::PathView path, IO::Path&& newPath);
	protected:
#if TRACK_ASSET_FILE_CHANGES
		friend FileChangeDetection;
		void NotifyFileAddedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath);
		void NotifyFileRemovedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath);
		void NotifyFileModifiedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath);
		void NotifyFileRenamedOnDisk(
			const IO::PathView directoryPath, const IO::PathView relativePreviousFilePath, const IO::PathView relativeNewFilePath
		);
#endif

		Identifier Import(
			const LibraryReference libraryAssetReference,
			const DatabaseEntry& libraryAssetEntry,
			Mask& assetsMask,
			const EnumFlags<ImportingFlags> flags,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
		Identifier ImportInternal(
			const LibraryReference libraryAssetReference,
			const DatabaseEntry& libraryAssetEntry,
			Mask& assetsMask,
			const EnumFlags<ImportingFlags> flags,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
	protected:
		mutable Threading::SharedMutex m_assetLoaderMapMutex;
		using AssetLoaderMap = UnorderedMap<Guid, AsyncLoaderCallback, Guid::Hash>;
		AssetLoaderMap m_assetLoaderMap;

#if TRACK_ASSET_FILE_CHANGES
		UniqueRef<FileChangeDetection> m_pFileChangeDetection;
		UnorderedMap<Guid, Time::Timestamp, Guid::Hash> m_fileChangeTimestamps;
		Vector<AssetModifiedFunction> m_assetModifiedCallbacks;
#endif

		Library m_assetLibrary;

		Threading::TimerHandle m_scheduledTimerHandle;
	};

	struct EngineManager final : public Manager
	{
		inline static constexpr Guid EngineAssetsTagGuid = "63d30187-bf88-4ab3-aeae-4f9e4674c761"_guid;

		EngineManager();
		virtual ~EngineManager();
	protected:
		friend Engine;
		[[nodiscard]] Threading::JobBatch LoadDefaultResources();
	};
}
