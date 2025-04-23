#include "Plugin.h"

#include <Http/Plugin.h>
#include <Http/Worker.h>

#include <Backend/Plugin.h>

#include <Renderer/Renderer.h>
#include <Renderer/Window/Window.h>
#include <Renderer/Window/DocumentData.h>

#include <AssetCompilerCore/Plugin.h>

#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Project/Project.h>
#include <Engine/Threading/JobRunnerThread.h>

#if USE_SENTRY
#if PLATFORM_APPLE

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#endif

#import <MetricKit/MetricKit.h>

#import <Sentry.h>
#import <Sentry-Swift.h>
#elif PLATFORM_ANDROID
#include <jni.h>
#include <sentry.h>
#else
#define SENTRY_BUILD_STATIC 1

#include <sentry.h>
#endif
#endif

#if USE_APPFLYER_SDK
#import <AppsFlyerLib/AppsFlyerLib.h>
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <Renderer/Window/iOS/AppDelegate.h>
#endif

#include <Common/System/Query.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Version.h>
#include <Common/Time/Timestamp.h>
#include <Common/Platform/Environment.h>
#include <Common/Platform/Distribution.h>
#include <Common/Platform/GetName.h>
#include <Common/Platform/GetDeviceModel.h>
#include <Common/Platform/GetProcessorCoreTypes.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/Library.h>
#include <Common/IO/Log.h>

#if USE_APPFLYER_SDK && PLATFORM_APPLE
@interface AppsFlyerHandler : NSObject <AppsFlyerDeepLinkDelegate>
@end

@implementation AppsFlyerHandler
- (void)didResolveDeepLink:(AppsFlyerDeepLinkResult*)result
{
	if (result.status == AFSDKDeepLinkResultStatusFound && result.deepLink)
	{
		using namespace ngine;
		Networking::Analytics::Plugin& plugin = *System::FindPlugin<Networking::Analytics::Plugin>();
		NSString* deepLinkValue = result.deepLink.deeplinkValue;
		plugin.OnReceivedDeepLink(ConstStringView{[deepLinkValue UTF8String], (ConstStringView::SizeType)[deepLinkValue length]});
	}
}
@end
#endif

namespace ngine::Networking::Analytics
{
#define ENABLE_INCUB8 0

	void Plugin::OnLoaded(Application&)
	{
		m_pHttpPlugin = System::FindPlugin<HTTP::Plugin>();
		m_pBackendPlugin = System::FindPlugin<Backend::Plugin>();

		if constexpr (Platform::TargetDistribution != Platform::Distribution::Local)
		{
			// Don't track errors or crashes in local builds
			InitializeErrorTracking();
		}

		Rendering::Window::GetOnWindowCreated().Add(this, &Plugin::OnWindowCreated);

#if USE_APPFLYER_SDK && PLATFORM_APPLE
		Rendering::Window::QueueOnWindowThread(
			[]()
			{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
				[[maybe_unused]] AppDelegate* appDelegate = (AppDelegate*)[[UIApplication sharedApplication] delegate];
#endif

				[AppsFlyerLib shared].appsFlyerDevKey = @APPSFLYER_DEV_KEY;
				[AppsFlyerLib shared].appleAppID = @APPSFLYER_APPLE_APPID;

				const bool isSandboxEnvironment = [[[[NSBundle mainBundle] appStoreReceiptURL] lastPathComponent] isEqual:@"sandboxReceipt"];
				[AppsFlyerLib shared].useUninstallSandbox = isSandboxEnvironment;
				[AppsFlyerLib shared].useReceiptValidationSandbox = isSandboxEnvironment;

				appDelegate->OnRegisteredForRemoteNotifications = ^(NSData* deviceToken) {
					[[AppsFlyerLib shared] registerUninstall:deviceToken];
				};

#if DEVELOPMENT_BUILD
				[AppsFlyerLib shared].isDebug = true;
#endif

				static AppsFlyerHandler* appsFlyerHandler = [[AppsFlyerHandler alloc] init];
				[AppsFlyerLib shared].deepLinkDelegate = appsFlyerHandler;

				[[NSNotificationCenter defaultCenter]
					addObserverForName:UIApplicationDidBecomeActiveNotification
											object:nil
											 queue:nil
									usingBlock:^(NSNotification*) {
										[[AppsFlyerLib shared] startWithCompletionHandler:^(NSDictionary<NSString*, id>* dictionary, NSError* error) {
											if (error)
											{
												LogError("Failed to initialize AppFlyer!");
												return;
											}
											if (dictionary)
											{
												return;
											}
										}];
									}];
			}
		);
#endif
	}

#if USE_APPFLYER_SDK && PLATFORM_APPLE
	void Plugin::OnReceivedDeepLink(const ConstStringView link)
	{
		if (m_pCurrentWindow.IsValid())
		{
			Array<Widgets::DocumentData, 1> documents{Widgets::DocumentData{IO::URI{IO::URI::StringType(link)}}};
			Threading::JobBatch jobBatch = m_pCurrentWindow->OpenDocuments(documents, Rendering::Window::OpenDocumentFlags{});
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
					jobManager.Queue(jobBatch, Threading::JobPriority::UserInterfaceAction);
				}
			}
		}
		else
		{
			Rendering::Window::QueueOnWindowThread(
				[this, link = String{link}]()
				{
					OnReceivedDeepLink(link);
				}
			);
		}
	}
#endif

	void Plugin::OnUnloaded(Application&)
	{
#if USE_SENTRY
		Internal::AssertEvents::GetInstance().RemoveAssertListener(this);

#if PLATFORM_APPLE
		[SentrySDK stopProfiler];
		[SentrySDK close];
#elif !PLATFORM_ANDROID
		[[maybe_unused]] const bool wasClosed = sentry_close() == 0;
		Assert(wasClosed);
#endif
#endif
	}

#if USE_SENTRY && PLATFORM_ANDROID
	extern "C"
	{
		JNIEXPORT jstring JNICALL Java_com_sceneri_App_NativeGameActivity_GetSentryDSN(JNIEnv* pEnv, jclass)
		{
			const ConstStringView dsn{SENTRY_DSN};
			return pEnv->NewStringUTF(String{dsn}.GetZeroTerminated());
		}

		JNIEXPORT jstring JNICALL Java_com_sceneri_App_NativeGameActivity_GetSentryRelease(JNIEnv* pEnv, jclass)
		{
			const Version engineVersion = System::Get<Engine>().GetInfo().GetVersion();
			const FlatString<15> versionString = engineVersion.ToString();
			return pEnv->NewStringUTF(versionString.GetZeroTerminated());
		}

		JNIEXPORT jstring JNICALL Java_com_sceneri_App_NativeGameActivity_GetSentryDistribution(JNIEnv* pEnv, jclass)
		{
			const ConstStringView distribution{TARGET_DISTRIBUTION};
			return pEnv->NewStringUTF(String{distribution}.GetZeroTerminated());
		}

		JNIEXPORT jstring JNICALL Java_com_sceneri_App_NativeGameActivity_GetSentryEnvironment(JNIEnv* pEnv, jclass)
		{
			const ConstStringView environment = Platform::GetEnvironmentString(Platform::GetEnvironment());
			return pEnv->NewStringUTF(String{environment}.GetZeroTerminated());
		}
	}
#endif

	void Plugin::InitializeErrorTracking()
	{
#if USE_SENTRY && !PLATFORM_ANDROID
		const Version engineVersion = System::Get<Engine>().GetInfo().GetVersion();
		const FlatString<15> versionString = engineVersion.ToString();
		const ConstStringView environment = Platform::GetEnvironmentString(Platform::GetEnvironment());

#if PLATFORM_APPLE
		[SentrySDK startWithConfigureOptions:^(SentryOptions* options) {
#define STRINGIZE(x) #x
#define OBJC_STRING(x) @STRINGIZE(x)
			options.dsn = OBJC_STRING(CREATE_DSN);

			options.releaseName = [NSString stringWithUTF8String:versionString.GetZeroTerminated().GetData()];

			options.dist = @TARGET_DISTRIBUTION;
			NSString* environmentNsString = [NSString stringWithUTF8String:environment.GetData()];
			options.environment = environmentNsString;

			options.sampleRate = @1.0;
			options.tracesSampleRate = @1.0;
#if SENTRY_TARGET_PROFILING_SUPPORTED
			options.profilesSampleRate = @1.0;
			options.enableAppLaunchProfiling = YES;
#endif
#if SENTRY_TARGET_REPLAY_SUPPORTED
			options.sessionReplay.onErrorSampleRate = 1.0;
			options.sessionReplay.sessionSampleRate = 1.0;
#endif
#if SENTRY_UIKIT_AVAILABLE
			options.enablePreWarmedAppStartTracing = YES;
#endif
		}];
#else
		const IO::PathView logPath = System::Get<Log>().GetFilePath();

		sentry_options_t* options = sentry_options_new();
		const ConstStringView dsn{SENTRY_DSN};
		sentry_options_set_dsn_n(options, dsn.GetData(), dsn.GetSize());

		IO::Path sentryDatabaseDirectory = IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("Sentry"));
#if PLATFORM_WINDOWS
		sentry_options_set_database_pathw_n(options, sentryDatabaseDirectory.GetZeroTerminated(), sentryDatabaseDirectory.GetSize());
#else
		sentry_options_set_database_path_n(options, sentryDatabaseDirectory.GetZeroTerminated(), sentryDatabaseDirectory.GetSize());
#endif

		IO::Path crashHandlerPath =
			IO::Path::Combine(IO::Path::GetExecutableDirectory(), IO::Path::Merge(MAKE_PATH("crashpad_handler"), IO::Library::ExecutablePostfix));
		Assert(crashHandlerPath.Exists());
#if PLATFORM_WINDOWS
		sentry_options_set_handler_pathw_n(options, crashHandlerPath.GetZeroTerminated(), crashHandlerPath.GetSize());
#else
		sentry_options_set_handler_path_n(options, crashHandlerPath.GetZeroTerminated(), crashHandlerPath.GetSize());
#endif
		sentry_options_set_release_n(options, versionString.GetZeroTerminated(), versionString.GetSize());
		const ConstStringView distribution{TARGET_DISTRIBUTION};
		sentry_options_set_dist_n(options, distribution.GetData(), distribution.GetSize());
		sentry_options_set_environment_n(options, environment.GetData(), environment.GetSize());
#if PLATFORM_WINDOWS
		sentry_options_add_attachmentw_n(options, logPath.GetData(), logPath.GetSize());
#else
		sentry_options_add_attachment_n(options, logPath.GetData(), logPath.GetSize());
#endif
		sentry_options_set_traces_sample_rate(options, 1.0);
		sentry_options_set_sample_rate(options, 1.0);

		const Asset::Manager& assetManager = System::Get<Asset::Manager>();
		const String certificatePath(assetManager.GetAssetBinaryPath("C4CA4253-0CD9-492F-8A11-CD1172F201AC"_asset).GetView().GetStringView());
		sentry_options_set_ca_certs_n(options, certificatePath.GetData(), certificatePath.GetSize());

		sentry_options_set_logger(
			options,
			[](sentry_level_t level, const char* message, va_list args, [[maybe_unused]] void* userdata)
			{
				DISABLE_CLANG_WARNING("-Wformat-nonliteral")
				char buffer[1024];
				PUSH_CLANG_WARNINGS
				vsnprintf(buffer, sizeof(buffer), message, args);
				POP_CLANG_WARNINGS

				switch (level)
				{
					case SENTRY_LEVEL_DEBUG:
					case SENTRY_LEVEL_INFO:
						LogMessage("[Sentry] {}", buffer);
						break;
					case SENTRY_LEVEL_WARNING:
						LogWarning("[Sentry] {}", buffer);
						break;
					case SENTRY_LEVEL_ERROR:
					case SENTRY_LEVEL_FATAL:
						LogError("[Sentry] {}", buffer);
						break;
				}
			},
			nullptr
		);

		[[maybe_unused]] const bool wasInitialized = sentry_init(options) == 0;
		Assert(wasInitialized);
#endif
#endif

		if (!IsDebuggerAttached())
		{
			Transaction registerBackendSessionTransaction = StartTransaction("session_registration", "to_backend");
			Transaction startupRequestsTransaction = StartTransaction("startup_requests", "to_backend");
			m_pBackendPlugin->GetGame().QueuePostSessionRegistrationCallback(
				[this,
			   registerBackendSessionTransaction = Move(registerBackendSessionTransaction)](const EnumFlags<Backend::SignInFlags> signInFlags
			  ) mutable
				{
					const bool success = signInFlags.AreAnySet(Backend::SignInFlags::HasSessionToken | Backend::SignInFlags::Offline);
					OnBackendSessionStartResponseReceived(success);

					if (signInFlags.IsNotSet(Backend::SignInFlags::HasSessionToken))
					{
						registerBackendSessionTransaction.Finish(TransactionStatus::UnknownError);
					}
				}
			);
			m_pBackendPlugin->GetGame().QueueStartupRequestsFinishedCallback(
				[startupRequestsTransaction = Move(startupRequestsTransaction)](const EnumFlags<Backend::SignInFlags> signInFlags) mutable
				{
					if (signInFlags.IsNotSet(Backend::SignInFlags::CompletedCoreRequests))
					{
						startupRequestsTransaction.Finish(TransactionStatus::UnknownError);
					}
				}
			);
		}

#if USE_SENTRY
		if (!IsDebuggerAttached())
		{
			Internal::AssertEvents::GetInstance().AddAssertListener(
				[](const char* file, const uint32 lineNumber, const bool isFirstTime, const char* message, [[maybe_unused]] void* pUserData)
				{
					if (!isFirstTime)
					{
						return;
					}

#if PLATFORM_APPLE
					// Capture the current call stack
					NSArray<NSString*>* callStackSymbols = [NSThread callStackSymbols];
					callStackSymbols = [[callStackSymbols reverseObjectEnumerator] allObjects];

					NSRegularExpression* regex = [NSRegularExpression
						regularExpressionWithPattern:@"^(\\d+)\\s+(\\S+)\\s+(0x[0-9a-fA-F]+)\\s+(.*) \\+ (\\d+)$"
																 options:0
																	 error:nil];

					// Transform the call stack symbols into SentryFrames
					NSMutableArray<SentryFrame*>* frames = [NSMutableArray array];
					for (NSString* symbol in callStackSymbols)
					{
						NSTextCheckingResult* match = [regex firstMatchInString:symbol options:0 range:NSMakeRange(0, symbol.length)];
						if (match)
						{
							SentryFrame* frame = [[SentryFrame alloc] init];

							frame.package = [symbol substringWithRange:[match rangeAtIndex:2]]; // Module name
							frame.instructionAddress = [symbol substringWithRange:[match rangeAtIndex:3]];
							frame.function = [symbol substringWithRange:[match rangeAtIndex:4]];
							frame.lineNumber = @([[symbol substringWithRange:[match rangeAtIndex:5]] integerValue]);
							frame.inApp = @YES; // Mark this frame as part of the app

							[frames addObject:frame];
						}
					}

					NSString* fileName = [NSString stringWithUTF8String:file];

					// Add the current assertion frame manually
					{
						SentryFrame* frame = [[SentryFrame alloc] init];
						frame.fileName = fileName;
						frame.lineNumber = @(lineNumber);
						[frames addObject:frame];
					}

					// Create the stack trace
					NSDictionary<NSString*, NSString*>* registers = @{};
					SentryStacktrace* stacktrace = [[SentryStacktrace alloc] initWithFrames:frames registers:registers];

					// Create the exception
					NSString* nsMessage = [NSString stringWithUTF8String:message];
					SentryException* exception = [[SentryException alloc] initWithValue:nsMessage type:@"Assertion"];
					exception.stacktrace = stacktrace;

					// Create the event and attach the exception
					SentryEvent* event = [[SentryEvent alloc] initWithLevel:kSentryLevelError];
					event.exceptions = @[ exception ];

					event.fingerprint = @[ @"Assertion", fileName, @(lineNumber) ];

					// Capture the event
					[SentrySDK captureEvent:event];
#else
					sentry_value_t event = sentry_value_new_event();
					sentry_value_t exception = sentry_value_new_exception("Assertion", message);
					sentry_value_set_by_key(exception, "file", sentry_value_new_string(file));
					sentry_value_set_by_key(exception, "line", sentry_value_new_int32(lineNumber));
					sentry_value_t stacktrace = sentry_value_new_stacktrace(NULL, 0);
					sentry_value_set_by_key(exception, "stacktrace", stacktrace);
					sentry_event_add_exception(event, exception);

					sentry_value_t fingerprint = sentry_value_new_list();
					sentry_value_append(fingerprint, sentry_value_new_string("Assertion"));
					sentry_value_append(fingerprint, sentry_value_new_string(file));
					sentry_value_append(fingerprint, sentry_value_new_int32(lineNumber));
					sentry_value_set_by_key(event, "fingerprint", fingerprint);

					sentry_capture_event(event);
#endif
				},
				this
			);
		}
#endif
	}

	void Plugin::Disable()
	{
		m_state = State::Disabled;
		Threading::UniqueLock lock(m_eventBufferLock);
		m_eventBuffer.Clear();
	}

#if PLATFORM_APPLE
	Transaction::Transaction(id<SentrySpan> transaction)
		: m_transaction(transaction)
	{
	}

	Transaction::Transaction(Transaction&& other)
		: m_transaction(other.m_transaction)
	{
		other.m_transaction = nil;
	}
	Transaction& Transaction::operator=(Transaction&& other)
	{
		m_transaction = other.m_transaction;
		other.m_transaction = nil;
		return *this;
	}

	Transaction::~Transaction()
	{
		if (m_transaction != nil)
		{
			[m_transaction finish];
			m_transaction = nil;
		}
	}

	void Transaction::Finish(const TransactionStatus status)
	{
		Assert(m_transaction != nil);
		if (m_transaction != nil)
		{
			switch (status)
			{
				case TransactionStatus::Success:
					[m_transaction finishWithStatus:kSentrySpanStatusOk];
					break;
				case TransactionStatus::DeadlineExceeded:
					[m_transaction finishWithStatus:kSentrySpanStatusDeadlineExceeded];
					break;
				case TransactionStatus::Unauthenticated:
					[m_transaction finishWithStatus:kSentrySpanStatusUnauthenticated];
					break;
				case TransactionStatus::PermissionDenied:
					[m_transaction finishWithStatus:kSentrySpanStatusPermissionDenied];
					break;
				case TransactionStatus::NotFound:
					[m_transaction finishWithStatus:kSentrySpanStatusNotFound];
					break;
				case TransactionStatus::ResourceExhausted:
					[m_transaction finishWithStatus:kSentrySpanStatusResourceExhausted];
					break;
				case TransactionStatus::InvalidArgument:
					[m_transaction finishWithStatus:kSentrySpanStatusInvalidArgument];
					break;
				case TransactionStatus::Unavailable:
					[m_transaction finishWithStatus:kSentrySpanStatusUnavailable];
					break;
				case TransactionStatus::CriticalError:
					[m_transaction finishWithStatus:kSentrySpanStatusInternalError];
					break;
				case TransactionStatus::UnknownError:
					[m_transaction finishWithStatus:kSentrySpanStatusUnknownError];
					break;
				case TransactionStatus::Cancelled:
					[m_transaction finishWithStatus:kSentrySpanStatusCancelled];
					break;
				case TransactionStatus::AlreadyExists:
					[m_transaction finishWithStatus:kSentrySpanStatusAlreadyExists];
					break;
				case TransactionStatus::Aborted:
					[m_transaction finishWithStatus:kSentrySpanStatusAborted];
					break;
				case TransactionStatus::OutOfRange:
					[m_transaction finishWithStatus:kSentrySpanStatusOutOfRange];
					break;
			}
			m_transaction = nil;
		}
	}

#else
	Transaction::Transaction(sentry_transaction_context_t* pContext, sentry_transaction_t* pTransaction)
		: m_pContext(pContext)
		, m_pTransaction(pTransaction)
	{
	}

	Transaction::~Transaction()
	{
#if USE_SENTRY
		if (m_pTransaction != nullptr)
		{
			sentry_transaction_finish(m_pTransaction);
		}
#endif
	}

	Transaction::Transaction(Transaction&& other)
		: m_pContext(other.m_pContext)
		, m_pTransaction(other.m_pTransaction)
	{
		other.m_pContext = nullptr;
		other.m_pTransaction = nullptr;
	}
	Transaction& Transaction::operator=(Transaction&& other)
	{
		m_pContext = other.m_pContext;
		m_pTransaction = other.m_pTransaction;
		other.m_pContext = nullptr;
		other.m_pTransaction = nullptr;
		return *this;
	}

	void Transaction::Finish([[maybe_unused]] const TransactionStatus status)
	{
		Assert(m_pTransaction != nullptr);
		if (m_pTransaction != nullptr)
		{
#if USE_SENTRY
			switch (status)
			{
				case TransactionStatus::Success:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_OK);
					break;
				case TransactionStatus::DeadlineExceeded:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_DEADLINE_EXCEEDED);
					break;
				case TransactionStatus::Unauthenticated:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_UNAUTHENTICATED);
					break;
				case TransactionStatus::PermissionDenied:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_PERMISSION_DENIED);
					break;
				case TransactionStatus::NotFound:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_NOT_FOUND);
					break;
				case TransactionStatus::ResourceExhausted:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_RESOURCE_EXHAUSTED);
					break;
				case TransactionStatus::InvalidArgument:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_INVALID_ARGUMENT);
					break;
				case TransactionStatus::Unavailable:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_UNAVAILABLE);
					break;
				case TransactionStatus::CriticalError:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_INTERNAL_ERROR);
					break;
				case TransactionStatus::UnknownError:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_UNKNOWN);
					break;
				case TransactionStatus::Cancelled:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_CANCELLED);
					break;
				case TransactionStatus::AlreadyExists:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_ALREADY_EXISTS);
					break;
				case TransactionStatus::Aborted:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_ABORTED);
					break;
				case TransactionStatus::OutOfRange:
					sentry_transaction_set_status(m_pTransaction, SENTRY_SPAN_STATUS_OUT_OF_RANGE);
					break;
			}
			sentry_transaction_finish(m_pTransaction);
#endif
			m_pTransaction = nullptr;
		}
	}
#endif

	Transaction Plugin::StartTransaction(const ConstStringView transactionName, const ConstStringView operation)
	{
#if PLATFORM_APPLE
		NSString* nsTransaction = [[NSString alloc] initWithBytes:transactionName.GetData()
																											 length:transactionName.GetSize()
																										 encoding:NSUTF8StringEncoding];
		NSString* nsOperation = [[NSString alloc] initWithBytes:operation.GetData() length:operation.GetSize() encoding:NSUTF8StringEncoding];

		return Transaction{[SentrySDK
			startTransactionWithContext:[[SentryTransactionContext alloc] initWithName:nsTransaction operation:nsOperation]]};
#elif USE_SENTRY
		sentry_transaction_context_t* pContext =
			sentry_transaction_context_new_n(transactionName.GetData(), transactionName.GetSize(), operation.GetData(), operation.GetSize());
		return Transaction{pContext, sentry_transaction_start(pContext, sentry_value_new_null())};
#else
		UNUSED(transactionName);
		UNUSED(operation);
		return {};
#endif
	}

	void Plugin::OnBackendSessionStartResponseReceived(const bool success)
	{
		StartSession(success);

#if USE_APPFLYER_SDK && PLATFORM_APPLE
		[[AppsFlyerLib shared] logEvent:AFEventLogin withValues:nil];
#endif
	}

	void Plugin::OnWindowCreated(Rendering::Window& window)
	{
		if (m_pCurrentWindow.IsInvalid())
		{
			m_pCurrentWindow = &window;
		}
	}

	void Plugin::SendEvent(const ConstStringView eventName, PropertiesCallback&& propertiesCallback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		writer.Serialize("name", eventName);

		if (propertiesCallback.IsValid())
		{
			writer.SerializeObjectWithCallback(
				"attributes",
				[propertiesCallback = Forward<PropertiesCallback>(propertiesCallback)](Serialization::Writer attributesWriter)
				{
					return propertiesCallback(attributesWriter);
				}
			);
		}

		SendEvent(writer);
	}

	void Plugin::StartSession(const bool succeededBackendSignin)
	{
		m_state = State::Active;

		SendEvent(
			"sessionStart",
			[this, succeededBackendSignin](Serialization::Writer attributesWriter)
			{
				attributesWriter.Serialize("firstTimeUser", m_pBackendPlugin->GetGame().IsFirstSession());
				attributesWriter.Serialize("platform", Platform::GetName(Platform::Current));
				attributesWriter.Serialize("deviceModel", Platform::GetDeviceModelName());
				attributesWriter.Serialize("version", System::Get<Engine>().GetInfo().GetVersion().ToString());
				if (!succeededBackendSignin)
				{
					attributesWriter.Serialize("failedSignIn", true);
				}

				attributesWriter.SerializeObjectWithCallback(
					"hardware",
					[this](Serialization::Writer attributesWriter)
					{
#if !PLATFORM_APPLE_VISIONOS
						const Math::Vector2ui screenSize = Rendering::Window::GetMainScreenUsableBounds();
#else
						const Math::Vector2ui screenSize = Math::Zero;
#endif
						const String screenSizeString = String().Format("{} x {}", screenSize.x, screenSize.y);
						attributesWriter.Serialize("mainScreenResolution", screenSizeString);

						const uint32 dpi = m_pCurrentWindow ? (uint32)m_pCurrentWindow->GetDotsPerInch() : 0u;
						attributesWriter.Serialize("mainScreenDpi", dpi);

						attributesWriter.Serialize("performanceCoreCount", Platform::GetPhysicalPerformanceCoreCount());
						attributesWriter.Serialize("logicalPerformanceCoreCount", Platform::GetLogicalPerformanceCoreCount());
						attributesWriter.Serialize("efficiencyCoreCount", Platform::GetLogicalEfficiencyCoreCount());

						return true;
					}
				);

				return true;
			}
		);

		Engine& engine = System::Get<Engine>();
		{
			Threading::JobBatch endSessionJobBatch{Threading::JobBatch::IntermediateStage};
			Threading::IntermediateStage& finishedSessionEndStage = Threading::CreateIntermediateStage();
			finishedSessionEndStage.AddSubsequentStage(endSessionJobBatch.GetFinishedStage());
			endSessionJobBatch.QueueAfterStartStage(Threading::CreateCallback(
				[this, &finishedSessionEndStage](Threading::JobRunnerThread&)
				{
					Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
					Serialization::Writer writer(serializedData);
					writer.Serialize("name", ConstStringView("sessionEnd"));
					SendEvent(writer, &finishedSessionEndStage);

#if USE_SENTRY
#if PLATFORM_APPLE
					[SentrySDK stopProfiler];
					[SentrySDK close];
#elif !PLATFORM_ANDROID
					[[maybe_unused]] const bool wasClosed = sentry_close() == 0;
					Assert(wasClosed);
#endif
#endif

					m_state = State::Disabled;
				},
				Threading::JobPriority::EndNetworkSession
			));
			engine.GetQuitJobBatch().QueueAfterStartStage(endSessionJobBatch);
		}

		// Send all buffered events
		Vector<EventEntry> bufferedEvents;
		{
			Threading::UniqueLock lock(m_eventBufferLock);
			bufferedEvents = Move(m_eventBuffer);
		}
		for (EventEntry& eventEntry : bufferedEvents)
		{
			Serialization::Writer entryWriter(eventEntry.data);
			SendEventInternal(entryWriter, eventEntry.pSentStage);
		}
	}

	void Plugin::SendEvent(Serialization::Writer writer, const Optional<Threading::IntermediateStage*> pSentStage)
	{
		SendEventInternal(writer, pSentStage);
	}

	void Plugin::SendEventInternal(Serialization::Writer writer, const Optional<Threading::IntermediateStage*> pSentStage)
	{
		switch (m_state)
		{
			case State::AwaitingSessionStart:
			{
				Threading::UniqueLock lock(m_eventBufferLock);
				m_eventBuffer.EmplaceBack(EventEntry{Serialization::Data(writer.GetData()), pSentStage});
				return;
			}
			case State::Active:
				break;
			case State::Disabled:
				return;
		}

		writer.Serialize("timestamp", Time::Timestamp::GetCurrent().ToString());
		const Platform::Environment environment = Platform::GetEnvironment();

		writer.SerializeObjectWithCallback(
			"attributes",
			[this, &backend = *m_pBackendPlugin, environment](Serialization::Writer attributesWriter)
			{
				attributesWriter.Serialize("userId", backend.GetGame().GetPublicUserIdentifier());
				attributesWriter.Serialize("sessionId", System::Get<Engine>().GetSessionGuid());
				Project& currentProject = System::Get<Project>();
				if (currentProject.IsValid())
				{
					attributesWriter.Serialize("productSessionId", currentProject.GetSessionGuid());
				}

				if (m_receivedShareSessionGuid.IsValid())
				{
					attributesWriter.Serialize("shareSessionId", m_receivedShareSessionGuid);
				}

				switch (environment)
				{
					case Platform::Environment::InternalDevelopment:
					case Platform::Environment::LocalBackendDevelopment:
					case Platform::Environment::InternalStaging:
						attributesWriter.Serialize("environment", ConstStringView("DEV"));
						break;
					case Platform::Environment::Staging:
						// Counting staging as Live until we have a live build (aka live on app store)
						attributesWriter.Serialize("environment", ConstStringView("LIVE"));
						// attributesWriter.Serialize("environment", ConstStringView("TEST"));
						break;
					case Platform::Environment::Live:
						attributesWriter.Serialize("environment", ConstStringView("LIVE"));
						break;
				}
				return true;
			}
		);

#if ENABLE_INCUB8
		m_pHttpPlugin->GetLowPriorityWorker().QueueRequest(
			Networking::HTTP::Worker::Request{
				HTTP::RequestType::Post,
				IO::URI::Merge(MAKE_URI("https://vortex.incub8.de/api/event?apikey="), MAKE_URI("VehGAfhccR6GonvmBmiI")),
				Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{
					Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"}
				}.GetView(),
				writer.SaveToBuffer<String>(),
				[pSentStage](const Networking::HTTP::Worker::RequestInfo&, const Networking::HTTP::Worker::ResponseData& responseData)
				{
					Serialization::Data responseSerializedData(responseData.m_responseBody);
					Serialization::Reader responseReader(responseSerializedData);
					[[maybe_unused]] const bool success = responseData.m_responseCode.IsSuccessful() && responseReader.GetValue().IsArray() &&
			                                          responseReader.GetArraySize() == 0;
					Assert(success);
					if (pSentStage)
					{
						pSentStage->SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					}
				}
			},
			Threading::JobPriority::LowPriorityBackendNetworking
		);
#endif
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Networking::Analytics::Plugin>();
#else
extern "C" HTTP_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Networking::Analytics::Plugin(application);
}
#endif
