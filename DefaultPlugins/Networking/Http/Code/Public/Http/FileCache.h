#pragma once

#include <Common/IO/File.h>
#include <Common/Math/Range.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicBool.h>

#include <Http/FileCacheState.h>

namespace ngine::Networking::HTTP
{
	struct FileCache
	{
		inline static constexpr uint16 Version = 2;
		inline static constexpr IO::PathView PartialFileExtension = MAKE_PATH(".partial");

		[[nodiscard]] static FileCache& GetInstance()
		{
			static FileCache fileCache;
			return fileCache;
		}
		using PartialFileRange = Math::Range<size>;

		struct Entry
		{
			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			PartialFileRange& EmplacePartialFile(const Math::Range<size> range);

			Vector<PartialFileRange> m_partialFileRanges;
		};

		struct EntryMap : public UnorderedMap<IO::Path, Entry, IO::Path::Hash>
		{
			using UnorderedMap::UnorderedMap;
			using UnorderedMap::ValueType;
		};

		struct ETagMap : public UnorderedMap<IO::URI, String, IO::URI::Hash>
		{
			using UnorderedMap::UnorderedMap;
			using UnorderedMap::ValueType;
		};

		enum class State : uint8
		{
			//! File not found in the cache at all
			Invalid,
			//! Requested range was not found in the cache
			RangeNotFound,
			//! The requested range was found in the cache and was a partial file
			Partial,
			//! The requested range was found in the cache and was the complete file
			Complete
		};

		struct Result
		{
			State state;
			IO::Path path;
		};

		FileCache();
		FileCache(const FileCache&) = delete;
		FileCache& operator=(const FileCache&) = delete;
		FileCache(FileCache&&) = delete;
		FileCache& operator=(FileCache&&) = delete;

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		void SaveIfNecessary();

		[[nodiscard]] bool Contains(const Guid assetGuid, const IO::PathView assetExtensions) const;
		void Remove(const Guid assetGuid, const IO::PathView assetExtensions);

		[[nodiscard]] Result Find(const Guid assetGuid, const IO::PathView assetExtensions, const Math::Range<size> range) const;

		State AddFileData(
			const Guid assetGuid,
			const IO::PathView assetExtensions,
			const Math::Range<size> range,
			ConstStringView fileData,
			const IO::ConstURIView remoteURI,
			const ConstStringView entityTag
		);

		[[nodiscard]] FileCacheState GetCachedFileState(const IO::ConstURIView uri) const;

		[[nodiscard]] bool ContainsETag(const IO::ConstURIView uri) const;
		FileCacheState ValidateRemoteFileTag(
			const Guid assetGuid, const IO::ConstURIView uri, const ConstStringView remoteETag, const IO::PathView assetExtensions
		);
		[[nodiscard]] static IO::Path GetAssetPath(const Guid assetGuid, const IO::PathView extensions);
		[[nodiscard]] static IO::Path GetAssetPath(const Guid assetGuid, const IO::PathView relativePath, const IO::PathView extensions);
		[[nodiscard]] static IO::Path GetAssetsDirectory();
	protected:
		void OnChanged();
		PartialFileRange& EmplacePartialFile(const IO::PathView cachedFilePath, const Math::Range<size> range);
	protected:
		mutable Threading::SharedMutex m_mutex;
		EntryMap m_map;

		ETagMap m_etagLookupMap;
		UnorderedMap<IO::URI, FileCacheState, IO::URI::Hash> m_cachedFileStates;

		Threading::Atomic<bool> m_isDirty{false};
		bool m_registeredSaveCallback{false};
	};
}
