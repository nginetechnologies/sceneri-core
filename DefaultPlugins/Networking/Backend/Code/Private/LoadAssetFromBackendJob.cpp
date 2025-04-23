#include "LoadAssetFromBackendJob.h"
#include "Plugin.h"
#include "Game.h"

#include <Engine/Asset/AssetManager.h>

#include <Http/ResponseCode.h>
#include <Http/LoadAssetFromNetworkJob.h>

#include <Common/System/Query.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/IO/AsyncLoadFromDiskJob.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobManager.h>
#include <Common/IO/Log.h>

namespace ngine::Networking::Backend
{
	struct RequestAssetsBatchJob final : public Threading::Job
	{
		RequestAssetsBatchJob()
			: Job(Priority::UserInterfaceLoading)
		{
		}

		using Callback = Function<void(const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry), 24>;

		inline static constexpr Time::Durationf Delay = 25_milliseconds;

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread&) override
		{
			Vector<Asset::Guid> assets;
			{
				Threading::UniqueLock lock(m_queuedAssetsMutex);

				Assert(m_queuedAssets.HasElements());

				assets.Reserve(m_queuedAssets.GetSize());
				for (const QueuedAsset& queuedAsset : m_queuedAssets)
				{
					assets.EmplaceBack(queuedAsset.guid);
				}

				{
					Threading::UniqueLock callbackLock(m_assetCallbacksMutex);
					Assert(m_assetCallbacks.IsEmpty());
					m_assetCallbacks.Reserve(m_queuedAssets.GetSize());

					for (QueuedAsset& queuedAsset : m_queuedAssets)
					{
						m_assetCallbacks.Emplace(queuedAsset.guid, Move(queuedAsset.callback));
					}
				}
				m_queuedAssets.Clear();
			}

			Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
			Networking::Backend::Game& backendGameAPI = backend.GetGame();

			backendGameAPI.RequestAssets(
				Move(assets),
				[this]([[maybe_unused]] const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry) mutable
				{
					if (pBackendAssetEntry.IsValid())
					{
						Threading::UniqueLock callbackLock(m_assetCallbacksMutex);
						if (auto it = m_assetCallbacks.Find(pBackendAssetEntry->m_guid); it != m_assetCallbacks.end())
						{
							it->second(pBackendAssetEntry);
							m_assetCallbacks.Remove(it);
						}
					}
				},
				[this]([[maybe_unused]] const bool success)
				{
					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
					SignalExecutionFinished(thread);

					{
						Threading::UniqueLock lock(m_assetCallbacksMutex);
						for (auto it = m_assetCallbacks.begin(), endIt = m_assetCallbacks.end(); it != endIt; ++it)
						{
							it->second(Invalid);
						}
						m_assetCallbacks.Clear();
					}

					Threading::UniqueLock lock(m_queuedAssetsMutex);
					Assert(m_isQueued);
					if (m_queuedAssets.HasElements())
					{
						[[maybe_unused]] const Threading::TimerHandle timerHandle = thread.GetJobManager().ScheduleAsyncJob(Delay, *this);
					}
					else
					{
						m_isQueued = false;
					}
				}
			);
		}

		[[nodiscard]] virtual Result OnExecute(Threading::JobRunnerThread&) override
		{
			return Result::AwaitExternalFinish;
		}

		void QueueAsset(const Asset::Guid assetGuid, Callback&& callback)
		{
			Threading::UniqueLock lock(m_queuedAssetsMutex);
			Assert(!m_queuedAssets.ContainsIf(
				[assetGuid](const QueuedAsset& asset)
				{
					return asset.guid == assetGuid;
				}
			));
			m_queuedAssets.EmplaceBack(QueuedAsset{assetGuid, Forward<Callback>(callback)});
			if (!m_isQueued)
			{
				m_isQueued = true;
				lock.Unlock();
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
				[[maybe_unused]] const Threading::TimerHandle timerHandle = thread.GetJobManager().ScheduleAsyncJob(Delay, *this);
			}
		}
	protected:
		struct QueuedAsset
		{
			Asset::Guid guid;
			Callback callback;
		};

		Threading::Mutex m_queuedAssetsMutex;
		Vector<QueuedAsset> m_queuedAssets;
		bool m_isQueued{false};

		Threading::Mutex m_assetCallbacksMutex;
		UnorderedMap<Asset::Guid, Callback, Asset::Guid::Hash> m_assetCallbacks;
	};

	[[nodiscard]] RequestAssetsBatchJob& GetRequestAssetsBatchJob()
	{
		static RequestAssetsBatchJob job;
		return job;
	}

	LoadAssetJob::LoadAssetJob(
		Game& gameAPI,
		const Asset::Guid assetGuid,
		IO::Path&& targetPath,
		const Priority priority,
		IO::AsyncLoadCallback&& callback,
		const ByteView target,
		const Math::Range<size> dataRange
	)
		: Job(priority)
		, m_gameAPI(gameAPI)
		, m_assetGuid(assetGuid)
		, m_targetPath(Forward<IO::Path>(targetPath))
		, m_callback(Forward<IO::AsyncLoadCallback>(callback))
		, m_target(target)
		, m_dataRange(dataRange)
	{
		Assert(m_targetPath.GetSize() > 0);
	}

	LoadAssetJob::~LoadAssetJob()
	{
		[[maybe_unused]] const EnumFlags<StateFlags> awaitingStateFlags = m_stateFlags & StateFlags::AwaitingFlags;
		Assert(awaitingStateFlags.AreNoneSet());
	}

	void LoadAssetJob::OnAwaitExternalFinish(Threading::JobRunnerThread& thread)
	{
		const EnumFlags<StateFlags> awaitingStateFlags = m_stateFlags & StateFlags::AwaitingFlags;
		Assert(awaitingStateFlags.GetNumberOfSetFlags() == 1);
		switch (*awaitingStateFlags.GetFirstSetFlag())
		{
			case StateFlags::AwaitingInitialExecution:
			case StateFlags::AwaitingBackendFileToken:
			{
				[[maybe_unused]] const bool clearedFlag = m_stateFlags.TryClearFlags(StateFlags::AwaitingInitialExecution);
				Assert(clearedFlag);
				if (m_gameAPI.HasFileToken())
				{
					if (m_gameAPI.GetAssetDatabase().VisitEntry(
								m_assetGuid,
								[this, &thread](const Optional<const AssetEntry*> pAssetEntry)
								{
									if (pAssetEntry.IsValid())
									{
										constexpr bool ensureUpToDateWithBackend = false;
										if constexpr (!ensureUpToDateWithBackend)
										{
											RequestFileFromNetwork(*pAssetEntry, thread);
											return true;
										}
										else
										{
											const bool isUpToDateWithBackend = pAssetEntry->m_flags.IsSet(AssetFlags::UpToDate) | m_gameAPI.IsOffline();
											if (isUpToDateWithBackend)
											{
												RequestFileFromNetwork(*pAssetEntry, thread);
												return true;
											}
										}
									}
									return false;
								}
							))
					{
						break;
					}

					RequestBackendAssetInfo();
				}
				else
				{
					m_stateFlags |= StateFlags::AwaitingBackendFileToken;
					m_gameAPI.QueueFileTokenCallback(
						[this](const EnumFlags<SignInFlags> signInFlags)
						{
							[[maybe_unused]] const bool wasCleared = m_stateFlags.TryClearFlags(StateFlags::AwaitingBackendFileToken);
							Assert(wasCleared);
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							if (LIKELY(signInFlags.IsSet(SignInFlags::HasFileToken)))
							{
								m_stateFlags |= StateFlags::AwaitingInitialExecution;
								Queue(thread);
							}
							else
							{
								m_gameAPI.GetAssetDatabase().VisitEntry(
									m_assetGuid,
									[this, &thread](const Optional<const AssetEntry*> pAssetEntry)
									{
										if (pAssetEntry.IsValid())
										{
											RequestFileFromNetwork(*pAssetEntry, thread);
										}
										else
										{
											m_callback({});
											SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
											delete this;
										}
									}
								);
							}
						}
					);
				}
			}
			break;
			case StateFlags::AwaitingLoadFromNetwork:
			case StateFlags::AwaitingLoadFromDisk:
			case StateFlags::AwaitingBackendResponse:
			case StateFlags::AwaitingImportedAssetsBackendResponse:
			case StateFlags::AwaitingFlags:
			case StateFlags::ReceivedBackendResponse:
			case StateFlags::ReceivedLoadFromNetwork:
			case StateFlags::ReceivedLoadFromDisk:
			case StateFlags::ReceivedFlags:
				ExpectUnreachable();
		}
	}

	LoadAssetJob::Result LoadAssetJob::OnExecute(Threading::JobRunnerThread&)
	{
		const EnumFlags<StateFlags> awaitingStateFlags = m_stateFlags & StateFlags::AwaitingFlags;
		Assert(awaitingStateFlags.GetNumberOfSetFlags() == 1);
		switch (*awaitingStateFlags.GetFirstSetFlag())
		{
			case StateFlags::AwaitingInitialExecution:
			case StateFlags::AwaitingBackendFileToken:
				return Result::AwaitExternalFinish;
			case StateFlags::AwaitingLoadFromNetwork:
			case StateFlags::AwaitingLoadFromDisk:
				return Result::FinishedAndDelete;
			case StateFlags::AwaitingBackendResponse:
			case StateFlags::AwaitingImportedAssetsBackendResponse:
			case StateFlags::AwaitingFlags:
			case StateFlags::ReceivedBackendResponse:
			case StateFlags::ReceivedLoadFromNetwork:
			case StateFlags::ReceivedLoadFromDisk:
			case StateFlags::ReceivedFlags:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	void LoadAssetJob::RequestBackendAssetInfo()
	{
		Assert(!m_stateFlags.AreAnySet(StateFlags::AwaitingBackendResponse | StateFlags::ReceivedBackendResponse));
		m_stateFlags |= StateFlags::AwaitingBackendResponse;
		GetRequestAssetsBatchJob().QueueAsset(
			m_assetGuid,
			[this](const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry)
			{
				if (pBackendAssetEntry.IsValid())
				{
					Assert(pBackendAssetEntry->m_guid == m_assetGuid);
					if (!m_stateFlags.AreAnySet(StateFlags::AwaitingLoadFromNetwork | StateFlags::ReceivedLoadFromNetwork))
					{
						RequestFileFromNetwork(*pBackendAssetEntry, *Threading::JobRunnerThread::GetCurrent());
					}
				}
				else
				{
					// Failed to parse asset
					LogWarning("Failed to get asset {} from backend", m_assetGuid);
				}

				const EnumFlags<StateFlags> previousAwaitingFlags = m_stateFlags.FetchAnd(~StateFlags::AwaitingBackendResponse) &
			                                                      StateFlags::AwaitingFlags;
				m_stateFlags |= StateFlags::ReceivedBackendResponse;

				if (pBackendAssetEntry.IsValid())
				{
					if ((previousAwaitingFlags & ~StateFlags::AwaitingBackendResponse).AreNoneSet())
					{
						m_callback({});
						SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						delete this;
					}
				}
				else
				{
					m_gameAPI.GetAssetDatabase().VisitEntry(
						m_assetGuid,
						[this, previousAwaitingFlags](const Optional<const AssetEntry*> pAssetEntry)
						{
							if (pAssetEntry.IsValid())
							{
								RequestFileFromNetwork(*pAssetEntry, *Threading::JobRunnerThread::GetCurrent());
							}
							else
							{
								// Failed to parse asset
								LogWarning("Failed to query asset {} from backend", m_assetGuid);
								if ((previousAwaitingFlags & ~StateFlags::AwaitingBackendResponse).AreNoneSet())
								{
									m_callback({});
									SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
									delete this;
								}
							}
						}
					);
				}
			}
		);
	}

	void LoadAssetJob::RequestFileFromNetwork(const AssetEntry& backendAssetEntry, Threading::JobRunnerThread& thread)
	{
		using Networking::HTTP::LoadAssetFromNetworkJob;

		EnumFlags<Networking::HTTP::LoadAssetFromNetworkJob::Flags> flags;

		IO::URI uriAzureQuery;
		IO::URI uriQuery;
		if (LIKELY(m_gameAPI.HasFileToken()))
		{
			// Use etag as query parameter for cdn
			if (backendAssetEntry.m_mainFileEtag.HasElements())
			{
				uriAzureQuery = IO::URI(String::Merge("?", m_gameAPI.GetFileURIToken(), "&cb=", backendAssetEntry.m_mainFileEtag));
				uriQuery = IO::URI(String::Merge("?cb=", backendAssetEntry.m_mainFileEtag));
			}
			else
			{
				uriAzureQuery = IO::URI(String::Merge("?", m_gameAPI.GetFileURIToken()));
			}
		}
		else
		{
			flags |= Networking::HTTP::LoadAssetFromNetworkJob::Flags::SkipExistingFilesValidation;
		}

		IO::Path assetPathName = IO::Path::Combine(backendAssetEntry.m_name.GetView(), Asset::Asset::FileExtension);

		IO::URI rootURI(backendAssetEntry.m_mainFileURI.GetParentPath());

		if (!m_targetPath.HasExtension())
		{
			m_targetPath = IO::Path::Merge(m_targetPath, backendAssetEntry.m_mainFileURI.GetAllExtensions().GetStringView());
		}

		Assert(!m_stateFlags.AreAnySet(StateFlags::AwaitingLoadFromNetwork | StateFlags::ReceivedLoadFromNetwork));
		m_stateFlags |= StateFlags::AwaitingLoadFromNetwork;

		IO::URI requestURI;
		IO::URI emptyURI;
		IO::URI::ConstStringViewType foundRange = backendAssetEntry.m_mainFileURI.GetView().GetStringView().FindFirstRange(
			MAKE_URI_LITERAL("sceneriassetstorage.blob.core.windows.net")
		);
		IO::URI::ConstStringViewType foundRangeCDN =
			backendAssetEntry.m_mainFileURI.GetView().GetStringView().FindFirstRange(MAKE_URI_LITERAL("cdnsceneri.azureedge.net"));
		if (foundRange.HasElements() || foundRangeCDN.HasElements())
		{
			requestURI = IO::URI::Merge(
				backendAssetEntry.m_mainFileURI.GetWithoutExtensions(),
				m_targetPath.GetAllExtensions().GetStringView(),
				uriAzureQuery
			);
		}
		else
		{
			requestURI =
				IO::URI::Merge(backendAssetEntry.m_mainFileURI.GetWithoutExtensions(), m_targetPath.GetAllExtensions().GetStringView(), uriQuery);
		}
		LoadAssetFromNetworkJob* pLoadFromNetworkJob = new LoadAssetFromNetworkJob(
			m_assetGuid,
			IO::Path(m_targetPath),
			IO::URI::Merge(requestURI, emptyURI),
			GetPriority(),
			[this](const ConstByteView data)
			{
				m_stateFlags |= StateFlags::ReceivedLoadFromNetwork;
				const EnumFlags<StateFlags> previousAwaitingFlags = m_stateFlags.FetchAnd(~StateFlags::AwaitingLoadFromNetwork) &
			                                                      StateFlags::AwaitingFlags;

				// Request all imported asset database assets
				if (m_targetPath.GetAllExtensions() == Asset::Database::AssetFormat.metadataFileExtension)
				{
					Serialization::Data assetData(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
					);
					if (LIKELY(assetData.IsValid()))
					{
						const Asset::Database assetDatabase(assetData, m_targetPath.GetParentPath());

						Vector<Asset::Guid> assets(Memory::Reserve, assetDatabase.GetImportedAssetCount());
						assetDatabase.IterateImportedAssets(
							[&assetManager = System::Get<Asset::Manager>(), &assets](const Guid assetGuid)
							{
								if (!assetManager.HasAsset(assetGuid) && !assetManager.GetAssetLibrary().HasAsset(assetGuid) && !assetManager.HasAsyncLoadCallback(assetGuid))
								{
									assets.EmplaceBack(assetGuid);
								}
								return Memory::CallbackResult::Continue;
							}
						);

						if (assets.HasElements())
						{
							m_stateFlags |= StateFlags::AwaitingImportedAssetsBackendResponse;
							m_gameAPI.RequestAssets(
								Move(assets),
								{},
								[this,
						     data = Vector<ByteType, size>(ArrayView<const ByteType, size>{data.GetData(), data.GetDataSize()}
						     )]([[maybe_unused]] const bool success)
								{
									const EnumFlags<StateFlags> previousAwaitingFlags =
										m_stateFlags.FetchAnd(~StateFlags::AwaitingImportedAssetsBackendResponse) & StateFlags::AwaitingFlags;

									if ((previousAwaitingFlags & ~StateFlags::AwaitingImportedAssetsBackendResponse).AreNoneSet())
									{
										m_callback(data.GetView());
										SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
										delete this;
									}
								}
							);
							return;
						}
					}
				}

				if ((previousAwaitingFlags & ~StateFlags::AwaitingLoadFromNetwork).AreNoneSet())
				{
					m_callback(data);
					SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					delete this;
				}
			},
			flags,
			m_target,
			m_dataRange
		);
		pLoadFromNetworkJob->Queue(thread);
	}

	void LoadAssetJob::RequestFileFromDisk(const AssetEntry& backendAssetEntry, Threading::JobRunnerThread& thread)
	{
		IO::Path assetPathName = IO::Path::Combine(backendAssetEntry.m_name.GetView(), Asset::Asset::FileExtension);

		if (!m_targetPath.HasExtension())
		{
			m_targetPath = IO::Path::Merge(m_targetPath, backendAssetEntry.m_mainFileURI.GetAllExtensions().GetStringView());
		}

		Assert(!m_stateFlags.AreAnySet(StateFlags::AwaitingLoadFromDisk | StateFlags::ReceivedLoadFromDisk));
		m_stateFlags |= StateFlags::AwaitingLoadFromDisk;

		Threading::Job* pJob = IO::CreateAsyncLoadFromDiskJob(
			m_targetPath,
			GetPriority(),
			[this](const ConstByteView data)
			{
				m_stateFlags |= StateFlags::ReceivedLoadFromDisk;
				const EnumFlags<StateFlags> previousAwaitingFlags = m_stateFlags.FetchAnd(~StateFlags::AwaitingLoadFromDisk) &
			                                                      StateFlags::AwaitingFlags;

				if ((previousAwaitingFlags & ~StateFlags::AwaitingLoadFromDisk).AreNoneSet())
				{
					m_callback(data);
					SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					delete this;
				}
			},
			m_target,
			m_dataRange
		);
		pJob->Queue(thread);
	}
}
