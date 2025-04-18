#include <Http/FileCache.h>

#include <Engine/Engine.h>

#include <Common/Project System/EngineInfo.h>

#include <Common/EnumFlags.h>
#include <Common/Memory/ReferenceWrapper.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Networking::HTTP
{
	bool FileCache::Entry::Serialize(const Serialization::Reader serializer)
	{
		m_partialFileRanges.Reserve((uint32)serializer.GetArraySize());
		for (const Optional<ConstStringView> partialFileRange : serializer.GetArrayView<ConstStringView>())
		{
			const uint16 dividerIndex = (uint16)partialFileRange->FindLastOf('-');

			const Math::Range<size> existingFileRange = Math::Range<size>::Make(
				partialFileRange->GetSubstring(partialFileRange->GetIteratorIndex(partialFileRange->begin()), dividerIndex).ToIntegral<size>(),
				partialFileRange->GetSubstring(dividerIndex + 1, partialFileRange->GetSize() - dividerIndex).ToIntegral<size>()
			);
			m_partialFileRanges.EmplaceBack(PartialFileRange{existingFileRange});
		}
		return true;
	}

	bool FileCache::Entry::Serialize(Serialization::Writer serializer) const
	{
		serializer.GetValue() = Serialization::Value(rapidjson::Type::kArrayType);
		serializer.ReserveArray(m_partialFileRanges.GetSize());
		FlatString<50> rangeString;
		for (const PartialFileRange partialFileRange : m_partialFileRanges)
		{
			rangeString.Format("{}-{}", partialFileRange.GetMinimum(), partialFileRange.GetSize());
			Serialization::Writer objectWriter = serializer.EmplaceArrayElement();
			objectWriter.SerializeInPlace(rangeString.GetView());
		}
		return true;
	}

	bool Serialize(FileCache::EntryMap& map, const Serialization::Reader serializer, const IO::PathView rootDirectory)
	{
		return SerializeWithCallback(
			map,
			serializer,
			[rootDirectory](auto& map, const ConstStringView pathString, FileCache::Entry&& value)
			{
				map.Emplace(IO::Path::Combine(rootDirectory, IO::Path(IO::Path::StringType(pathString))), Forward<FileCache::Entry>(value));
			}
		);
	}

	bool Serialize(const FileCache::EntryMap& map, Serialization::Writer serializer, const IO::PathView rootDirectory)
	{
		return SerializeWithCallback<String>(
			map,
			serializer,
			[rootDirectory](const IO::Path& key) -> String
			{
				return String(key.GetRelativeToParent(rootDirectory).GetStringView());
			}
		);
	}

	bool Serialize(FileCache::ETagMap& map, const Serialization::Reader serializer)
	{
		return SerializeWithCallback(
			map,
			serializer,
			[](auto& map, const ConstStringView uri, String&& etag)
			{
				map.Emplace(IO::URI(uri), Forward<String>(etag));
			}
		);
	}

	bool Serialize(const FileCache::ETagMap& map, Serialization::Writer serializer)
	{
		return SerializeWithCallback<String>(
			map,
			serializer,
			[](const IO::URIView uri) -> String
			{
				return String(uri.GetStringView());
			}
		);
	}

	[[nodiscard]] IO::Path GetPartialAssetDirectory(const IO::PathView cachedAssetPath)
	{
		return IO::Path::Merge(cachedAssetPath, FileCache::PartialFileExtension);
	}

	[[nodiscard]] IO::Path GetPartialAssetPath(const IO::PathView cachedAssetPath, const Math::Range<size> range)
	{
		Assert(cachedAssetPath.HasElements());
		return IO::Path::Combine(
			GetPartialAssetDirectory(cachedAssetPath),
			IO::Path::StringType().Format("{}-{}", range.GetMinimum(), range.GetSize()).GetView()
		);
	}

	FileCache::FileCache()
	{
#if !PLATFORM_WEB
		const IO::Path filePath = IO::Path::Merge(GetAssetsDirectory(), MAKE_PATH(".json"));
		[[maybe_unused]] const bool wasRead = Serialization::DeserializeFromDisk(filePath, *this);
#endif
	}

	/* static */ IO::Path FileCache::GetAssetsDirectory()
	{
		return IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("AssetCache"), MAKE_PATH("Assets"));
	}

	/* static */ IO::Path FileCache::GetAssetPath(const Guid assetGuid, const IO::PathView extensions)
	{
		return IO::Path::Combine(GetAssetsDirectory(), IO::Path::Merge(assetGuid.ToString().GetView(), extensions));
	}

	/* static */ IO::Path FileCache::GetAssetPath(const Guid assetGuid, const IO::PathView relativePath, const IO::PathView extensions)
	{
		return IO::Path::Combine(GetAssetsDirectory(), relativePath, IO::Path::Merge(assetGuid.ToString().GetView(), extensions));
	}

	bool FileCache::Serialize(const Serialization::Reader serializer)
	{
		const IO::Path cachedNetworkAssetsDirectory = GetAssetsDirectory();
		IO::PathView cachedNetworkAssetsDirectoryView = cachedNetworkAssetsDirectory;

		Threading::UniqueLock lock(m_mutex);
		if (serializer.ReadWithDefaultValue<uint16>("version", 0) == Version)
		{
			const bool readFiles = serializer.Serialize("files", m_map, cachedNetworkAssetsDirectoryView);
			const bool readETags = serializer.Serialize("etags", m_etagLookupMap);
			return readFiles || readETags;
		}
		else
		{
			// Out of date, evict cache
			return true;
		}
	}

	bool FileCache::Serialize(Serialization::Writer serializer) const
	{
		const IO::Path cachedNetworkAssetsDirectory = GetAssetsDirectory();
		IO::PathView cachedNetworkAssetsDirectoryView = cachedNetworkAssetsDirectory;

		serializer.Serialize("version", Version);
		serializer.Serialize("files", m_map, cachedNetworkAssetsDirectoryView);
		serializer.Serialize("etags", m_etagLookupMap);
		return true;
	}

	void FileCache::SaveIfNecessary()
	{
#if !PLATFORM_WEB
		bool expected = true;
		if (m_isDirty.CompareExchangeStrong(expected, false))
		{
			const IO::Path filePath = IO::Path::Merge(GetAssetsDirectory(), MAKE_PATH(".json"));
			if (!IO::Path(filePath.GetParentPath()).Exists())
			{
				IO::Path(filePath.GetParentPath()).CreateDirectories();
			}

			Threading::UniqueLock lock(m_mutex);
			[[maybe_unused]] const bool wasWritten = Serialization::SerializeToDisk(filePath, *this, Serialization::SavingFlags{});
			Assert(wasWritten);
		}
#endif
	}

	void FileCache::OnChanged()
	{
#if !PLATFORM_WEB
		bool expected = false;
		if (m_isDirty.CompareExchangeStrong(expected, true))
		{
			SaveIfNecessary(); /*
			 if(!m_registeredSaveCallback)
			 {
			   m_registeredSaveCallback = true;
			   System::Get<Engine>().GetQuitJobBatch().QueueAfterStartStage(Threading::CreateCallback(
			     [this](Threading::JobRunnerThread&)
			     {
			       SaveIfNecessary();
			     },
			     Threading::JobPriority::SaveOnClose
			   ));
			 }*/
		}
#endif
	}

	bool FileCache::Contains(const Guid assetGuid, const IO::PathView localFilePath) const
	{
#if !PLATFORM_WEB
		Threading::SharedLock lock(m_mutex);
		IO::Path cachedFilePath = GetAssetPath(assetGuid, localFilePath);
		return m_map.Contains(cachedFilePath);
#else
		UNUSED(assetGuid);
		UNUSED(localFilePath);
		return false;
#endif
	}

	FileCache::Result FileCache::Find(const Guid assetGuid, const IO::PathView localFilePath, const Math::Range<size> range) const
	{
#if !PLATFORM_WEB
		Threading::SharedLock lock(m_mutex);
		IO::Path cachedFilePath = GetAssetPath(assetGuid, localFilePath);
		if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
		{
			const Entry& __restrict entry = it->second;
			if (entry.m_partialFileRanges.HasElements())
			{
				const bool requestedCompleteFile = range.GetMaximum() == (Math::NumericLimits<size>::Max - 1);
				if (!requestedCompleteFile)
				{
					for (const PartialFileRange& partialFileRange : entry.m_partialFileRanges)
					{
						if (partialFileRange.GetMinimum() >= range.GetMinimum() && range.GetMaximum() <= partialFileRange.GetMaximum())
						{
							return Result{State::Partial, GetPartialAssetPath(cachedFilePath, range)};
						}
					}
				}
				return Result{State::RangeNotFound};
			}
			else
			{
				// An empty vector of partial files indicates that the complete file exists
				return Result{State::Complete, Move(cachedFilePath)};
			}
		}
#else
		UNUSED(assetGuid);
		UNUSED(localFilePath);
		UNUSED(range);
#endif
		return Result{State::Invalid};
	}

	FileCache::PartialFileRange& FileCache::Entry::EmplacePartialFile(const Math::Range<size> range)
	{
		for (PartialFileRange& __restrict partialFileRange : m_partialFileRanges)
		{
			if (partialFileRange == range)
			{
				return partialFileRange;
			}
		}

		PartialFileRange* insertionIt = std::lower_bound(
			m_partialFileRanges.begin().Get(),
			m_partialFileRanges.end().Get(),
			range,
			[](const PartialFileRange& existingPartialFile, const Math::Range<size> newRange)
			{
				return newRange.GetMinimum() > existingPartialFile.GetMinimum();
			}
		);
		return m_partialFileRanges.Emplace(insertionIt, Memory::Uninitialized, PartialFileRange{range});
	}

	FileCache::PartialFileRange& FileCache::EmplacePartialFile(const IO::PathView cachedFilePath, const Math::Range<size> range)
	{
		Threading::UniqueLock lock(m_mutex);
		if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
		{
			Entry& __restrict entry = it->second;
			entry.EmplacePartialFile(range);
		}

		Entry& entry = m_map.Emplace(cachedFilePath, Entry{})->second;
		FileCache::PartialFileRange& partialFileRange = entry.m_partialFileRanges.EmplaceBack(PartialFileRange{range});
		return partialFileRange;
	}

	// TODO: Move the file writing to a low priority job

	FileCache::State FileCache::AddFileData(
		const Guid assetGuid,
		const IO::PathView localFilePath,
		Math::Range<size> range,
		ConstStringView fileData,
		const IO::ConstURIView remoteURI,
		const ConstStringView newEntityTag
	)
	{
		Threading::UniqueLock lock(m_mutex);
		const auto entityTagIt = m_etagLookupMap.Find(remoteURI.GetStringView());
		if (entityTagIt == m_etagLookupMap.end() || entityTagIt->second != newEntityTag)
		{
			if (entityTagIt != m_etagLookupMap.end())
			{
				entityTagIt->second = newEntityTag;
			}
			else
			{
				m_etagLookupMap.Emplace(IO::URI(remoteURI.GetStringView()), String(newEntityTag));
			}

			auto cachedFileStateIt = m_cachedFileStates.Find(remoteURI.GetStringView());
			if (cachedFileStateIt != m_cachedFileStates.end())
			{
				cachedFileStateIt->second = FileCacheState::Valid;
			}
			else
			{
				m_cachedFileStates.Emplace(remoteURI.GetStringView(), FileCacheState::Valid);
			}
		}

#if !PLATFORM_WEB
		const bool isCompleteFile = range.GetMinimum() == 0 && range.GetMaximum() == (Math::NumericLimits<size>::Max - 1);
		if (isCompleteFile)
		{
			range = Math::Range<size>::Make(0, fileData.GetDataSize());
		}

		Assert(fileData.GetDataSize() == range.GetSize());

		IO::Path cachedFilePath = GetAssetPath(assetGuid, localFilePath.GetAllExtensions());
		if (range.GetMinimum() == 0)
		{
			if (isCompleteFile)
			{
				if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
				{
					Entry& __restrict entry = it->second;

					for (const PartialFileRange& __restrict partialFileRange : entry.m_partialFileRanges)
					{
						const IO::Path partialFilePath = GetPartialAssetPath(cachedFilePath, partialFileRange);
						[[maybe_unused]] const bool wasRemoved = partialFilePath.RemoveFile();
						Assert(wasRemoved);
					}
					// Clear to indicate that we are a complete file
					entry.m_partialFileRanges.Clear();
				}
				else
				{
					m_map.Emplace(cachedFilePath, Entry{});
				}

				lock.Unlock();
				{
					IO::File file(IO::Path(cachedFilePath), IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
					if (!file.IsValid())
					{
						IO::Path(cachedFilePath.GetParentPath()).CreateDirectories();
						file = IO::File(IO::Path(cachedFilePath), IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
					}

					Assert(file.IsValid());
					if (LIKELY(file.IsValid()))
					{
						file.Write(fileData);
					}
				}

				IO::Path(localFilePath.GetParentPath()).CreateDirectories();

				if (cachedFilePath != localFilePath && (!IO::Path{localFilePath}.Exists() || IO::Path{localFilePath}.IsSymbolicLink()))
				{
					// Create symlink in local path
					[[maybe_unused]] bool wasLinkCreated = cachedFilePath.CreateSymbolicLinkToThis(IO::Path(localFilePath));
					if (!wasLinkCreated && IO::Path(localFilePath).GetSymbolicLinkTarget() != cachedFilePath)
					{
						IO::Path(localFilePath).RemoveFile();
						wasLinkCreated = cachedFilePath.CreateSymbolicLinkToThis(IO::Path(localFilePath));
						Assert(wasLinkCreated && IO::Path(localFilePath).GetSymbolicLinkTarget() == cachedFilePath);
					}
				}

				OnChanged();
				return State::Complete;
			}
			else
			{
				const IO::Path filePartialFilePath = GetPartialAssetPath(cachedFilePath, range);
				if (!IO::Path(filePartialFilePath.GetParentPath()).Exists())
				{
					IO::Path(filePartialFilePath.GetParentPath()).CreateDirectories();
				}

				if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
				{
					Entry& __restrict entry = it->second;
					PartialFileRange& emplacedFileRange = entry.EmplacePartialFile(range);

					// Attempt to merge partial files if they make up a contiguous block
					PartialFileRange& nextFileRange = *(&emplacedFileRange + 1);
					if (entry.m_partialFileRanges.IsWithinBounds(&nextFileRange) && emplacedFileRange.GetMaximum() == nextFileRange.GetMinimum())
					{
						entry.m_partialFileRanges.Remove(&nextFileRange);
						lock.Unlock();

						IO::File file(filePartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
						Assert(file.IsValid());
						if (LIKELY(file.IsValid()))
						{
							file.Write(fileData);
						}

						const IO::Path nextFilePartialFilePath = GetPartialAssetPath(cachedFilePath, nextFileRange);

						IO::File nextFile(nextFilePartialFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
						Assert(nextFile.IsValid());
						if (LIKELY(file.IsValid() & nextFile.IsValid()))
						{
							file.Write((IO::FileView)nextFile);

							nextFile.Close();
							nextFilePartialFilePath.RemoveFile();
						}
					}
					else
					{
						lock.Unlock();

						IO::File file(filePartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
						Assert(file.IsValid());
						if (LIKELY(file.IsValid()))
						{
							file.Write(fileData);
						}
					}
				}
				else
				{
					Entry entry;
					entry.m_partialFileRanges.EmplaceBack(PartialFileRange{range});
					m_map.Emplace(cachedFilePath, Move(entry));

					lock.Unlock();

					IO::File file(filePartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
					if (!file.IsValid())
					{
						IO::Path(filePartialFilePath.GetParentPath()).CreateDirectories();
						file = IO::File(filePartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
					}

					Assert(file.IsValid());
					if (LIKELY(file.IsValid()))
					{
						file.Write(fileData);
					}
				}
			}
		}
		else
		{
			IO::Path partialFilePath = GetPartialAssetPath(cachedFilePath, range);
			{
				IO::Path partialFileDirectory(partialFilePath.GetParentPath());
				if (!partialFileDirectory.Exists())
				{
					partialFileDirectory.CreateDirectory();
				}
			}

			if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
			{
				Entry& __restrict entry = it->second;
				PartialFileRange& emplacedFileRange = entry.EmplacePartialFile(range);
				PartialFileRange& previousPartialFileRange = *(&emplacedFileRange - 1);
				// Merge the file into the file before this if it exists
				if (entry.m_partialFileRanges.IsWithinBounds(&previousPartialFileRange) && previousPartialFileRange.GetMaximum() == emplacedFileRange.GetMinimum())
				{
					IO::Path previousPartialFilePath = GetPartialAssetPath(cachedFilePath, previousPartialFileRange);

					{
						previousPartialFileRange =
							Math::Range<size>::MakeStartToEnd(previousPartialFileRange.GetMinimum(), emplacedFileRange.GetMaximum());
						entry.m_partialFileRanges.Remove(&emplacedFileRange);

						PartialFileRange& nextFileRange = emplacedFileRange;
						// Merge the file after this if it exists
						if (entry.m_partialFileRanges.IsWithinBounds(&nextFileRange) && previousPartialFileRange.GetMaximum() == nextFileRange.GetMinimum())
						{
							IO::Path nextPartialFilePath = GetPartialAssetPath(cachedFilePath, nextFileRange);

							previousPartialFileRange =
								Math::Range<size>::MakeStartToEnd(previousPartialFileRange.GetMinimum(), nextFileRange.GetMaximum());
							entry.m_partialFileRanges.Remove(&nextFileRange);

							lock.Unlock();

							IO::File previousFile(previousPartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
							Assert(previousFile.IsValid());
							if (LIKELY(previousFile.IsValid()))
							{
								previousFile.Write(fileData);
							}

							{
								IO::File existingFile(nextPartialFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
								Assert(existingFile.IsValid());
								if (LIKELY(previousFile.IsValid() & existingFile.IsValid()))
								{
									previousFile.Write((IO::FileView)existingFile);
									nextPartialFilePath.RemoveFile();
								}
							}
						}
						else
						{
							lock.Unlock();

							IO::File previousFile(previousPartialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
							Assert(previousFile.IsValid());
							if (LIKELY(previousFile.IsValid()))
							{
								previousFile.Write(fileData);
							}
						}
					}

					if (previousPartialFileRange.GetMinimum() != 0)
					{
						IO::Path newPartialFilePath = GetPartialAssetPath(cachedFilePath, previousPartialFileRange);
						Assert(!newPartialFilePath.Exists());
						[[maybe_unused]] const bool wasMoved = previousPartialFilePath.MoveFileTo(newPartialFilePath);
						Assert(wasMoved);
					}
				}
				else
				{
					PartialFileRange& nextFileRange = *(&emplacedFileRange + 1);
					// Merge the file after this if it exists
					if (entry.m_partialFileRanges.IsWithinBounds(&nextFileRange) && emplacedFileRange.GetMaximum() == nextFileRange.GetMinimum())
					{
						IO::Path nextPartialFilePath = GetPartialAssetPath(cachedFilePath, nextFileRange);

						emplacedFileRange = Math::Range<size>::MakeStartToEnd(emplacedFileRange.GetMinimum(), nextFileRange.GetMaximum());
						entry.m_partialFileRanges.Remove(&nextFileRange);

						lock.Unlock();

						IO::File file(partialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
						Assert(file.IsValid());
						if (LIKELY(file.IsValid()))
						{
							file.Write(fileData);
						}

						{
							IO::File existingFile(nextPartialFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
							Assert(existingFile.IsValid());
							if (LIKELY(file.IsValid() & existingFile.IsValid()))
							{
								file.Write((IO::FileView)existingFile);

								file.Close();
								nextPartialFilePath.RemoveFile();
							}
						}
					}
					else
					{
						lock.Unlock();

						IO::File file(partialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
						Assert(file.IsValid());
						if (LIKELY(file.IsValid()))
						{
							file.Write(fileData);
						}
					}
				}
			}
			else
			{
				Entry entry;
				entry.m_partialFileRanges.EmplaceBack(PartialFileRange{range});
				m_map.Emplace(cachedFilePath, Move(entry));

				lock.Unlock();

				IO::File file(partialFilePath, IO::AccessModeFlags::Write | IO::AccessModeFlags::Binary);
				Assert(file.IsValid());
				if (LIKELY(file.IsValid()))
				{
					file.Write(fileData);
				}
			}
		}

		lock.Unlock();
		OnChanged();
		return State::Partial;
#else
		UNUSED(assetGuid);
		UNUSED(localFilePath);
		UNUSED(range);
		UNUSED(fileData);
		UNUSED(remoteURI);
		UNUSED(newEntityTag);
		return State::Invalid;
#endif
	}

	FileCacheState FileCache::GetCachedFileState(const IO::ConstURIView uri) const
	{
		Threading::SharedLock lock(m_mutex);
		auto it = m_cachedFileStates.Find(uri.GetStringView());
		if (it != m_cachedFileStates.end())
		{
			return it->second;
		}
		return FileCacheState::Unknown;
	}

	bool FileCache::ContainsETag(const IO::ConstURIView uri) const
	{
		Threading::SharedLock lock(m_mutex);
		return m_etagLookupMap.Contains(uri.GetStringView());
	}

	FileCacheState FileCache::ValidateRemoteFileTag(
		const Guid assetGuid, const IO::ConstURIView uri, const ConstStringView remoteETag, const IO::PathView assetExtensions
	)
	{
		Threading::UniqueLock lock(m_mutex);
		if (auto cachedFileStateIt = m_cachedFileStates.Find(uri.GetStringView()); cachedFileStateIt != m_cachedFileStates.end())
		{
			if (auto existingETagIt = m_etagLookupMap.Find(uri.GetStringView());
			    existingETagIt != m_etagLookupMap.end() && existingETagIt->second == remoteETag)
			{
				cachedFileStateIt->second = FileCacheState::Valid;
				return FileCacheState::Valid;
			}
			else
			{
				cachedFileStateIt->second = FileCacheState::OutOfDate;

				if (existingETagIt != m_etagLookupMap.end())
				{
					m_etagLookupMap.Remove(existingETagIt);
				}

				const IO::Path cachedFilePath = GetAssetPath(assetGuid, assetExtensions);
				if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
				{
					if (it->second.m_partialFileRanges.HasElements())
					{
						const IO::Path partialDirectory = GetPartialAssetDirectory(cachedFilePath);
						Assert(partialDirectory.Exists() || partialDirectory.IsDirectory());
						partialDirectory.EmptyDirectoryRecursively();
						[[maybe_unused]] const bool wasRemoved = partialDirectory.RemoveDirectory();
						Assert(wasRemoved || !partialDirectory.Exists());
					}
					else
					{
						Assert(cachedFilePath.IsFile());
						[[maybe_unused]] const bool wasRemoved = cachedFilePath.RemoveFile();
						Assert(wasRemoved);
					}

					m_map.Remove(it);
				}

				lock.Unlock();
				OnChanged();
				return FileCacheState::OutOfDate;
			}
		}
		else if (auto existingETagIt = m_etagLookupMap.Find(uri.GetStringView()); existingETagIt != m_etagLookupMap.end())
		{
			if (existingETagIt->second == remoteETag)
			{
				m_cachedFileStates.Emplace(uri.GetStringView(), FileCacheState::Valid);
				return FileCacheState::Valid;
			}
			else
			{
				m_etagLookupMap.Remove(existingETagIt);
				m_cachedFileStates.Emplace(uri.GetStringView(), FileCacheState::OutOfDate);

				const IO::Path cachedFilePath = GetAssetPath(assetGuid, assetExtensions);
				if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
				{
					if (it->second.m_partialFileRanges.HasElements())
					{
						const IO::Path partialDirectory = GetPartialAssetDirectory(cachedFilePath);
						if (partialDirectory.Exists())
						{
							Assert(partialDirectory.IsDirectory());
							partialDirectory.EmptyDirectoryRecursively();
							[[maybe_unused]] const bool wasRemoved = partialDirectory.RemoveDirectory();
							Assert(wasRemoved || !partialDirectory.Exists());
						}
					}
					else if (cachedFilePath.Exists())
					{
						Assert(cachedFilePath.IsFile());
						[[maybe_unused]] const bool wasRemoved = cachedFilePath.RemoveFile();
						Assert(wasRemoved);
					}

					m_map.Remove(it);
				}
			}

			lock.Unlock();
			OnChanged();
			return FileCacheState::OutOfDate;
		}
		else
		{
			m_cachedFileStates.Emplace(uri.GetStringView(), FileCacheState::OutOfDate);

			const IO::Path cachedFilePath = GetAssetPath(assetGuid, assetExtensions);
			if (auto it = m_map.Find(cachedFilePath); it != m_map.end())
			{
				if (it->second.m_partialFileRanges.HasElements())
				{
					const IO::Path partialDirectory = GetPartialAssetDirectory(cachedFilePath);
					Assert(partialDirectory.Exists() || partialDirectory.IsDirectory());
					partialDirectory.EmptyDirectoryRecursively();
					[[maybe_unused]] const bool wasRemoved = partialDirectory.RemoveDirectory();
					Assert(wasRemoved || !partialDirectory.Exists());
				}
				else
				{
					[[maybe_unused]] const bool wasRemoved = cachedFilePath.RemoveFile();
				}

				m_map.Remove(it);
			}

			return FileCacheState::OutOfDate;
		}
	}
}
