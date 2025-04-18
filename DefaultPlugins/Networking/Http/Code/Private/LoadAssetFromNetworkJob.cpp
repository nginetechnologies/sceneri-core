#include <Http/LoadAssetFromNetworkJob.h>

#include <Http/FileCache.h>
#include <Http/Plugin.h>
#include <Http/Worker.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/IO/AsyncLoadFromDiskJob.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/LocalAssetDatabase.h>

namespace ngine::Networking::HTTP
{
	LoadAssetFromNetworkJob::LoadAssetFromNetworkJob(
		const Asset::Guid assetGuid,
		IO::Path&& localPath,
		IO::URI&& remoteURI,
		const Priority priority,
		IO::AsyncLoadCallback&& callback,
		const EnumFlags<Flags> flags,
		const ByteView target,
		const Math::Range<size> dataRange
	)
		: Job(priority)
		, m_assetGuid(assetGuid)
		, m_localPath(Forward<IO::Path>(localPath))
		, m_remoteURI(Forward<IO::URI>(remoteURI))
		, m_callback(Forward<IO::AsyncLoadCallback>(callback))
		, m_flags(flags)
		, m_target(target)
		, m_dataRange(dataRange)
	{
		Assert(m_localPath.GetSize() > 0);
		Assert(m_remoteURI.GetSize() > 0);
		m_remoteURI.EscapeSpaces();
	}

	[[nodiscard]] IO::URI GetAdjustedURI(const IO::PathView localPath, const IO::PathView rootLocalPath, const IO::URIView rootRemoteURI)
	{
		IO::Path relativePathURI(localPath.GetRelativeToParent(rootLocalPath));
		if constexpr (PLATFORM_WINDOWS)
		{
			relativePathURI.MakeForwardSlashes();
		}
		return IO::URI::Combine(rootRemoteURI, relativePathURI.GetView().GetStringView());
	}

	LoadAssetFromNetworkJob::LoadAssetFromNetworkJob(
		const Asset::Guid assetGuid,
		const IO::PathView localPath,
		const IO::PathView rootLocalPath,
		const IO::URIView rootRemoteURI,
		const IO::URIView uriQueryString,
		const Priority priority,
		IO::AsyncLoadCallback&& callback,
		const EnumFlags<Flags> flags,
		const ByteView target,
		const Math::Range<size> dataRange
	)
		: LoadAssetFromNetworkJob(
				assetGuid,
				IO::Path(localPath),
				IO::URI::Merge(GetAdjustedURI(localPath, rootLocalPath, rootRemoteURI), uriQueryString),
				priority,
				Forward<IO::AsyncLoadCallback>(callback),
				flags,
				target,
				dataRange
			)
	{
	}

	void LoadAssetFromNetworkJob::OnDataReceived(const ConstByteView fileData)
	{
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();

		if (LIKELY(fileData.HasElements()))
		{
			if (m_target.HasElements())
			{
				m_target.CopyFrom(fileData);
			}

			Assert(fileData.GetDataSize() <= m_dataRange.GetSize());
			m_callback(fileData);
		}
		else
		{
			switch (m_state)
			{
				case State::AwaitingInitialExecution:
				case State::AwaitingFileCacheValidityCheck:
					ExpectUnreachable();
				case State::AwaitingLocalFileStreamFinish:
				{
					// Loading the local file failed, fall back to the file cache
					if (!QueueLoadCachedFile())
					{
						// Not present in the cache, skip to loading from network
						QueueDownloadRemoteFile();
					}
				}
				break;
				case State::AwaitingPartialCachedFileStreamFinish:
				case State::AwaitingCompleteCachedFileStreamFinish:
				{
					// Loading from the file cache failed, fall back to the network
					QueueDownloadRemoteFile();
				}
				break;
				case State::AwaitingRemoteFileDownloadFinish:
				{
					// All paths failed, notify the requester
					m_callback({});
					SignalExecutionFinishedAndDestroying(thread);
					delete this;
				}
				break;
			}
		}
	}

	void LoadAssetFromNetworkJob::QueueLoadLocalFile()
	{
		m_state = State::AwaitingLocalFileStreamFinish;
		Threading::Job* pJob = IO::CreateAsyncLoadFromDiskJob(
			m_localPath,
			GetPriority(),
			IO::AsyncLoadCallback(*this, &LoadAssetFromNetworkJob::OnDataReceived),
			{},
			m_dataRange
		);
		pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
	}

	bool LoadAssetFromNetworkJob::QueueLoadCachedFile()
	{
		// Now check if it exists in the network asset cache
		FileCache& fileCache = FileCache::GetInstance();
		const FileCache::Result fileCacheResult = fileCache.Find(m_assetGuid, m_localPath.GetAllExtensions(), m_dataRange);
		switch (fileCacheResult.state)
		{
			case FileCache::State::Invalid:
			case FileCache::State::RangeNotFound:
				return false;
			case FileCache::State::Partial:
			{
				m_state = State::AwaitingPartialCachedFileStreamFinish;
				Threading::Job* pJob = IO::CreateAsyncLoadFromDiskJob(
					fileCacheResult.path,
					GetPriority(),
					IO::AsyncLoadCallback(*this, &LoadAssetFromNetworkJob::OnDataReceived),
					{},
					Math::Range<size>::Make(0, m_dataRange.GetSize())
				);
				pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
				return true;
			}
			case FileCache::State::Complete:
			{
				m_state = State::AwaitingCompleteCachedFileStreamFinish;
				Threading::Job* pJob = IO::CreateAsyncLoadFromDiskJob(
					fileCacheResult.path,
					GetPriority(),
					IO::AsyncLoadCallback(*this, &LoadAssetFromNetworkJob::OnDataReceived),
					{},
					m_dataRange
				);
				pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
				return true;
			}
		}
		ExpectUnreachable();
	}

	void LoadAssetFromNetworkJob::QueueDownloadRemoteFile()
	{
		m_state = State::AwaitingRemoteFileDownloadFinish;
		static Threading::SharedMutex requestMutex;

		struct OngoingRequest
		{
			IO::URI uri;
			Math::Range<size> range;
			InlineVector<IO::AsyncLoadCallback, 2> callbacks;
		};

		static Vector<OngoingRequest> ongoingRequests;
		{
			Threading::UniqueLock lock(requestMutex);

			for (OngoingRequest& __restrict request : ongoingRequests)
			{
				if (request.uri == m_remoteURI && request.range.Contains(m_dataRange))
				{
					request.callbacks.EmplaceBack(IO::AsyncLoadCallback(*this, &LoadAssetFromNetworkJob::OnDataReceived));
					return;
				}
			}

			ongoingRequests.EmplaceBack(
				OngoingRequest{IO::URI(m_remoteURI), m_dataRange, IO::AsyncLoadCallback(*this, &LoadAssetFromNetworkJob::OnDataReceived)}
			);
		}

		System::FindPlugin<HTTP::Plugin>()->GetHighPriorityWorker().QueueRequest(
			Networking::HTTP::Worker::Request{
				Networking::HTTP::RequestType::Get,
				m_remoteURI,
				{},
				{},
				[this, range = m_dataRange](const Worker::RequestInfo&, const Networking::HTTP::Worker::ResponseData& responseData) mutable
				{
					if (responseData.m_responseCode.IsSuccessful())
					{
						ConstByteView data{
							responseData.m_responseBody.GetData(),
							Math::Min(responseData.m_responseBody.GetDataSize(), range.GetSize())
						};

						ConstStringView entityTag = responseData.FindHeader("ETag");
						if (entityTag.HasElements())
						{
							entityTag = FilterEntityTag(entityTag);
						}
						const IO::ConstURIView remoteURI = m_remoteURI.GetView().GetWithoutQueryString();

						const bool requestedCompleteFile = range.GetMaximum() == (Math::NumericLimits<size>::Max - 1);
						FileCache& fileCache = FileCache::GetInstance();
						fileCache.AddFileData(
							m_assetGuid,
							m_localPath,
							requestedCompleteFile ? range : Math::Range<size>::Make(range.GetMinimum(), responseData.m_responseBody.GetDataSize()),
							responseData.m_responseBody,
							remoteURI,
							entityTag
						);

						{
							Threading::UniqueLock lock(requestMutex);
							for (OngoingRequest& __restrict request : ongoingRequests)
							{
								if (request.uri == m_remoteURI && request.range.Contains(range))
								{
									OngoingRequest copiedRequest = Move(request);
									ongoingRequests.Remove(&request);
									lock.Unlock();

									for (const IO::AsyncLoadCallback& __restrict callback : copiedRequest.callbacks)
									{
										callback(data);
									}
									break;
								}
							}
						}
					}
					else
					{
						Threading::UniqueLock lock(requestMutex);
						for (OngoingRequest& __restrict request : ongoingRequests)
						{
							if (request.uri == m_remoteURI && request.range.Contains(range))
							{
								OngoingRequest copiedRequest = Move(request);
								ongoingRequests.Remove(&request);
								lock.Unlock();

								for (const IO::AsyncLoadCallback& __restrict callback : copiedRequest.callbacks)
								{
									callback({});
								}
								break;
							}
						}
					}
				},
				[](const Worker::RequestInfo&, ConstStringView)
				{
				},
				m_dataRange
			},
			GetPriority()
		);
	}

	void LoadAssetFromNetworkJob::OnAwaitExternalFinish(Threading::JobRunnerThread&)
	{
		switch (m_state)
		{
			case State::AwaitingInitialExecution:
			{
				Networking::HTTP::Plugin& httpPlugin = *System::FindPlugin<Networking::HTTP::Plugin>();

				if (m_flags.IsSet(Flags::SkipExistingFilesValidation))
				{
					QueueLoadLocalFile();
					return;
				}

				m_state = State::AwaitingFileCacheValidityCheck;
				CheckFileCacheValidityAsync(
					m_remoteURI,
					httpPlugin,
					[this](const FileCacheState fileState) mutable
					{
						switch (fileState)
						{
							case FileCacheState::Valid:
							case FileCacheState::ValidationFailed:
							{
								QueueLoadLocalFile();
							}
							break;
							case FileCacheState::OutOfDate:
							{
								QueueDownloadRemoteFile();
							}
							break;
							case FileCacheState::Unknown:
								ExpectUnreachable();
						}
					}
				);
			}
			break;
			case State::AwaitingFileCacheValidityCheck:
			case State::AwaitingLocalFileStreamFinish:
			case State::AwaitingPartialCachedFileStreamFinish:
			case State::AwaitingCompleteCachedFileStreamFinish:
			case State::AwaitingRemoteFileDownloadFinish:
				ExpectUnreachable();
		}
	};

	LoadAssetFromNetworkJob::Result LoadAssetFromNetworkJob::OnExecute(Threading::JobRunnerThread&)
	{
		switch (m_state)
		{
			case State::AwaitingInitialExecution:
				return Result::AwaitExternalFinish;
			case State::AwaitingRemoteFileDownloadFinish:
				return Result::AwaitExternalFinish;
			case State::AwaitingFileCacheValidityCheck:
			case State::AwaitingLocalFileStreamFinish:
			case State::AwaitingPartialCachedFileStreamFinish:
			case State::AwaitingCompleteCachedFileStreamFinish:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	using OngoingValidityRequestCallbacks = Vector<LoadAssetFromNetworkJob::OngoingValidityRequestCallback>;

	static Threading::SharedMutex validityRequestMutex;
	static UnorderedMap<IO::URI, OngoingValidityRequestCallbacks, IO::URI::Hash> ongoingValidityRequests;

	void LoadAssetFromNetworkJob::CheckFileCacheValidityAsync(
		const IO::URI& uri, Networking::HTTP::Plugin& httpPlugin, OngoingValidityRequestCallback&& callback
	)
	{
		FileCache& fileCache = FileCache::GetInstance();
		// First check if we haven't already validated this URI's cache validity for this session
		const IO::ConstURIView checkedURI = uri.GetView().GetWithoutQueryString();
		switch (fileCache.GetCachedFileState(checkedURI))
		{
			case FileCacheState::Valid:
				callback(FileCacheState::Valid);
				break;
			case FileCacheState::OutOfDate:
				callback(FileCacheState::OutOfDate);
				break;
			case FileCacheState::ValidationFailed:
				callback(FileCacheState::ValidationFailed);
				break;
			case FileCacheState::Unknown:
			{
				if (fileCache.ContainsETag(checkedURI))
				{
					{
						Threading::UniqueLock validityRequestLock(validityRequestMutex);

						auto it = ongoingValidityRequests.Find(uri);
						if (it != ongoingValidityRequests.end())
						{
							it->second.EmplaceBack(Move(callback));
							return;
						}

						ongoingValidityRequests.Emplace(IO::URI(uri), Move(callback));
					}

					httpPlugin.GetHighPriorityWorker().QueueRequest(
						Networking::HTTP::Worker::Request{
							Networking::HTTP::RequestType::Headers,
							uri,
							{},
							{},
							[this, uri = IO::URI(uri)](const Worker::RequestInfo&, const Networking::HTTP::Worker::ResponseData& responseData) mutable
							{
								const IO::ConstURIView checkedURI = uri.GetView().GetWithoutQueryString();
								ConstStringView remoteEntityTag = responseData.FindHeader("ETag");
								remoteEntityTag = FilterEntityTag(remoteEntityTag);
								FileCache& fileCache = FileCache::GetInstance();
								const FileCacheState fileCacheState = fileCache.ValidateRemoteFileTag(
									m_assetGuid,
									IO::URI(checkedURI.GetStringView()),
									remoteEntityTag,
									m_localPath.GetAllExtensions()
								);

								{
									Threading::UniqueLock lock(validityRequestMutex);
									auto it = ongoingValidityRequests.Find(uri);
									Assert(it != ongoingValidityRequests.end());
									if (LIKELY(it != ongoingValidityRequests.end()))
									{
										OngoingValidityRequestCallbacks copiedCallbacks = Move(it->second);
										ongoingValidityRequests.Remove(it);
										lock.Unlock();

										for (const OngoingValidityRequestCallback& __restrict cachedCallback : copiedCallbacks)
										{
											cachedCallback(fileCacheState);
										}
									}
								}
							}
						},
						GetPriority()
					);
				}
				else
				{
					callback(FileCacheState::OutOfDate);
				}
			}
			break;
		}
	}
}
