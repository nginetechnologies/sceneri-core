#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Function/Function.h>
#include <Common/Function/CopyableFunction.h>
#include <Common/Function/Event.h>
#include <Common/Memory/Tuple.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Http/RequestType.h>
#include <Http/RequestData.h>

#include <Backend/RequestCallback.h>
#include <Backend/RequestFlags.h>
#include <Backend/Leaderboard.h>
#include <Backend/Server.h>
#include <Backend/Game.h>
#include <Backend/Admin.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Networking::HTTP
{
	struct Plugin;
	struct ResponseCode;
}

namespace ngine::Asset
{
	struct Database;
}

namespace ngine::Networking::Backend
{
	struct AssetCreationInfo;

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "087DEE23-94E5-44D9-88E9-AF82B784F025"_guid;

		Plugin(Application&)
			: m_game(*this)
			, m_server(*this)
			, m_admin(*this)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		virtual void OnUnloaded(Application& application) override;
		// ~IPlugin

		[[nodiscard]] Game& GetGame()
		{
			return m_game;
		}
		[[nodiscard]] const Game& GetGame() const
		{
			return m_game;
		}
		[[nodiscard]] Server& GetServer()
		{
			return m_server;
		}
		[[nodiscard]] const Server& GetServer() const
		{
			return m_server;
		}
		[[nodiscard]] Admin& GetAdmin()
		{
			return m_admin;
		}
		[[nodiscard]] const Admin& GetAdmin() const
		{
			return m_admin;
		}

		void PopulateAsset(
			const Asset::Guid assetGuid,
			const Asset::DatabaseEntry& assetEntry,
			const Asset::Database& assetDatabase,
			const IO::PathView assetsFolderPath,
			Serialization::Writer writer
		);
		[[nodiscard]] FixedCapacityVector<AssetCreationInfo> PopulateAssetBatch(Asset::Database& assetDatabase);
		[[nodiscard]] Serialization::Data PopulateAssetBatchData(
			const ArrayView<const AssetCreationInfo> assets, Asset::Database& assetDatabase, const IO::PathView assetsFolderPath
		);

		//! Called when there is a chance in progress during asset creation
		//! Positive number means new assets are being processed, negative means completion finished for X assets
		using AssetProgressCallback = CopyableFunction<void(const bool success, int32 assetCount), 24>;

		struct AssetInfo
		{
			ConstStringView assetName;
			ConstStringView assetDescription;
			ngine::Guid contextGuid;
			Asset::Guid guid;
			UnorderedMap<String, String, String::Hash> keyValueStorage;
			String dependencies;
		};
		using CreateAssetCallback = Function<void(const Optional<uint32> assetIdentifier, const ngine::Guid assetGuid), 24>;
		void CreateAsset(
			IO::URI&& endpointURI,
			const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
			AssetInfo&& assetInfo,
			CreateAssetCallback&& callback
		);
		using UploadAssetFilesCallback = Function<
			void(const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback),
			24>;
		void CreateAsset(
			IO::URI&& endpointURI,
			const ArrayView<Pair<ZeroTerminatedStringView, ZeroTerminatedStringView>> headers,
			const Asset::Guid assetGuid,
			const Asset::DatabaseEntry& assetEntry,
			const IO::PathView assetsFolderPath,
			UploadAssetFilesCallback&& uploadCallback,
			AssetProgressCallback&& progressCallback
		);

		void CreateAssets(
			IO::URI&& endpointURI,
			const ArrayView<Pair<ZeroTerminatedStringView, ZeroTerminatedStringView>> headers,
			IO::Path&& assetDatabasePath,
			Asset::Database&& assetDatabase,
			UploadAssetFilesCallback&& uploadCallback,
			AssetProgressCallback&& progressCallback
		);

		using UploadAssetFileCallback = Function<void(HTTP::FormDataPart&& part, AssetProgressCallback&& progressCallback), 24>;
		void UploadAssetFiles(
			UploadAssetFileCallback&& callback, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback
		);
	protected:
		void QueueRequestInternal(
			const HTTP::RequestType requestType,
			const IO::URIView endpointName,
			const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
			HTTP::RequestData&& requestData,
			RequestCallback&& callback
		);
		void QueueStreamedRequestInternal(
			const HTTP::RequestType requestType,
			const EnumFlags<RequestFlags> requestFlags,
			const IO::URIView endpointName,
			const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
			HTTP::RequestData&& requestData,
			StreamResultCallback&& callback,
			StreamingFinishedCallback&& finishedCallback
		);
		void
		QueueGameRequest(const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback);
		void QueueServerRequest(
			const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback
		);

		void CreateAssetsInternal(
			IO::URI&& endpointURI,
			const ArrayView<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>> headers,
			Asset::Database&& assetDatabase,
			IO::Path&& assetsFolderPath,
			UploadAssetFilesCallback&& uploadCallback,
			AssetProgressCallback&& progressCallback,
			FixedCapacityVector<AssetCreationInfo>&& assets
		);
	protected:
		friend struct Server;
		friend struct Game;
		friend struct Admin;
		friend struct Leaderboard;

		HTTP::Plugin* m_pHttpPlugin = nullptr;

		Game m_game;
		Server m_server;
		Admin m_admin;
	};
}
