#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS

#import "Renderer/Window/iOS/AppDelegate.h"
#import <Renderer/Window/iOS/SceneDelegate.h>
#import <Renderer/Window/iOS/ViewController.h>
#import <Renderer/Window/iOS/MetalView.h>

#include <Engine/EngineSystems.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Window/DocumentData.h>

#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Platform/Environment.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Project System/EngineAssetFormat.h>
#include <Common/Project System/ProjectAssetFormat.h>

#import <StoreKit/SKPayment.h>
#import <StoreKit/SKPaymentTransaction.h>

#if PLATFORM_APPLE_VISIONOS
#import <Renderer-Swift.h>
#endif

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters& commandLineParameters);

@implementation Application

#if PLATFORM_APPLE_MACOS
AppDelegate* _Nullable strongDelegate = nil;
- (id _Nonnull)init
{
	self = [super init];
	AppDelegate* delegate = [[AppDelegate alloc] init];
	strongDelegate = delegate;
	self.delegate = delegate;
	return self;
}
#endif

@end

@interface AppDelegate ()

@end

@implementation AppDelegate

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (BOOL)application:(UIApplication* _Nonnull)application
	willFinishLaunchingWithOptions:(NSDictionary<UIApplicationLaunchOptionsKey, id>* _Nullable)launchOptions
{
	return YES;
}
#endif

#if PLATFORM_APPLE_VISIONOS
MixedRealityInterface* _Nullable m_mixedRealityInterface = nullptr;
#endif

NSThread* _Nullable m_mainEngineThread = nil;
ngine::Threading::Atomic<bool> m_startedEngine{false};

#if PLATFORM_APPLE_MACOS
ngine::Threading::Atomic<bool> hasCreatedWindow{false};
#endif

- (id _Nonnull)init
{
	self = [super init];

#if PLATFORM_APPLE_MACOS
#endif

	return self;
}

- (void)loadStoredSecurityBookmarks
{
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
	// Attempt to load and resolve bookmarks to user files
	NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
	NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
	if (securityBookmarks != nil)
	{
		for (NSData* bookmarkData : securityBookmarks)
		{
			BOOL bookmarkDataIsStale = false;
			NSError* error;
			NSURL* url = [NSURL URLByResolvingBookmarkData:bookmarkData
																						 options:NSURLBookmarkResolutionWithSecurityScope
																			 relativeToURL:nil
																 bookmarkDataIsStale:&bookmarkDataIsStale
																							 error:&error];
			if (bookmarkDataIsStale)
			{
				// TODO: Remove from bookmarks and save
			}
			else if (url != nil)
			{
				[url startAccessingSecurityScopedResource];
			}
		}
	}
#endif
}

#if PLATFORM_APPLE_MACOS
- (void)applicationWillFinishLaunching:(NSNotification* _Nonnull)notification
{
}

- (void)handleURLEvent:(NSAppleEventDescriptor* _Nonnull)event withReplyEvent:(NSAppleEventDescriptor* _Nonnull)replyEvent
{
	NSString* urlStr = [[event paramDescriptorForKeyword:keyDirectObject] stringValue];
	NSURL* url = [NSURL URLWithString:urlStr];
	NSLog(@"Launched with URL: %@", [url absoluteString]);
	// Handle the URL
}

/*- (BOOL)application:(NSApplication *)application openFile:(NSString *)filename {
    NSLog(@"App launched with file: %@", filename);
    // Handle the file
    return YES;
}*/
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (BOOL)application:(UIApplication* _Nonnull)application didFinishLaunchingWithOptions:(NSDictionary* _Nullable)launchOptions
#else
- (void)applicationDidFinishLaunching:(NSNotification* _Nonnull)notification
#endif
{
	[self loadStoredSecurityBookmarks];

	using namespace ngine;

	const bool isSandboxPurchasingEnvironment = [[[[NSBundle mainBundle] appStoreReceiptURL] lastPathComponent] isEqual:@"sandboxReceipt"];
	m_isSandboxPurchasingEnvironment = isSandboxPurchasingEnvironment;
	Assert(isSandboxPurchasingEnvironment || Platform::GetEnvironment() != Platform::Environment::Live);

#if PLATFORM_APPLE_VISIONOS
	m_mixedRealityInterface = [[MixedRealityInterface alloc] init];
#endif

	m_mainEngineThread = [[NSThread alloc] initWithTarget:self selector:@selector(runEngineThread) object:nil];
	[m_mainEngineThread start];

	while (!m_startedEngine)
		;

#if !PLATFORM_APPLE_VISIONOS
	[[SKPaymentQueue defaultQueue] addTransactionObserver:self];
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	[application registerForRemoteNotifications];
#elif PLATFORM_APPLE_MACOS
	System::Get<Threading::JobManager>().QueueCallback(
		[](Threading::JobRunnerThread& thread)
		{
			Threading::JobBatch jobBatch;
			Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{jobBatch});

			if (jobBatch.IsValid())
			{
				jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[](Threading::JobRunnerThread&)
					{
						hasCreatedWindow = true;
					},
					Threading::JobPriority::UserInterfaceAction
				));
				thread.Queue(jobBatch);
			}
			else
			{
				hasCreatedWindow = true;
			}
		},
		Threading::JobPriority::WindowResizing
	);
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
	return YES;
#endif
}

#if PLATFORM_APPLE_MACOS
- (void)application:(NSApplication* _Nonnull)application openURLs:(NSArray<NSURL*>* _Nonnull)urls
{
	NSWindow* mainWindow = [NSApp mainWindow];
	ViewController* rootController = (ViewController*)[mainWindow contentViewController];
	MetalView* metalView = (MetalView*)rootController.view;

	using namespace ngine;
	if (!hasCreatedWindow || metalView.engineWindow == nullptr)
	{
		Rendering::Window::QueueOnWindowThread(
			[self, application, urls]()
			{
				[self application:application openURLs:urls];
			}
		);
		return;
	}

	Rendering::Window* pWindow = (Rendering::Window*)metalView.engineWindow;
	Assert(pWindow != nullptr);

	EnumFlags<Rendering::Window::OpenDocumentFlags> openDocumentFlags;

	FixedCapacityVector<Widgets::DocumentData> documents(Memory::Reserve, (uint32)[urls count]);
	for (const NSURL* url : urls)
	{
		if (url.fileURL)
		{
			[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS
			if (isSecurityScoped)
			{
				// Save in user defaults so we can access this resource in future sessions
				NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
														 includingResourceValuesForKeys:nil
																							relativeToURL:nil
																											error:nil];
				if (bookmarkData != nil)
				{
					NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
					NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
					if (securityBookmarks == nil)
					{
						securityBookmarks = [[NSArray<NSData*> alloc] init];
					}
					NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
					[mutableSecurityBookmarks addObject:bookmarkData];
					[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
				}
			}
#endif

			const char* filePath = url.fileSystemRepresentation;

			using namespace ngine;
			const IO::PathView pathView{filePath, (IO::PathView::SizeType)strlen(filePath)};
			if (pathView.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension && pathView.GetFileNameWithoutExtensions() != MAKE_PATH("Project"))
			{
				documents.EmplaceBack(Widgets::DocumentData{
					IO::Path::Combine(pathView, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
				});
			}
			else
			{
				documents.EmplaceBack(Widgets::DocumentData{IO::Path(pathView)});
			}

			// Always open local files for editing
			openDocumentFlags |= Rendering::Window::OpenDocumentFlags::EnableEditing;
		}
		else
		{
			NSString* string = url.absoluteString;
			IO::URI uri = IO::URI{IO::URI::StringType{[string UTF8String], (IO::URI::SizeType)[string length]}};
			if (Optional<IO::URI> unescapedURI = IO::URI::Unescape(uri))
			{
				uri = Move(*unescapedURI);
			}

			{
				IO::URI::StringType uriString(uri);

				// Apple APIs strip the :
				uriString.ReplaceFirstOccurrence(MAKE_URI_LITERAL("https//"), MAKE_NATIVE_LITERAL("https://"));
				uriString.ReplaceFirstOccurrence(MAKE_URI_LITERAL("file//"), MAKE_NATIVE_LITERAL("file://"));

				uri = IO::URI(Move(uriString));
			}

			const IO::ConstURIView protocol = uri.GetView().GetProtocol();

			if (protocol == MAKE_URI_LITERAL("file://"))
			{
				[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
					if (bookmarkData != nil)
					{
						NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
						NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
						if (securityBookmarks == nil)
						{
							securityBookmarks = [[NSArray<NSData*> alloc] init];
						}
						NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
						[mutableSecurityBookmarks addObject:bookmarkData];
						[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
					}
				}
#endif

				const IO::Path path{uri.GetView().GetStringView()};
				if (path.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension && uri.GetFileNameWithoutExtensions() != MAKE_URI("Project"))
				{
					documents.EmplaceBack(Widgets::DocumentData{
						IO::Path::Combine(path, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
					});
				}
				else
				{
					documents.EmplaceBack(Widgets::DocumentData{IO::Path(path)});
				}

				// Always open local files for editing
				openDocumentFlags |= Rendering::Window::OpenDocumentFlags::EnableEditing;
			}
			else if (protocol == MAKE_URI_LITERAL("https://"))
			{
				if (uri.GetView().HasQueryString())
				{
					documents.EmplaceBack(Widgets::DocumentData{IO::URI(uri)});
				}
				else
				{
					// Project URIs always refer to the root folder, and not the project metadata file
					// We patch this up by just applying the folder name and extension again.
					IO::URI projectExtension(IO::URI::StringType(ProjectAssetFormat.metadataFileExtension.GetStringView()));
					if (uri.GetAllExtensions().GetStringView() == projectExtension.GetView().GetStringView() && uri.GetFileNameWithoutExtensions() != MAKE_URI("Project"))
					{
						uri = IO::URI::Combine(uri, IO::URI::Merge(MAKE_URI("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()));
					}

					documents.EmplaceBack(Widgets::DocumentData{Move(uri)});

					openDocumentFlags |= Rendering::Window::OpenDocumentFlags::CopyToLocal;
				}
			}
			else
			{
				IO::ConstURIView action = uri.GetView().GetPath().GetFirstPath();
				if (action == MAKE_URI_LITERAL("projects") || action == MAKE_URI_LITERAL("app_asset"))
				{
					const IO::ConstURIView query = uri.GetView().GetQueryString();
					const IO::ConstURIView assetGuidView = query.GetQueryParameterValue(IO::ConstURIView(MAKE_URI_LITERAL("guid")));
					Asset::Guid assetGuid = Asset::Guid::TryParse(assetGuidView.GetStringView());
					if (assetGuid.IsValid())
					{
						documents.EmplaceBack(assetGuid);

						openDocumentFlags |= Rendering::Window::OpenDocumentFlags::CopyToLocal;
					}
				}
				else
				{
					documents.EmplaceBack(uri);
				}
			}
		}
	}

	if (documents.HasElements())
	{
		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
		jobManager.QueueCallback(
			[pWindow, documents = Move(documents), openDocumentFlags](Threading::JobRunnerThread& thread)
			{
				Threading::JobBatch jobBatch = pWindow->OpenDocuments(documents, openDocumentFlags);
				if (jobBatch.IsValid())
				{
					thread.Queue(jobBatch);
				}
			},
			Threading::JobPriority::UserInterfaceAction
		);
	}
}

- (BOOL)application:(NSApplication* _Nonnull)application
	continueUserActivity:(nonnull NSUserActivity*)userActivity
		restorationHandler:(nonnull void (^)(NSArray<id<NSUserActivityRestoring>>* _Nonnull))restorationHandler
{
	NSWindow* mainWindow = [NSApp mainWindow];
	ViewController* rootController = (ViewController*)[mainWindow contentViewController];
	MetalView* metalView = (MetalView*)rootController.view;

	using namespace ngine;
	if (!hasCreatedWindow || metalView.engineWindow == nullptr)
	{
		Rendering::Window::QueueOnWindowThread(
			[self, application, userActivity, restorationHandler]()
			{
				[self application:application continueUserActivity:userActivity restorationHandler:restorationHandler];
			}
		);
		return true;
	}

	Rendering::Window* pWindow = (Rendering::Window*)metalView.engineWindow;
	Assert(pWindow != nullptr);

	NSURL* url = userActivity.webpageURL;

	EnumFlags<Rendering::Window::OpenDocumentFlags> openDocumentFlags;

	FixedCapacityVector<Widgets::DocumentData> documents(Memory::Reserve, 1);
	// for (const NSURL* url : urls)
	{
		if (url.fileURL)
		{
			[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS
			if (isSecurityScoped)
			{
				// Save in user defaults so we can access this resource in future sessions
				NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
														 includingResourceValuesForKeys:nil
																							relativeToURL:nil
																											error:nil];
				if (bookmarkData != nil)
				{
					NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
					NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
					if (securityBookmarks == nil)
					{
						securityBookmarks = [[NSArray<NSData*> alloc] init];
					}
					NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
					[mutableSecurityBookmarks addObject:bookmarkData];
					[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
				}
			}
#endif

			const char* filePath = url.fileSystemRepresentation;

			using namespace ngine;
			const IO::PathView pathView{filePath, (IO::PathView::SizeType)strlen(filePath)};
			if (pathView.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension && pathView.GetFileNameWithoutExtensions() != MAKE_PATH("Project"))
			{
				documents.EmplaceBack(Widgets::DocumentData{
					IO::Path::Combine(pathView, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
				});
			}
			else
			{
				documents.EmplaceBack(Widgets::DocumentData{IO::Path(pathView)});
			}

			// Always open local files for editing
			openDocumentFlags |= Rendering::Window::OpenDocumentFlags::EnableEditing;
		}
		else
		{
			NSString* string = url.absoluteString;
			IO::URI uri = IO::URI{IO::URI::StringType{[string UTF8String], (IO::URI::SizeType)[string length]}};
			if (Optional<IO::URI> unescapedURI = IO::URI::Unescape(uri))
			{
				uri = Move(*unescapedURI);
			}

			{
				IO::URI::StringType uriString(uri);

				// Apple APIs strip the :
				uriString.ReplaceFirstOccurrence(MAKE_URI_LITERAL("https//"), MAKE_NATIVE_LITERAL("https://"));
				uriString.ReplaceFirstOccurrence(MAKE_URI_LITERAL("file//"), MAKE_NATIVE_LITERAL("file://"));

				uri = IO::URI(Move(uriString));
			}

			const IO::ConstURIView protocol = uri.GetView().GetProtocol();

			if (protocol == MAKE_URI_LITERAL("file://"))
			{
				[[maybe_unused]] const bool isSecurityScoped = [url startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [url bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
					if (bookmarkData != nil)
					{
						NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
						NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
						if (securityBookmarks == nil)
						{
							securityBookmarks = [[NSArray<NSData*> alloc] init];
						}
						NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
						[mutableSecurityBookmarks addObject:bookmarkData];
						[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
					}
				}
#endif

				const IO::Path path{uri.GetView().GetStringView()};
				if (path.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension && uri.GetFileNameWithoutExtensions() != MAKE_URI("Project"))
				{
					documents.EmplaceBack(Widgets::DocumentData{
						IO::Path::Combine(path, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
					});
				}
				else
				{
					documents.EmplaceBack(Widgets::DocumentData{IO::Path(path)});
				}

				// Always open local files for editing
				openDocumentFlags |= Rendering::Window::OpenDocumentFlags::EnableEditing;
			}
			else if (protocol == MAKE_URI_LITERAL("https://"))
			{
				if (uri.GetView().HasQueryString())
				{
					documents.EmplaceBack(Widgets::DocumentData{IO::URI(uri)});
				}
				else
				{
					// Project URIs always refer to the root folder, and not the project metadata file
					// We patch this up by just applying the folder name and extension again.
					IO::URI projectExtension(IO::URI::StringType(ProjectAssetFormat.metadataFileExtension.GetStringView()));
					if (uri.GetAllExtensions().GetStringView() == projectExtension.GetView().GetStringView() && uri.GetFileNameWithoutExtensions() != MAKE_URI("Project"))
					{
						uri = IO::URI::Combine(uri, IO::URI::Merge(MAKE_URI("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()));
					}

					documents.EmplaceBack(Widgets::DocumentData{Move(uri)});

					openDocumentFlags |= Rendering::Window::OpenDocumentFlags::CopyToLocal;
				}
			}
			else
			{
				IO::ConstURIView action = uri.GetView().GetPath().GetFirstPath();
				if (action == MAKE_URI_LITERAL("projects") || action == MAKE_URI_LITERAL("app_asset"))
				{
					const IO::ConstURIView query = uri.GetView().GetQueryString();
					const IO::ConstURIView assetGuidView = query.GetQueryParameterValue(IO::ConstURIView(MAKE_URI_LITERAL("guid")));
					Asset::Guid assetGuid = Asset::Guid::TryParse(assetGuidView.GetStringView());
					if (assetGuid.IsValid())
					{
						documents.EmplaceBack(assetGuid);

						openDocumentFlags |= Rendering::Window::OpenDocumentFlags::CopyToLocal;
					}
				}
				else
				{
					documents.EmplaceBack(uri);
				}
			}
		}
	}

	if (documents.HasElements())
	{
		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
		jobManager.QueueCallback(
			[pWindow, documents = Move(documents), openDocumentFlags](Threading::JobRunnerThread& thread)
			{
				Threading::JobBatch jobBatch = pWindow->OpenDocuments(documents, openDocumentFlags);
				if (jobBatch.IsValid())
				{
					thread.Queue(jobBatch);
				}
			},
			Threading::JobPriority::UserInterfaceAction
		);
	}
	return false;
}
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (UISceneConfiguration* _Nonnull)application:(UIApplication* _Nonnull)application
			 configurationForConnectingSceneSession:(UISceneSession* _Nonnull)connectingSceneSession
																			options:(UISceneConnectionOptions* _Nonnull)options
{
	UISceneConfiguration* configuration = [[UISceneConfiguration alloc] initWithName:@"Default Configuration"
																																			 sessionRole:connectingSceneSession.role];
	configuration.delegateClass = SceneDelegate.class;
	return configuration;
}
#endif

- (void)runEngineThread
{
	using namespace ngine;
	EngineSystems* pEngineSystems = CreateEngine(CommandLine::InitializationParameters::GetGlobalParameters()).StealOwnership();
	m_pEngineSystems = pEngineSystems;

	Engine& engine = reinterpret_cast<EngineSystems*>(m_pEngineSystems)->m_engine;
	engine.DoTick();

	NSRunLoop* runLoop = [NSRunLoop currentRunLoop];

#if PLATFORM_APPLE_IOS
	CADisplayLink* displayLink = [[UIScreen mainScreen] displayLinkWithTarget:self selector:@selector(update)];
	[displayLink addToRunLoop:runLoop forMode:NSDefaultRunLoopMode];
#else
	const Time::Durationd updateRate = Time::Durationd::FromSeconds(1.0f / 90.0f);
	NSTimer* timer = [NSTimer scheduledTimerWithTimeInterval:updateRate.GetSeconds()
																									 repeats:YES
																										 block:^(NSTimer* _Nonnull) {
																											 [self update];
																										 }];
	[runLoop addTimer:timer forMode:NSDefaultRunLoopMode];
#endif

	pEngineSystems->m_startupJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
		[](Threading::JobRunnerThread&)
		{
			m_startedEngine = true;
		},
		Threading::JobPriority::LoadPlugin
	));
	Threading::JobRunnerThread::GetCurrent()->Queue(pEngineSystems->m_startupJobBatch);

	do
	{
		[runLoop runMode:NSDefaultRunLoopMode beforeDate:[NSDate dateWithTimeIntervalSinceNow:0.1]];
	} while (!engine.IsAboutToQuit());

	engine.OnBeforeQuit();

#if PLATFORM_APPLE_MACOS
	[[NSApplication sharedApplication] terminate:nil];
#endif
}

- (void)update
{
	@autoreleasepool
	{
		using namespace ngine;
		Engine& engine = reinterpret_cast<EngineSystems*>(m_pEngineSystems)->m_engine;
		if (UNLIKELY(!engine.DoTick()))
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->DoRunNextJob();
			}
		}
	}
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)applicationWillTerminate:(UIApplication* _Nonnull)application
#else
- (void)applicationWillTerminate:(NSNotification* _Nonnull)notification
#endif
{
#if !PLATFORM_APPLE_VISIONOS
	// Called when the application is about to terminate. Save data if appropriate. See also
	// applicationDidEnterBackground:.
	[[SKPaymentQueue defaultQueue] removeTransactionObserver:self];
#endif

	ngine::Engine& engine = reinterpret_cast<ngine::EngineSystems*>(m_pEngineSystems)->m_engine;
	engine.Quit();

	[m_mainEngineThread cancel];
}

#if PLATFORM_APPLE_MACOS
- (BOOL)applicationShouldTerminateAfterLastWindowClosed:(NSApplication* _Nonnull)application
{
	return YES;
}
#endif

#if !PLATFORM_APPLE_VISIONOS
NSArray<SKProduct*>* _Nullable m_products = nullptr;
SKProductsRequest* _Nullable m_request = nullptr;
// SKProductsRequestDelegate protocol method.
- (void)productsRequest:(SKProductsRequest* _Nonnull)request didReceiveResponse:(SKProductsResponse* _Nonnull)response
{
	using namespace ngine;
	Assert(m_request == request);
	ngine::FixedCapacityVector<SKProduct*> products(ngine::Memory::Reserve, (uint32)[response.products count]);
	for (SKProduct* product in response.products)
	{
		products.EmplaceBack(product);
	}
}

- (void)requestInAppPurchaseProducts:(NSArray<NSString*>* _Nonnull)products
{
	Assert(m_request == nullptr);

	NSMutableSet<NSString*>* identifiersSet = [[NSMutableSet<NSString*> alloc] initWithCapacity:[products count]];
	for (NSString* productIdentifier : products)
	{
		[identifiersSet addObject:productIdentifier];
	}

	m_request = [[SKProductsRequest alloc] initWithProductIdentifiers:identifiersSet];
	m_request.delegate = self;
	[m_request start];
}

- (void)queuePurchase:(SKProduct* _Nonnull)product quantity:(ngine::uint32)quantity
{
	SKMutablePayment* payment = [SKMutablePayment paymentWithProduct:product];
	payment.quantity = quantity;
	[[SKPaymentQueue defaultQueue] addPayment:payment];
}

- (void)paymentQueue:(SKPaymentQueue* _Nonnull)queue updatedTransactions:(NSArray* _Nonnull)transactions
{
}

- (void)paymentQueue:(SKPaymentQueue* _Nonnull)queue removedTransactions:(NSArray<SKPaymentTransaction*>* _Nonnull)transactions
{
}

- (void)finishTransaction:(SKPaymentTransaction* _Nonnull)transaction
{
	[[SKPaymentQueue defaultQueue] finishTransaction:transaction];
}
#endif

#if !PLATFORM_APPLE_VISIONOS
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (BOOL)application:(UIApplication* _Nonnull)application
						openURL:(NSURL* _Nonnull)url
						options:(NSDictionary<UIApplicationOpenURLOptionsKey, id>* _Nonnull)options
{
	if ([url isFileURL] && [[[url absoluteString] pathExtension] isEqualToString:@"nproject"])
	{
		return YES;
	}

	return NO;
}
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)application:(UIApplication* _Nonnull)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData* _Nonnull)deviceToken
#else
- (void)application:(NSApplication* _Nonnull)application didRegisterForRemoteNotificationsWithDeviceToken:(NSData* _Nonnull)deviceToken
#endif
{
	if (OnRegisteredForRemoteNotifications != nil)
	{
		OnRegisteredForRemoteNotifications(deviceToken);
	}
}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
- (void)application:(UIApplication* _Nonnull)application didFailToRegisterForRemoteNotificationsWithError:(NSError* _Nonnull)error
#else
- (void)application:(NSApplication* _Nonnull)application didFailToRegisterForRemoteNotificationsWithError:(NSError* _Nonnull)error
#endif
{
}

#if PLATFORM_APPLE_VISIONOS
InitializeLayerRendererCallback _Nullable m_initializeLayerRendererCallback = nullptr;

- (void)setInitializeLayerRendererCallback:(InitializeLayerRendererCallback _Nullable)callbackBlock
{
	m_initializeLayerRendererCallback = callbackBlock;
}

- (void)initializeLayerRenderer:(cp_layer_renderer_t _Nonnull)layerRenderer pLogicalDevice:(void* _Nonnull)pLogicalDevice
{
	m_initializeLayerRendererCallback(layerRenderer, pLogicalDevice);
}

- (void)startFullImmersiveSpace:(ImmersiveSpaceCallback _Nonnull)callbackBlock pLogicalDevice:(void* _Nonnull)pLogicalDevice
{
	[m_mixedRealityInterface startFullImmersiveSpaceWithPLogicalDevice:pLogicalDevice completionHandler:callbackBlock];
}

- (void)startMixedImmersiveSpace:(ImmersiveSpaceCallback _Nonnull)callbackBlock pLogicalDevice:(void* _Nonnull)pLogicalDevice
{
	[m_mixedRealityInterface startMixedImmersiveSpaceWithPLogicalDevice:pLogicalDevice completionHandler:callbackBlock];
}

- (void)stopImmersiveSpace:(ImmersiveSpaceCallback _Nonnull)callbackBlock
{
	[m_mixedRealityInterface stopImmersiveSpaceWithCompletionHandler:callbackBlock];
}
#endif

@end
#endif
