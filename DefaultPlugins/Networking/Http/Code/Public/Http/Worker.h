#pragma once

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/TimerHandle.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Function/Function.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Pair.h>
#include <Common/Memory/CountBits.h>
#include <Common/IO/URI.h>
#include <Common/Platform/DllExport.h>
#include <Common/Math/Range.h>
#include <Common/EnumFlags.h>
#include <Http/RequestType.h>

#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/SaltedIdentifierStorage.h>

#include "ResponseCode.h"
#include "RequestData.h"

#if PLATFORM_EMSCRIPTEN
#define USE_EMSCRIPTEN_FETCH 1
#include <Common/Threading/WebWorker.h>
#elif USE_LIBCURL
struct curl_slist;
#endif

namespace ngine::Threading
{
	struct JobRunnerThread;
}

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Networking::HTTP
{
	struct Worker : protected Threading::Job
	{
		inline static constexpr uint32 MaximumSimultaneousRequestCount = 20;
		using RequestIdentifier = TIdentifier<uint32, Memory::GetBitWidth(MaximumSimultaneousRequestCount), MaximumSimultaneousRequestCount>;

		Worker(const Threading::JobPriority priority, const Asset::Manager& assetManager);
		virtual ~Worker();

		// Threading::Job
		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override;
		virtual Result OnExecute(Threading::JobRunnerThread&) override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "HTTP Worker";
		}
#endif
		// ~Threading::Job

		struct ResponseData
		{
			[[nodiscard]] ConstStringView FindHeader(const ConstStringView name) const
			{
				ConstStringView searchedHeaders = m_responseHeaders;
				while (searchedHeaders.HasElements())
				{
					if (searchedHeaders.GetSubstringUpTo(name.GetSize()) == name)
					{
						searchedHeaders += name.GetSize() + 2;

						const uint32 carriageReturnPosition = searchedHeaders.FindFirstOf('\r');
						const uint32 lineFeedPosition = searchedHeaders.FindFirstOf('\n');
						if (carriageReturnPosition != ConstStringView::InvalidPosition)
						{
							return searchedHeaders.GetSubstringUpTo(carriageReturnPosition);
						}
						else if (lineFeedPosition != ConstStringView::InvalidPosition)
						{
							return searchedHeaders.GetSubstringUpTo(lineFeedPosition);
						}
						else
						{
							// Reached end of headers, return all of it
							return searchedHeaders;
						}
					}
					else
					{
						const uint32 endPosition = searchedHeaders.FindFirstOf('\n');
						if (endPosition != ConstStringView::InvalidPosition)
						{
							searchedHeaders = searchedHeaders.GetSubstringFrom(endPosition + 1);
						}
						else
						{
							return {};
						}
					}
				}
				return {};
			}

			[[nodiscard]] bool HasHeader(const ConstStringView name) const
			{
				return FindHeader(name).HasElements();
			}

			ConstStringView m_responseHeaders;
			ConstStringView m_responseBody;
			ResponseCode m_responseCode;
			int64 m_contentLength;
		};

		struct RequestInfo;
		using ProgressCallback = Function<void(const RequestInfo& requestInfo, String& responseBody), 64>;
		using FinishedCallback = Function<void(const RequestInfo& requestInfo, const ResponseData& responseData), 48>;

		struct Request
		{
			RequestType m_requestType;
			IO::URI::ConstZeroTerminatedStringView m_url;
			ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> m_headers;
			RequestData m_data;
			FinishedCallback m_finishedCallback = [](const RequestInfo&, const ResponseData&)
			{
			};
			ProgressCallback m_progressCallback;
			Math::Range<size> m_downloadRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1);

			enum class Flags : uint8
			{
				SkipBody = 1 << 0
			};
			EnumFlags<Flags> m_flags;
		};

		HTTP_EXPORT_API virtual void QueueRequest(Request&& request, const Threading::JobPriority priority);
		HTTP_EXPORT_API virtual void QueryDownloadSize(
			const IO::URI::ConstZeroTerminatedStringView url, const Threading::JobPriority priority, Function<void(size), 24>&& callback
		);

		struct RequestInfo
		{
			IO::URI m_url;
			Threading::JobPriority m_priority;
			FinishedCallback m_onFinishedCallback;
			ProgressCallback m_progressCallback;
			RequestType m_requestType;
			RequestData m_requestData;
			Vector<String> m_requestHeaders;
			String m_responseHeaders;
			String m_responseBody;
			Math::Range<size> m_downloadRange = Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1);
			long long m_downloadProgress = 0;

#if USE_EMSCRIPTEN_FETCH
			Optional<Worker*> m_pWorker;
			Vector<const char*> m_requestHeadersViews;

			Threading::Mutex m_requestBodyMutex;

			//! Counter for number of ongoing events, including the request itself
			Threading::Atomic<uint32> m_asyncEventCounter{1};
			UniquePtr<RequestInfo> pThis;

			ResponseCode m_responseCode;
			int64 m_contentLength;
#elif USE_LIBCURL
			curl_slist* m_pHeaders{nullptr};
			void* m_pHandle{nullptr};
#endif

			RequestIdentifier m_identifier;
		};

#if USE_EMSCRIPTEN_FETCH
		static void QueueFetchOnParentWorker(const int arg1, const int arg2);
#endif
		UniquePtr<RequestInfo> RemoveRequest(const RequestIdentifier requestIdentifier);
	protected:
		void ProcessQueuedRequests();
	protected:
#if USE_LIBCURL
		void* m_pMultiHandle = nullptr;
#elif PLATFORM_EMSCRIPTEN
		Threading::WebWorker m_webWorker;
#endif
		bool m_hasOngoingRequests = false;

		Threading::SharedMutex m_queuedRequestsMutex;

		using QueuedRequest = UniquePtr<RequestInfo>;
		Vector<QueuedRequest> m_queuedRequests;
		TSaltedIdentifierStorage<RequestIdentifier> m_requestIdentifiers;
		TIdentifierArray<UniquePtr<RequestInfo>, RequestIdentifier> m_requests;

#if USE_LIBCURL
		String m_caCertificateFilePath;
#endif

		Threading::TimerHandle m_scheduledTimerHandle;

		enum class Status : uint8
		{
			None,
			Scheduled,
			Update,
			AwaitExternalFinish,
		};
		Threading::Atomic<Status> m_status{Status::None};
	};
}
