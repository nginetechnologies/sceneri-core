#pragma once

#include <Common/Threading/AtomicBool.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/Function/Event.h>
#include <Common/Function/CopyableFunction.h>
#include <Common/Asset/Guid.h>
#include <Http/RequestType.h>

#include <Backend/RequestCallback.h>

namespace ngine
{
	struct PluginInfo;

	namespace IO
	{
		struct Path;
	}
}

namespace ngine::Networking::HTTP
{
	struct Plugin;
	struct FormDataPart;

	using RequestBody = String;
	struct FormData;
	using RequestData = Variant<RequestBody, FormData>;
}

namespace ngine::Asset
{
	struct Guid;
	struct Database;
	struct DatabaseEntry;
}

namespace ngine::Networking::Backend
{
	struct Plugin;

	struct Admin
	{
		Admin(Plugin& plugin);

		void RegisterSession(const ConstStringView email, const ConstStringView password);
		void VerifyTwoFactorCode(const ConstStringView multiFactorKey, const uint64 secretToken);
		void QueueRequest(
			const HTTP::RequestType requestType, const IO::URIView endpointName, HTTP::RequestData&& requestData, RequestCallback&& callback
		);
		void QueueStreamedRequest(
			const HTTP::RequestType requestType,
			const IO::URIView endpointName,
			String&& requestBody,
			StreamResultCallback&& callback,
			StreamingFinishedCallback&& finishedCallback
		);

		Event<void(void*, const ConstStringView multiFactorKey), 24> OnRequestTwoFactorCode;
		Event<void(void*), 24> OnRegisteredSession;

		//! Called when there is a chance in progress during asset creation
		//! Positive number means new assets are being processed, negative means completion finished for X assets
		using AssetProgressCallback = CopyableFunction<void(const bool success, int32 assetCount), 24>;
		//! Create assets and upload files for all assets into a plug-in
		void CreateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback);

		using ActivationCallback = Function<void(const bool success), 24>;
		//! Activates all the assets
		void ActivateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback);
		//! Deactivates all the assets
		void DeactivateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback);
		//! Deletes all the assets
		void DeleteAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback);
		//! Deletes all the assets stored files
		void DeleteAssetFiles(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback);

		void UploadAssetFile(const uint32 assetId, HTTP::FormDataPart&& fileData, RequestCallback&& finishedCallback);

		enum class MaintenanceModeState : uint8
		{
			Disabled,
			Enabled,
			NoConnection
		};
		using CheckMaintenanceModeCallback = Function<void(const MaintenanceModeState state), 24>;
		void CheckMaintenanceEnabled(CheckMaintenanceModeCallback&& callback);
	protected:
		void OnRegisteredSessionInternal();
		void
		UploadAssetFiles(const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback);
	protected:
		friend struct Backend::Plugin;
		Threading::Atomic<bool> m_hasSessionToken = false;
		String m_sessionToken;
		Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> m_sessionTokenHeader;

		Plugin& m_backend;
	};
}
