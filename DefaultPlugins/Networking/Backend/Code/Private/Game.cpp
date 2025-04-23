#include "Game.h"
#include "Common.h"

#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Event/EventManager.h>
#include <Engine/DataSource/Data.h>
#include <Engine/DataSource/CachedQuery.h>

#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Texture/TextureAsset.h>

#include <Backend/Plugin.h>
#include <Http/ResponseCode.h>
#include <Http/FileCache.h>

#include <Renderer/Window/Window.h>

#if PLATFORM_APPLE
#include <Renderer/Window/iOS/AppDelegate.h>
#include <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/ApplePayView.h>

#import <StoreKit/SKProduct.h>

#if PLATFORM_APPLE_IOS
#include <GoogleSignIn/GoogleSignIn.h>
#endif

#endif

#if USE_STEAM
PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wsigned-enum-bitfield")
DISABLE_CLANG_WARNING("-Winvalid-utf8")
DISABLE_CLANG_WARNING("-Wcast-align")

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(4996)

#include <steam/steam_api.h>

POP_MSVC_WARNINGS
POP_CLANG_WARNINGS
#endif

#if PLATFORM_ANDROID
#include <Common/../../../Launchers/Common/Android/GameActivityWindow.h>
#endif

#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Math/Random.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Version.h>
#include <Common/IO/URI.h>
#include <Common/IO/Log.h>
#include <Common/IO/PathView.h>
#include <Common/System/Query.h>
#include <Common/Time/Format/Timestamp.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Format/Guid.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Project System/PluginAssetFormat.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#if PLATFORM_EMSCRIPTEN
#include <emscripten/threading.h>
#endif

namespace ngine::Networking::Backend
{
	[[nodiscard]] bool SupportsSignInWithApple()
	{
		if constexpr (PLATFORM_APPLE)
		{
			return true;
		}

#if PLATFORM_EMSCRIPTEN
		// Current multi-threading restrictions prevent third-party sign in popups & referrals
		return !emscripten_has_threading_support();
#else
		return false;
#endif
	}

	[[nodiscard]] bool SupportsSignInWithGoogle()
	{
		if constexpr (HAS_GOOGLE_SIGN_IN_SDK || PLATFORM_ANDROID)
		{
			return true;
		}

#if PLATFORM_EMSCRIPTEN
		// Current multi-threading restrictions prevent third-party sign in popups & referrals
		return !emscripten_has_threading_support();
#else
		return false;
#endif
	}

	Game::Game(Plugin& plugin)
		: m_supportedSignInFlags{SignInFlags::SignedIn | SignInFlags::Guest | (SignInFlags::Apple * SupportsSignInWithApple()) | (SignInFlags::Google * SupportsSignInWithGoogle()) | SignInFlags::Email | SignInFlags::InternalFlags}
		, m_cachedData(MAKE_PATH("Assets.json"))
		, m_backend(plugin)
		, m_assetDatabase(*this)
		, m_persistentUserStorage(*this)
		, m_playersDataSource(*this)
	{
		ParseCachedData();

#if USE_STEAM
		SteamErrMsg errorMessage;
		if (SteamAPI_InitEx(&errorMessage) == k_ESteamAPIInitResult_OK)
		{
			m_supportedSignInFlags |= SignInFlags::Steam;
		}
#endif

		if (m_cachedData.IsValid())
		{
			// Only allow offline & cached modes if we have successfully launched once before and have cached datas
			for (const SignInFlags signInProvider : m_cachedData.GetSignInProviders())
			{
				const CachedData::LoginInfo& cachedLoginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (cachedLoginInfo.userIdentifier.HasElements() || cachedLoginInfo.authorizationToken.HasElements())
				{
					m_supportedSignInFlags |= SignInFlags::Offline | SignInFlags::Cached;
					break;
				}
			}
		}

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.AddRequestMoreDataListener(
			dataSourceCache.FindOrRegister(Asset::Manager::DataSourceGuid),
			DataSource::Cache::OnRequestMoreDataEvent::ListenerData{
				*this,
				[this](
					Game&,
					const uint32 maximumCount,
					const DataSource::PropertyIdentifier sortedPropertyIdentifier,
					const DataSource::SortingOrder sortingOrder,
					const Optional<const Tag::Query*> pTagQuery,
					const DataSource::SortedQueryIndices& sortingQuery,
					const DataSource::GenericDataIndex lastDataIndex
				)
				{
					if (pTagQuery.IsInvalid())
					{
						// Don't submit backend request for queryless data sources
						return;
					}

					Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
					const Tag::Identifier projectTagIdentifier = tagRegistry.FindOrRegister(ProjectAssetFormat.assetTypeGuid);
					if (pTagQuery->m_allowedFilterMask.IsSet(projectTagIdentifier))
					{
						RequestPaginatedParameters requestPaginatedParameters = {};
						requestPaginatedParameters.count = maximumCount;

						DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
						static const DataSource::PropertyIdentifier playsCountPropertyIdentifier =
							dataSourceCache.FindOrRegisterPropertyIdentifier("asset_plays_count");
						static const DataSource::PropertyIdentifier likesCountPropertyIdentifier =
							dataSourceCache.FindOrRegisterPropertyIdentifier("asset_likes_count");
						if (sortedPropertyIdentifier == playsCountPropertyIdentifier)
						{
							requestPaginatedParameters.orderBy = Networking::Backend::Game::RequestOrderBy::Plays;
						}
						else if (sortedPropertyIdentifier == likesCountPropertyIdentifier)
						{
							requestPaginatedParameters.orderBy = Networking::Backend::Game::RequestOrderBy::Likes;
						}
						else
						{
							requestPaginatedParameters.orderBy = Networking::Backend::Game::RequestOrderBy::Id;
						}

						requestPaginatedParameters.tags.Reserve(pTagQuery->m_requiredFilterMask.GetNumberOfSetBits());
						static const Array ignoredTagIdentifiers{
							tagRegistry.FindOrRegister(Tags::ProjectTemplateTagGuid),
							tagRegistry.FindOrRegister(Tags::EditableLocalProjectTagGuid),
							tagRegistry.FindOrRegister(Tags::PlayableLocalProjectTagGuid),
						};
						for (const Tag::Identifier::IndexType requiredTagIdentifierIndex :
				         pTagQuery->m_requiredFilterMask.GetSetBitsIterator(0, tagRegistry.GetMaximumUsedIdentifierCount()))
						{
							const Tag::Identifier requiredTagIdentifier = Tag::Identifier::MakeFromValidIndex(requiredTagIdentifierIndex);
							if (!ignoredTagIdentifiers.GetView().Contains(requiredTagIdentifier))
							{
								requestPaginatedParameters.tags.EmplaceBack(tagRegistry.GetAssetGuid(requiredTagIdentifier));
							}
						}

						requestPaginatedParameters.sortingOrder = sortingOrder;

						Asset::Manager& assetManager = System::Get<Asset::Manager>();

						assetManager.LockRead();

						assetManager.IterateData(
							sortingQuery,
							[&backendAssetDatabase = m_assetDatabase,
				       &requestPaginatedParameters,
				       &assetManager](const ngine::DataSource::Interface::Data data)
							{
								const Asset::Identifier assetIdentifier = data.GetExpected<Asset::Identifier>();
								const Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);
								requestPaginatedParameters.afterAssetId = backendAssetDatabase.GetBackendAssetIdentifier(assetGuid);
							},
							Math::Range<ngine::DataSource::GenericDataIndex>::MakeStartToEnd(lastDataIndex, 1)
						);
						Assert(requestPaginatedParameters.afterAssetId != 0);

						assetManager.UnlockRead();

						RequestPaginatedAssetsOfType(
							ProjectAssetFormat.assetTypeGuid,
							requestPaginatedParameters,
							[this](const bool, const Networking::HTTP::ResponseCode responseCode, const Serialization::Reader assetReader)
							{
								if (responseCode.IsSuccessful())
								{
									Asset::Manager& assetManager = System::Get<Asset::Manager>();
									HandlePaginatedAssetResponse(assetReader, assetManager);

									/*if (const Optional<uint32> lastAssetId = assetReader.Read<uint32>("last_asset_id");
						        lastAssetId.IsValid() && *lastAssetId != previousLastAssetId)
						      {
						      }*/
								}
								else
								{
									Assert(false);
								}
							}
						);
					}

					// TODO: Support for known backend contexts / asset type guids too
				}
			}
		);
	}

	Game::~Game()
	{
#if USE_STEAM
		SteamAPI_Shutdown();
#endif
	}

	void Game::OnSignInCompletedInternal(
		const EnumFlags<SignInFlags> signInFlags,
		Optional<CachedData::LoginInfo>&& cachedLogInInfo,
		SignInCallback&& completeSignInCallback,
		const Networking::HTTP::ResponseCode responseCode,
		const Optional<Serialization::Reader> responseReader,
		const Optional<Rendering::Window*> pWindow
	)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide a sign in provider");
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");
		Assert(signInFlags.AreNoneSet(SignInFlags::InternalFlags), "Internal flags should not be propagated");
		Assert(cachedLogInInfo.IsInvalid() || signInFlags.IsNotSet(SignInFlags::Cached), "Cached log-in must not update stored details");

		Assert(m_signInFlags.AreNoneSet(SignInFlags::AllProviders), "Sign in can't complete while already logged in");

		if (signInFlags.IsSet(SignInFlags::SignedIn))
		{
			m_signInFlags |= signInFlags;

			completeSignInCallback(m_signInFlags.GetFlags());

			const SignInFlags signInProvider = (signInFlags & SignInFlags::AllProviders).GetFlags();
			ConstStringView signInMode;
			if (signInFlags.IsSet(SignInFlags::Offline))
			{
				signInMode = "offline";
			}
			else if (signInFlags.IsSet(SignInFlags::Cached))
			{
				signInMode = "cached";
			}
			else
			{
				signInMode = "explicit";
			}

			LogMessage("Registered {} session with {}", signInMode, GetSignInFlagString(signInProvider));

			if (cachedLogInInfo.IsValid())
			{
				m_cachedData.SetLoginInfo(signInProvider, Move(*cachedLogInInfo));
			}
			else if (signInFlags.IsNotSet(SignInFlags::Cached))
			{
				// If this was a user-initiated log-in that didn't result in a cached login, clear all prior cached logins
				ClearCachedLoginInfo(signInProvider, pWindow);
			}

			if (responseReader.IsValid())
			{
				m_sessionToken = *responseReader->Read<String>("session_token");
				m_sessionTokenHeader = Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"x-session-token", m_sessionToken};
				const PersistentUserIdentifier localPlayerInternalIdentifier = PersistentUserIdentifier{
					*responseReader->Read<PersistentUserIdentifier::Type>("player_id")
				};
				Time::Timestamp accountCreationDate = responseReader->ReadWithDefaultValue<Time::Timestamp>("player_created_at", Time::Timestamp{});

				m_publicUserId = *responseReader->Read<ConstStringView>("public_uid");
				m_signInFlags |= SignInFlags::HasSessionToken |
				                 SignInFlags::IsFirstSession * !responseReader->ReadWithDefaultValue("seen_before", true);
				m_cachedData.SetLocalPlayerInternalIdentifier(localPlayerInternalIdentifier);

				m_playersDataSource.FindOrEmplacePlayer(
					localPlayerInternalIdentifier,
					[this, accountCreationDate](DataSource::Players::PlayerInfo& localPlayerInfo)
					{
						m_localPlayerIdentifier = localPlayerInfo.identifier;

						Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
						localPlayerInfo.tags.Set(tagRegistry.FindOrRegister(DataSource::Players::LocalPlayerTagGuid));
						localPlayerInfo.tags.Set(tagRegistry.FindOrRegister(DataSource::Players::GameCreatorGuid));

						localPlayerInfo.accountCreationDate = accountCreationDate;
						m_playersDataSource.OnDataChanged();
					}
				);

				if (const Optional<Engine*> pEngine = System::Find<Engine>())
				{
					Threading::JobBatch endSessionJobBatch{Threading::JobBatch::IntermediateStage};
					Threading::IntermediateStage& finishedSessionEndStage = Threading::CreateIntermediateStage();
					finishedSessionEndStage.AddSubsequentStage(endSessionJobBatch.GetFinishedStage());
					endSessionJobBatch.QueueAfterStartStage(Threading::CreateCallback(
						[this, &finishedSessionEndStage](Threading::JobRunnerThread&)
						{
							if (m_signInFlags.TryClearFlags(SignInFlags::HasSessionToken))
							{
								QueueRequest(
									HTTP::RequestType::Post,
									MAKE_URI("game/v1/session/end"),
									{},
									[&finishedSessionEndStage](const bool, const HTTP::ResponseCode, const Serialization::Reader)
									{
										// TODO: Do this when we finish sending instead of when we receive a response
										finishedSessionEndStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
									}
								);
							}
						},
						Threading::JobPriority::EndNetworkSession
					));
					pEngine->GetQuitJobBatch().QueueAfterStartStage(endSessionJobBatch);
				}

				// Get SAS token to download files
				RequestFileURIToken();
			}

			m_signInFlags |= SignInFlags::FinishedSessionRegistration;

			{
				Threading::UniqueLock lock(m_queuedRequestsMutex);
				for (QueuedFunction& function : m_sessionRegistrationCallbacks)
				{
					function(m_signInFlags.GetFlags());
				}
				m_sessionRegistrationCallbacks.Clear();
			}

			if (signInFlags.IsNotSet(SignInFlags::Offline))
			{
				RequestPlayerAssets(ProjectAssetFormat.assetTypeGuid);
			}
			else
			{
				// Trigger file token callbacks
				Threading::UniqueLock lock(m_queuedRequestsMutex);
				for (QueuedFunction& function : m_fileTokenCallbacks)
				{
					function(m_signInFlags.GetFlags());
				}
				m_fileTokenCallbacks.Clear();

				Events::Manager& eventsManager = System::Get<Events::Manager>();
				// Notify that a user with an existing profile signed in
				eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent("2f0444f8-4ed0-4b7f-a40c-55ed32652351"_guid));
			}
		}
		else
		{
			const SignInFlags signInProvider = (signInFlags & SignInFlags::AllProviders).GetFlags();
			LogMessage("Failed to register session with {}", GetSignInFlagString(signInProvider));

			completeSignInCallback(EnumFlags<SignInFlags>{m_signInFlags.GetFlags()} | signInFlags);

			if (!responseCode.IsConnectionFailure())
			{
				// Ensure we don't reuse cached log-in info for this provider
				ClearCachedLoginInfo(signInProvider, pWindow);
			}
		}
	}

	void Game::OnStartupRequestsFinishedInternal(const EnumFlags<SignInFlags> signInFlags)
	{
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");

		{
			Threading::UniqueLock lock(m_queuedRequestsMutex);
			for (QueuedFunction& function : m_startupRequestsCallbacks)
			{
				function(signInFlags);
			}
			m_startupRequestsCallbacks.Clear();
		}

		m_cachedData.SaveIfNecessary();
		Networking::HTTP::FileCache::GetInstance().SaveIfNecessary();
	}

	ConstStringView GetSignInFlagString(const SignInFlags signInProvider)
	{
		switch (signInProvider)
		{
			case SignInFlags::Apple:
				return "Apple";
			case SignInFlags::Google:
				return "Google";
			case SignInFlags::Email:
				return "Email";
			case SignInFlags::Steam:
				return "Steam";
			case SignInFlags::Guest:
				return "Guest";
			case SignInFlags::Offline:
				return "Offline";
			case SignInFlags::Cached:
				return "Cached";
			case SignInFlags::SignedIn:
				return "Signed In";
			case Backend::SignInFlags::None:
			case SignInFlags::ProviderCount:
			case SignInFlags::AllProviders:
			case SignInFlags::RequestedSignIn:
			case SignInFlags::HasSessionToken:
			case SignInFlags::IsFirstSession:
			case SignInFlags::HasFileToken:
			case SignInFlags::HasLocalPlayerProfile:
			case SignInFlags::FinishedSessionRegistration:
			case SignInFlags::CompletedCoreRequests:
			case SignInFlags::InternalFlags:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	SignInFlags GetSignInFlag(const ConstStringView signInProviderString)
	{
		for (const SignInFlags signInProvider : EnumFlags<SignInFlags>{SignInFlags::AllProviders})
		{
			if (signInProviderString.EqualsCaseInsensitive(GetSignInFlagString(signInProvider)))
			{
				return signInProvider;
			}
		}
		return SignInFlags::None;
	}

	void Game::CheckCanSignInAsync(EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, Rendering::Window& window)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide at least one sign in provider");
		Assert(signInFlags.IsNotSet(SignInFlags::Success), "Success flag should not be set when querying sign in");
		Assert(signInFlags.AreNoneSet(SignInFlags::InternalFlags), "Internal flags should not be used");

		const EnumFlags<SignInFlags> supportedSignInFlags = m_supportedSignInFlags;
		const EnumFlags<SignInFlags> signInProviders = signInFlags & SignInFlags::AllProviders;
		signInFlags = signInFlags & ~SignInFlags::AllProviders;
		if (supportedSignInFlags.AreAnyNotSet(signInFlags))
		{
			// Indicate that the sign in isn't supported
			for (const SignInFlags signInProvider : signInProviders)
			{
				callback(signInFlags | signInProvider);
			}
			return;
		}

		for (const SignInFlags signInProvider : signInProviders)
		{
			if (supportedSignInFlags.IsNotSet(signInProvider))
			{
				// Indicate that sign in with the specified provider isn't supported
				callback(signInFlags | signInProvider);
				continue;
			}

			if (signInFlags.IsSet(SignInFlags::Offline))
			{
				callback(signInFlags | signInProvider | SignInFlags::Supported);
				continue;
			}

			if (signInFlags.IsSet(SignInFlags::Cached))
			{
				CheckCanCachedSignInAsyncInternal(signInProvider, signInFlags, SignInCallback{callback}, window);
			}
			else
			{
				// Report that we can support regular sign in with the provider
				callback(signInFlags | signInProvider | SignInFlags::Supported);
			}
		}
	}

	void Game::CheckCanCachedSignInAsyncInternal(
		const SignInFlags signInProvider,
		const EnumFlags<SignInFlags> signInFlags,
		SignInCallback&& callback,
		[[maybe_unused]] Rendering::Window& window
	)
	{
		Assert(m_supportedSignInFlags.IsSet(signInProvider));
		Assert(m_supportedSignInFlags.AreAllSet(signInFlags));
		Assert(signInFlags.IsSet(SignInFlags::Cached), "Cached flag must be set");
		Assert(signInFlags.IsNotSet(SignInFlags::Success), "Success flag should not be set when querying sign in");
		// Check if cached sign in is possible
		switch (signInProvider)
		{
			case SignInFlags::Apple:
			{
				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
#if PLATFORM_APPLE
					Rendering::Window::QueueOnWindowThread(
						[this, signInFlags = signInFlags | signInProvider, callback = Forward<SignInCallback>(callback), &window]()
						{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
							UIWindow* pWindow = (__bridge UIWindow*)window.GetOSHandle();
							ViewController* rootController = (ViewController*)[pWindow rootViewController];
#else
							NSWindow* pWindow = (__bridge NSWindow*)window.GetOSHandle();
							ViewController* rootController = (ViewController*)[pWindow contentViewController];
#endif
							const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(SignInFlags::Apple);
							NSString* userIdentifierNS = [[NSString alloc] initWithBytes:loginInfo.userIdentifier.GetData()
																																		length:loginInfo.userIdentifier.GetDataSize()
																																	encoding:NSUTF16LittleEndianStringEncoding];
							NSString* refreshTokenNS = [[NSString alloc] initWithBytes:loginInfo.authorizationToken.GetData()
																																	length:loginInfo.authorizationToken.GetDataSize()
																																encoding:NSUTF16LittleEndianStringEncoding];

							[rootController checkCachedAppleSignInState:^(const AppleCachedSignInQueryResult result) {
								switch (result)
								{
									case AppleCachedSignInQueryResultRevoked:
									case AppleCachedSignInQueryResultNotFound:
									{
										// Clear the cached log-in info
										ClearCachedLoginInfo((signInFlags & SignInFlags::AllProviders).GetFlags(), window);
										// Indicate that cached sign in isn't possible
										callback(signInFlags);
									}
									break;
									case AppleCachedSignInQueryResultAuthorized:
										callback(signInFlags | SignInFlags::Supported);
										break;
									case AppleCachedSignInQueryResultTransferred:
									{
										Assert(false, "Sign in with Apple credential transfer is not currently supported");
										// Clear the cached log-in info
										ClearCachedLoginInfo((signInFlags & SignInFlags::AllProviders).GetFlags(), window);
										// Indicate that cached sign in isn't possible
										callback(signInFlags);
									}
									break;
								}
							}
																					 userIdentifier:userIdentifierNS
																						 refreshToken:refreshTokenNS];
						}
					);
					return;
#endif
				}
			}
			break;
			case SignInFlags::Google:
			{
#if HAS_GOOGLE_SIGN_IN_SDK
				if ([[GIDSignIn sharedInstance] hasPreviousSignIn])
				{
					callback(signInFlags | signInProvider | SignInFlags::Supported);
					return;
				}
#endif

				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
					callback(signInFlags | signInProvider | SignInFlags::Supported);
					return;
				}
			}
			break;
			case SignInFlags::Email:
			{
				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
					callback(signInFlags | signInProvider | SignInFlags::Supported);
					return;
				}
			}
			break;
			case SignInFlags::Steam:
			{
#if USE_STEAM
				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements())
				{
					if (ISteamUser* pSteamUser = SteamUser())
					{
						const CSteamID steamIdentifier = pSteamUser->GetSteamID();
						if (steamIdentifier.IsValid())
						{
							callback(signInFlags | signInProvider | SignInFlags::Supported);
							return;
						}
					}
				}
#endif
			}
			break;
			case SignInFlags::Guest:
			{
				// Allow "cached" / automatic guest sign in if it's the only avaialble log-in method
				const EnumFlags<SignInFlags> supportedProviders = m_supportedSignInFlags & SignInFlags::AllProviders;
				if ((supportedProviders & ~SignInFlags::Offline).GetNumberOfSetFlags() == 1)
				{
					callback(signInFlags | signInProvider | SignInFlags::Supported);
					return;
				}
			}
			break;
			case Backend::SignInFlags::None:
			case SignInFlags::SignedIn:
			case SignInFlags::Offline:
			case SignInFlags::Cached:
			case SignInFlags::ProviderCount:
			case SignInFlags::AllProviders:
			case SignInFlags::RequestedSignIn:
			case SignInFlags::HasSessionToken:
			case SignInFlags::IsFirstSession:
			case SignInFlags::HasFileToken:
			case SignInFlags::HasLocalPlayerProfile:
			case SignInFlags::FinishedSessionRegistration:
			case SignInFlags::CompletedCoreRequests:
			case SignInFlags::InternalFlags:
				ExpectUnreachable();
		}

		// Indicate that sign in with the specified flags isn't supported
		callback(signInFlags | signInProvider);
	}

	void Game::SignIn(const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, Rendering::Window& window)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide a sign in provider");
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");
		Assert(signInFlags.IsNotSet(SignInFlags::SignedIn), "Signed in flag should not be set when requesting sign in");
		Assert(signInFlags.AreNoneSet(SignInFlags::InternalFlags), "Internal flags should not be used");
		Assert(m_signInFlags.AreNoneSet(SignInFlags::AllProviders), "Can't sign in twice");

		if (signInFlags.IsSet(SignInFlags::Cached))
		{
			CompleteCachedSignInInternal(signInFlags, Forward<SignInCallback>(callback), window);
		}
		else if (signInFlags.IsSet(SignInFlags::Offline))
		{
			CompleteOfflineSignInInternal(signInFlags | SignInFlags::Cached, Forward<SignInCallback>(callback), window);
		}
		else
		{
			SignInInternal(signInFlags, Forward<SignInCallback>(callback), window);
		}
	}

	void Game::CompleteCachedSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide a sign in provider");
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");
		Assert(signInFlags.IsNotSet(SignInFlags::SignedIn), "Signed in flag should not be set when requesting sign in");
		Assert(signInFlags.IsSet(SignInFlags::Cached), "Cached flags must be set");
		Assert(signInFlags.IsNotSet(SignInFlags::Offline), "Offline flag must not be set");
		Assert(signInFlags.IsNotSet(SignInFlags::Success), "Success flag should not be set when completing cached sign in");
		Assert(signInFlags.AreNoneSet(SignInFlags::InternalFlags), "Internal flags should not be used");

		const EnumFlags<SignInFlags> supportedSignInFlags{m_supportedSignInFlags};

		const SignInFlags signInProvider = (signInFlags & SignInFlags::AllProviders).GetFlags();
		switch (signInProvider)
		{
			case SignInFlags::Steam:
			{
				CompleteSteamSignInInternal(signInFlags, Forward<SignInCallback>(callback), pWindow);
			}
			break;
			case SignInFlags::Apple:
			{
				CompleteCachedAppleSignInInternal(signInFlags, Forward<SignInCallback>(callback), pWindow);
			}
			break;
			case SignInFlags::Google:
			{
#if HAS_GOOGLE_SIGN_IN_SDK
				if ([[GIDSignIn sharedInstance] hasPreviousSignIn])
				{
					[GIDSignIn.sharedInstance restorePreviousSignInWithCompletion:^(GIDGoogleUser* user, NSError* err) {
						if (err != nil)
						{
							// Indicate that signing in failed
							callback(signInFlags);
							return;
						}
						if (user == nil)
						{
							// Indicate that signing in failed
							callback(signInFlags);
							return;
						}

						if (user.refreshToken == nil)
						{
							// Indicate that signing in failed
							callback(signInFlags);
							return;
						}

						NSString* userID = user.userID;
						GIDToken* refreshToken = user.refreshToken;
						NSString* refreshTokenString = refreshToken.tokenString;

						const ConstStringView userIdentifierView{[userID UTF8String], (uint32)[userID length]};
						const ConstStringView refreshTokenView{[refreshTokenString UTF8String], (uint32)[refreshTokenString length]};
						m_cachedData
							.SetLoginInfo(SignInFlags::Google, CachedData::LoginInfo{UnicodeString{userIdentifierView}, UnicodeString{refreshTokenView}});

						CompleteCachedGoogleSignInInternal(signInFlags, Move(callback), pWindow);
					}];
					return;
				}
#endif

				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
					CompleteCachedGoogleSignInInternal(signInFlags, Move(callback), pWindow);
				}
			}
			break;
			case SignInFlags::Email:
			{
				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(signInProvider);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
					CompleteEmailSignInInternal(signInFlags, Move(callback), loginInfo.userIdentifier, loginInfo.authorizationToken, pWindow);
				}
			}
			break;
			case SignInFlags::Guest:
			{
				SignInInternal(SignInFlags::Guest, Forward<SignInCallback>(callback), pWindow);
			}
			break;
			case Backend::SignInFlags::None:
			case SignInFlags::SignedIn:
			case SignInFlags::Offline:
			case SignInFlags::Cached:
			case SignInFlags::ProviderCount:
			case SignInFlags::AllProviders:
			case SignInFlags::RequestedSignIn:
			case SignInFlags::HasSessionToken:
			case SignInFlags::IsFirstSession:
			case SignInFlags::HasFileToken:
			case SignInFlags::HasLocalPlayerProfile:
			case SignInFlags::FinishedSessionRegistration:
			case SignInFlags::CompletedCoreRequests:
			case SignInFlags::InternalFlags:
				ExpectUnreachable();
		}
	}

	void Game::CompleteOfflineSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide a sign in provider");
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");
		Assert(signInFlags.IsNotSet(SignInFlags::SignedIn), "Signed in flag should not be set when requesting sign in");
		Assert(signInFlags.IsSet(SignInFlags::Cached), "Cached flags must be set");
		Assert(signInFlags.IsSet(SignInFlags::Offline), "Offline flags must be set");
		Assert(signInFlags.IsNotSet(SignInFlags::Success), "Success flag should not be set when completing cached offline sign in");
		Assert(signInFlags.AreNoneSet(SignInFlags::InternalFlags), "Internal flags should not be used");

		Assert(m_supportedSignInFlags.AreAllSet(signInFlags));

		OnSignInCompletedInternal(
			signInFlags | SignInFlags::SignedIn,
			Optional<CachedData::LoginInfo>{},
			Move(callback),
			Networking::HTTP::ResponseCode{},
			Invalid,
			pWindow
		);
	}

	void Game::PopulateCommonSignInMetadata(Serialization::Writer writer)
	{
		const Environment& environment = GetEnvironment();
		writer.Serialize("game_key", environment.m_gameAPI.m_apiKey);
		writer.Serialize("development_mode", IsDevelopmentMode(Platform::GetEnvironment()));

		Version gameVersion{0, 0, 1};
		Guid sessionGuid;
		if (const Optional<Engine*> pEngine = System::Find<Engine>())
		{
			gameVersion = pEngine->GetInfo().GetVersion();
			sessionGuid = pEngine->GetSessionGuid();
		}
		else
		{
			sessionGuid = Guid::Generate();
		}
		writer.Serialize("game_version", gameVersion);
		writer.Serialize("session_id", sessionGuid);
	}

	void Game::CompleteSteamSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Steam), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);
		writer.Serialize("platform", ConstStringView{"steam"});
		String steamPlayerIdentifier;
#if USE_STEAM
		steamPlayerIdentifier.Format("{}", SteamUser()->GetSteamID().ConvertToUint64());
#endif
		writer.Serialize("player_identifier", steamPlayerIdentifier);

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/v2/session"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, signInFlags, callback = Forward<SignInCallback>(callback), pWindow, steamPlayerIdentifier = Move(steamPlayerIdentifier)](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				Optional<CachedData::LoginInfo> cachedLoginInfo;
				if (responseCode.IsSuccessful() && signInFlags.IsNotSet(SignInFlags::Cached))
				{
					cachedLoginInfo = CachedData::LoginInfo{steamPlayerIdentifier};
				}

				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Move(cachedLoginInfo),
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::CompleteAppleSignInInternal(
		const EnumFlags<SignInFlags> signInFlags,
		SignInCallback&& callback,
		const ConstStringView userIdentifier,
		const ConstStringView authorizationCode,
		const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Apple), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToDisk);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);
		writer.Serialize("apple_authorization_code", authorizationCode);

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/session/apple"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, signInFlags, callback = Move(callback), pWindow, userIdentifier = String{userIdentifier}](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				Optional<CachedData::LoginInfo> cachedLoginInfo;
				if (responseCode.IsSuccessful())
				{
					if (const Optional<ConstStringView> refreshToken = responseReader.Read<ConstStringView>("refresh_token"))
					{
						cachedLoginInfo = CachedData::LoginInfo{Move(userIdentifier), UnicodeString{*refreshToken}};
					}
				}

				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Move(cachedLoginInfo),
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::CompleteCachedAppleSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Apple), "Sign in provider flag must be set");
		Rendering::Window::QueueOnWindowThread(
			[this, signInFlags, callback = Forward<SignInCallback>(callback), pWindow]() mutable
			{
				const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(SignInFlags::Apple);
				if (loginInfo.userIdentifier.HasElements() && loginInfo.authorizationToken.HasElements())
				{
					Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToDisk);
					Serialization::Writer writer(serializedData);
					PopulateCommonSignInMetadata(writer);
					writer.Serialize("refresh_token", loginInfo.authorizationToken);

					const Environment& environment = GetEnvironment();
					m_backend.QueueRequestInternal(
						HTTP::RequestType::Post,
						MAKE_URI("game/session/apple"),
						Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
							Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
							environment.m_gameAPI.m_versionHeader
						}
							.GetView(),
						writer.SaveToBuffer<String>(),
						[this, signInFlags, callback = Move(callback), pWindow](
							[[maybe_unused]] const bool success,
							const HTTP::ResponseCode responseCode,
							const Serialization::Reader responseReader
						) mutable
						{
							OnSignInCompletedInternal(
								signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
								Optional<CachedData::LoginInfo>{},
								Move(callback),
								responseCode,
								responseReader,
								pWindow
							);
						}
					);
				}
				else
				{
					OnSignInCompletedInternal(
						signInFlags,
						Optional<CachedData::LoginInfo>{},
						Move(callback),
						Networking::HTTP::ResponseCode{},
						Invalid,
						pWindow
					);
				}
			}
		);
	}

	void Game::CompleteGoogleSignInInternal(
		const EnumFlags<SignInFlags> signInFlags,
		SignInCallback&& callback,
		const ConstStringView userIdentifier,
		const ConstStringView idToken,
		const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Google), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToDisk);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);

		writer.Serialize("id_token", idToken);

		if constexpr (PLATFORM_APPLE)
		{
			writer.Serialize("platform", ConstStringView{"ios"});
		}
		else if constexpr (PLATFORM_ANDROID)
		{
			writer.Serialize("platform", ConstStringView{"android"});
		}
		else if constexpr (PLATFORM_WEB)
		{
			writer.Serialize("platform", ConstStringView{"web"});
		}
		else
		{
			writer.Serialize("platform", ConstStringView{"desktop"});
		}

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/session/google"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, signInFlags, callback = Move(callback), pWindow, userIdentifier = UnicodeString(userIdentifier)](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				Optional<CachedData::LoginInfo> cachedLoginInfo;
				if (responseCode.IsSuccessful())
				{
					if (const Optional<ConstStringView> refreshToken = responseReader.Read<ConstStringView>("refresh_token"))
					{
						cachedLoginInfo = CachedData::LoginInfo{Move(userIdentifier), UnicodeString{*refreshToken}};
					}
				}

				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Move(cachedLoginInfo),
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::CompleteCachedGoogleSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Google), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToDisk);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);

		const CachedData::LoginInfo& loginInfo = m_cachedData.GetLoginInfo(SignInFlags::Google);

		writer.Serialize("refresh_token", loginInfo.authorizationToken);

		if constexpr (PLATFORM_APPLE)
		{
			writer.Serialize("platform", ConstStringView{"ios"});
		}
		else if constexpr (PLATFORM_ANDROID)
		{
			writer.Serialize("platform", ConstStringView{"android"});
		}
		else if constexpr (PLATFORM_WEB)
		{
			writer.Serialize("platform", ConstStringView{"web"});
		}
		else
		{
			writer.Serialize("platform", ConstStringView{"desktop"});
		}

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/session/google"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, signInFlags, callback = Move(callback), pWindow](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Optional<CachedData::LoginInfo>{},
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::CompleteEmailSignInInternal(
		const EnumFlags<SignInFlags> signInFlags,
		SignInCallback&& callback,
		const ConstUnicodeStringView email,
		const ConstUnicodeStringView token,
		const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Email), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);

		writer.Serialize("email", email);
		writer.Serialize("token", token);

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/v2/session/white-label"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this,
		   signInFlags,
		   callback = Forward<SignInCallback>(callback),
		   pWindow,
		   email = UnicodeString{email},
		   token = UnicodeString{token
		   }]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader) mutable
			{
				Optional<CachedData::LoginInfo> cachedLoginInfo;
				if (responseCode.IsSuccessful() && signInFlags.IsNotSet(SignInFlags::Cached))
				{
					cachedLoginInfo = CachedData::LoginInfo{Move(email), Move(token)};
				}

				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Move(cachedLoginInfo),
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::CompleteGuestSignInInternal(
		const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
	)
	{
		Assert(signInFlags.IsSet(SignInFlags::Guest), "Sign in provider flag must be set");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		PopulateCommonSignInMetadata(writer);
		writer.Serialize("platform", ConstStringView{"iOS"});
		writer.Serialize("player_identifier", ConstStringView{"1337"});

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("game/v2/session"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				environment.m_gameAPI.m_versionHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, signInFlags, callback = Forward<SignInCallback>(callback), pWindow](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				OnSignInCompletedInternal(
					signInFlags | (SignInFlags::SignedIn * responseCode.IsSuccessful()),
					Invalid,
					Move(callback),
					responseCode,
					responseReader,
					pWindow
				);
			}
		);
	}

	void Game::SignInWithEmail(
		const ConstUnicodeStringView email,
		const ConstUnicodeStringView password,
		SignInCallback&& callback,
		const Optional<Rendering::Window*> pWindow
	)
	{
		const EnumFlags<SignInFlags> supportedSignInFlags = m_supportedSignInFlags;
		if (supportedSignInFlags.IsNotSet(SignInFlags::Email))
		{
			// Indicate we couldn't sign in with email
			callback(SignInFlags::Email);
			return;
		}

		// Clear any cached sign ins with the provider
		ClearCachedLoginInfo(SignInFlags::Email, pWindow);

		// Try to sign in with email, done in two steps
		// First initial sign-in attempt, then we exchange the cacheable authorization token for a one-time session
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("email", email);
		writer.Serialize("password", password);
		writer.Serialize("remember", true);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("white-label-login/login"));

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"domain-key", environment.m_backendAPIDomainKey},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"is-development", "true"},
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[this, email = UnicodeString{email}, callback = Move(callback), pWindow](
				[[maybe_unused]] const bool success,
				const HTTP::ResponseCode responseCode,
				const Serialization::Reader responseReader
			) mutable
			{
				if (responseCode.IsSuccessful())
				{
					if (Optional<ConstStringView> sessionToken = responseReader.Read<ConstStringView>("session_token"))
					{
						m_cachedData.SetLoginInfo(SignInFlags::Email, CachedData::LoginInfo{email, UnicodeString(*sessionToken)});
						CompleteEmailSignInInternal(SignInFlags::Email, Move(callback), email, UnicodeString{*sessionToken}, pWindow);
					}
					else
					{
						// Indicate sign-in failure
						callback(SignInFlags::Email);
					}
				}
				else
				{
					// Indicate sign-in failure
					callback(SignInFlags::Email);
				}
			}
		);
	}

	void Game::SignUpWithEmail(const ConstUnicodeStringView email, const ConstUnicodeStringView password, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("email", email);
		writer.Serialize("password", password);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("white-label-login/sign-up"));

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"domain-key", environment.m_backendAPIDomainKey},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"is-development", "true"},
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			Forward<RequestCallback>(callback)
		);
	}

	void Game::RequestResetPassword(const ConstUnicodeStringView email, BinaryCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("email", email);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("white-label-login/request-reset-password"));

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"domain-key", environment.m_backendAPIDomainKey},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"is-development", "true"},
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[callback = Move(callback)]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader)
			{
				callback(responseCode.IsSuccessful());
			}
		);
	}

	void Game::ResetPassword(UnicodeString&& token, UnicodeString&& userId, UnicodeString&& password, BinaryCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		uint64 userIdNumber = userId.ToIntegral<uint64>();

		writer.Serialize("user_id", userIdNumber);
		writer.Serialize("token", token);
		writer.Serialize("password", password);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("white-label-login/reset-password"));

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"domain-key", environment.m_backendAPIDomainKey},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"is-development", "true"},
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[callback = Move(callback)]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader)
			{
				callback(responseCode.IsSuccessful());
			}
		);
	}

	void Game::VerifyEmail(UnicodeString&& token, UnicodeString&& userId, BinaryCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		uint64 userIdNumber = userId.ToIntegral<uint64>();

		writer.Serialize("token", token);
		writer.Serialize("user_id", userIdNumber);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("white-label-login/verify"));

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"domain-key", environment.m_backendAPIDomainKey},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"is-development", "true"},
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[callback = Move(callback)]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader)
			{
				callback(responseCode.IsSuccessful());
			}
		);
	}

	void Game::DeleteAccount(Rendering::Window& window)
	{
		const EnumFlags<SignInFlags> signedInProviders = m_signInFlags & SignInFlags::AllProviders;
		Assert(signedInProviders.GetNumberOfSetFlags() <= 1, "User was signed into more than one provider");
		if (signedInProviders.GetNumberOfSetFlags() == 1)
		{
			IO::URI::StringType endPoint;
			endPoint.Format(MAKE_URI_LITERAL("game/player"));

			m_backend.QueueRequestInternal(
				HTTP::RequestType::Delete,
				IO::URI(Move(endPoint)),
				Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{m_sessionTokenHeader}.GetView(),
				{},
				[]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader)
				{
					Assert(success);
				}
			);

			const SignInFlags signInProvider = signedInProviders.GetFlags();
			OnSignedOutInternal(signInProvider, window);
		}
	}

#if PLATFORM_WEB
	EM_JS(void, SignInWithGoogle, (), { signInWithGoogle(); });
	static Function<void(const ConstStringView), 24> OnGoogleSignInResponse;
	extern "C"
	{
		void onGoogleSignIn(const char* authCode)
		{
			Assert(OnGoogleSignInResponse.IsValid());
			OnGoogleSignInResponse(ConstStringView{authCode, (ConstStringView::SizeType)strlen(authCode)});
			OnGoogleSignInResponse = {};
		}
	}

	EM_JS(void, SignInWithApple, (), { signInWithApple(); });
	static Function<void(const ConstStringView authCode, const ConstStringView idToken), 24> OnAppleSignInResponse;
	extern "C"
	{
		void onAppleSignIn(const char* authCode, const char* idToken)
		{
			Assert(OnAppleSignInResponse.IsValid());
			OnAppleSignInResponse(
				ConstStringView{authCode, (ConstStringView::SizeType)strlen(authCode)},
				ConstStringView{idToken, (ConstStringView::SizeType)strlen(idToken)}
			);
			OnAppleSignInResponse = {};
		}
	}
#endif

	void Game::SignInInternal(const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow)
	{
		Assert((signInFlags & SignInFlags::AllProviders).AreAnySet(), "Must provide a sign in provider");
		Assert((signInFlags & SignInFlags::AllProviders).GetNumberOfSetFlags() == 1, "Can only log in with one provider at a time");
		Assert(signInFlags.IsNotSet(SignInFlags::SignedIn), "Signed in flag should not be set when requesting sign in");
		Assert(signInFlags.IsNotSet(SignInFlags::Cached), "Cached flag must not be set");
		Assert(signInFlags.IsNotSet(SignInFlags::Offline), "Offline mode is only compatible with cached sign ins");

		const SignInFlags signInProvider = (signInFlags & SignInFlags::AllProviders).GetFlags();
		ClearCachedLoginInfo(signInProvider, pWindow);

		switch (signInProvider)
		{
			case SignInFlags::Apple:
			{
#if PLATFORM_APPLE
				Rendering ::Window::QueueOnWindowThread(
					[this, signInFlags, callback = Forward<SignInCallback>(callback), &window = *pWindow]() mutable
					{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
						UIWindow* pWindow = (__bridge UIWindow*)window.GetOSHandle();
						ViewController* rootController = (ViewController*)[pWindow rootViewController];
#else
						NSWindow* pWindow = (__bridge NSWindow*)window.GetOSHandle();
						ViewController* rootController = (ViewController*)[pWindow contentViewController];
#endif
						SignInCallback forwardedCallback = Move(callback);
						[rootController startAppleSignIn:^(const AppleSignInResult result, NSString* userIdentifier, NSString* authorizationCode) {
							switch (result)
							{
								case AppleSignInResultLoggedIn:
								{
									const ConstStringView userIdentifierView{[userIdentifier UTF8String], (uint32)[userIdentifier length]};
									const ConstStringView authorizationCodeView{[authorizationCode UTF8String], (uint32)[authorizationCode length]};

									CompleteAppleSignInInternal(
										signInFlags,
										SignInCallback{forwardedCallback},
										userIdentifierView,
										authorizationCodeView,
										window
									);
								}
								break;
								case AppleSignInResultFailure:
								{
									OnSignInCompletedInternal(
										signInFlags,
										Optional<CachedData::LoginInfo>{},
										SignInCallback{forwardedCallback},
										Networking::HTTP::ResponseCode{401},
										Invalid,
										window
									);
								}
								break;
							}
						}];
					}
				);
				return;
#elif PLATFORM_WEB
				Assert(!OnAppleSignInResponse.IsValid());
				OnAppleSignInResponse = [this,
				                         signInFlags,
				                         callback = Forward<SignInCallback>(callback),
				                         &window = *pWindow](const ConstStringView idToken, const ConstStringView authorizationCode) mutable
				{
					if (idToken.IsEmpty() || authorizationCode.IsEmpty())
					{
						// Indicate that signing in failed
						callback(signInFlags);
						return;
					}

					CompleteAppleSignInInternal(signInFlags, Move(callback), idToken, authorizationCode, window);
				};
				Rendering::Window::QueueOnWindowThread(
					[]()
					{
						SignInWithApple();
					}
				);
				return;
#else
				break;
#endif
			}
			case SignInFlags::Google:
			{
#if HAS_GOOGLE_SIGN_IN_SDK
				Assert(pWindow.IsValid(), "Window must be provided to Sign in with Google");
				Rendering::Window::QueueOnWindowThread(
					[this, signInFlags, callback = Forward<SignInCallback>(callback), &window = *pWindow]() mutable
					{
						GIDSignIn* googleSignIn = [GIDSignIn sharedInstance];

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
						UIWindow* pWindow = (__bridge UIWindow*)window.GetOSHandle();
						ViewController* rootController = (ViewController*)[pWindow rootViewController];
#else
						NSWindow* pWindow = (__bridge NSWindow*)window.GetOSHandle();
						ViewController* rootController = (ViewController*)[pWindow contentViewController];
#endif

						[googleSignIn
							signInWithPresentingViewController:rootController
																			completion:^(GIDSignInResult* _Nullable signInResult, NSError* _Nullable error) {
																				if (error != nil)
																				{
																					// Indicate that signing in failed
																					callback(signInFlags);
																					return;
																				}
																				if (signInResult == nil)
																				{
																					// Indicate that signing in failed
																					callback(signInFlags);
																					return;
																				}

																				[signInResult.user
																					refreshTokensIfNeededWithCompletion:^(GIDGoogleUser* _Nullable user, NSError* _Nullable err) {
																						if (err != nil)
																						{
																							// Indicate that signing in failed
																							callback(signInFlags);
																							return;
																						}
																						if (user == nil)
																						{
																							// Indicate that signing in failed
																							callback(signInFlags);
																							return;
																						}

																						if (user.idToken == nil)
																						{
																							// Indicate that signing in failed
																							callback(signInFlags);
																							return;
																						}

																						NSString* userID = user.userID;
																						GIDToken* idToken = user.idToken;
																						NSString* idTokenString = idToken.tokenString;

																						const ConstStringView userIdentifierView{[userID UTF8String], (uint32)[userID length]};
																						const ConstStringView idTokenView{
																							[idTokenString UTF8String],
																							(uint32)[idTokenString lengthOfBytesUsingEncoding:NSUTF8StringEncoding]
																						};
																						CompleteGoogleSignInInternal(
																							signInFlags,
																							SignInCallback{callback},
																							userIdentifierView,
																							idTokenView,
																							window
																						);
																					}];
																			}];
					}
				);
				return;
#elif PLATFORM_ANDROID
				pWindow->GetAndroidGameActivityWindow()->SignInWithGoogle(
					[this,
				   signInFlags,
				   callback = Forward<SignInCallback>(callback),
				   &window = *pWindow](const ConstStringView userIdentifier, const ConstStringView idToken) mutable
					{
						if (userIdentifier.IsEmpty() || idToken.IsEmpty())
						{
							// Indicate that signing in failed
							callback(signInFlags);
							return;
						}

						CompleteGoogleSignInInternal(signInFlags, Move(callback), userIdentifier, idToken, window);
					}
				);
				return;
#elif PLATFORM_WEB
				Assert(!OnGoogleSignInResponse.IsValid());
				OnGoogleSignInResponse =
					[this, signInFlags, callback = Forward<SignInCallback>(callback), &window = *pWindow](const ConstStringView idToken) mutable
				{
					if (idToken.IsEmpty())
					{
						// Indicate that signing in failed
						callback(signInFlags);
						return;
					}

					// Not known from web (backend can find it though)
					const ConstStringView userIdentifier;
					CompleteGoogleSignInInternal(signInFlags, Move(callback), userIdentifier, idToken, window);
				};
				Rendering::Window::QueueOnWindowThread(
					[]()
					{
						SignInWithGoogle();
					}
				);
				return;
#else
				break;
#endif
			}

			case SignInFlags::Steam:
			{
				CompleteSteamSignInInternal(signInFlags, Forward<SignInCallback>(callback), pWindow);
				return;
			}
			case SignInFlags::Email:
			{
				Assert(false, "Email sign-in must go through SignInWithEmail or cached sign-in");
			}
			break;
			case SignInFlags::Guest:
			{
				if (m_signInFlags.IsSet(SignInFlags::RequestedSignIn))
				{
					Assert(false, "TODO: Multi-sign in");
					// Workaround for multiple windows
					// TODO: Rename Game -> GameSession and keep it as a component
					// This way we could have split-screen with multiple active sessions etc
					return;
				}
				m_signInFlags |= SignInFlags::RequestedSignIn;
				CompleteGuestSignInInternal(signInFlags, Move(callback), pWindow);
				return;
			}
			case Backend::SignInFlags::None:
			case SignInFlags::SignedIn:
			case SignInFlags::Offline:
			case SignInFlags::Cached:
			case SignInFlags::ProviderCount:
			case SignInFlags::AllProviders:
			case SignInFlags::RequestedSignIn:
			case SignInFlags::HasSessionToken:
			case SignInFlags::IsFirstSession:
			case SignInFlags::HasFileToken:
			case SignInFlags::HasLocalPlayerProfile:
			case SignInFlags::FinishedSessionRegistration:
			case SignInFlags::CompletedCoreRequests:
			case SignInFlags::InternalFlags:
				ExpectUnreachable();
		}

		// Indicate log-in failure
		OnSignInCompletedInternal(
			signInFlags,
			Optional<CachedData::LoginInfo>{},
			Forward<SignInCallback>(callback),
			Networking::HTTP::ResponseCode{401},
			Invalid,
			pWindow
		);
	}

	void Game::ClearCachedLoginInfo(const SignInFlags signInProvider, [[maybe_unused]] const Optional<Rendering::Window*> pWindow)
	{
		m_cachedData.ClearLoginInfo(signInProvider);

#if HAS_GOOGLE_SIGN_IN_SDK
		if (signInProvider == SignInFlags::Google)
		{
			[[GIDSignIn sharedInstance] signOut];
		}
#elif PLATFORM_ANDROID
		pWindow->GetAndroidGameActivityWindow()->SignOutOfAllProviders();
#endif
	}

	void Game::SetCachedLoginInfo(const SignInFlags signInProvider, UnicodeString&& userIdentifier, UnicodeString&& authenticationToken)
	{
		m_cachedData.SetLoginInfo(
			signInProvider,
			CachedData::LoginInfo{Forward<UnicodeString>(userIdentifier), Forward<UnicodeString>(authenticationToken)}
		);
	}

	void Game::SignOut(Rendering::Window& window)
	{
		const EnumFlags<SignInFlags> signedInProviders = m_signInFlags & SignInFlags::AllProviders;
		Assert(signedInProviders.GetNumberOfSetFlags() <= 1, "User was signed into more than one provider");
		if (signedInProviders.GetNumberOfSetFlags() == 1)
		{
			const SignInFlags signInProvider = signedInProviders.GetFlags();
			OnSignedOutInternal(signInProvider, window);
		}
	}

	void Game::OnSignedOutInternal(const SignInFlags signInProvider, Rendering::Window& window)
	{
		m_signInFlags.Clear();

		ClearCachedLoginInfo(signInProvider, window);

		m_cachedData.SetLocalPlayerInternalIdentifier({});

		m_sessionToken.Clear();
		m_sessionTokenHeader = {};
		m_localPlayerIdentifier = {};
		m_publicUserId = {};
		m_fileURIToken = {};

		m_inventory.Clear();
		m_persistentUserStorage.Clear();

		m_currency = {};

		OnSignedOut();
	}

	void Game::QueuePostSessionRegistrationCallback(QueuedFunction&& function)
	{
		if (m_signInFlags.AreAnySet(SignInFlags::HasSessionToken | SignInFlags::Offline))
		{
			function(m_signInFlags.GetFlags());
		}
		else
		{
			Threading::UniqueLock lock(m_queuedRequestsMutex);
			if (m_signInFlags.AreAnySet(SignInFlags::HasSessionToken | SignInFlags::Offline))
			{
				function(m_signInFlags.GetFlags());
			}
			else
			{
				m_sessionRegistrationCallbacks.EmplaceBack(Forward<QueuedFunction>(function));
			}
		}
	}

	void Game::QueueFileTokenCallback(QueuedFunction&& function)
	{
		if (m_signInFlags.AreAnySet(SignInFlags::HasFileToken | SignInFlags::Offline))
		{
			function(m_signInFlags.GetFlags());
		}
		else
		{
			Threading::UniqueLock lock(m_queuedRequestsMutex);
			if (m_signInFlags.AreAnySet(SignInFlags::HasFileToken | SignInFlags::Offline))
			{
				function(m_signInFlags.GetFlags());
			}
			else
			{
				m_fileTokenCallbacks.EmplaceBack(Forward<QueuedFunction>(function));
			}
		}
	}

	void Game::QueueStartupRequestsFinishedCallback(QueuedFunction&& function)
	{
		if (m_signInFlags.AreAllSet(SignInFlags::CompletedCoreRequests) || m_signInFlags.IsSet(SignInFlags::Offline))
		{
			function(m_signInFlags.GetFlags());
		}
		else
		{
			Threading::UniqueLock lock(m_queuedRequestsMutex);
			if (m_signInFlags.AreAllSet(SignInFlags::CompletedCoreRequests) || m_signInFlags.IsSet(SignInFlags::Offline))
			{
				function(m_signInFlags.GetFlags());
			}
			else
			{
				m_startupRequestsCallbacks.EmplaceBack(Forward<QueuedFunction>(function));
			}
		}
	}

	void Game::PurchaseApplePay([[maybe_unused]] Rendering::Window& window, [[maybe_unused]] uint64 amount)
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		UIWindow* pWindow = (__bridge UIWindow*)window.GetOSHandle();
		ViewController* rootController = (ViewController*)[pWindow rootViewController];
#else
		NSWindow* pWindow = (__bridge NSWindow*)window.GetOSHandle();
		ViewController* rootController = (ViewController*)[pWindow contentViewController];
#endif

		Rendering ::Window::QueueOnWindowThread(
			[this, rootController, amount]()
			{
				[rootController checkOut:^(NSString* applePayResult, NSUInteger totalAmount) {
					Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
					Serialization::Writer writer(serializedData);

					writer.Serialize("wallet_id", GetCurrency().m_userWalletId);
					writer.Serialize("amount", totalAmount);

					const ConstStringView applePayDataView{[applePayResult UTF8String], (uint32)[applePayResult length]};
					writer.Serialize("apple_pay_data", applePayDataView);

					QueueRequest(
						HTTP::RequestType::Post,
						MAKE_URI("game/purchase/currency/applepay"),
						writer.SaveToBuffer<String>(),
						[this, rootController](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader)
						{
							if (success || responseCode.IsSuccessful())
							{
								[rootController callPkPaymentAuthorizationCompletionCallback:PKPaymentAuthorizationStatusSuccess];
								GetPlayerCurrencyBalance(
									[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
									{
										HandlePlayerCurrencyBalance(reader);
									}
								);
							}
							else
							{
								[rootController callPkPaymentAuthorizationCompletionCallback:PKPaymentAuthorizationStatusFailure];
							}
						}

					);
				}:amount];
			}
		);

#endif
	}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_MACOS
	bool Game::ValidateApplePurchases()
	{
		// should be Base64 encoded Bundle.main.appStoreReceiptURL
		NSURL* receiptURL = [[NSBundle mainBundle] appStoreReceiptURL];
		NSData* receipt = [NSData dataWithContentsOfURL:receiptURL];
		if (receipt == nullptr)
		{
			return false;
		}

		NSString* nsEncodedReceipt = [receipt base64EncodedStringWithOptions:0];
		const ConstStringView encodedReceipt = {[nsEncodedReceipt UTF8String], (uint32)[nsEncodedReceipt length]};

		Serialization::Data serializedData(rapidjson::kArrayType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.SerializeArrayCallbackInPlace(
			[encodedReceipt](Serialization::Writer elementWriter, [[maybe_unused]] const uint32 index)
			{
				elementWriter.GetValue() = Serialization::Value(rapidjson::Type::kObjectType);
				elementWriter.Serialize("receipt_data", encodedReceipt);
				return true;
			},
			1
		);

		QueueRequest(
			HTTP::RequestType::Post,
			MAKE_URI("game/v1/purchase"),
			writer.SaveToBuffer<String>(),
			[](const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				if (success)
				{
					const uint32 orderId = *reader.Read<uint32>("order_id");
					UNUSED(orderId);

					// TODO: We need to know transaction ids mapped to this purchase / validation call in order to close them
				  // TODO: Repeatedly call the order status API
				  // Once that returns success, query contents of the order(s)
				  // For each asset in the order, check external_identifier and map to the Apple SKProduct
				  // Call finishTransaction for each open transaction with those product identifiers.
				}
			}
		);
		return true;
	}

	void Game::OnReceivedAppleProductInfo(const ngine::ArrayView<void*> skProducts)
	{
		Assert(m_appleSKProducts.IsEmpty());
		m_appleSKProducts.CopyFrom(m_appleSKProducts.begin(), skProducts);

		for (void* product : skProducts)
		{
			SKProduct* skProduct = (__bridge SKProduct*)product;
			const ConstStringView productIdentifier = {
				[[skProduct productIdentifier] UTF8String],
				(uint32)[[skProduct productIdentifier] length]
			};

			for (AssetEntry& entry : m_awaitingAppStoreProducts)
			{
				if (entry.m_appStoreProductIdentifier == productIdentifier)
				{
					entry.m_appleProduct = product;
				}
			}
			m_awaitingAppStoreProducts.Clear();
		}
	}

	void Game::OnApplePurchaseEvent([[maybe_unused]] void* skPaymentTransaction)
	{
		/*switch (transaction.transactionState)
		{
		    // Call the appropriate custom method for the transaction state.
		    case SKPaymentTransactionStatePurchasing:
		        [self showTransactionAsInProgress:transaction deferred:NO];
		        break;
		    case SKPaymentTransactionStateDeferred:
		        [self showTransactionAsInProgress:transaction deferred:YES];
		        break;
		    case SKPaymentTransactionStateFailed:
		        [self failedTransaction:transaction];
		        break;
		    case SKPaymentTransactionStatePurchased:
		        [self completeTransaction:transaction];
		        break;
		    case SKPaymentTransactionStateRestored:
		        break;
		}*/
	}
#endif

	void Game::HandleAssetResponse(const Serialization::Reader assetReader, Asset::Manager& assetManager)
	{
		const Optional<ConstStringView> assetGuidString = assetReader.Read<ConstStringView>("uuid");
		if (LIKELY(assetGuidString.IsValid()))
		{
			const bool isActive = assetReader.ReadWithDefaultValue<bool>("active", true);
			const Asset::Guid assetGuid = Asset::Guid::TryParse(*assetGuidString);
			if (isActive)
			{
				m_cachedData.AddAsset(assetGuid, assetReader);

				m_assetDatabase.ParseAsset(assetReader, assetGuid, assetManager, AssetDatabase::ParsingFlags::GlobalAssets, *this);
			}
			else
			{
				m_cachedData.RemoveAsset(assetGuid);
				m_assetDatabase.DeactivateAsset(assetGuid, assetManager);
			}
		}
	}

	void Game::HandlePaginatedAssetResponse(const Serialization::Reader reader, Asset::Manager& assetManager)
	{
		if (const Optional<Serialization::Reader> assetsReader = reader.FindSerializer("assets"))
		{
			for (const Serialization::Reader assetReader : assetsReader->GetArrayView())
			{
				const Optional<Guid> assetGuid = assetReader.Read<Guid>("uuid");
				Assert(assetGuid.IsValid());
				if (LIKELY(assetGuid.IsValid()))
				{
					const bool isActive = assetReader.ReadWithDefaultValue<bool>("active", true);
					if (isActive)
					{
						m_cachedData.AddAsset(*assetGuid, assetReader);

						m_assetDatabase.ParseAsset(assetReader, *assetGuid, assetManager, AssetDatabase::ParsingFlags::GlobalAssets, *this);
					}
					else
					{
						m_cachedData.RemoveAsset(*assetGuid);
						m_assetDatabase.DeactivateAsset(*assetGuid, assetManager);
					}
				}
			}
		}
	}

	void Game::HandleAssetResponseWithTag(const Serialization::Reader assetReader, Asset::Manager& assetManager, Guid tag)
	{
		const Optional<ConstStringView> assetGuidString = assetReader.Read<ConstStringView>("uuid");
		if (LIKELY(assetGuidString.IsValid()))
		{
			const bool isActive = assetReader.ReadWithDefaultValue<bool>("active", true);
			const Asset::Guid assetGuid = Asset::Guid::TryParse(*assetGuidString);
			if (isActive)
			{
				m_cachedData.AddAsset(assetGuid, assetReader);

				if (tag == DataSource::Players::AvatarAssetTag)
				{
					m_assetDatabase.ParseAsset(assetReader, assetGuid, assetManager, AssetDatabase::ParsingFlags::Avatar, *this, tag);
					m_playersDataSource.OnDataChanged();
				}
				else
				{
					m_assetDatabase.ParseAsset(assetReader, assetGuid, assetManager, AssetDatabase::ParsingFlags::GlobalAssets, *this, tag);
				}
			}
			else
			{
				m_cachedData.RemoveAsset(assetGuid);
				m_assetDatabase.DeactivateAsset(assetGuid, assetManager);
			}
		}
	}

	void Game::HandleBuyableAssetResponse(
		const Serialization::Reader assetReader, Asset::Manager& assetManager, uint64_t price, ConstStringView catalogId
	)
	{
		const Optional<ConstStringView> assetGuidString = assetReader.Read<ConstStringView>("uuid");
		if (LIKELY(assetGuidString.IsValid()))
		{
			const bool isActive = assetReader.ReadWithDefaultValue<bool>("active", true);
			const Asset::Guid assetGuid = Asset::Guid::TryParse(*assetGuidString);
			if (isActive)
			{
				m_cachedData.AddAsset(assetGuid, assetReader);
				m_assetDatabase.ParseAsset(assetReader, assetGuid, assetManager, AssetDatabase::ParsingFlags::GlobalAssets, *this);
				// iterate over map to insert asset prices
				Asset::Library& assetLibrary = System::Get<Asset::Manager>().GetAssetLibrary();

				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

				m_assetDatabase.VisitEntry(
					assetGuid,
					[catalogId, price, assetGuid, &assetLibrary, &assetManager, &tagRegistry](
						const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry
					)
					{
						if (pBackendAssetEntry.IsValid())
						{
							pBackendAssetEntry->m_catalogListingId = catalogId;

							pBackendAssetEntry->m_price = uint32(price);
							Asset::Identifier assetId = assetLibrary.GetAssetIdentifier(assetGuid);
							Assert(assetId.IsValid());
							if (LIKELY(assetId.IsValid()))
							{
								// LogWarning("{}",pBackendAssetEntry->m_name);
							  // LogWarning("{}",pBackendAssetEntry->m_playsCount);
							  // String dataToRead = assetReader.GetData().SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
							  // LogWarning("{}", dataToRead);
								assetLibrary.SetTagAsset(tagRegistry.FindOrRegister(ngine::Asset::Library::PurchasableAssetTagGuid), assetId);
								assetLibrary.OnDataChanged();
								assetManager.OnDataChanged();
							}
						}
					}
				);
			}
			else
			{
				m_cachedData.RemoveAsset(assetGuid);
				m_assetDatabase.DeactivateAsset(assetGuid, assetManager);
			}
		}
	}

	void Game::HandleStreamedAssetResponse(
		const StreamedRequestType request, const Serialization::Reader assetReader, Asset::Manager& assetManager
	)
	{
		if (const Optional<Serialization::Reader> expectedObjectCount = assetReader.FindSerializer("totalAssetToStream"))
		{
			GetAssetDatabase().ReserveAssets(expectedObjectCount->ReadInPlaceWithDefaultValue<uint32>(0));
		}
		else if (const Optional<Serialization::Reader> streamedObjectCount = assetReader.FindSerializer("streamedObjectCount"))
		{
			if (const Optional<Time::Timestamp> mostRecentAssetTimestamp = assetReader.Read<Time::Timestamp>("mostRecentlyUpdated"))
			{
				m_cachedData.TryUpdateTimestamp(request, *mostRecentAssetTimestamp);
			}
		}
		else
		{
			HandleAssetResponse(assetReader, assetManager);
		}
	}

	void Game::HandlePlayerGetResponse(const Serialization::Reader playerResponseReader)
	{
		if (const Optional<Serialization::Reader> itemsReader = playerResponseReader.FindSerializer("items"))
		{
			Assert(itemsReader->IsArray());
			if (LIKELY(itemsReader->IsArray()))
			{
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				for (const Serialization::Reader playerReader : itemsReader->GetArrayView())
				{
					PersistentUserIdentifier internalPlayerIdentifier;
					if (const Optional<PersistentUserIdentifier::Type> playerId = playerReader.Read<PersistentUserIdentifier::Type>("player_id"))
					{
						internalPlayerIdentifier = PersistentUserIdentifier{*playerId};
					}
					else
					{
						continue;
					}

					m_playersDataSource.FindOrEmplacePlayer(
						internalPlayerIdentifier,
						[playerReader, &tagRegistry](DataSource::Players::PlayerInfo& playerInfo)
						{
							playerInfo.username = playerReader.ReadWithDefaultValue<UnicodeString>("username", {});
							playerInfo.description = playerReader.ReadWithDefaultValue<UnicodeString>("biography", {});
							playerInfo.followerCount = playerReader.ReadWithDefaultValue<uint32>("follower_count", 0);
							playerInfo.followingCount = playerReader.ReadWithDefaultValue<uint32>("following_count", 0);
							playerInfo.gameCount = playerReader.ReadWithDefaultValue<uint32>("game_count", 0);
							playerInfo.thumbnailGuid = playerInfo.thumbnailEditGuid =
								playerReader.ReadWithDefaultValue<Asset::Guid>("picture_asset_uuid", {});
							const Guid playerGuid = playerReader.ReadWithDefaultValue<Guid>("profile_uuid", {});
							if (!playerInfo.accountCreationDate.IsValid())
							{
								playerInfo.accountCreationDate = playerReader.ReadWithDefaultValue<Time::Timestamp>("player_created_at", Time::Timestamp{});
							}
							const Tag::Identifier playerTagIdentifier = tagRegistry.FindOrRegister(playerGuid);
							playerInfo.playerTagIdentifier = playerTagIdentifier;

							playerInfo.tags.Set(playerTagIdentifier);
							if (playerInfo.gameCount > 0)
							{
								playerInfo.tags.Set(tagRegistry.FindOrRegister(DataSource::Players::GameCreatorGuid));
							}
						}
					);
				}

				m_playersDataSource.OnDataChanged();

				// To be uncommented when players will be able to upload their own profile picture
				// GetAssetDatabase().ReserveAssets(profilePictures.GetSize());
				// GetCachedData().Reserve(profilePictures.GeSize());
				/*RequestAssets(
				  Move(profilePictures),
				  [this, &assetManager = System::Get<Asset::Manager>()](const Serialization::Reader assetReader) mutable
				  {
				    HandleAssetResponse(assetReader, assetManager);
				  },
				  [this]([[maybe_unused]] const bool success)
				        {
				        }
				);*/
			}
		}
	}

	void Game::HandlePlayerFollowingsGetResponse(const Serialization::Reader playerResponseReader)
	{
		Assert(playerResponseReader.IsArray());
		if (LIKELY(playerResponseReader.IsArray()))
		{
			for (const Serialization::Reader playerReader : playerResponseReader.GetArrayView())
			{
				if (const Optional<PersistentUserIdentifier::Type> playerId = playerReader.Read<PersistentUserIdentifier::Type>("follows"))
				{
					const PersistentUserIdentifier internalPlayerIdentifier{*playerId};
					m_playersDataSource.FindOrEmplacePlayer(
						internalPlayerIdentifier,
						[](DataSource::Players::PlayerInfo& playerInfo)
						{
							playerInfo.followedByMainPlayer = true;
						}
					);
				}
			}
			m_playersDataSource.OnDataChanged();
		}
	}

	void Game::HandleSinglePlayerGetResponse(const Serialization::Reader playerReader)
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		if (const Optional<PersistentUserIdentifier::Type> playerId = playerReader.Read<PersistentUserIdentifier::Type>("player_id"))
		{
			const PersistentUserIdentifier internalPlayerIdentifier{*playerId};
			m_playersDataSource.FindOrEmplacePlayer(
				internalPlayerIdentifier,
				[this, playerReader, &tagRegistry](DataSource::Players::PlayerInfo& playerInfo)
				{
					if (Optional<UnicodeString> username = playerReader.Read<UnicodeString>("username"))
					{
						playerInfo.username = Move(*username);
					}
					if (Optional<UnicodeString> biography = playerReader.Read<UnicodeString>("biography"))
					{
						playerInfo.description = Move(*biography);
					}
					if (const Optional<uint32> followerCount = playerReader.Read<uint32>("follower_count"))
					{
						playerInfo.followerCount = *followerCount;
					}
					if (const Optional<uint32> followingCount = playerReader.Read<uint32>("following_count"))
					{
						playerInfo.followingCount = *followingCount;
					}
					if (const Optional<uint32> gameCount = playerReader.Read<uint32>("game_count"))
					{
						playerInfo.gameCount = *gameCount;
						if (*gameCount > 0)
						{
							playerInfo.tags.Set(tagRegistry.FindOrRegister(DataSource::Players::GameCreatorGuid));
						}
					}
					if (const Optional<Guid> thumbnailGuid = playerReader.Read<Guid>("picture_asset_uuid"))
					{
						playerInfo.thumbnailGuid = playerInfo.thumbnailEditGuid = *thumbnailGuid;

						// TODO: Only relevant when we have custom avatars
						/*Vector<Asset::Guid> profilePictures;
					  profilePictures.EmplaceBack(*thumbnailGuid);
					  GetAssetDatabase().ReserveAssets(profilePictures.GetSize());
					  GetCachedData().Reserve(profilePictures.GeSize());
					  RequestAssetsInternal(
					    Move(profilePictures),
					    [this, &assetManager = System::Get<Asset::Manager>()](const Serialization::Reader assetReader) mutable
					    {
					      HandleAssetResponse(assetReader, assetManager);
					    },
					    {

					             }
					  );*/
					}
					if (const Optional<Guid> profileGuid = playerReader.Read<Guid>("profile_uuid"))
					{
						playerInfo.playerTagIdentifier = tagRegistry.FindOrRegister(*profileGuid);
						playerInfo.tags.Set(playerInfo.playerTagIdentifier);
					}
					m_playersDataSource.OnDataChanged();
				}
			);
		}
	}

	void Game::HandlePlayerLikedAssetResponse(const Serialization::Reader assetReader)
	{
		Assert(assetReader.IsArray());
		if (LIKELY(assetReader.IsArray()))
		{
			for (const Serialization::Reader likeReader : assetReader.GetArrayView())
			{
				if (const Optional<Guid> guid = likeReader.Read<Guid>("asset_uuid"))
				{
					m_assetDatabase.TryEmplaceAndVisitEntry(
						*guid,
						[](Networking::Backend::AssetEntry& assetEntry)
						{
							assetEntry.m_flags |= AssetFlags::Liked;
						}
					);
				}
			}
		}
	}

	void Game::HandlePlayerCurrencyBalance(const Serialization::Reader reader)
	{
		if (const Optional<Serialization::Reader> balancesReader = reader.FindSerializer("balances"))
		{
			Assert(balancesReader->IsArray());
			if (LIKELY(balancesReader->IsArray()))
			{
				for (const Serialization::Reader balanceReader : balancesReader->GetArrayView())
				{
					if (const Optional<Serialization::Reader> currencyReader = balanceReader.FindSerializer("currency"))
					{
						if (const Optional<ConstStringView> currencyName = currencyReader->Read<ConstStringView>("name"))
						{
							if (*currencyName == "Scenero")
							{
								if (const Optional<ConstStringView> currencyAmount = balanceReader.Read<ConstStringView>("amount"))
								{
									m_currency.m_balance = currencyAmount->ToIntegral<uint64>();
									m_playersDataSource.OnDataChanged();
								}
								if (const Optional<String> walletId = balanceReader.Read<String>("wallet_id"))
								{
									m_currency.m_userWalletId = *walletId;
								}
							}
						}
					}
				}
			}
		}
	}

	void Game::HandleBuyableAssetsList(const Serialization::Reader reader)
	{
		String dataToRead = reader.GetData().SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
		UnorderedMap<ConstStringView, Asset::Guid, ConstStringView::Hash> assetUlidGuids;
		Vector<Asset::Guid> assetGuids;
		if (const Optional<Serialization::Reader> assetDetailsReader = reader.FindSerializer("assets_details");
		    assetDetailsReader.IsValid() && !assetDetailsReader->GetValue().IsNull())
		{
			assetGuids.Reserve((uint32)assetDetailsReader->GetArraySize());
			for (const Serialization::Reader assetDetailReader : assetDetailsReader->GetArrayView())
			{
				assetUlidGuids.Emplace(*assetDetailReader.Read<ConstStringView>("id"), *assetDetailReader.Read<Guid>("legacy_uuid"));
				assetGuids.EmplaceBack(*assetDetailReader.Read<Guid>("legacy_uuid"));
			}
		}

		UnorderedMap<Asset::Guid, uint64, Guid::Hash> assetPrices;
		UnorderedMap<Asset::Guid, String, Guid::Hash> assetCatalogListingIds;
		if (const Optional<Serialization::Reader> entriesReader = reader.FindSerializer("entries"))
		{
			Assert(entriesReader->IsArray());
			if (LIKELY(entriesReader->IsArray()))
			{
				for (const Serialization::Reader entryReader : entriesReader->GetArrayView())
				{
					if (const Optional<ConstStringView> entityId = entryReader.Read<ConstStringView>("entity_id"))
					{
						decltype(assetUlidGuids)::const_iterator it = assetUlidGuids.Find(*entityId);
						if (it != assetUlidGuids.end())
						{
							Asset::Guid assetGuid = it->second;
							if (const Optional<String> catalogListingId = entryReader.Read<String>("catalog_listing_id"))
							{
								assetCatalogListingIds.Emplace(assetGuid, *catalogListingId);

								if (const Optional<Serialization::Reader> pricesReader = entryReader.FindSerializer("prices"))
								{
									Assert(pricesReader->IsArray());
									if (LIKELY(pricesReader->IsArray()))
									{
										for (const Serialization::Reader priceReader : pricesReader->GetArrayView())
										{
											if (const Optional<ConstStringView> priceAmount = priceReader.Read<ConstStringView>("amount"))
											{
												assetPrices.Emplace(assetGuid, priceAmount->ToIntegral<uint64>());
											}
										}
									}
								}
							}
						}
					}
				}
			}
		}

		if (assetGuids.HasElements())
		{
			RequestAssetsInternal(
				Move(assetGuids),
				[this,
			   &assetManager = System::Get<Asset::Manager>(),
			   assetCatalogListingIds = Move(assetCatalogListingIds),
			   assetPrices = Move(assetPrices
			   )](const Serialization::Reader assetReader, [[maybe_unused]] const ConstStringView objectJson) mutable
				{
					const Optional<Guid> assetGuid = assetReader.Read<Guid>("uuid");
					if (LIKELY(assetGuid.IsValid()))
					{
						auto price = assetPrices.Find(*assetGuid);
						auto catalogId = assetCatalogListingIds.Find(*assetGuid);
						HandleBuyableAssetResponse(assetReader, assetManager, price->second, catalogId->second);
					}
				},
				[this]([[maybe_unused]] const bool success)
				{
					GetCachedData().SaveIfNecessary();
				}
			);
		}
	}

	void Game::HandleAssetLikeCountsResponse(const Serialization::Reader assetReader)
	{
		Assert(assetReader.IsArray());
		if (LIKELY(assetReader.IsArray()))
		{
			for (const Serialization::Reader likeReader : assetReader.GetArrayView())
			{
				if (const Optional<Guid> guid = likeReader.Read<Guid>("asset_uuid"))
				{
					if (const Optional<uint64> likeCount = likeReader.Read<uint64>("number_of_likes"))
					{
						m_assetDatabase.TryEmplaceAndVisitEntry(
							*guid,
							[likeCount = *likeCount](Networking::Backend::AssetEntry& assetEntry)
							{
								assetEntry.m_likesCount = likeCount;
							}
						);
					}
				}
			}
		}
	}

	void Game::HandleAssetPlayCountsResponse(const Serialization::Reader assetReader)
	{
		Assert(assetReader.IsArray());
		if (LIKELY(assetReader.IsArray()))
		{
			for (const Serialization::Reader playCountReader : assetReader.GetArrayView())
			{
				if (const Optional<Guid> guid = playCountReader.Read<Guid>("asset_uuid"))
				{
					if (const Optional<uint64> playsCount = playCountReader.Read<uint64>("number_of_plays"))
					{
						m_assetDatabase.TryEmplaceAndVisitEntry(
							*guid,
							[playsCount = *playsCount](Networking::Backend::AssetEntry& assetEntry)
							{
								assetEntry.m_playsCount = playsCount;
							}
						);
					}
				}
			}
		}
	}

	void Game::RequestFileURIToken()
	{
		QueueRequest(
			HTTP::RequestType::Get,
			MAKE_URI("game/v1/filetoken"),
			{},
			[this]([[maybe_unused]] const bool success, [[maybe_unused]] const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				Assert(code.IsSuccessful());
				const bool wasFileTokenUpdated = reader.SerializeInPlace(m_fileURIToken);
				Assert(wasFileTokenUpdated);
				const EnumFlags<SignInFlags> appendedFlag = SignInFlags::HasFileToken * (code.IsSuccessful() & wasFileTokenUpdated);
				const EnumFlags<SignInFlags> newFlags = m_signInFlags.FetchOr(appendedFlag) | appendedFlag;

				Threading::UniqueLock lock(m_queuedRequestsMutex);
				for (QueuedFunction& function : m_fileTokenCallbacks)
				{
					function(newFlags);
				}
				m_fileTokenCallbacks.Clear();
			}
		);
	}

	void Game::ParseCachedData()
	{
		m_cachedData.Parse(
			*this,
			m_assetDatabase,
			System::Get<Asset::Manager>(),
			AssetDatabase::ParsingFlags::GlobalAssets | AssetDatabase::ParsingFlags::FromCache
		);
	}

	void Game::RequestAvatarsPackage()
	{
		// Request avatars package asset
		constexpr Guid avatarPackageAssetGuid = "3b457cd5-f87d-43f3-8353-dabebc35ff37"_guid;
		RequestAssetContents(
			avatarPackageAssetGuid,
			[this](const Serialization::Reader assetReader, [[maybe_unused]] const ConstStringView objectJson)
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				HandleAssetResponseWithTag(assetReader, assetManager, DataSource::Players::AvatarAssetTag);
			},
			[]([[maybe_unused]] const bool success)
			{
			}
		);
	}

	void Game::RequestMarketplaceAssets()
	{
		GetBuyableGamesList(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				HandleBuyableAssetsList(reader);
			}
		);
		GetBuyableCharactersList(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				HandleBuyableAssetsList(reader);
			}
		);

		RequestPlayerInventory();
	}

	void Game::RequestExplorePagePaginatedProjectAssets()
	{
		constexpr Guid projectAssetGuid = ProjectAssetFormat.assetTypeGuid;
		Array requestPaginatedParameters{
			// Featured
			RequestPaginatedParameters{15, true, RequestOrderBy::Date},
			// Platformers
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "f5777f96-a43c-4ebd-872a-1eeda5719aaf"_guid},
			// Racing
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "0c26b65a-dcc6-4109-aa99-44ccfc84ba67"_guid},
			// Hypercasual
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "5f1de2fb-690d-4f75-bbc3-b5e83a5d08d9"_guid},
			// Enterprise
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "3839fd96-d90f-4df2-8fd0-e0e494cff402"_guid},
			// Shooters
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "26146dde-42b7-44b8-9d39-57d6e89d39fb"_guid},
			// Sports
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "d5ba24c3-ad3a-4b62-b8ba-c178f511a55f"_guid},
			// Learn Components
			RequestPaginatedParameters{35, false, RequestOrderBy::Plays, "95b8e280-5b62-4235-a18a-805d24f34e8d"_guid}
		};

		for (const RequestPaginatedParameters& parameters : requestPaginatedParameters)
		{
			RequestPaginatedAssetsOfType(
				projectAssetGuid,
				parameters,
				[this,
			   &assetManager = System::Get<Asset::Manager>(
				 )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader assetReader)
				{
					if (success || responseCode.IsSuccessful())
					{
						HandlePaginatedAssetResponse(assetReader, assetManager);
					}
					else
					{
						Assert(false);
					}
				}
			);
		}
	}

	void Game::RequestPlayerProfileInfo()
	{
		RequestPlayerProfileById(
			GetLocalPlayerInternalIdentifier(),
			[this](const bool success)
			{
				Events::Manager& eventsManager = System::Get<Events::Manager>();

				if (!success)
				{
					// Create default profile
					m_playersDataSource.FindOrEmplacePlayer(
						m_cachedData.GetLocalPlayerInternalIdentifier(),
						[](DataSource::Players::PlayerInfo& playerInfo)
						{
							// Provide some examples of default avatars and randomly pick one so default isn't always the same
							constexpr Array defaultAvatarAssetGuids{
								"0e54cfc6-6e67-5cce-40ea-4e2fba418a84"_guid,
								"9b3b1a80-8b2c-a371-7ec8-f465e0f088c8"_guid,
								"25751056-fbb7-669f-9c81-f628a60c516d"_guid,
								"45f3ecd7-6407-8044-e023-2a450b991fed"_guid,
								"effe4654-aa3f-0819-582b-b2fedca8afb4"_guid,
								"69d7dc2a-45e1-e7a0-8a04-6b7930b9da39"_guid,
								"3775950f-3552-f4b1-aa02-f298f4fb842d"_guid,
								"35d60bda-e763-819f-8f5e-dd7be1cb0c37"_guid,
								"4083558e-8cfb-af89-92a0-8325de548e7e"_guid
							};

							const Guid defaultAvatarAssetGuid =
								defaultAvatarAssetGuids[Math::Random((uint8)0u, (uint8(defaultAvatarAssetGuids.GetSize() - 1)))];
							playerInfo.thumbnailGuid = playerInfo.thumbnailEditGuid = defaultAvatarAssetGuid;
						}
					);

					// Notify that we need to create a profile
					eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent("c5b31d8e-1fc1-4aa7-bfb2-301e3b4c7039"_guid));

					m_playersDataSource.OnDataChanged();
				}
				else
				{
					// Notify that a user with an existing profile signed in
					eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent("2f0444f8-4ed0-4b7f-a40c-55ed32652351"_guid));
				}

				const SignInFlags newFlag = SignInFlags::HasLocalPlayerProfile;
				const EnumFlags<SignInFlags> flags = m_signInFlags.FetchOr(newFlag) | newFlag;
				if (flags.AreAllSet(SignInFlags::CompletedCoreRequests))
				{
					OnStartupRequestsFinishedInternal(flags);
				}
			}
		);

		GetPlayerProfiles(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				Assert(success);
				HandlePlayerGetResponse(reader);
			}
		);

		GetPlayerLikedAssets(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				Assert(success);
				HandlePlayerLikedAssetResponse(reader);
			}
		);

		GetPlayerFollowings(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				HandlePlayerFollowingsGetResponse(reader);
			}
		);

		GetPlayerCurrencyBalance(
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				HandlePlayerCurrencyBalance(reader);
			}
		);
	}

	ConstStringView Game::GetFileURIToken() const
	{
		Assert(m_signInFlags.IsSet(SignInFlags::HasFileToken));
		return m_fileURIToken.m_token;
	}

	Game::CachedData::CachedData(const IO::PathView cacheName)
		: m_assetsData(IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("CachedAssets"), cacheName))
	{
	}

	inline static constexpr Array<ConstZeroTerminatedStringView, (uint8)Game::StreamedRequestType::Count, Game::StreamedRequestType>
		StreamedRequestKeys{
			"timestamp_all_assets",
			"timestamp_all_projects", // TODO: Only request projects relevant to the explore page (request featured -> by tag? And paginated?)
			"timestamp_all_mesh_scenes",
			"timestamp_all_mesh_parts",
			"timestamp_all_textures",
			"timestamp_all_material_instances",
			"timestamp_all_scenes", // TODO: Only request scenes that should show up in the asset library (to avoid scenes from UGC projects)
			"timestamp_all_audio",
			"timestamp_all_animations",
			"timestamp_all_skeletons",
			"timestamp_all_mesh_skins",
			"timestamp_all_plugins"
		};

	bool Game::CachedData::ParseInternal(
		Game& gameAPI, AssetDatabase& assetDatabase, Asset::Manager& assetManager, const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags
	)
	{
		if (m_assetsData.IsValid())
		{
			Threading::UniqueLock lock(m_assetsMutex);
			Serialization::Reader reader(m_assetsData);
			if (reader.ReadWithDefaultValue<uint16>("version", 0) != Version)
			{
				// Out of date, throw away cache
				return false;
			}

			const Optional<ConstStringView> environmentString = reader.Read<ConstStringView>("environment");
			if (environmentString.IsValid())
			{
				const Platform::Environment environment = Platform::GetEnvironment(*environmentString);
				if (environment == Platform::GetEnvironment())
				{
					for (StreamedRequestType request = StreamedRequestType::First; request < StreamedRequestType::Count; ++request)
					{
						m_streamedRequestTimestamps[request] = reader.ReadWithDefaultValue<Time::Timestamp>(StreamedRequestKeys[request], {});
					}

					const Optional<uint32> internalPlayerIdentifier = reader.Read<uint32>("local_player_internal_identifier");
					if (internalPlayerIdentifier.IsValid())
					{
						m_localPlayerInternalIdentifier = *internalPlayerIdentifier;
					}

					if (const Optional<Serialization::Reader> signInProvidersReader = reader.FindSerializer("sign_in_providers"))
					{
						for (const SignInFlags signInProvider : EnumFlags<SignInFlags>{SignInFlags::AllProviders})
						{
							if (const Optional<Serialization::Reader> signInProviderReader = signInProvidersReader->FindSerializer(GetSignInFlagString(signInProvider)))
							{
								m_savedLoginInfo[Math::Log2((uint8)signInProvider)] = LoginInfo{
									signInProviderReader->ReadWithDefaultValue<UnicodeString>("user_id", UnicodeString{}),
									signInProviderReader->ReadWithDefaultValue<UnicodeString>("auth_token", UnicodeString{})
								};
								m_savedSignInProviders |= signInProvider;
							}
						}
					}

					if (const Optional<Serialization::Reader> assetsReader = reader.FindSerializer("assets"))
					{
						assetDatabase.ReserveAssets((uint32)assetsReader->GetMemberCount());
						m_assetCapacity += (uint32)assetsReader->GetMemberCount();
						m_assetsMap.Reserve(m_assetCapacity);

						for (const Serialization::Member<Serialization::Reader> assetMember : assetsReader->GetMemberView())
						{
							assetDatabase.ParseAsset(assetMember.value, Guid::TryParse(assetMember.key), assetManager, parsingFlags, gameAPI);
						}
					}
					return true;
				}
			}
		}
		return false;
	}

	void Game::CachedData::Parse(
		Game& gameAPI, AssetDatabase& assetDatabase, Asset::Manager& assetManager, const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags
	)
	{
		if (!ParseInternal(gameAPI, assetDatabase, assetManager, parsingFlags))
		{
			m_streamedRequestTimestamps = {};

			Threading::UniqueLock lock(m_assetsMutex);
			m_assetsData = Serialization::Value(rapidjson::kObjectType);
		}
		else
		{
			Threading::UniqueLock lock(m_assetsMutex);
			Serialization::Value& assetsValue = m_assetsData.GetDocument().FindMember("assets")->value;
			m_assetCapacity += assetsValue.MemberCount();
			m_assetsMap.Reserve(m_assetCapacity);

			Serialization::Reader assetsReader(assetsValue, m_assetsData);
			for (Serialization::Member<Serialization::Reader> assetMember : assetsReader.GetMemberView())
			{
				const Serialization::TValue& value = assetMember.value.GetValue();
				Assert(value.IsValid());
				m_assetsMap.Emplace(Guid(Guid::TryParse(assetMember.key)), Move(const_cast<Serialization::TValue&>(value).GetValue()));
			}
		}
	}

	void Game::CachedData::Reserve(const uint32 count)
	{
		Threading::UniqueLock lock(m_assetsMutex);
		m_assetCapacity += count;
		m_assetsMap.Reserve(m_assetCapacity);
	}

	void Game::CachedData::AddAsset(const Guid assetGuid, const Serialization::Reader assetReader)
	{
		{
			Threading::UniqueLock lock(m_assetsMutex);
			auto it = m_assetsMap.Find(assetGuid);
			if (it != m_assetsMap.end())
			{
				it->second = Serialization::Value(assetReader.GetValue(), m_assetsData.GetDocument().GetAllocator());
			}
			else
			{
				m_assetsMap.Emplace(Guid(assetGuid), Serialization::Value(assetReader.GetValue(), m_assetsData.GetDocument().GetAllocator()));
			}
		}

		OnChanged();
	}

	void Game::CachedData::RemoveAsset(const Guid assetGuid)
	{
		{
			Threading::UniqueLock lock(m_assetsMutex);
			auto it = m_assetsMap.Find(assetGuid);
			if (it != m_assetsMap.end())
			{
				m_assetsMap.Remove(it);
				lock.Unlock();
				OnChanged();
			}
		}
	}

	bool FileURIToken::Serialize(const Serialization::Reader reader)
	{
		reader.Serialize("file_token", m_token);
		reader.Serialize("expires_at", m_expiresAt);
		return true;
	}

	void Game::CachedData::TryUpdateTimestamp(const StreamedRequestType request, const Time::Timestamp timestamp)
	{
		Time::Timestamp& storedTimestamp = m_streamedRequestTimestamps[request];
		if (storedTimestamp < timestamp)
		{
			OnChanged();
			storedTimestamp = timestamp;
		}
	}

	bool Game::CachedData::Save(const IO::PathView cacheName)
	{
		if (m_assetsData.IsValid())
		{
			Threading::UniqueLock lock(m_assetsMutex);
			Serialization::Writer writer(m_assetsData);

			writer.Serialize("version", Version);

			m_assetsData.GetDocument().RemoveMember("assets");
			Serialization::Object assetsObject;
			assetsObject.Reserve(m_assetsMap.GetSize(), m_assetsData.GetDocument());
			for (auto it = m_assetsMap.begin(), endIt = m_assetsMap.end(); it != endIt; ++it)
			{
				Assert(it->second.IsObject());
				assetsObject.AddMember(
					it->first.ToString().GetView(),
					Serialization::TValue(it->second, m_assetsData.GetDocument()),
					m_assetsData.GetDocument()
				);
			}
			m_assetsData.GetDocument().AddMember("assets", Move(assetsObject.GetValue()), m_assetsData.GetDocument().GetAllocator());

			writer.Serialize("environment", Platform::GetEnvironmentString(Platform::GetEnvironment()));

			for (StreamedRequestType request = StreamedRequestType::First; request < StreamedRequestType::Count; ++request)
			{
				writer.Serialize(StreamedRequestKeys[request], m_streamedRequestTimestamps[request]);
			}

			writer.Serialize("local_player_internal_identifier", m_localPlayerInternalIdentifier.Get());

			m_assetsData.GetDocument().RemoveMember("sign_in_providers");
			writer.SerializeObjectWithCallback(
				"sign_in_providers",
				[this](Serialization::Writer signInProvidersWriter)
				{
					bool wroteAny{false};
					for (const SignInFlags signInProvider : m_savedSignInProviders)
					{
						const LoginInfo& loginInfo = GetLoginInfo(signInProvider);
						if (loginInfo.userIdentifier.HasElements() || loginInfo.authorizationToken.HasElements())
						{
							wroteAny |= signInProvidersWriter.SerializeObjectWithCallback(
								GetSignInFlagString(signInProvider),
								[&loginInfo](Serialization::Writer signInProviderWriter)
								{
									signInProviderWriter.Serialize("user_id", loginInfo.userIdentifier);
									signInProviderWriter.Serialize("auth_token", loginInfo.authorizationToken);
									return true;
								}
							);
						}
					}
					return wroteAny;
				}
			);

			const IO::Path targetPath = IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), MAKE_PATH("CachedAssets"), cacheName);
			const IO::Path directory(targetPath.GetParentPath());
			if (!directory.Exists())
			{
				directory.CreateDirectories();
			}
			return m_assetsData.SaveToFile(targetPath, Serialization::SavingFlags{});
		}

		return false;
	}

	void Game::CachedData::OnChanged()
	{
		bool expected = false;
		if (m_isDirty.CompareExchangeStrong(expected, true) && !m_registeredSaveCallback)
		{
			m_registeredSaveCallback = true;
			if (const Optional<Engine*> pEngine = System::Find<Engine>())
			{
				pEngine->GetQuitJobBatch().QueueAfterStartStage(Threading::CreateCallback(
					[this](Threading::JobRunnerThread&)
					{
						SaveIfNecessary();
					},
					Threading::JobPriority::SaveOnClose
				));
			}
		}
	}

	void Game::CachedData::SaveIfNecessary()
	{
		bool expected = true;
		if (m_isDirty.CompareExchangeStrong(expected, false))
		{
			[[maybe_unused]] const bool wasSaved = Save(MAKE_PATH("Assets.json"));
			Assert(wasSaved);
		}
	}

	void Game::RequestAllAssets(StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback)
	{
		IO::URI::StringType requestEndpoint;
		const Time::Timestamp timestamp = m_cachedData.GetTimestamp(StreamedRequestType::AllAssets);
		if (timestamp.IsValid())
		{
			requestEndpoint.Format(MAKE_URI_LITERAL("game/v1/assets?include_ugc=true&since={}"), timestamp);
		}
		else
		{
			requestEndpoint.Format(MAKE_URI_LITERAL("game/v1/assets?include_ugc=true"));
		}
		QueueStreamedRequest(
			HTTP::RequestType::Get,
			RequestFlags{},
			IO::URI(Move(requestEndpoint)),
			{},
			Forward<StreamResultCallback>(callback),
			Forward<StreamingFinishedCallback>(finishedCallback)
		);
	}

	void
	Game::RequestPaginatedAssetsOfType(const Guid assetTypeGuid, RequestPaginatedParameters paginatedParameters, RequestCallback&& callback)
	{
		IO::URI::StringType requestEndpoint;

		if (paginatedParameters.afterAssetId == 0 && paginatedParameters.orderBy == RequestOrderBy::Id)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&order_by=id"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending
			);
		}
		else if (paginatedParameters.orderBy == RequestOrderBy::Id)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&after={}&order_by=id"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending,
				paginatedParameters.afterAssetId
			);
		}
		else if (paginatedParameters.afterAssetId == 0 && paginatedParameters.orderBy == RequestOrderBy::Date)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&order_by=date"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending
			);
		}
		else if (paginatedParameters.orderBy == RequestOrderBy::Date)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&after={}&after_date={}&order_by=date"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending,
				paginatedParameters.afterAssetId,
				paginatedParameters.afterDate
			);
		}
		else if (paginatedParameters.afterAssetId == 0 && paginatedParameters.orderBy == RequestOrderBy::Likes)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&order_by=likes"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending
			);
		}
		else if (paginatedParameters.orderBy == RequestOrderBy::Likes)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&after={}&after_likes={}&order_by=likes"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending,
				paginatedParameters.afterAssetId,
				paginatedParameters.afterLikes
			);
		}
		else if (paginatedParameters.afterAssetId == 0 && paginatedParameters.orderBy == RequestOrderBy::Plays)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&order_by=plays"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending
			);
		}
		else if (paginatedParameters.orderBy == RequestOrderBy::Plays)
		{
			requestEndpoint.Format(
				MAKE_URI_LITERAL("game/v1/assets/list?context_uuid={}&count={}&featured_only={}&asc={}&after={}&after_plays={}&order_by=plays"),
				assetTypeGuid,
				paginatedParameters.count,
				paginatedParameters.featuredOnly,
				paginatedParameters.sortingOrder == DataSource::SortingOrder::Ascending,
				paginatedParameters.afterAssetId,
				paginatedParameters.afterPlays
			);
		}

		if (!paginatedParameters.tags.IsEmpty())
		{
			IO::URI::StringType requestTag;
			requestTag.Format(MAKE_URI_LITERAL("&tags="));
			requestEndpoint += requestTag;
			int32 i = 0;
			for (Guid tag : paginatedParameters.tags)
			{
				IO::URI::StringType requestTagArg;
				if (i > 0)
				{
					requestTagArg.Format(MAKE_URI_LITERAL(",{}"), tag.ToString());
				}
				else
				{
					requestTagArg.Format(MAKE_URI_LITERAL("{}"), tag.ToString());
				}
				requestEndpoint += requestTagArg;
				i++;
			}
		}

		if (paginatedParameters.playerId.IsValid())
		{
			IO::URI::StringType requestCreatorId;
			requestCreatorId.Format(MAKE_URI_LITERAL("&creator_id={}"), paginatedParameters.playerId.Get());
			requestEndpoint += requestCreatorId;
		}

		if (!paginatedParameters.search.IsEmpty())
		{
			IO::URI::StringType requestSearch;
			requestSearch.Format(MAKE_URI_LITERAL("&search={}"), paginatedParameters.search);
			requestEndpoint += requestSearch;
		}

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(requestEndpoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void Game::RequestPlayerAssets(const Guid assetTypeGuid, const uint32 afterAssetId)
	{
		RequestPaginatedParameters playerParams = {};
		playerParams.count = 15;
		playerParams.orderBy = Networking::Backend::Game::RequestOrderBy::Plays;
		playerParams.playerId = GetLocalPlayerInternalIdentifier();
		playerParams.afterAssetId = afterAssetId;

		RequestPaginatedAssetsOfType(
			assetTypeGuid,
			playerParams,
			[this, assetTypeGuid, previousLastAssetId = afterAssetId, &assetManager = System::Get<Asset::Manager>()](
				const bool,
				const Networking::HTTP::ResponseCode responseCode,
				const Serialization::Reader assetReader
			)
			{
				if (responseCode.IsSuccessful())
				{
					HandlePaginatedAssetResponse(assetReader, assetManager);

					if (const Optional<uint32> lastAssetId = assetReader.Read<uint32>("last_asset_id");
				      lastAssetId.IsValid() && *lastAssetId != previousLastAssetId)
					{
						RequestPlayerAssets(assetTypeGuid, *lastAssetId);
					}
				}
				else
				{
					Assert(false);
				}
			}
		);
	}

	void Game::RequestAllAssetsOfType(
		const StreamedRequestType request,
		const EnumFlags<RequestFlags> requestFlags,
		const Guid assetTypeGuid,
		StreamResultCallback&& callback,
		StreamingFinishedCallback&& finishedCallback
	)
	{
		IO::URI::StringType requestEndpoint;
		const Time::Timestamp timestamp = m_cachedData.GetTimestamp(request);
		if (timestamp.IsValid())
		{
			requestEndpoint.Format(MAKE_URI_LITERAL("game/v1/assets?include_ugc=true&since={}&context_uuid={}"), timestamp, assetTypeGuid);
		}
		else
		{
			requestEndpoint.Format(MAKE_URI_LITERAL("game/v1/assets?include_ugc=true&context_uuid={}"), assetTypeGuid);
		}
		QueueStreamedRequest(
			HTTP::RequestType::Get,
			requestFlags,
			IO::URI(Move(requestEndpoint)),
			{},
			Forward<StreamResultCallback>(callback),
			Forward<StreamingFinishedCallback>(finishedCallback)
		);
	}

	void Game::RequestAllAssetLikesCountOfType(const Guid assetTypeGuid, RequestCallback&& callback)
	{
		IO::URI::StringType requestEndpoint;
		requestEndpoint.Format(MAKE_URI_LITERAL("game/asset/likecount?&context_uuid={}"), assetTypeGuid);
		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(requestEndpoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void Game::RequestAllAssetPlaysCountOfType(const Guid assetTypeGuid, RequestCallback&& callback)
	{
		IO::URI::StringType requestEndpoint;
		requestEndpoint.Format(MAKE_URI_LITERAL("game/asset/playcount?&context_uuid={}"), assetTypeGuid);
		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(requestEndpoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void
	Game::RequestAssetsInternal(Vector<Asset::Guid>&& assets, StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback)
	{
		Assert(assets.HasElements());
		const IO::URIView formatURI = MAKE_URI("game/v1/assets/stream/by/id?asset_ids=");

		constexpr uint32 maximumAssetsPerRequest = 40;

		struct Info
		{
			Info(Info&&) = default;
			Info& operator=(Info&&) = default;
			Info(const Info&) = delete;
			Info& operator=(const Info&) = delete;
			~Info()
			{
				if (m_finishedCallback.IsValid())
				{
					m_finishedCallback(m_succeeded.Load());
				}
			}

			StreamResultCallback m_callback;
			StreamingFinishedCallback m_finishedCallback;
			Threading::Atomic<bool> m_succeeded{true};
		};
		SharedPtr<Info> pInfo =
			SharedPtr<Info>::Make(Info{Forward<StreamResultCallback>(callback), Forward<StreamingFinishedCallback>(finishedCallback)});

		ArrayView<const Asset::Guid> remainingAssets = assets;

		while (remainingAssets.HasElements())
		{
			const ArrayView<const Asset::Guid> requestedAssets = remainingAssets.GetSubView(0, maximumAssetsPerRequest);
			remainingAssets += requestedAssets.GetSize();

			constexpr uint32 guidStringSize = 36;
			IO::URI::StringType uriString(Memory::Reserve, formatURI.GetSize() + uint16((guidStringSize + 1) * requestedAssets.GetSize()));
			uriString = formatURI.GetStringView();
			uriString += requestedAssets[0].ToString();
			for (const Asset::Guid assetGuid : (requestedAssets + 1))
			{
				uriString += ',';
				uriString += assetGuid.ToString();
			}

			QueueStreamedRequest(
				HTTP::RequestType::Get,
				RequestFlags::HighPriority,
				IO::URI(Move(uriString)),
				{},
				[this, pInfo](const Serialization::Reader assetReader, [[maybe_unused]] const ConstStringView objectJson) mutable
				{
					if (const Optional<Serialization::Reader> streamedObjectCount = assetReader.FindSerializer("streamedObjectCount"))
					{
						m_cachedData.SaveIfNecessary();
					}
					else
					{
						pInfo->m_callback(assetReader, {});
					}
				},
				[pInfo](const bool success)
				{
					Assert(success);
					if (!success)
					{
						bool expected = true;
						while (!pInfo->m_succeeded.CompareExchangeWeak(expected, false))
							;
					}
				}

			);
		}
	}

	void Game::RequestAssets(Vector<Asset::Guid>&& assets, AssetStreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback)
	{
		AssetDatabase& assetDatabase = GetAssetDatabase();
		assets.RemoveAllOccurrencesPredicate(
			[&assetDatabase, &callback](const Asset::Guid assetGuid)
			{
				return assetDatabase.VisitEntry(
					assetGuid,
					[&callback](const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry) -> ErasePredicateResult
					{
						if (pBackendAssetEntry.IsValid())
						{
							if (pBackendAssetEntry->m_flags.IsSet(AssetFlags::UpToDate))
							{
								if (callback.IsValid())
								{
									callback(pBackendAssetEntry);
								}
								return ErasePredicateResult::Remove;
							}
						}
						return ErasePredicateResult::Continue;
					}
				);
			}
		);
		if (assets.IsEmpty())
		{
			finishedCallback(true);
			return;
		}

		if (m_signInFlags.IsSet(SignInFlags::HasSessionToken))
		{
			GetAssetDatabase().ReserveAssets(assets.GetSize());
			GetCachedData().Reserve(assets.GetSize());
			RequestAssetsInternal(
				Forward<Vector<Asset::Guid>>(assets),
				[this,
			   &assetManager = System::Get<Asset::Manager>(),
			   callback = Forward<AssetStreamResultCallback>(callback
			   )](const Serialization::Reader assetReader, [[maybe_unused]] const ConstStringView objectJson) mutable
				{
					HandleAssetResponse(assetReader, assetManager);

					if (callback.IsValid())
					{
						AssetDatabase& assetDatabase = GetAssetDatabase();
						const Optional<Guid> assetGuid = assetReader.Read<Guid>("uuid");
						if (assetGuid.IsValid())
						{
							assetDatabase.VisitEntry(
								*assetGuid,
								[&callback](const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry)
								{
									callback(pBackendAssetEntry);
								}
							);
						}
					}
				},
				[finishedCallback = Forward<StreamingFinishedCallback>(finishedCallback)](const bool success)
				{
					if (finishedCallback.IsValid())
					{
						finishedCallback(success);
					}
				}
			);
		}
		else if (m_signInFlags.IsSet(SignInFlags::Offline))
		{
			if (callback.IsValid())
			{
				for (const Asset::Guid assetGuid : assets)
				{
					m_assetDatabase.VisitEntry(
						assetGuid,
						[&callback](const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry)
						{
							callback(pBackendAssetEntry);
						}
					);
				}
			}
			finishedCallback(false);
		}
		else
		{
			QueuePostSessionRegistrationCallback(
				[this,
			   assets = Forward<Vector<Asset::Guid>>(assets),
			   callback = Forward<AssetStreamResultCallback>(callback),
			   finishedCallback = Forward<StreamingFinishedCallback>(finishedCallback)]([[maybe_unused]] const EnumFlags<SignInFlags>) mutable
				{
					RequestAssets(Move(assets), Move(callback), Move(finishedCallback));
				}
			);
		}
	}

	void
	Game::RequestAssetContents(const Asset::Guid assetGuid, StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback)
	{
		IO::URI::StringType uriString;
		uriString.Format(MAKE_URI_LITERAL("game/v1/assets/stream/contents/by/id?asset_id={}"), assetGuid);

		struct Info
		{
			Info(Info&&) = default;
			Info& operator=(Info&&) = default;
			Info(const Info&) = delete;
			Info& operator=(const Info&) = delete;
			~Info()
			{
				if (m_finishedCallback.IsValid())
				{
					m_finishedCallback(m_succeeded.Load());
				}
			}

			StreamResultCallback m_callback;
			StreamingFinishedCallback m_finishedCallback;
			Threading::Atomic<bool> m_succeeded{true};
		};
		SharedPtr<Info> pInfo =
			SharedPtr<Info>::Make(Info{Forward<StreamResultCallback>(callback), Forward<StreamingFinishedCallback>(finishedCallback)});

		QueueStreamedRequest(
			HTTP::RequestType::Get,
			RequestFlags::HighPriority,
			IO::URI(Move(uriString)),
			{},
			[this, pInfo](const Serialization::Reader assetReader, [[maybe_unused]] const ConstStringView objectJson) mutable
			{
				if (const Optional<Serialization::Reader> streamedObjectCount = assetReader.FindSerializer("streamedObjectCount"))
				{
					m_cachedData.SaveIfNecessary();
				}
				else
				{
					pInfo->m_callback(assetReader, {});
				}
			},
			[pInfo](const bool success)
			{
				if (!success)
				{
					bool expected = true;
					while (!pInfo->m_succeeded.CompareExchangeWeak(expected, false))
						;
				}
			}
		);
	}

	void Game::RequestInventory(StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback)
	{
		QueueStreamedRequest(
			HTTP::RequestType::Get,
			RequestFlags::HighPriority,
			MAKE_URI("game/v1/player/inventory"),
			{},
			Forward<StreamResultCallback>(callback),
			Forward<StreamingFinishedCallback>(finishedCallback)
		);
	}

	void Game::RequestPlayerInventory()
	{
		IO::URI::StringType requestEndpoint;
		requestEndpoint.Format(MAKE_URI_LITERAL("game/player/inventory"));
		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(requestEndpoint)),
			{},
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				if (code.IsSuccessful())
				{
					if (const Optional<Serialization::Reader> itemsReader = reader.FindSerializer("items"))
					{
						Assert(itemsReader->IsArray());
						if (LIKELY(itemsReader->IsArray()))
						{
							Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							for (const Serialization::Reader itemReader : itemsReader->GetArrayView())
							{
								Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
								const Optional<Guid> assetGuid = itemReader.Read<Guid>("asset_uuid");
								Assert(assetGuid.IsValid());
								if (LIKELY(assetGuid.IsValid()))
								{
									m_assetDatabase.TryEmplaceAndVisitEntry(
										*assetGuid,
										[assetGuid = *assetGuid, &assetManager, &assetLibrary, &tagRegistry](Networking::Backend::AssetEntry&)
										{
											Asset::Identifier assetId = assetManager.GetAssetIdentifier(assetGuid);
											if (assetId.IsValid())
											{
												assetManager.SetTagAsset(tagRegistry.FindOrRegister(ngine::Asset::Library::OwnedAssetTagGuid), assetId);
												assetManager.OnDataChanged();
											}
											Asset::Identifier libraryAssetId = assetLibrary.GetAssetIdentifier(assetGuid);
											if (libraryAssetId.IsValid())
											{
												assetLibrary.SetTagAsset(tagRegistry.FindOrRegister(ngine::Asset::Library::OwnedAssetTagGuid), libraryAssetId);
												assetLibrary.OnDataChanged();
											}
										}
									);
								}
							}
						}
					}
				}
				else
				{
					Assert(false);
				}
			}
		);
	}

	void Game::PurchaseAssets(const ArrayView<const AssetCatalogItem> assets, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		Serialization::Data serializedItemsData(rapidjson::kArrayType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writerItems(serializedItemsData);

		writerItems.SerializeArrayCallbackInPlace(
			[assets](Serialization::Writer elementWriter, const uint32 index)
			{
				elementWriter.GetValue() = Serialization::Value(rapidjson::Type::kObjectType);
				elementWriter.Serialize("catalog_listing_id", assets[index].m_catalogListingId);
				elementWriter.Serialize("quantity", assets[index].m_quantity);
				return true;
			},
			assets.GetSize()
		);

		writer.Serialize("wallet_id", m_currency.m_userWalletId);
		writer.GetValue().AddMember("items", Move(serializedItemsData.GetDocument()), writer.GetDocument().GetAllocator());

		QueueRequest(
			HTTP::RequestType::Post,
			MAKE_URI("game/purchase"),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				callback(success, responseCode, responseReader);
			}
		);
	}

	void Game::Currency::AddAssetToCart(Guid assetGuid)
	{
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::Game& backendGameAPI = backend.GetGame();
		backendGameAPI.m_assetDatabase.TryEmplaceAndVisitEntry(
			assetGuid,
			[this, assetGuid, &backendGameAPI](Networking::Backend::AssetEntry& assetEntry)
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				if (!m_assetsInCart.Find(assetGuid))
				{
					m_assetsInCart.EmplaceBack(assetGuid);
				}
				assetEntry.m_flags |= AssetFlags::IsInCart;

				Asset::Identifier libraryAssetId = assetLibrary.GetAssetIdentifier(assetGuid);
				if (LIKELY(libraryAssetId.IsValid()))
				{
					assetLibrary.SetTagAsset(tagRegistry.FindOrRegister(ngine::Asset::Library::IsInCartTagGuid), libraryAssetId);
					assetLibrary.OnDataChanged();
				}

				backendGameAPI.m_playersDataSource.OnDataChanged();

				if (m_assetsInCart.GetSize() == 1)
				{
					Events::Manager& eventsManager = System::Get<Events::Manager>();
					eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent("08e4ec53-c6fc-485f-ab42-b9d19bfc5667"_guid));
				}
			}
		);
	}

	void Game::Currency::RemoveAssetFromCart(Guid assetGuid)
	{
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::Game& backendGameAPI = backend.GetGame();
		backendGameAPI.m_assetDatabase.TryEmplaceAndVisitEntry(
			assetGuid,
			[this, assetGuid, &backendGameAPI](Networking::Backend::AssetEntry& assetEntry)
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				m_assetsInCart.RemoveAllOccurrences(assetGuid);
				assetEntry.m_flags &= ~AssetFlags::IsInCart;

				Asset::Identifier libraryAssetId = assetLibrary.GetAssetIdentifier(assetGuid);
				if (LIKELY(libraryAssetId.IsValid()))
				{
					assetLibrary.ClearTagAsset(tagRegistry.FindOrRegister(ngine::Asset::Library::IsInCartTagGuid), libraryAssetId);
					assetLibrary.OnDataChanged();
				}

				backendGameAPI.m_playersDataSource.OnDataChanged();

				if (m_assetsInCart.GetSize() == 0)
				{
					Events::Manager& eventsManager = System::Get<Events::Manager>();
					eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent("c8677cbf-4fbb-4770-96ce-ee83c055a5e0"_guid));
				}
			}
		);
	}

	void Game::Currency::AddAmount(uint64 amount)
	{
		m_balance += amount;
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::Game& backendGameAPI = backend.GetGame();
		backendGameAPI.m_playersDataSource.OnDataChanged();
	}

	void Game::Currency::RemoveAmount(uint64 amount)
	{
		if (m_balance >= amount)
		{
			m_balance -= amount;
		}
		else
		{
			m_balance = 0;
		}
		Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Backend::Game& backendGameAPI = backend.GetGame();
		backendGameAPI.m_playersDataSource.OnDataChanged();
	}

	uint64 Game::Currency::GetCartTotalPrice()
	{
		Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Backend::Game& backendGameAPI = backend.GetGame();

		uint64 totalPrice = 0;
		for (uint32 i = 0; i < m_assetsInCart.GetSize(); i++)
		{
			backendGameAPI.m_assetDatabase.VisitEntry(
				m_assetsInCart[i],
				[&totalPrice](const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry)
				{
					Assert(pBackendAssetEntry.IsValid());
					if (LIKELY(pBackendAssetEntry.IsValid()))
					{
						totalPrice += pBackendAssetEntry->m_price;
					}
				}
			);
		}
		return totalPrice;
	}

	void Game::CreatePlayerAsset(
		const Asset::Guid assetGuid,
		const Asset::DatabaseEntry& assetEntry,
		const IO::PathView assetsFolderPath,
		AssetProgressCallback&& progressCallback
	)
	{
		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/asset"), GetLocalPlayerInternalIdentifier().Get());

		m_backend.CreateAsset(
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{m_sessionTokenHeader}.GetView(),
			assetGuid,
			assetEntry,
			assetsFolderPath,
			Plugin::UploadAssetFilesCallback(*this, &Game::UploadPlayerAssetFiles),
			Forward<AssetProgressCallback>(progressCallback)
		);
	}

	void Game::UpdatePlayerAsset(
		const uint32 assetId,
		const ConstStringView name,
		const Guid contextGuid,
		const Asset::Guid guid,
		const bool isComplete,
		RequestCallback&& callback
	)
	{
		// const Serialization::Reader assetReader = reader.GetSerializer("asset");

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("completed", isComplete);
		writer.SerializeObjectWithCallback(
			"data",
			[name = (ConstStringView)name, contextGuid, guid](Serialization::Writer writer)
			{
				writer.Serialize("name", name);
				writer.Serialize("context_id", contextGuid);
				writer.Serialize("uuid", guid);

				// Can also use kv_storage, filters and data_entities here
				return true;
			}
		);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/assets/{}"), GetLocalPlayerInternalIdentifier().Get(), assetId);

		QueueRequest(
			HTTP::RequestType::Put,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				callback(success, responseCode, responseReader);
			}
		);
	}

	void Game::LikeAsset(const uint32 assetId, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/asset/{}/like/{}"), assetId, GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void Game::PlayAsset(const uint32 assetId, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/asset/{}/play/{}"), assetId, GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void Game::UnlikeAsset(const uint32 assetId, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/asset/{}/unlike/{}"), assetId, GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode code, const Serialization::Reader reader)
			{
				callback(success, code, reader);
			}
		);
	}

	void Game::GetPlayerLikedAssets(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/likes"), GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetPlayerFollowings(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/followings"), GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetPlayerCurrencyBalance(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/balances/player"), GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetBuyableGamesList(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		const uint32 perPage = 15;
		endPoint.Format(MAKE_URI_LITERAL("game/catalog/key/games/prices?per_page={}"), perPage);

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetBuyableCharactersList(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		const uint32 perPage = 15;
		endPoint.Format(MAKE_URI_LITERAL("game/catalog/key/characters/prices?per_page={}"), perPage);

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::FollowPlayer(const PersistentUserIdentifier playerID, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/follow"), playerID.Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::UnfollowPlayer(const PersistentUserIdentifier playerID, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/unfollow"), playerID.Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetPlayerProfiles(RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/list?&count={}&context_uuid={}"), 50000, ProjectAssetFormat.assetTypeGuid.ToString());

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::CreatePlayerProfile(
		const ConstUnicodeStringView username,
		ConstUnicodeStringView biography,
		const Asset::Guid profilePictureGuid,
		RequestCallback&& callback
	)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("username", username);
		writer.Serialize("biography", biography);
		writer.Serialize("picture_asset_uuid", profilePictureGuid.ToString());

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/profile"), GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::UpdatePlayerProfile(
		const ConstUnicodeStringView username,
		ConstUnicodeStringView biography,
		const Asset::Guid profilePictureGuid,
		RequestCallback&& callback
	)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		writer.Serialize("username", username);
		writer.Serialize("biography", biography);
		writer.Serialize("picture_asset_uuid", profilePictureGuid.ToString());

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/player/{}/profile"), GetLocalPlayerInternalIdentifier().Get());

		QueueRequest(
			HTTP::RequestType::Put,
			IO::URI(Move(endPoint)),
			writer.SaveToBuffer<String>(),
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::GetPlayerProfileByUsername(const ConstUnicodeStringView username, RequestCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(
			MAKE_URI_LITERAL("game/player/{}/profile?&context_uuid={}&username={}"),
			username,
			ProjectAssetFormat.assetTypeGuid.ToString(),
			username
		);

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				callback(success, responseCode, reader);
			}
		);
	}

	void Game::RequestPlayerProfileById(const PersistentUserIdentifier playerId, Function<void(const bool success), 24>&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint
			.Format(MAKE_URI_LITERAL("game/player/{}/profile?&context_uuid={}"), playerId.Get(), ProjectAssetFormat.assetTypeGuid.ToString());

		QueueRequest(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			{},
			[this,
		   callback = Forward<Function<void(const bool success), 24>>(callback
		   )](const bool success, const HTTP::ResponseCode, const Serialization::Reader reader)
			{
				if (success)
				{
					HandleSinglePlayerGetResponse(reader);
				}

				callback(success);
			}
		);
	}

	void Game::UploadPlayerAssetFiles(
		const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback
	)
	{
		m_backend.UploadAssetFiles(
			[this, assetIdentifier](HTTP::FormDataPart&& part, AssetProgressCallback&& progressCallback)
			{
				progressCallback(true, 1);
				UploadPlayerAssetFile(
					assetIdentifier,
					Forward<HTTP::FormDataPart>(part),
					[progressCallback = Forward<AssetProgressCallback>(progressCallback
			     )](const bool success, const HTTP::ResponseCode, const Serialization::Reader)
					{
						Assert(success);
						progressCallback(success, -1);
					}
				);
			},
			assetEntry,
			progressCallback
		);
	}

	void Game::CreatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback)
	{
		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/assets"), GetLocalPlayerInternalIdentifier().Get());

		m_backend.CreateAssets(
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			Forward<IO::Path>(assetDatabasePath),
			Forward<Asset::Database>(assetDatabase),
			[this](const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback)
			{
				UploadPlayerAssetFiles(assetIdentifier, assetEntry, progressCallback);
			},
			Forward<AssetProgressCallback>(progressCallback)
		);
	}

	void Game::ActivatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback)
	{
		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/assets/activate"), GetLocalPlayerInternalIdentifier().Get());

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		// Make sure the database contains itself
		assetDatabase.RegisterAsset(
			assetDatabase.GetGuid(),
			Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, {}, Move(assetDatabasePath)},
			assetDatabasePath.GetParentPath()
		);

		Serialization::Data assetsData(rapidjson::kArrayType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer assetsWriter(assetsData);

		assetDatabase.IterateAssets(
			[assetsWriter](const Asset::Guid assetGuid, const Asset::DatabaseEntry&) mutable
			{
				Serialization::Writer assetWriter = assetsWriter.EmplaceArrayElement();
				assetWriter.SerializeInPlace(assetGuid);
				return Memory::CallbackResult::Continue;
			}
		);

		writer.GetValue().AddMember("assets", Move(assetsData.GetDocument()), writer.GetDocument().GetAllocator());

		Assert(m_signInFlags.IsSet(SignInFlags::HasSessionToken));
		progressCallback(true, 1);
		m_backend.QueueStreamedRequestInternal(
			HTTP::RequestType::Post,
			RequestFlags{},
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[](const Serialization::Reader objectReader, [[maybe_unused]] const ConstStringView objectJson)
			{
				LogError(
					"Failed to activate asset {} with error '{}'",
					objectReader.ReadWithDefaultValue<Asset::Guid>("asset_uuid", {}),
					objectReader.ReadWithDefaultValue<ConstStringView>("failure_message", "No error provided.")
				);
			},
			[progressCallback](const bool success)
			{
				Assert(success);
				progressCallback(success, -1);
			}
		);
	}

	void Game::DeactivatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback)
	{
		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/assets/deactivate"), GetLocalPlayerInternalIdentifier().Get());

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		// Make sure the database contains itself
		assetDatabase.RegisterAsset(
			assetDatabase.GetGuid(),
			Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, {}, Move(assetDatabasePath)},
			assetDatabasePath.GetParentPath()
		);

		Serialization::Data assetsData(rapidjson::kArrayType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer assetsWriter(assetsData);

		assetDatabase.IterateAssets(
			[assetsWriter](const Asset::Guid assetGuid, const Asset::DatabaseEntry&) mutable
			{
				Serialization::Writer assetWriter = assetsWriter.EmplaceArrayElement();
				assetWriter.SerializeInPlace(assetGuid);
				return Memory::CallbackResult::Continue;
			}
		);

		writer.GetValue().AddMember("assets", Move(assetsData.GetDocument()), writer.GetDocument().GetAllocator());

		Assert(m_signInFlags.IsSet(SignInFlags::HasSessionToken));
		progressCallback(true, 1);
		m_backend.QueueStreamedRequestInternal(
			HTTP::RequestType::Post,
			RequestFlags{},
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			writer.SaveToBuffer<String>(),
			[](const Serialization::Reader objectReader, [[maybe_unused]] const ConstStringView objectJson)
			{
				LogError(
					"Failed to deactivate asset {} with error '{}'",
					objectReader.ReadWithDefaultValue<Asset::Guid>("asset_uuid", {}),
					objectReader.ReadWithDefaultValue<ConstStringView>("failure_message", "No error provided.")
				);
			},
			[progressCallback](const bool success)
			{
				Assert(success);
				progressCallback(success, -1);
			}
		);
	}

	void Game::ActivatePlayerAsset(const Asset::Guid assetGuid)
	{
		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/asset/{}/activate"), GetLocalPlayerInternalIdentifier().Get(), assetGuid);

		QueueRequest(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			{},
			[]([[maybe_unused]] const bool success, const HTTP::ResponseCode, const Serialization::Reader) mutable
			{
				Assert(success);
			}
		);
	}

	void Game::UploadPlayerAssetFile(const uint32 assetId, HTTP::FormDataPart&& fileData, RequestCallback&& callback)
	{
		HTTP::FormData formData;
		HTTP::FormDataPart& filePart = formData.m_parts.EmplaceBack(Forward<HTTP::FormDataPart>(fileData));
		filePart.m_name = "file";

		formData.EmplacePart("purpose", "text/plain", ConstStringView("FILE"));

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("game/v1/player/{}/assets/{}/file"), GetLocalPlayerInternalIdentifier().Get(), assetId);

		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "multipart/form-data"},
				m_sessionTokenHeader
			}
				.GetView(),
			Move(formData),
			[callback = Forward<RequestCallback>(callback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				callback(success, responseCode, responseReader);
			}
		);
	}

	void Game::QueueStreamedRequest(
		const HTTP::RequestType requestType,
		const EnumFlags<RequestFlags> requestFlags,
		const IO::URIView endpointName,
		String&& requestBody,
		StreamResultCallback&& callback,
		StreamingFinishedCallback&& finishedCallback
	)
	{
		Assert(m_signInFlags.IsSet(SignInFlags::HasSessionToken));
		m_backend.QueueStreamedRequestInternal(
			requestType,
			requestFlags,
			endpointName,
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{m_sessionTokenHeader}.GetView(),
			Forward<String>(requestBody),
			Forward<StreamResultCallback>(callback),
			Forward<StreamingFinishedCallback>(finishedCallback)
		);
	}

	void
	Game::QueueRequest(const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback)
	{
		if (Ensure(m_signInFlags.IsSet(SignInFlags::HasSessionToken)))
		{
			m_backend.QueueRequestInternal(
				requestType,
				endpointName,
				Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
					Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
					m_sessionTokenHeader
				}
					.GetView(),
				Forward<String>(requestBody),
				Forward<RequestCallback>(callback)
			);
		}
	}

}
