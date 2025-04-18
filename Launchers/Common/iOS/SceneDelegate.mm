#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <Renderer/Window/iOS/SceneDelegate.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Window/Window.h>
#include <Renderer/Window/iOS/MetalView.h>
#include <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/ViewExtensions.h>
#include <Renderer/Window/DocumentData.h>

#include <Engine/Engine.h>
#include <Engine/Entity/ComponentSoftReference.h>

#include <Common/Asset/Guid.h>
#include <Common/IO/File.h>
#include <Common/IO/Path.h>
#include <Common/IO/URI.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Memory/Variant.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Jobs/JobManager.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Common/Asset/Asset.h>
#include <Common/Project System/EngineAssetFormat.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Writer.h>
#include <Common/System/Query.h>

#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>

@interface WindowScene ()
@end

@implementation WindowScene
@end

@interface SceneDelegate ()

@end

@implementation SceneDelegate

@synthesize window = _window;

#if !PLATFORM_APPLE_VISIONOS
UIScreen* previousScreen = nil;
#endif

ngine::Threading::Atomic<bool> hasCreatedWindow{false};

- (void)scene:(UIScene*)scene willConnectToSession:(UISceneSession*)session options:(UISceneConnectionOptions*)connectionOptions
{
	UIWindowScene* pWindowScene = (UIWindowScene*)scene;
	if (pWindowScene == nil)
	{
		return;
	}

#if !PLATFORM_APPLE_VISIONOS
	previousScreen = pWindowScene.screen;
#endif

	UISceneSessionRole role = pWindowScene.session.role;
	if (role == UIWindowSceneSessionRoleApplication || [role isEqual:@"UIWindowSceneSessionRoleApplication"])
	{
#if PLATFORM_APPLE_MACCATALYST
		if (UITitlebar* pTitlebar = [pWindowScene titlebar])
		{
			pTitlebar.titleVisibility = UITitlebarTitleVisibilityHidden;
			pTitlebar.toolbar = nullptr;
		}
#endif

		using namespace ngine;
		Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
		jobManager.QueueCallback(
			[self, connectionOptions, scene, pWindowScene](Threading::JobRunnerThread& thread)
			{
				Threading::JobBatch jobBatch;
				Rendering::Window::RequestWindowCreation(Rendering::Window::CreationRequestParameters{(__bridge void*)pWindowScene, jobBatch});

				jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[self, connectionOptions, scene, pWindowScene](Threading::JobRunnerThread&)
					{
						hasCreatedWindow = true;

						Rendering::Window::QueueOnWindowThread(
							[self, connectionOptions, scene, pWindowScene]()
							{
								self.window = pWindowScene.keyWindow;
								UIView* view = self.window.rootViewController.view;
								[view addInteraction:[[UIDropInteraction alloc] initWithDelegate:self]];

								if ([connectionOptions.URLContexts count] > 0)
								{
									[self scene:scene openURLContexts:connectionOptions.URLContexts];
								}

								if ([connectionOptions.userActivities count] > 0)
								{
									for (NSUserActivity* userActivity in connectionOptions.userActivities)
									{
										[self scene:scene continueUserActivity:userActivity];
									}
								}
							}
						);
					},
					Threading::JobPriority::UserInterfaceAction
				));

				thread.Queue(jobBatch);
			},
			Threading::JobPriority::UserInterfaceAction
		);
	}
}

- (void)sceneDidDisconnect:(UIScene*)scene
{
	using namespace ngine;

	UIWindowScene* windowScene = (UIWindowScene*)scene;
	UIWindow* pUIWindow = windowScene.keyWindow;
	UIViewController* pViewController = pUIWindow.rootViewController;
	UIView* pView = pViewController.view;
	MetalView* pMetalView = [pView findAsType:[MetalView class]];
	Rendering::Window* pWindow = (Rendering::Window*)[pMetalView engineWindow];
	if (pWindow != nullptr)
	{
		pWindow->Close();
	}
}

- (void)windowScene:(UIWindowScene*)windowScene
	didUpdateCoordinateSpace:(id<UICoordinateSpace>)previousCoordinateSpace
			interfaceOrientation:(UIInterfaceOrientation)previousInterfaceOrientation
					 traitCollection:(UITraitCollection*)previousTraitCollection
{
	if (windowScene.keyWindow != nil)
	{
		UIWindow* window = windowScene.keyWindow;

		const CGRect newBounds = windowScene.coordinateSpace.bounds;
		window.bounds = newBounds;
		window.frame = newBounds;

		MetalView* pView = [window.rootViewController.view findAsType:[MetalView class]];
		ngine::Rendering::Window* pWindow = (ngine::Rendering::Window*)[pView engineWindow];
		if (pWindow != nullptr)
		{
			if (windowScene.interfaceOrientation != previousInterfaceOrientation)
			{
				pWindow->OnDisplayRotationChanged();
			}

#if !PLATFORM_APPLE_VISIONOS
			if (window.screen != previousScreen)
			{
				previousScreen = window.screen;
				pWindow->OnMoved(ngine::Math::Zero);
			}
#endif
		}
	}
}

- (void)windowScene:(UIWindowScene*)windowScene
	performActionForShortcutItem:(UIApplicationShortcutItem*)shortcutItem
						 completionHandler:(void (^)(BOOL succeeded))completionHandler API_UNAVAILABLE(tvos)
{
}

- (void)windowScene:(UIWindowScene*)windowScene userDidAcceptCloudKitShareWithMetadata:(CKShareMetadata*)cloudKitShareMetadata
{
	// TODO: Investigate CloudKit sharing
}

- (void)scene:(UIScene*)scene openURLContexts:(NSSet<UIOpenURLContext*>*)URLContexts
{
	using namespace ngine;

	UIWindowScene* windowScene = scene != nil ? (UIWindowScene*)scene : self.window.windowScene;
	UIWindow* pUIWindow = windowScene.keyWindow;
	MetalView* pView = [pUIWindow.rootViewController.view findAsType:[MetalView class]];
	Rendering::Window* pWindow = (Rendering::Window*)[pView engineWindow];

	if (!hasCreatedWindow || pWindow == nullptr)
	{
		Rendering::Window::QueueOnWindowThread(
			[self, scene, URLContexts]()
			{
				[self scene:scene openURLContexts:URLContexts];
			}
		);
		return;
	}

	EnumFlags<Rendering::Window::OpenDocumentFlags> openDocumentFlags;

	FixedCapacityVector<Widgets::DocumentData> documents(Memory::Reserve, (uint32)[URLContexts count]);
	for (const UIOpenURLContext* context : URLContexts)
	{
		if (context.URL.fileURL)
		{
			[[maybe_unused]] const bool isSecurityScoped = [context.URL startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			if (isSecurityScoped)
			{
				// Save in user defaults so we can access this resource in future sessions
				NSData* bookmarkData = [context.URL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
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

			const char* filePath = context.URL.fileSystemRepresentation;

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
			NSString* string = context.URL.absoluteString;
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
				[[maybe_unused]] const bool isSecurityScoped = [context.URL startAccessingSecurityScopedResource];
#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
				if (isSecurityScoped)
				{
					// Save in user defaults so we can access this resource in future sessions
					NSData* bookmarkData = [context.URL bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
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

- (void)scene:(UIScene*)scene continueUserActivity:(NSUserActivity*)userActivity
{
	using namespace ngine;

	UIWindowScene* windowScene = scene != nil ? (UIWindowScene*)scene : self.window.windowScene;
	UIWindow* pUIWindow = windowScene.keyWindow;
	MetalView* pView = [pUIWindow.rootViewController.view findAsType:[MetalView class]];
	Rendering::Window* pWindow = (Rendering::Window*)[pView engineWindow];

	if (!hasCreatedWindow || pWindow == nullptr)
	{
		Rendering::Window::QueueOnWindowThread(
			[self, scene, userActivity]()
			{
				[self scene:scene continueUserActivity:userActivity];
			}
		);
		return;
	}

	// if(userActivity.activityType == NSUserActivityTypeBrowsingWeb)
	{
		Vector<Widgets::DocumentData> documents;
		EnumFlags<Rendering::Window::OpenDocumentFlags> openDocumentFlags{Rendering::Window::OpenDocumentFlags::CopyToLocal};

		NSURL* url = userActivity.webpageURL;

		NSString* string = url.absoluteString;
		IO::URI uri = IO::URI{IO::URI::StringType{[string UTF8String], (IO::URI::SizeType)[string length]}};
		if (Optional<IO::URI> unescapedURI = IO::URI::Unescape(uri))
		{
			uri = Move(*unescapedURI);
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
			const IO::PathView extension = path.GetRightMostExtension();
			if (extension == ProjectAssetFormat.metadataFileExtension && path.GetFileNameWithoutExtensions() != MAKE_PATH("Project"))
			{
				documents.EmplaceBack(Widgets::DocumentData{
					IO::Path::Combine(path, IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
				});
			}
			else if (extension == ProjectAssetFormat.metadataFileExtension.GetStringView() || extension == Asset::Asset::FileExtension.GetStringView())
			{
				documents.EmplaceBack(Widgets::DocumentData{IO::Path(path)});
			}
		}
		else if (protocol == MAKE_URI_LITERAL("https://"))
		{
			if (uri.GetView().HasQueryString())
			{
				documents.EmplaceBack(Widgets::DocumentData{IO::URI(uri)});
			}
			else if (const IO::URIView extension = uri.GetRightMostExtension();
			         extension == ProjectAssetFormat.metadataFileExtension.GetStringView() ||
			         extension == Asset::Asset::FileExtension.GetStringView())
			{
				if (extension == ProjectAssetFormat.metadataFileExtension.GetStringView() && uri.GetFileNameWithoutExtensions() != MAKE_URI("Project"))
				{
					documents.EmplaceBack(Widgets::DocumentData{
						IO::URI::Combine(uri, IO::URI::Merge(MAKE_URI("Project"), ProjectAssetFormat.metadataFileExtension.GetStringView()))
					});
				}
				else
				{
					documents.EmplaceBack(uri);
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
					}
				}
				else
				{
					documents.EmplaceBack(uri);
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
}

- (BOOL)dropInteraction:(UIDropInteraction*)interaction canHandleSession:(id<UIDropSession>)session
{
	return [session hasItemsConformingToTypeIdentifiers:@[ @"com.ngine.Editor-asset" ]];
}

- (void)dropInteraction:(UIDropInteraction*)interaction sessionDidEnter:(id<UIDropSession>)session
{
}

- (UIDropProposal*)dropInteraction:(UIDropInteraction*)interaction sessionDidUpdate:(id<UIDropSession>)session
{
	return [[UIDropProposal alloc] initWithDropOperation:UIDropOperationCopy];
}

- (void)dropInteraction:(UIDropInteraction*)interaction sessionDidExit:(id<UIDropSession>)session
{
}

- (void)dropInteraction:(UIDropInteraction*)interaction sessionDidEnd:(id<UIDropSession>)session
{
}

- (void)dropInteraction:(UIDropInteraction*)interaction performDrop:(id<UIDropSession>)session
{
	using namespace ngine;

	// Wait for the window to finish creation if necessary
	while (!hasCreatedWindow)
		;

	UIWindow* pUIWindow = self.window;
	MetalView* pView = [pUIWindow.rootViewController.view findAsType:[MetalView class]];
	Rendering::Window& window = *(Rendering::Window*)[pView engineWindow];

	CGPoint point = [session locationInView:pView];
	const float scaleFactor = window.GetPhysicalDevicePixelRatio();
	point.x *= scaleFactor;
	point.y *= scaleFactor;

	const ScreenCoordinate coordinate{Math::Vector2d{point.x, point.y}};

	struct DragAndDropData
	{
		Threading::Atomic<uint32> m_loadedItemCount{0};
		InlineVector<ngine::Widgets::DragAndDropData, 4> m_draggedItems;
	};

	SharedPtr<DragAndDropData> pData = SharedPtr<DragAndDropData>::Make(DragAndDropData{
		0,
		InlineVector<Widgets::DragAndDropData, 4>{Memory::ConstructWithSize, Memory::Uninitialized, (uint32)session.items.count}
	});

	for (uint32 i = 0, n = (uint32)session.items.count; i < n; ++i)
	{
		UIDragItem* dragItem = [session.items objectAtIndex:i];
		[dragItem.itemProvider
			loadInPlaceFileRepresentationForTypeIdentifier:UTTypeURL.identifier
																	 completionHandler:^(
																		 NSURL* url,
																		 [[maybe_unused]] BOOL openedInPlace,
																		 [[maybe_unused]] NSError* _Nullable error
																	 ) {
																		 //	NSURL* url = (NSURL*)urlItem;
																		 if (url.isFileURL)
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
																					 NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults
																						 objectForKey:@"securityBookmarks"];
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
																			 DragAndDropData& data = *pData;
																			 new (&data.m_draggedItems[i])
																				 Widgets::DragAndDropData{IO::Path(filePath, (IO::Path::SizeType)strlen(filePath))};

																			 if (++data.m_loadedItemCount == data.m_draggedItems.GetSize())
																			 {
																				 // TODO: Restructure OnStartDragItemsIntoWindow to allow files to
					                               // be empty to indicate unknown contents Unfortunately iOS drag and
					                               // drop limits us to only know what is being dropped once it has
					                               // been dropped.
																				 const bool startedDrag = window.OnStartDragItemsIntoWindow(
																					 window.ConvertScreenToLocalCoordinates(coordinate),
																					 data.m_draggedItems
																				 );
																				 if (startedDrag)
																				 {
																					 [[maybe_unused]] const bool wasDropped = window.OnDropItemsIntoWindow(
																						 window.ConvertScreenToLocalCoordinates(coordinate),
																						 data.m_draggedItems
																					 );
																				 }
																			 }
																		 }
																		 else
																		 {
																			 Assert(false, "Not implemented");
																		 }
																	 }];
	}
}

- (void)sceneDidEnterBackground:(UIScene*)scene
{
}

- (void)sceneWillResignActive:(UIScene*)scene
{
	using namespace ngine;
	UIWindowScene* windowScene = (UIWindowScene*)scene;
	UIWindow* pUIWindow = windowScene.keyWindow;
	MetalView* pView = [pUIWindow.rootViewController.view findAsType:[MetalView class]];
	Rendering::Window* pWindow = (Rendering::Window*)[pView engineWindow];
	if (pWindow != nullptr)
	{
		pWindow->OnLostKeyboardFocus();
		pWindow->OnSwitchToBackground();
		Rendering::LogicalDevice& logicalDevice = pWindow->GetLogicalDevice();
		logicalDevice.WaitUntilIdle();
		Assert(!pWindow->IsInForeground());
	}
}

- (void)sceneWillEnterForeground:(UIScene*)scene
{
}

- (void)sceneDidBecomeActive:(UIScene*)scene
{
	using namespace ngine;
	UIWindowScene* windowScene = (UIWindowScene*)scene;
	UIWindow* pUIWindow = windowScene.keyWindow;
	UIViewController* pViewController = pUIWindow.rootViewController;
	UIView* pView = pViewController.view;
	MetalView* pMetalView = [pView findAsType:[MetalView class]];
	Rendering::Window* pWindow = (Rendering::Window*)[pMetalView engineWindow];
	if (pWindow != nullptr)
	{
		pWindow->OnSwitchToForeground();
		pWindow->OnReceivedKeyboardFocus();
	}
}

@end
#endif
