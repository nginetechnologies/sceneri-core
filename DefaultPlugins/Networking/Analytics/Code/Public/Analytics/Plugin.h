#pragma once

#include <Http/RequestType.h>

#include <Common/Plugin/Plugin.h>
#include <Common/Function/Function.h>
#include <Common/Memory/Tuple.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Platform/Environment.h>

#if PLATFORM_APPLE
@protocol SentrySpan;
#else
struct sentry_transaction_context_s;
typedef struct sentry_transaction_context_s sentry_transaction_context_t;
struct sentry_transaction_s;
typedef struct sentry_transaction_s sentry_transaction_t;
struct sentry_span_s;
typedef struct sentry_span_s sentry_span_t;
#endif

namespace ngine::Threading
{
	struct IntermediateStage;
}

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Networking::HTTP
{
	struct Plugin;
}

namespace ngine::Networking::Backend
{
	struct Plugin;
}

namespace ngine
{
	struct ProjectInfo;
}

namespace ngine::Networking::Analytics
{
	enum class TransactionStatus : uint8
	{
		Success,
		DeadlineExceeded,
		Unauthenticated,
		PermissionDenied,
		NotFound,
		ResourceExhausted,
		InvalidArgument,
		Unavailable,
		CriticalError,
		UnknownError,
		Cancelled,
		AlreadyExists,
		Aborted,
		OutOfRange
	};

	struct Transaction
	{
		Transaction() = default;
#if PLATFORM_APPLE
		Transaction(id<SentrySpan> transaction);
#else
		Transaction(sentry_transaction_context_t* pContext, sentry_transaction_t* pTransaction);
#endif
		Transaction(Transaction&&);
		Transaction& operator=(Transaction&&);
		Transaction(const Transaction&) = delete;
		Transaction& operator=(const Transaction&) = delete;
		~Transaction();

		void Finish(const TransactionStatus status);
	protected:
#if PLATFORM_APPLE
		id<SentrySpan> m_transaction;
#else
		sentry_transaction_context_t* m_pContext{nullptr};
		sentry_transaction_t* m_pTransaction{nullptr};
#endif
	};

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "50472FD7-1240-418B-B988-4BAD607632B6"_guid;

		enum class ProjectAction : uint8
		{
			Created,
			Loaded,
		};

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		virtual void OnUnloaded(Application& application) override;
		// ~IPlugin

		using PropertiesCallback = Function<bool(Serialization::Writer writer), 24>;
		void SendEvent(const ConstStringView eventName, PropertiesCallback&& propertiesCallback = {});

		void SetReceivedShareSessionGuid(const ngine::Guid guid)
		{
			m_receivedShareSessionGuid = guid;
		}

		[[nodiscard]] Transaction StartTransaction(const ConstStringView transactionName, const ConstStringView operation);

		void OnReceivedDeepLink(const ConstStringView link);
	protected:
		void Disable();
		void OnBackendSessionStartResponseReceived(const bool success);
		void OnWindowCreated(Rendering::Window& window);
		void StartSession(const bool succeededBackendSignin);
		void SendEvent(Serialization::Writer, const Optional<Threading::IntermediateStage*> pSentStage = {});
	protected:
		void SendEventInternal(Serialization::Writer, const Optional<Threading::IntermediateStage*> pSentStage = {});

		void InitializeErrorTracking();
	protected:
		HTTP::Plugin* m_pHttpPlugin = nullptr;
		Backend::Plugin* m_pBackendPlugin = nullptr;
		Optional<Rendering::Window*> m_pCurrentWindow = nullptr;

		// We need to store any event data before a valid session is established
		struct EventEntry
		{
			Serialization::Data data;
			Optional<Threading::IntermediateStage*> pSentStage;
		};
		Vector<EventEntry> m_eventBuffer;
		Threading::Mutex m_eventBufferLock;

		enum class State : uint8
		{
			AwaitingSessionStart,
			Active,
			Disabled
		};
		State m_state = State::AwaitingSessionStart;

		ngine::Guid m_receivedShareSessionGuid;
	};
}
