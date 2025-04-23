#pragma once

#include <Http/RequestType.h>

#include <Backend/AssetDatabase.h>
#include <Backend/PlayersDataSource.h>
#include <Backend/Inventory.h>
#include <Backend/PersistentUserStorage.h>
#include <Backend/RequestCallback.h>
#include <Backend/RequestFlags.h>
#include <Backend/SignInFlags.h>

#include <Engine/DataSource/SortingOrder.h>

#include <Common/Asset/AssetDatabase.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Function/CopyableFunction.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/IO/ForwardDeclarations/PathView.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Time/Timestamp.h>

namespace ngine
{
	struct Engine;

	namespace IO
	{
		struct Path;
	}
} // namespace ngine

namespace ngine::Asset
{
	struct Guid;
	struct Database;
	struct DatabaseEntry;
} // namespace ngine::Asset

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Networking::HTTP
{
	struct Plugin;
	struct ResponseCode;
	struct FormDataPart;
} // namespace ngine::Networking::HTTP

namespace ngine::Networking::Backend
{
	struct Plugin;

	struct Game
	{
		enum class StreamedRequestType : uint8
		{
			First,
			AllAssets = First,
			AllProjects,
			AllMeshScenes,
			AllMeshParts,
			AllTextures,
			AllMaterialInstances,
			AllScenes,
			AllAudio,
			AllAnimations,
			AllSkeletons,
			AllMeshSkins,
			AllPlugins,
			Count,
			AvatarAssets
		};

		enum class RequestOrderBy : uint8
		{
			Id,
			Date,
			Likes,
			Plays
		};

		struct RequestPaginatedParameters
		{
			uint32 count = 50;
			bool featuredOnly = false;
			RequestOrderBy orderBy = RequestOrderBy::Id;
			Vector<Guid> tags;

			PersistentUserIdentifier playerId;
			DataSource::SortingOrder sortingOrder{DataSource::SortingOrder::Descending};
			UnicodeString search;

			uint32 afterAssetId{0};
			Time::Timestamp afterDate;
			uint32 afterLikes{0};
			uint32 afterPlays{0};
		};

		Game(Plugin& plugin);
		~Game();

		[[nodiscard]] PersistentUserIdentifier GetLocalPlayerInternalIdentifier() const
		{
			return m_cachedData.GetLocalPlayerInternalIdentifier();
		}
		[[nodiscard]] PerSessionUserIdentifier GetLocalPlayerIdentifier() const
		{
			return m_localPlayerIdentifier;
		}
		[[nodiscard]] ConstStringView GetPublicUserIdentifier() const
		{
			return m_publicUserId;
		}
		[[nodiscard]] bool HasSessionToken() const
		{
			return m_signInFlags.IsSet(SignInFlags::HasSessionToken);
		}
		[[nodiscard]] bool IsFirstSession() const
		{
			return m_signInFlags.IsSet(SignInFlags::IsFirstSession);
		}
		[[nodiscard]] bool IsOffline() const
		{
			return m_signInFlags.IsSet(SignInFlags::Offline);
		}
		[[nodiscard]] bool HasFinishedSessionRegistration() const
		{
			return m_signInFlags.IsSet(SignInFlags::FinishedSessionRegistration);
		}
		[[nodiscard]] bool HasFileToken() const
		{
			return m_signInFlags.IsSet(SignInFlags::HasFileToken);
		}

		[[nodiscard]] AssetDatabase& GetAssetDatabase()
		{
			return m_assetDatabase;
		}
		[[nodiscard]] const AssetDatabase& GetAssetDatabase() const
		{
			return m_assetDatabase;
		}
		[[nodiscard]] const Inventory& GetInventory() const
		{
			return m_inventory;
		}
		[[nodiscard]] PersistentUserStorage& GetPersistentUserStorage()
		{
			return m_persistentUserStorage;
		}
		[[nodiscard]] const PersistentUserStorage& GetPersistentUserStorage() const
		{
			return m_persistentUserStorage;
		}

		using SignInCallback = CopyableFunction<void(const EnumFlags<SignInFlags> signInFlags), 24>;
		//! Checks whether the local user is allowed to attempt to sign in with the specified providers and flags
		//! The callback will be invoked once per provider
		void CheckCanSignInAsync(const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, Rendering::Window& window);
		void SignIn(const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, Rendering::Window& window);
		void SignOut(Rendering::Window& window);
		ThreadSafe::Event<void(void*), 24> OnSignedOut;
		[[nodiscard]] EnumFlags<SignInFlags> GetSupportedSignInFlags() const
		{
			return m_supportedSignInFlags;
		}
		[[nodiscard]] EnumFlags<SignInFlags> GetCachedSignInProviders() const
		{
			return m_cachedData.GetSignInProviders();
		}

		void SetCachedLoginInfo(const SignInFlags signInProvider, UnicodeString&& userIdentifier, UnicodeString&& authenticationToken);
		void ClearCachedLoginInfo(const SignInFlags signInProvider, const Optional<Rendering::Window*> pWindow);

		void SignInWithEmail(
			const ConstUnicodeStringView email,
			const ConstUnicodeStringView password,
			SignInCallback&& callback,
			const Optional<Rendering::Window*> pWindow = Invalid
		);

		using BinaryCallback = CopyableFunction<void(const bool success), 24>;
		void SignUpWithEmail(const ConstUnicodeStringView email, const ConstUnicodeStringView password, RequestCallback&& callback);
		void ResetPassword(UnicodeString&& token, UnicodeString&& userId, UnicodeString&& password, BinaryCallback&& callback);
		void RequestResetPassword(const ConstUnicodeStringView email, BinaryCallback&& callback);
		void VerifyEmail(UnicodeString&& token, UnicodeString&& userId, BinaryCallback&& callback);

		void DeleteAccount(Rendering::Window& window);

		using QueuedFunction = Function<void(const EnumFlags<SignInFlags> signInFlags), 24>;
		void QueuePostSessionRegistrationCallback(QueuedFunction&& function);
		void QueueFileTokenCallback(QueuedFunction&& function);
		void QueueStartupRequestsFinishedCallback(QueuedFunction&& function);

		void RequestAllAssets(StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback);
		void RequestAllAssetsOfType(
			const StreamedRequestType request,
			const EnumFlags<RequestFlags> requestFlags,
			const Guid assetTypeGuid,
			StreamResultCallback&& callback,
			StreamingFinishedCallback&& finishedCallback
		);

		void RequestPaginatedAssetsOfType(const Guid assetTypeGuid, RequestPaginatedParameters paginatedParameters, RequestCallback&& callback);
		void HandlePaginatedAssetResponse(const Serialization::Reader assetReader, Asset::Manager& assetManager);

		void RequestAllAssetLikesCountOfType(const Guid assetTypeGuid, RequestCallback&& callback);
		void RequestAllAssetPlaysCountOfType(const Guid assetTypeGuid, RequestCallback&& callback);
		void RequestAssets(Vector<Asset::Guid>&& assets, AssetStreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback);
		void RequestAssetContents(const Asset::Guid assetGuid, StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback);
		void
		HandleStreamedAssetResponse(const StreamedRequestType request, const Serialization::Reader assetReader, Asset::Manager& assetManager);
		void HandleAssetResponse(const Serialization::Reader assetReader, Asset::Manager& assetManager);
		void HandleAssetResponseWithTag(const Serialization::Reader assetReader, Asset::Manager& assetManager, Guid tag);

		void HandlePlayerLikedAssetResponse(const Serialization::Reader assetReader);
		void HandleAssetLikeCountsResponse(const Serialization::Reader assetReader);
		void HandleAssetPlayCountsResponse(const Serialization::Reader assetReader);
		void HandlePlayerCurrencyBalance(const Serialization::Reader assetReader);

		void HandleBuyableAssetsList(const Serialization::Reader assetReader);
		void HandleBuyableAssetResponse(
			const Serialization::Reader assetReader, Asset::Manager& assetManager, uint64_t price, ConstStringView catalogId
		);

		void HandlePlayerGetResponse(const Serialization::Reader assetReader);
		void HandleSinglePlayerGetResponse(const Serialization::Reader assetReader);
		void HandlePlayerFollowingsGetResponse(const Serialization::Reader playerResponseReader);

		void RequestInventory(StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback);
		void PurchaseAssets(const ArrayView<const AssetCatalogItem> assets, RequestCallback&& callback);

		void PurchaseApplePay(Rendering::Window& window, uint64 amount);

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_MACOS
		bool ValidateApplePurchases();
#endif

		//! Called when there is a chance in progress during asset creation
		//! Positive number means new assets are being processed, negative means completion finished for X assets
		using AssetProgressCallback = CopyableFunction<void(const bool success, int32 assetCount), 24>;

		void CreatePlayerAsset(
			const Asset::Guid assetGuid,
			const Asset::DatabaseEntry& assetEntry,
			const IO::PathView assetsFolderPath,
			AssetProgressCallback&& progressCallback
		);

		void UpdatePlayerAsset(
			const uint32 assetId,
			const ConstStringView name,
			const Guid contextGuid,
			const Asset::Guid guid,
			const bool isComplete,
			RequestCallback&& callback
		);
		void UploadPlayerAssetFile(const uint32 assetId, HTTP::FormDataPart&& fileData, RequestCallback&& callback);
		void UploadPlayerAssetFiles(
			const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback
		);

		void CreatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback);

		void ActivatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback);
		void DeactivatePlayerAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback);

		void ActivatePlayerAsset(const Asset::Guid guid);

		void LikeAsset(const uint32 assetId, RequestCallback&& callback);
		void PlayAsset(const uint32 assetId, RequestCallback&& callback);

		void UnlikeAsset(const uint32 assetId, RequestCallback&& callback);

		void GetPlayerLikedAssets(RequestCallback&& callback);

		void GetPlayerFollowings(RequestCallback&& callback);

		void GetPlayerCurrencyBalance(RequestCallback&& callback);

		void GetBuyableGamesList(RequestCallback&& callback);
		void GetBuyableCharactersList(RequestCallback&& callback);

		void FollowPlayer(const PersistentUserIdentifier playerId, RequestCallback&& callback);
		void UnfollowPlayer(const PersistentUserIdentifier playerId, RequestCallback&& callback);

		void GetPlayerProfiles(RequestCallback&& callback);

		void CreatePlayerProfile(
			const ConstUnicodeStringView username,
			ConstUnicodeStringView biography,
			const Asset::Guid profilePictureGuid,
			RequestCallback&& callback
		);

		void UpdatePlayerProfile(
			const ConstUnicodeStringView username,
			ConstUnicodeStringView biography,
			const Asset::Guid profilePictureGuid,
			RequestCallback&& callback
		);

		void GetPlayerProfileByUsername(const ConstUnicodeStringView username, RequestCallback&& callback);

		void RequestPlayerProfileById(const PersistentUserIdentifier playerId, Function<void(const bool success), 24>&& callback);

		[[nodiscard]] ConstStringView GetFileURIToken() const;

		void
		QueueRequest(const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback);

		void QueueStreamedRequest(
			const HTTP::RequestType requestType,
			const EnumFlags<RequestFlags> requestFlags,
			const IO::URIView endpointName,
			String&& requestBody,
			StreamResultCallback&& callback,
			StreamingFinishedCallback&& finishedCallback
		);

		struct CachedData
		{
			inline static constexpr uint16 Version = 2;

			CachedData(const IO::PathView cacheName);

			void Reserve(const uint32 count);
			void AddAsset(const Guid assetGuid, const Serialization::Reader assetReader);
			void RemoveAsset(const Guid assetGuid);
			void Parse(
				Game& gameAPI, AssetDatabase& assetDatabase, Asset::Manager& assetManager, const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags
			);

			void TryUpdateTimestamp(const StreamedRequestType request, const Time::Timestamp timestamp);
			[[nodiscard]] Time::Timestamp GetTimestamp(const StreamedRequestType request) const
			{
				return m_streamedRequestTimestamps[request];
			}

			[[nodiscard]] bool IsValid() const
			{
				return m_assetsData.IsValid();
			}

			void OnChanged();
			void SaveIfNecessary();

			[[nodiscard]] PersistentUserIdentifier GetLocalPlayerInternalIdentifier() const
			{
				return m_localPlayerInternalIdentifier;
			}

			void SetLocalPlayerInternalIdentifier(PersistentUserIdentifier playerIdentifier)
			{
				m_localPlayerInternalIdentifier = playerIdentifier;
				OnChanged();
			}

			struct LoginInfo
			{
				UnicodeString userIdentifier;
				UnicodeString authorizationToken;
			};

			[[nodiscard]] const LoginInfo& GetLoginInfo(const SignInFlags signInProvider) const LIFETIME_BOUND
			{
				return m_savedLoginInfo[Math::Log2((uint8)signInProvider)];
			}
			void SetLoginInfo(const SignInFlags signInProvider, LoginInfo&& loginInfo)
			{
				m_savedLoginInfo[Math::Log2((uint8)signInProvider)] = Forward<LoginInfo>(loginInfo);
				m_savedSignInProviders |= signInProvider;
				OnChanged();
				SaveIfNecessary();
			}
			void ClearLoginInfo(const SignInFlags signInProvider)
			{
				m_savedLoginInfo[Math::Log2((uint8)signInProvider)] = {};
				m_savedSignInProviders.Clear(signInProvider);
				OnChanged();
				SaveIfNecessary();
			}
			[[nodiscard]] EnumFlags<SignInFlags> GetSignInProviders() const
			{
				return m_savedSignInProviders;
			}
		protected:
			[[nodiscard]] bool Save(const IO::PathView cacheName);
			[[nodiscard]] bool ParseInternal(
				Game& gameAPI, AssetDatabase& assetDatabase, Asset::Manager& assetManager, const EnumFlags<AssetDatabase::ParsingFlags> parsingFlags
			);
		protected:
			Threading::Atomic<bool> m_isDirty{false};
			bool m_registeredSaveCallback{false};
			Threading::SharedMutex m_assetsMutex;
			Serialization::Data m_assetsData;
			UnorderedMap<Guid, Serialization::Value, Guid::Hash> m_assetsMap;
			uint32 m_assetCapacity{0};
			Array<Time::Timestamp, (uint8)StreamedRequestType::Count, StreamedRequestType> m_streamedRequestTimestamps;
			PersistentUserIdentifier m_localPlayerInternalIdentifier;

			EnumFlags<SignInFlags> m_savedSignInProviders;
			Array<LoginInfo, (uint8)SignInFlags::ProviderCount> m_savedLoginInfo;
		};

		[[nodiscard]] CachedData& GetCachedData()
		{
			return m_cachedData;
		}

		[[nodiscard]] DataSource::Players& GetPlayersDataSource()
		{
			return m_playersDataSource;
		}

		struct Currency
		{
			friend struct Backend::Game;
			Currency(){

			};

			uint64 GetBalance() const
			{
				return m_balance;
			}

			uint64 GetNumberAssetsInCart() const
			{
				return m_assetsInCart.GetSize();
			}

			ArrayView<Guid> GetCart()
			{
				return m_assetsInCart.GetView();
			}

			void AddAmount(uint64 amount);
			void RemoveAmount(uint64 amount);
			void AddAssetToCart(Guid assetGuid);
			void RemoveAssetFromCart(Guid assetGuid);
			uint64 GetCartTotalPrice();
		protected:
			Vector<Guid> m_assetsInCart;
			uint64 m_balance;
			String m_userWalletId;
		};

		[[nodiscard]] Currency& GetCurrency()
		{
			return m_currency;
		}

		void RequestExplorePagePaginatedProjectAssets();
		void RequestMarketplaceAssets();

		void RequestAvatarsPackage();
		void RequestPlayerProfileInfo();
		void RequestPlayerInventory();
	protected:
		void CheckCanCachedSignInAsyncInternal(
			const SignInFlags signInProvider, const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, Rendering::Window& window
		);
		void PopulateCommonSignInMetadata(Serialization::Writer writer);
		void CompleteCachedSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void CompleteOfflineSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void CompleteSteamSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void CompleteAppleSignInInternal(
			const EnumFlags<SignInFlags> signInFlags,
			SignInCallback&& callback,
			const ConstStringView userIdentifier,
			const ConstStringView authorizationCode,
			const Optional<Rendering::Window*> pWindow
		);
		void CompleteCachedAppleSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void CompleteGoogleSignInInternal(
			const EnumFlags<SignInFlags> signInFlags,
			SignInCallback&& callback,
			const ConstStringView userIdentifier,
			const ConstStringView idToken,
			const Optional<Rendering::Window*> pWindow
		);
		void CompleteCachedGoogleSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void CompleteEmailSignInInternal(
			const EnumFlags<SignInFlags> signInFlags,
			SignInCallback&& callback,
			const ConstUnicodeStringView email,
			const ConstUnicodeStringView token,
			const Optional<Rendering::Window*> pWindow
		);
		void CompleteGuestSignInInternal(
			const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow
		);
		void SignInInternal(const EnumFlags<SignInFlags> signInFlags, SignInCallback&& callback, const Optional<Rendering::Window*> pWindow);

		void OnSignedOutInternal(const SignInFlags signInProvider, Rendering::Window& window);

		void OnSignInCompletedInternal(
			const EnumFlags<SignInFlags> signInFlags,
			Optional<CachedData::LoginInfo>&& cachedLogInInfo,
			SignInCallback&& completeSignInCallback,
			const Networking::HTTP::ResponseCode responseCode,
			const Optional<Serialization::Reader> reader,
			const Optional<Rendering::Window*> pWindow
		);
		void OnStartupRequestsFinishedInternal(const EnumFlags<SignInFlags> signInFlags);

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_MACOS
		void OnReceivedAppleProductInfo(const ngine::ArrayView<void*> skProducts);
		void OnApplePurchaseEvent(void* skPaymentTransactions);
#endif

		void RequestFileURIToken();
		void ParseCachedData();
		void RequestAssetsInternal(Vector<Asset::Guid>&& assets, StreamResultCallback&& callback, StreamingFinishedCallback&& finishedCallback);
		void RequestPlayerAssets(const Guid assetTypeGuid, const uint32 afterAssetId = 0);
	protected:
		friend struct Backend::Plugin;
		EnumFlags<SignInFlags> m_supportedSignInFlags;
		AtomicEnumFlags<SignInFlags> m_signInFlags;
		CachedData m_cachedData;

		String m_sessionToken;
		Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> m_sessionTokenHeader;

		PerSessionUserIdentifier m_localPlayerIdentifier;

		String m_publicUserId;
		FileURIToken m_fileURIToken;

		Plugin& m_backend;

		AssetDatabase m_assetDatabase;
		Inventory m_inventory;
		PersistentUserStorage m_persistentUserStorage;

		Currency m_currency;

		DataSource::Players m_playersDataSource;

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
		Vector<ReferenceWrapper<AssetEntry>> m_awaitingAppStoreProducts;
		Vector<void*> m_appleSKProducts;
#endif

		Threading::Mutex m_queuedRequestsMutex;
		Vector<QueuedFunction> m_sessionRegistrationCallbacks;
		Vector<QueuedFunction> m_fileTokenCallbacks;
		Vector<QueuedFunction> m_startupRequestsCallbacks;
	};

	ENUM_FLAG_OPERATORS(Game::StreamedRequestType);
} // namespace ngine::Networking::Backend
