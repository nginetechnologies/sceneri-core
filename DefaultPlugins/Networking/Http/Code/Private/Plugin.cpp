#include <Http/Plugin.h>
#include <Http/Worker.h>
#include <Http/LoadAssetFromNetworkJob.h>

#include <Common/System/Query.h>
#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Clamp.h>
#include <Common/Serialization/Version.h>
#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/IO/Format/URI.h>
#include <Common/IO/Format/ZeroTerminatedURIView.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

#if PLATFORM_EMSCRIPTEN
#include <emscripten/fetch.h>
#elif USE_LIBCURL
#include <curl.h>
#endif

#include <Common/IO/Log.h>
#pragma clang optimize off
namespace ngine::Networking::HTTP
{
#if USE_LIBCURL
	namespace Internal
	{
		void* Allocate(const size allocation)
		{
			return Memory::Allocate(allocation);
		}

		void Deallocate(void* pPtr)
		{
			Memory::Deallocate(pPtr);
		}

		void* Reallocate(void* pPtr, const size allocation)
		{
			if (pPtr == nullptr)
			{
				return Memory::Allocate(allocation);
			}

			return Memory::Reallocate(pPtr, allocation);
		}

		char* DuplicateString(const char* pString)
		{
			if (pString != nullptr)
			{
				const size stringLength = strlen(pString);
				char* pTarget = reinterpret_cast<char*>(Memory::Allocate(stringLength + 1));
				Memory::CopyWithoutOverlap(pTarget, pString, stringLength);
				pTarget[stringLength] = '\0';
				return pTarget;
			}
			else
			{
				return nullptr;
			}
		}

		void* AllocateClass(const size classSize, const size count)
		{
			void* pData = Memory::Allocate(classSize * count);
			Memory::Set(pData, 0, classSize * count);
			return pData;
		}
	}
#endif

	Plugin::Plugin(Application&)
	{
	}

	void Plugin::OnLoaded(Application&)
	{
#if USE_LIBCURL
		// Select the SSL backend
		const curl_ssl_backend** availableSslBackends;
		curl_global_sslset(CURLSSLBACKEND_NONE, NULL, &availableSslBackends);
		uint32 sslBackendCount = 0;
		for (; availableSslBackends[sslBackendCount] != nullptr; ++sslBackendCount)
			;
		const ArrayView<const curl_ssl_backend*> backends{availableSslBackends, sslBackendCount};

		const Array preferredSslBackendsOrder
		{
#if PLATFORM_APPLE
			CURLSSLBACKEND_SECURETRANSPORT,
#endif
				CURLSSLBACKEND_OPENSSL, CURLSSLBACKEND_GNUTLS,
#if PLATFORM_WINDOWS
				CURLSSLBACKEND_SCHANNEL,
#endif
				CURLSSLBACKEND_WOLFSSL, CURLSSLBACKEND_BEARSSL,
		};
		[[maybe_unused]] CURLsslset sslSetResult = CURLSSLSET_UNKNOWN_BACKEND;
		for (const curl_sslbackend preferredSslBackend : preferredSslBackendsOrder)
		{
			if (backends.ContainsIf(
						[preferredSslBackend](const curl_ssl_backend* sslBackend)
						{
							return sslBackend->id == preferredSslBackend;
						}
					))
			{
				sslSetResult = curl_global_sslset(preferredSslBackend, nullptr, nullptr);
				Assert(sslSetResult == CURLSSLSET_OK);
				break;
			}
		}
		Assert(sslSetResult == CURLSSLSET_OK);

#if PLATFORM_WINDOWS
		WORD versionRequested = MAKEWORD(2, 2);
		WSADATA wsaData;

		if (WSAStartup(versionRequested, &wsaData))
			return;

		if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2)
		{
			WSACleanup();

			return;
		}
#endif

		[[maybe_unused]] const CURLcode initResult = curl_global_init_mem(
			CURL_GLOBAL_ALL & ~CURL_GLOBAL_WIN32,
			Internal::Allocate,
			Internal::Deallocate,
			Internal::Reallocate,
			Internal::DuplicateString,
			Internal::AllocateClass
		);
		Assert(initResult == CURLE_OK);

        curl_version_info_data *ver = curl_version_info(CURLVERSION_NOW);
        LogMessage("libcurl version {}.{}.{}\n",
               (ver->version_num >> 16) & 0xff,
               (ver->version_num >> 8) & 0xff,
               ver->version_num & 0xff);
#endif

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		m_pHighPriorityWorker = CreateWorker(Threading::JobPriority::HighPriorityAsyncNetworkingRequests, assetManager);
		m_pLowPriorityWorker = CreateWorker(Threading::JobPriority::LowPriorityAsyncNetworkingRequests, assetManager);

		assetManager.OnLoadAssetDatabase.Add(
			*this,
			[&assetManager](Plugin&, const Serialization::Reader assetDatabaseReader)
			{
				for (const Serialization::Member<Serialization::Reader> assetEntryMember : assetDatabaseReader.GetMemberView())
				{
					if (!assetEntryMember.value.GetValue().IsObject())
					{
						continue;
					}

					if (Optional<IO::URI> pAssetUri = assetEntryMember.value.Read<IO::URI>("uri"))
					{
						const Asset::Guid assetGuid = Asset::Guid::TryParse(assetEntryMember.key);
						if (LIKELY(assetGuid.IsValid()))
						{
							assetManager.RegisterAsyncLoadCallback(
								assetGuid,
								[assetURI = Move(*pAssetUri)](
									const Asset::Guid assetGuid,
									const IO::PathView path,
									Threading::JobPriority priority,
									IO::AsyncLoadCallback&& callback,
									const ByteView target,
									const Math::Range<size> dataRange
								) -> Optional<Threading::Job*>
								{
									return new Networking::HTTP::LoadAssetFromNetworkJob(
										assetGuid,
										IO::Path(path),
										IO::URI(assetURI),
										priority,
										Forward<IO::AsyncLoadCallback>(callback),
										Networking::HTTP::LoadAssetFromNetworkJob::Flags{},
										target,
										dataRange
									);
								}
							);
						}
					}
				}
			}
		);
	}

	Plugin::~Plugin()
	{
		m_pHighPriorityWorker.DestroyElement();
		m_pLowPriorityWorker.DestroyElement();

#if USE_LIBCURL
		curl_global_cleanup();
#endif
	}

	Worker::Worker(const Threading::JobPriority priority, [[maybe_unused]] const Asset::Manager& assetManager)
		// TODO: Set dynamic priority based on requests
		: Job(priority)
#if USE_LIBCURL && !PLATFORM_ANDROID
		, m_caCertificateFilePath(String(assetManager.GetAssetBinaryPath("C4CA4253-0CD9-492F-8A11-CD1172F201AC"_asset).GetView().GetStringView()
	    ))
#endif
	{
#if USE_LIBCURL && PLATFORM_ANDROID
		IO::Path targetPath = IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("cacert.pem"));
		if (!targetPath.Exists())
		{
			[[maybe_unused]] const bool wereDirectoriesCreated = IO::Path(targetPath.GetParentPath()).CreateDirectories();
			Assert(wereDirectoriesCreated);

			IO::Path sourcePath(assetManager.GetAssetBinaryPath("C4CA4253-0CD9-492F-8A11-CD1172F201AC"_asset));
			[[maybe_unused]] const bool wasCopied = sourcePath.CopyFileTo(targetPath);
			Assert(wasCopied);
		}

		m_caCertificateFilePath = String(targetPath.GetView().GetStringView());
#endif

#if USE_LIBCURL
		m_pMultiHandle = curl_multi_init();
#endif
	}

	Worker::~Worker()
	{
#if USE_LIBCURL
		curl_multi_cleanup(m_pMultiHandle);
#endif

		if (m_scheduledTimerHandle.IsValid())
		{
			System::Get<Threading::JobManager>().CancelAsyncJob(m_scheduledTimerHandle);
		}
	}

	void Worker::OnAwaitExternalFinish(Threading::JobRunnerThread& thread)
	{
		bool hasOngoingRequests = m_hasOngoingRequests;
		{
			Threading::SharedLock lock(m_queuedRequestsMutex);
			if (m_queuedRequests.HasElements())
			{
				hasOngoingRequests = true;
			}
		}

		if (hasOngoingRequests)
		{
			constexpr Time::Durationf defaultWaitTime = 0.1_milliseconds;

#if USE_LIBCURL
			long timeoutInMilliseconds;
			curl_multi_timeout(m_pMultiHandle, &timeoutInMilliseconds);
#else
			constexpr long timeoutInMilliseconds = -1;
#endif

			Time::Durationf waitTime;
			if (timeoutInMilliseconds != -1)
			{
				waitTime = Time::Durationf::FromMilliseconds(Math::Min((uint64)defaultWaitTime.GetMilliseconds(), (uint64)timeoutInMilliseconds));
			}
			else
			{
				waitTime = defaultWaitTime;
			}

			Status expected = Status::AwaitExternalFinish;
			[[maybe_unused]] const bool wasExchanged = m_status.CompareExchangeStrong(expected, Status::Scheduled);
			Assert(wasExchanged);

			SignalExecutionFinished(thread);

			m_scheduledTimerHandle = thread.GetJobManager().ScheduleAsyncJob(waitTime, *this);
		}
		else
		{
			Status expected = Status::AwaitExternalFinish;
			[[maybe_unused]] const bool wasExchanged = m_status.CompareExchangeStrong(expected, Status::None);
			Assert(wasExchanged);

			SignalExecutionFinished(thread);
		}
	}

	UniquePtr<Worker::RequestInfo> Worker::RemoveRequest(const RequestIdentifier requestIdentifier)
	{
		UniquePtr<RequestInfo> pRequest(Move(m_requests[requestIdentifier]));
		m_requestIdentifiers.ReturnIdentifier(requestIdentifier);

		{
			// Queue to process other pending requests if we have any
			Threading::SharedLock lock(m_queuedRequestsMutex);
			if (m_queuedRequests.HasElements())
			{
				lock.Unlock();

				Status expected = Status::None;
				if (m_status.CompareExchangeStrong(expected, Status::Scheduled))
				{
					if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
					{
						TryQueue(*pThread);
					}
					else
					{
						TryQueue(System::Get<Threading::JobManager>());
					}
				}
			}
		}

		return Move(pRequest);
	}

#if PLATFORM_EMSCRIPTEN
	constexpr bool SupportsStreamedRequests = true;

	union FetchProgressArgumentsUnion
	{
		Worker::RequestInfo* pRequest;
		struct
		{
			int arg1;
			int arg2;
		};
	};

	void OnFetchProgressOnParentWorker(const int arg1, const int arg2)
	{
		FetchProgressArgumentsUnion arguments;
		arguments.arg1 = arg1;
		arguments.arg2 = arg2;

		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();

		jobManager.QueueCallback(
			[&requestInfo = *arguments.pRequest](Threading::JobRunnerThread&)
			{
				Assert(requestInfo.m_progressCallback.IsValid());
				{
					Threading::UniqueLock lock(requestInfo.m_requestBodyMutex);
					requestInfo.m_progressCallback(requestInfo, requestInfo.m_responseBody);
				}

				const uint32 previousEventCount = requestInfo.m_asyncEventCounter.FetchSubtract(1);
				Assert(previousEventCount > 0);
				if (previousEventCount == 1)
				{
					UniquePtr<Worker::RequestInfo> pRequestInfo = Move(requestInfo.pThis);

					if constexpr (PROFILE_BUILD)
					{
						Threading::UniqueLock lock(Threading::TryLock, requestInfo.m_requestBodyMutex);
						Assert(lock.IsLocked(), "No locking after fetch should be possible");
					}
					if constexpr (!SupportsStreamedRequests)
					{
						// Ensure we trigger the progress callback at least once
						requestInfo.m_progressCallback(requestInfo, requestInfo.m_responseBody);
					}
					requestInfo.m_onFinishedCallback(
						requestInfo,
						Worker::ResponseData{
							requestInfo.m_responseHeaders,
							requestInfo.m_responseBody,
							requestInfo.m_responseCode,
							requestInfo.m_contentLength
						}
					);
				}
			},
			arguments.pRequest->m_priority,
			"HTTP Fetch Progress"
		);
	}

	void OnFetchProgress(emscripten_fetch_t* fetch)
	{
		Assert(!emscripten_is_main_browser_thread());
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(fetch->userData);

		Assert(requestInfo.m_asyncEventCounter.Load() > 0);

		const uint64 receivedSize = (size)fetch->numBytes;
		uint64 totalSize;
		{
			Threading::UniqueLock lock(requestInfo.m_requestBodyMutex);
			totalSize = Math::Min(receivedSize, (uint64)requestInfo.m_downloadRange.GetSize() - requestInfo.m_responseBody.GetDataSize());
			Assert(requestInfo.m_responseBody.GetDataSize() + totalSize <= requestInfo.m_downloadRange.GetSize());

			requestInfo.m_responseBody += ConstStringView(fetch->data, (uint32)totalSize);
			Assert(requestInfo.m_responseBody.GetDataSize() <= requestInfo.m_downloadRange.GetSize());
		}

		if (LIKELY(totalSize > 0) && requestInfo.m_progressCallback.IsValid())
		{
			[[maybe_unused]] const uint32 previousEventCount = requestInfo.m_asyncEventCounter.FetchAdd(1);
			Assert(previousEventCount > 0);

			FetchProgressArgumentsUnion arguments;
			arguments.pRequest = &requestInfo;
			emscripten_wasm_worker_post_function_vii(
				EMSCRIPTEN_WASM_WORKER_ID_PARENT,
				OnFetchProgressOnParentWorker,
				arguments.arg1,
				arguments.arg2
			);
		}
	}

	struct FetchData
	{
		UniquePtr<Worker::RequestInfo> pRequestInfo;
	};

	union FetchArgumentsUnion
	{
		FetchData* pData;
		struct
		{
			int arg1;
			int arg2;
		};
	};
	void OnFetchSuccessOnParentWorker(const int arg1, const int arg2)
	{
		FetchArgumentsUnion arguments;
		arguments.arg1 = arg1;
		arguments.arg2 = arg2;

		FetchData& __restrict data = *arguments.pData;
		Assert(data.pRequestInfo->m_asyncEventCounter.Load() > 0);

		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
		Threading::JobPriority priority = data.pRequestInfo->m_priority;
		jobManager.QueueCallback(
			[pOwnedRequestInfo = Move(data.pRequestInfo)](Threading::JobRunnerThread&) mutable
			{
				Worker::RequestInfo& requestInfo = *pOwnedRequestInfo;
				requestInfo.pThis = Move(pOwnedRequestInfo);

				const uint32 previousEventCount = requestInfo.m_asyncEventCounter.FetchSubtract(1);
				Assert(previousEventCount > 0);
				if (previousEventCount == 1)
				{
					pOwnedRequestInfo = Move(requestInfo.pThis);

					if constexpr (PROFILE_BUILD)
					{
						Threading::UniqueLock lock(Threading::TryLock, requestInfo.m_requestBodyMutex);
						Assert(lock.IsLocked(), "No locking after fetch should be possible");
					}

					if constexpr (!SupportsStreamedRequests)
					{
						// Ensure we trigger the progress callback at least once
						if (requestInfo.m_progressCallback.IsValid())
						{
							requestInfo.m_progressCallback(requestInfo, requestInfo.m_responseBody);
						}
					}
					requestInfo.m_onFinishedCallback(
						requestInfo,
						Worker::ResponseData{
							requestInfo.m_responseHeaders,
							requestInfo.m_responseBody,
							requestInfo.m_responseCode,
							requestInfo.m_contentLength
						}
					);
				}
			},
			priority,
			"HTTP Fetch Success"
		);
	};

	void OnFetchReadyStateChanged(emscripten_fetch_t* fetch)
	{
		Assert(!emscripten_is_main_browser_thread());
		Assert(fetch->userData != nullptr);
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(fetch->userData);
		Assert(requestInfo.m_asyncEventCounter.Load() > 0);

		{
			Threading::UniqueLock lock(requestInfo.m_requestBodyMutex);
			requestInfo.m_responseBody.Reserve((uint32)fetch->totalBytes);
		}
	}

	void OnFetchSuccess(emscripten_fetch_t* fetch)
	{
		Assert(!emscripten_is_main_browser_thread());
		Assert(fetch->userData != nullptr);
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(fetch->userData);
		Worker& worker = *requestInfo.m_pWorker;
		Assert(requestInfo.m_asyncEventCounter > 0);

		UniquePtr<Worker::RequestInfo> pRequestInfo = worker.RemoveRequest(requestInfo.m_identifier);
		Assert(pRequestInfo.IsValid());

		String responseHeaders(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)emscripten_fetch_get_response_headers_length(fetch));
		emscripten_fetch_get_response_headers(fetch, responseHeaders.GetData(), responseHeaders.GetSize());
		pRequestInfo->m_responseHeaders = Move(responseHeaders);

		if constexpr (!SupportsStreamedRequests)
		{
			Threading::UniqueLock lock(requestInfo.m_requestBodyMutex);
			if (pRequestInfo->m_responseBody.IsEmpty())
			{
				const uint64 receivedSize = fetch->numBytes;
				const uint64 totalSize = Math::Min(receivedSize, requestInfo.m_downloadRange.GetSize() - requestInfo.m_responseBody.GetDataSize());
				requestInfo.m_responseBody = ConstStringView(fetch->data, (uint32)totalSize);
			}
		}

		pRequestInfo->m_responseCode = ResponseCode{fetch->status};
		pRequestInfo->m_contentLength = fetch->numBytes;
		emscripten_fetch_close(fetch);

		FetchArgumentsUnion arguments;
		arguments.pData = new FetchData{Move(pRequestInfo)};

		emscripten_wasm_worker_post_function_vii(
			EMSCRIPTEN_WASM_WORKER_ID_PARENT,
			OnFetchSuccessOnParentWorker,
			arguments.arg1,
			arguments.arg2
		);
	}

	void OnFetchFailOnParentWorker(const int arg1, const int arg2)
	{
		FetchArgumentsUnion arguments;
		arguments.arg1 = arg1;
		arguments.arg2 = arg2;

		FetchData& __restrict data = *arguments.pData;
		Assert(data.pRequestInfo->m_asyncEventCounter.Load() > 0);

		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
		Threading::JobPriority priority = data.pRequestInfo->m_priority;
		jobManager.QueueCallback(
			[pOwnedRequestInfo = Move(data.pRequestInfo)](Threading::JobRunnerThread&) mutable
			{
				Worker::RequestInfo& requestInfo = *pOwnedRequestInfo;
				requestInfo.pThis = Move(pOwnedRequestInfo);

				const uint32 previousEventCount = requestInfo.m_asyncEventCounter.FetchSubtract(1);
				Assert(previousEventCount > 0);
				if (previousEventCount == 1)
				{
					pOwnedRequestInfo = Move(requestInfo.pThis);

					if constexpr (PROFILE_BUILD)
					{
						Threading::UniqueLock lock(Threading::TryLock, requestInfo.m_requestBodyMutex);
						Assert(lock.IsLocked(), "No locking after fetch should be possible");
					}

					if constexpr (!SupportsStreamedRequests)
					{
						// Ensure we trigger the progress callback at least once
						if (requestInfo.m_progressCallback.IsValid())
						{
							requestInfo.m_progressCallback(requestInfo, requestInfo.m_responseBody);
						}
					}
					requestInfo.m_onFinishedCallback(
						requestInfo,
						Worker::ResponseData{
							requestInfo.m_responseHeaders,
							requestInfo.m_responseBody,
							requestInfo.m_responseCode,
							requestInfo.m_contentLength
						}
					);
				}
			},
			priority,
			"HTTP Fetch Fail"
		);
	};

	void OnFetchFail(emscripten_fetch_t* fetch)
	{
		Assert(!emscripten_is_main_browser_thread());
		Assert(fetch->userData != nullptr);
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(fetch->userData);
		Worker& worker = *requestInfo.m_pWorker;

		Assert(requestInfo.m_asyncEventCounter.Load() > 0);

		UniquePtr<Worker::RequestInfo> pRequestInfo = worker.RemoveRequest(requestInfo.m_identifier);
		Assert(pRequestInfo.IsValid());

		String responseHeaders(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)emscripten_fetch_get_response_headers_length(fetch));
		emscripten_fetch_get_response_headers(fetch, responseHeaders.GetData(), responseHeaders.GetSize());
		pRequestInfo->m_responseHeaders = Move(responseHeaders);
		pRequestInfo->m_responseCode = ResponseCode{fetch->status};
		pRequestInfo->m_contentLength = fetch->numBytes;
		emscripten_fetch_close(fetch);

		FetchArgumentsUnion arguments;
		arguments.pData = new FetchData{Move(pRequestInfo)};
		emscripten_wasm_worker_post_function_vii(EMSCRIPTEN_WASM_WORKER_ID_PARENT, OnFetchFailOnParentWorker, arguments.arg1, arguments.arg2);
	}

	struct QueueFetchData
	{
		emscripten_fetch_attr_t attr;
		Worker& worker;
		Worker::RequestInfo& requestInfo;
		emscripten_fetch_t* pHandle{nullptr};
	};

	union QueueFetchArgumentsUnion
	{
		QueueFetchData* pData;
		struct
		{
			int arg1;
			int arg2;
		};
	};

	void QueueFetchOnOnWorker(const int arg1, const int arg2)
	{
		QueueFetchArgumentsUnion arguments;
		arguments.arg1 = arg1;
		arguments.arg2 = arg2;

		QueueFetchData& data = *arguments.pData;

		emscripten_fetch_t* pHandle = emscripten_fetch(&data.attr, data.requestInfo.m_url.GetZeroTerminated());
		Assert(pHandle != nullptr);
		data.pHandle = pHandle;

		delete arguments.pData;
	};
#endif

#if USE_LIBCURL
	struct PrivateData
	{
		PrivateData()
		{
		}
		~PrivateData()
		{
		}

		union
		{
			Worker::RequestIdentifier requestIdentifier;
			void* pPointer;
		};
	};
#endif

	void Worker::ProcessQueuedRequests()
	{
		Threading::UniqueLock lock(m_queuedRequestsMutex);
		while (m_queuedRequests.HasElements())
		{
			RequestIdentifier requestIdentifier = m_requestIdentifiers.AcquireIdentifier();
			if (requestIdentifier.IsInvalid())
			{
				break;
			}

			UniquePtr<RequestInfo>& queuedRequest = m_requests[requestIdentifier] = m_queuedRequests.PopAndGetBack();
			queuedRequest->m_identifier = requestIdentifier;

#if USE_LIBCURL
			PrivateData privateData;
			privateData.requestIdentifier = requestIdentifier;
			curl_easy_setopt(queuedRequest->m_pHandle, CURLOPT_PRIVATE, privateData.pPointer);
			curl_multi_add_handle(m_pMultiHandle, queuedRequest->m_pHandle);

#elif PLATFORM_EMSCRIPTEN
			emscripten_fetch_attr_t attr;
			emscripten_fetch_attr_init(&attr);
			strcpy(attr.requestMethod, GetRequestTypeString(queuedRequest->m_requestType));
			attr.attributes = EMSCRIPTEN_FETCH_LOAD_TO_MEMORY | (EMSCRIPTEN_FETCH_STREAM_DATA * SupportsStreamedRequests) |
			                  EMSCRIPTEN_FETCH_PERSIST_FILE;
			const bool isHighPriority = queuedRequest->m_priority <= Threading::JobPriority::HighPriorityBackendNetworking;
			if (isHighPriority)
			{
				attr.attributes |= 256; // EMSCRIPTEN_FETCH_HIGH_PRIORITY;
			}
			else
			{
				attr.attributes |= 512; // EMSCRIPTEN_FETCH_LOW_PRIORITY;
			}

			attr.onreadystatechange = OnFetchReadyStateChanged;
			attr.onsuccess = OnFetchSuccess;
			attr.onprogress = OnFetchProgress;
			attr.onerror = OnFetchFail;
			attr.userData = queuedRequest.Get();
			Assert(attr.userData != nullptr);

			switch (queuedRequest->m_requestType)
			{
				case RequestType::Post:
				{
					if (const Optional<const RequestBody*> pRequestBody = queuedRequest->m_requestData.Get<RequestBody>())
					{
						attr.requestData = pRequestBody->GetData();
						attr.requestDataSize = pRequestBody->GetDataSize();
					}
					else if (const Optional<const FormData*> pFormData = queuedRequest->m_requestData.Get<FormData>())
					{
						queuedRequest->m_requestHeaders.EmplaceBack("Transfer-Encoding");
						queuedRequest->m_requestHeaders.EmplaceBack("chunked");

						queuedRequest->m_requestHeaders.EmplaceBack("Content-Type");
						queuedRequest->m_requestHeaders.EmplaceBack("multipart/x-form-data");

						for (const FormData::Part& __restrict part : pFormData->m_parts)
						{
							queuedRequest->m_requestHeaders.EmplaceBack(part.m_name);
							queuedRequest->m_requestHeaders.EmplaceBack(part.m_fileName);

							if (const Optional<const HTTP::FormDataPart::StoredData*> storedData = part.m_data.Get<HTTP::FormDataPart::StoredData>())
							{
								queuedRequest->m_requestHeaders.EmplaceBack(String().Format("{}", storedData->GetDataSize()));
								queuedRequest->m_requestHeaders.EmplaceBack(
									ConstStringView{reinterpret_cast<const char*>(storedData->GetData()), (uint32)storedData->GetDataSize()}
								);
							}
							else
							{
								const ConstByteView data = part.m_data.GetExpected<ConstByteView>();
								queuedRequest->m_requestHeaders.EmplaceBack(String().Format("{}", data.GetDataSize()));
								queuedRequest->m_requestHeaders.EmplaceBack(
									ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)data.GetDataSize()}
								);
							}
						}
					}

					/*if(keyStr === "Content-Type" && valueStr === "multipart/x-form-data") {
					  assert(requestHeaders);
					  var partName = GROWABLE_HEAP_U32()[((requestHeaders) >>> 2) >>> 0];
					  assert(partName);
					  var partFileName = GROWABLE_HEAP_U32()[(((requestHeaders) + (4)) >>> 2) >>> 0];
					  assert(partFileName);
					  var partData = GROWABLE_HEAP_U32()[(((requestHeaders) + (8)) >>> 2) >>> 0];
					  assert(partData);

					  var partNameStr = UTF8ToString(partName);
					  var partFileNameStr = UTF8ToString(partFileName);
					  console.log("found part name " + partNameStr);
					  console.log("found part file name " + partFileNameStr);

					  if(formData === null) {
					      formData = new FormData();
					  }

					  var partSlice = GROWABLE_HEAP_U8().slice(partData, partData + size);

					  if(partFileNameStr.length > 0) {
					      formData.append(partNameStr, new Blob([partSlice]), partFileNameStr);
					  }
					  else {
					      formData.append(partNameStr, new Blob([partSlice]));
					  }

					  requestHeaders += 12;
					 }
					  else {
					  headers.append(keyStr, valueStr);
					  }*/
				}
				break;
				default:
				{
					if (const Optional<const RequestBody*> pRequestBody = queuedRequest->m_requestData.Get<RequestBody>())
					{
						if (pRequestBody->HasElements())
						{
							attr.requestData = pRequestBody->GetData();
							attr.requestDataSize = pRequestBody->GetDataSize();

							queuedRequest->m_requestHeaders.EmplaceBack("Content-Length");
							String contentLengthHeader;
							contentLengthHeader.Format("{}", pRequestBody->GetDataSize());
							queuedRequest->m_requestHeaders.EmplaceBack(Move(contentLengthHeader));
						}
					}
				}
				break;
			}

			if (queuedRequest->m_downloadRange.GetSize() > 0 && queuedRequest->m_downloadRange != Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1))
			{
				queuedRequest->m_requestHeaders.EmplaceBack("Range");

				String rangeString;
				rangeString.Format("bytes={}-{}", queuedRequest->m_downloadRange.GetMinimum(), queuedRequest->m_downloadRange.GetMaximum());
				queuedRequest->m_requestHeaders.EmplaceBack(Move(rangeString));
			}

			for (String& header : queuedRequest->m_requestHeaders)
			{
				queuedRequest->m_requestHeadersViews.EmplaceBack(header.GetZeroTerminated());
			}
			queuedRequest->m_requestHeadersViews.EmplaceBack(nullptr);

			attr.requestHeaders = queuedRequest->m_requestHeadersViews.GetData();

			QueueFetchData* pData = new QueueFetchData{Move(attr), *this, *queuedRequest};
			em_proxying_queue* queue = emscripten_proxy_get_system_queue();
			pthread_t target = emscripten_main_runtime_thread_id();
			[[maybe_unused]] const bool queued = emscripten_proxy_async(
																						 queue,
																						 target,
																						 [](void* pUserData)
																						 {
																							 QueueFetchData& data = *reinterpret_cast<QueueFetchData*>(pUserData);

																							 QueueFetchArgumentsUnion arguments;
																							 arguments.pData = &data;
																							 emscripten_wasm_worker_post_function_vii(
																								 data.requestInfo.m_pWorker->m_webWorker.GetIdentifier(),
																								 QueueFetchOnOnWorker,
																								 arguments.arg1,
																								 arguments.arg2
																							 );
																						 },
																						 pData
																					 ) == 1;
			Assert(queued);
#endif
		}
	}

	Worker::Result Worker::OnExecute(Threading::JobRunnerThread& thread)
	{
		{
			Status expected = Status::Scheduled;
			[[maybe_unused]] const bool wasExchanged = m_status.CompareExchangeStrong(expected, Status::Update);
			Assert(wasExchanged);
		}

		m_scheduledTimerHandle = {};
		ProcessQueuedRequests();

#if USE_LIBCURL
		int runningHandleCount = 0;
		curl_multi_perform(m_pMultiHandle, &runningHandleCount);
		m_hasOngoingRequests = runningHandleCount > 0;

		int remainingMessageCount = 0;
		for (CURLMsg* pMessage = curl_multi_info_read(m_pMultiHandle, &remainingMessageCount); pMessage != nullptr;
		     pMessage = curl_multi_info_read(m_pMultiHandle, &remainingMessageCount))
		{
			switch (pMessage->msg)
			{
				case CURLMSG_DONE:
				{
					UniquePtr<RequestInfo> pRequestInfo = [this, pMessage]()
					{
						PrivateData privateData;
						curl_easy_getinfo(pMessage->easy_handle, CURLINFO_PRIVATE, &privateData.pPointer);

						return RemoveRequest(privateData.requestIdentifier);
					}();

					ResponseData responseData{pRequestInfo->m_responseHeaders, pRequestInfo->m_responseBody};
					curl_easy_getinfo(pMessage->easy_handle, CURLINFO_RESPONSE_CODE, &responseData.m_responseCode);
					curl_easy_getinfo(pMessage->easy_handle, CURLINFO_CONTENT_LENGTH_DOWNLOAD_T, &responseData.m_contentLength);

					curl_multi_remove_handle(m_pMultiHandle, pMessage->easy_handle);
					curl_easy_cleanup(pMessage->easy_handle);

					const Threading::JobPriority priority = pRequestInfo->m_priority;
					thread.QueueCallbackFromThread(
						priority,
						[responseData = Move(responseData), pRequestInfo = Move(pRequestInfo)](Threading::JobRunnerThread&)
						{
							pRequestInfo->m_onFinishedCallback(*pRequestInfo, responseData);

							if (pRequestInfo->m_pHeaders != nullptr)
							{
								curl_slist_free_all(pRequestInfo->m_pHeaders);
							}
						},
						"Finish HTTP request"
					);
				}
				break;
				default:
					break;
			}
		}
#else
		UNUSED(thread);
#endif

		{
			Status expected = Status::Update;
			[[maybe_unused]] const bool wasExchanged = m_status.CompareExchangeStrong(expected, Status::AwaitExternalFinish);
			Assert(wasExchanged);
		}
		return Result::AwaitExternalFinish;
	}

#if USE_LIBCURL
	size WriteHeaderCallback(const void* pSource, size dataSize, size memberSize, void* pUserData)
	{
		Worker::RequestInfo& requestInfo = *reinterpret_cast<Worker::RequestInfo*>(pUserData);
		const size receivedSize = dataSize * memberSize;
		const size totalSize = receivedSize;
		requestInfo.m_responseHeaders += ConstStringView(reinterpret_cast<const char*>(pSource), (uint32)totalSize);
		/*if (LIKELY(totalSize > 0))
		{
		  requestInfo.m_progressCallback(requestInfo.m_responseBody);
		}*/
		return totalSize;
	}

	size WriteResponseBodyCallback(const void* pSource, size dataSize, size memberSize, void* pUserData)
	{
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(pUserData);
		const size receivedSize = dataSize * memberSize;
		const size totalSize = Math::Min(receivedSize, requestInfo.m_downloadRange.GetSize() - requestInfo.m_responseBody.GetDataSize());
		Assert(requestInfo.m_responseBody.GetDataSize() + totalSize <= requestInfo.m_downloadRange.GetSize());
		requestInfo.m_responseBody += ConstStringView(reinterpret_cast<const char*>(pSource), (uint32)totalSize);
		Assert(requestInfo.m_responseBody.GetDataSize() <= requestInfo.m_downloadRange.GetSize());
		if (LIKELY(totalSize > 0) && requestInfo.m_progressCallback.IsValid())
		{
			requestInfo.m_progressCallback(requestInfo, requestInfo.m_responseBody);
		}
		return totalSize;
	}

	size ReadRequestBodyCallback(char* pDestination, size_t dataSize, size_t memberSize, void* pUserData)
	{
		const size totalSize = dataSize * memberSize;
		Worker::RequestInfo& __restrict requestInfo = *reinterpret_cast<Worker::RequestInfo*>(pUserData);
		if (const Optional<RequestBody*> pRequestBody = requestInfo.m_requestData.Get<RequestBody>())
		{
			const size copiedBytes = Math::Min(totalSize, (size)pRequestBody->GetDataSize());
			Memory::CopyWithoutOverlap(pDestination, pRequestBody->GetData(), copiedBytes);
			if (copiedBytes > 0)
			{
				pRequestBody->Remove(pRequestBody->GetView().GetSubstring(0, (uint32)(copiedBytes / sizeof(char))));
			}
			return copiedBytes;
		}
		return 0;
	}

	struct MimeBufferInfo
	{
		ConstByteView m_data;
		size m_position = 0;
	};
	using MimePartData = Variant<IO::File, MimeBufferInfo>;

	size_t ReadMimePartCallback(char* const pTargetBuffer, const size_t dataSize, const size_t memberSize, void* const pUserData)
	{
		MimePartData& __restrict partData = *reinterpret_cast<MimePartData*>(pUserData);

		if (const Optional<IO::File*> pFile = partData.Get<IO::File>())
		{
			const size readSize = Math::Min(dataSize * memberSize, size(pFile->GetSize() - pFile->Tell()));
			ArrayView<char, size> targetView{pTargetBuffer, readSize};
			if (pFile->ReadIntoView(targetView))
			{
				return targetView.GetDataSize();
			}
			else
			{
				return 0;
			}
		}
		else
		{
			MimeBufferInfo& bufferInfo = partData.GetExpected<MimeBufferInfo>();
			const size readSize = Math::Min(dataSize * memberSize, bufferInfo.m_data.GetDataSize() - bufferInfo.m_position);
			ByteView target{pTargetBuffer, readSize};
			ConstByteView source = bufferInfo.m_data.GetSubView((size)0, readSize);
			if (target.CopyFrom(source))
			{
				bufferInfo.m_position += readSize;
				return readSize;
			}
			else
			{
				return 0;
			}
		}
	}

	int SeekMimePartCallback(void* const pUserData, const curl_off_t offset, const int origin)
	{
		MimePartData& __restrict partData = *reinterpret_cast<MimePartData*>(pUserData);
		if (const Optional<IO::File*> pFile = partData.Get<IO::File>())
		{
			if (pFile->Seek((long)offset, static_cast<IO::SeekOrigin>(origin)))
			{
				return CURL_SEEKFUNC_OK;
			}
		}
		else
		{
			MimeBufferInfo& bufferInfo = partData.GetExpected<MimeBufferInfo>();
			switch (origin)
			{
				case SEEK_END:
				{
					if (offset > curl_off_t(bufferInfo.m_data.GetDataSize()) || offset < 0)
					{
						return CURL_SEEKFUNC_FAIL;
					}

					bufferInfo.m_position = size(bufferInfo.m_data.GetDataSize() - offset);
					return CURL_SEEKFUNC_OK;
				}
				case SEEK_CUR:
				{
					if (offset > curl_off_t(bufferInfo.m_data.GetDataSize() - bufferInfo.m_position) || -offset > curl_off_t(bufferInfo.m_position))
					{
						return CURL_SEEKFUNC_FAIL;
					}
					bufferInfo.m_position += offset;
					return CURL_SEEKFUNC_OK;
				}
				case SEEK_SET:
				{
					if (offset < 0 || offset > curl_off_t(bufferInfo.m_data.GetDataSize()))
					{
						return CURL_SEEKFUNC_FAIL;
					}
					bufferInfo.m_position = (size)offset;
					return CURL_SEEKFUNC_OK;
				}
				default:
					return CURL_SEEKFUNC_FAIL;
			}
		}

		return CURL_SEEKFUNC_FAIL;
	}

	void FreeMimePartCallback(void* const pUserData)
	{
		MimePartData* __restrict pPartData = reinterpret_cast<MimePartData*>(pUserData);
		delete pPartData;
	}

	int OnProgressCallback(
		[[maybe_unused]] void* pUserData,
		[[maybe_unused]] const curl_off_t totalDownloadSize,
		[[maybe_unused]] const curl_off_t currentDownloadSize,
		[[maybe_unused]] const curl_off_t totalUploadSize,
		[[maybe_unused]] const curl_off_t currentUploadSize
	)
	{
		return CURL_PROGRESSFUNC_CONTINUE;
	}

#if DEBUG_BUILD
	int DebugCallback(
		[[maybe_unused]] CURL* handle,
		[[maybe_unused]] const curl_infotype type,
		[[maybe_unused]] const char* data,
		[[maybe_unused]] const size_t size,
		[[maybe_unused]] void* const userData
	)
	{
		return 0;
	}
#endif
#endif

	void Worker::QueueRequest(Request&& request, const Threading::JobPriority priority)
	{
#if HTTP_DEBUG || PLATFORM_EMSCRIPTEN
		Vector<String> requestInfoHeaders;
		requestInfoHeaders.Reserve(request.m_headers.GetSize() * 3);

		for (const Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> headerPair : request.m_headers)
		{
			requestInfoHeaders.EmplaceBack(headerPair.key);
			if (headerPair.value.HasElements())
			{
				requestInfoHeaders.EmplaceBack(headerPair.value);
			}
		}

		UniquePtr<RequestInfo> pRequestInfo = UniquePtr<RequestInfo>::Make();
		pRequestInfo->m_url = IO::URI(request.m_url);
		pRequestInfo->m_priority = priority;
		pRequestInfo->m_onFinishedCallback = Move(request.m_finishedCallback);
		pRequestInfo->m_progressCallback = Move(request.m_progressCallback);
		pRequestInfo->m_requestType = request.m_requestType;
		pRequestInfo->m_requestData = Move(request.m_data);
		pRequestInfo->m_requestHeaders = Move(requestInfoHeaders);
		pRequestInfo->m_downloadRange = request.m_downloadRange;

#if USE_EMSCRIPTEN_FETCH
		pRequestInfo->m_pWorker = this;
#endif

#else
		UniquePtr<RequestInfo> pRequestInfo = UniquePtr<RequestInfo>::Make(RequestInfo{
			IO::URI(request.m_url),
			priority,
			Move(request.m_finishedCallback),
			Move(request.m_progressCallback),
			request.m_requestType,
			Move(request.m_data),
			"",
			String(),
			String(),
			request.m_downloadRange,
			0
		});
#endif

#if USE_LIBCURL
		CURL* pRequest = curl_easy_init();
		pRequestInfo->m_pHandle = pRequest;

		curl_easy_setopt(pRequest, CURLOPT_URL, pRequestInfo->m_url.GetZeroTerminated().GetData());

		// Don't request encoded data when requesting a range
		// Theoretically this should be fine but Azure has been returning bad headers in this case
		if (request.m_downloadRange.GetSize() == 0 || request.m_downloadRange == Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1))
		{
			curl_easy_setopt(pRequest, CURLOPT_ACCEPT_ENCODING, "");
			curl_easy_setopt(pRequest, CURLOPT_TRANSFER_ENCODING, 1);
		}

		for (const Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> headerPair : request.m_headers)
		{
			String headerString;
			headerString.Format("{}: {}", headerPair.key, headerPair.value);
			pRequestInfo->m_pHeaders = curl_slist_append(pRequestInfo->m_pHeaders, headerString.GetZeroTerminated());
		}

		String userAgent;
		if (const Optional<const Engine*> pEngine = System::Find<Engine>())
		{
			userAgent = String().Format("sceneri-agent/{}", pEngine->GetInfo().GetVersion().ToString());
		}
		else
		{
			userAgent = "sceneri-agent/0.3";
		}
		curl_easy_setopt(pRequest, CURLOPT_USERAGENT, userAgent.GetZeroTerminated().GetData());
		curl_easy_setopt(pRequest, CURLOPT_HTTP_VERSION, CURL_HTTP_VERSION_2_0);

		curl_easy_setopt(pRequest, CURLOPT_WRITEFUNCTION, WriteResponseBodyCallback);
		curl_easy_setopt(pRequest, CURLOPT_WRITEDATA, (RequestInfo*)pRequestInfo.Get());

		curl_easy_setopt(pRequest, CURLOPT_HEADERFUNCTION, WriteHeaderCallback);
		curl_easy_setopt(pRequest, CURLOPT_HEADERDATA, (RequestInfo*)pRequestInfo.Get());

		curl_easy_setopt(pRequest, CURLOPT_NOPROGRESS, pRequestInfo->m_progressCallback.IsValid() ? 0L : 1L);

#if DEBUG_BUILD
		curl_easy_setopt(pRequest, CURLOPT_VERBOSE, 1L);
		curl_easy_setopt(pRequest, CURLOPT_DEBUGFUNCTION, DebugCallback);
#endif

		switch (request.m_requestType)
		{
			case RequestType::Post:
			{
				if (const Optional<const RequestBody*> pRequestBody = pRequestInfo->m_requestData.Get<RequestBody>())
				{
					curl_easy_setopt(pRequest, CURLOPT_POSTFIELDS, pRequestBody->GetData());
					curl_easy_setopt(pRequest, CURLOPT_POSTFIELDSIZE, pRequestBody->GetDataSize());
					curl_easy_setopt(pRequest, CURLOPT_POST, 1);
				}
				else if (const Optional<const FormData*> pFormData = pRequestInfo->m_requestData.Get<FormData>())
				{
					curl_mime* pMultiPart = curl_mime_init(pRequest);

					for (const FormData::Part& __restrict part : pFormData->m_parts)
					{
						curl_mimepart* pPart = curl_mime_addpart(pMultiPart);
						curl_mime_name(pPart, part.m_name.GetZeroTerminated());

						if (part.m_fileName.HasElements())
						{
							curl_mime_filename(pPart, part.m_fileName.GetZeroTerminated());
						}

						curl_mime_type(pPart, part.m_contentType.GetZeroTerminated());

						if (const Optional<const HTTP::FormDataPart::StoredData*> storedData = part.m_data.Get<HTTP::FormDataPart::StoredData>())
						{
							/*MimePartData* pData = new MimePartData{IO::File(IO::Path(*pFilePath), IO::AccessModeFlags::ReadBinary,
							IO::SharingFlags::DisallowWrite)}; const size fileSize = pData->GetExpected<IO::File>().GetSize(); curl_mime_data_cb(pPart,
							fileSize, ReadMimePartCallback, SeekMimePartCallback, FreeMimePartCallback, pData);*/

							curl_mime_data(pPart, reinterpret_cast<const char*>(storedData->GetData()), storedData->GetDataSize());
						}
						else
						{
							const ConstByteView data = part.m_data.GetExpected<ConstByteView>();
							curl_mime_data(pPart, reinterpret_cast<const char*>(data.GetData()), data.GetDataSize());
						}
					}

					curl_easy_setopt(pRequest, CURLOPT_MIMEPOST, pMultiPart);
				}
				else
				{
					curl_easy_setopt(pRequest, CURLOPT_POST, 1);
				}
			}
			break;
			default:
			{
				if (const Optional<const RequestBody*> pRequestBody = pRequestInfo->m_requestData.Get<RequestBody>())
				{
					if (pRequestBody->HasElements())
					{
						pRequestInfo->m_pHeaders = curl_slist_append(
							pRequestInfo->m_pHeaders,
							String().Format("Content-Length: {}", pRequestBody->GetDataSize()).GetZeroTerminated()
						);

						curl_easy_setopt(pRequest, CURLOPT_INFILESIZE, pRequestBody->GetDataSize());
						curl_easy_setopt(pRequest, CURLOPT_READFUNCTION, ReadRequestBodyCallback);
						curl_easy_setopt(pRequest, CURLOPT_UPLOAD, 1);
						curl_easy_setopt(pRequest, CURLOPT_READDATA, (RequestInfo*)pRequestInfo.Get());
					}
				}
			}
			break;
		}

		curl_easy_setopt(pRequest, CURLOPT_XFERINFOFUNCTION, OnProgressCallback);
		curl_easy_setopt(pRequest, CURLOPT_XFERINFODATA, (RequestInfo*)pRequestInfo.Get());

		curl_easy_setopt(pRequest, CURLOPT_HTTPHEADER, pRequestInfo->m_pHeaders);

		curl_easy_setopt(pRequest, CURLOPT_SSLCERTTYPE, "PEM");
		curl_easy_setopt(pRequest, CURLOPT_CAINFO, m_caCertificateFilePath.GetData());
		curl_easy_setopt(pRequest, CURLOPT_SSL_VERIFYPEER, 1L);

		if (request.m_downloadRange.GetSize() > 0 && request.m_downloadRange != Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1))
		{
			String range;
			range.Format("{}-{}", request.m_downloadRange.GetMinimum(), request.m_downloadRange.GetMaximum());
			curl_easy_setopt(pRequest, CURLOPT_RANGE, range.GetZeroTerminated().GetData());
		}
		curl_easy_setopt(pRequest, CURLOPT_NOBODY, request.m_flags.IsSet(Request::Flags::SkipBody));
		curl_easy_setopt(pRequest, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(pRequest, CURLOPT_POSTREDIR, CURL_REDIR_POST_ALL);

		switch (request.m_requestType)
		{
			case RequestType::Headers:
				curl_easy_setopt(pRequest, CURLOPT_NOBODY, 1);
				break;
			case RequestType::Get:
				break;
			case RequestType::Post:
				break;
			case RequestType::Put:
				curl_easy_setopt(pRequest, CURLOPT_PUT, 1);
				break;
			case RequestType::Patch:
				curl_easy_setopt(pRequest, CURLOPT_CUSTOMREQUEST, "PATCH");
				break;
			case RequestType::Delete:
				curl_easy_setopt(pRequest, CURLOPT_CUSTOMREQUEST, "DELETE");
				break;
		}
#endif

		{
			Threading::UniqueLock lock(m_queuedRequestsMutex);
			QueuedRequest* __restrict pUpperBoundIt = std::upper_bound(
				m_queuedRequests.begin().Get(),
				m_queuedRequests.end().Get(),
				priority,
				[](const Threading::JobPriority priority, const QueuedRequest& existingRequest) -> bool
				{
					return priority > existingRequest->m_priority;
				}
			);
			m_queuedRequests.Emplace(pUpperBoundIt, Memory::Uninitialized, Move(pRequestInfo));
		}

		{
			Status expected = Status::None;
			if (m_status.CompareExchangeStrong(expected, Status::Scheduled))
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					TryQueue(*pThread);
				}
				else
				{
					TryQueue(System::Get<Threading::JobManager>());
				}
			}
		}
	}

	void Worker::QueryDownloadSize(
		const IO::URI::ConstZeroTerminatedStringView url, const Threading::JobPriority priority, Function<void(size), 24>&& callback
	)
	{
		QueueRequest(
			Worker::Request{
				RequestType::Get,
				url,
				{},
				{},
				[callback = Forward<decltype(callback)>(callback)](const Worker::RequestInfo&, const Worker::ResponseData& responseData)
				{
					callback((size)responseData.m_contentLength);
				},
				[](const Worker::RequestInfo&, ConstStringView)
				{
				},
				Math::Range<size>::Make(0, 0),
				Worker::Request::Flags::SkipBody
			},
			priority
		);
	}

	UniquePtr<Worker> Plugin::CreateWorker(const Threading::JobPriority priority, const Asset::Manager& assetManager) const
	{
		return UniquePtr<Worker>::Make(priority, assetManager);
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Networking::HTTP::Plugin>();
#else
extern "C" HTTP_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Networking::HTTP::Plugin(application);
}
#endif
