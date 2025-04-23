#include "Admin.h"
#include "Common.h"

#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Texture/TextureAsset.h>

#include <Backend/Plugin.h>
#include <Http/ResponseCode.h>
#include <Http/RequestData.h>

#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Version.h>
#include <Common/IO/URI.h>
#include <Common/IO/Log.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Format/Guid.h>
#include <Common/Asset/Format/Guid.h>

#include <Common/System/Query.h>

namespace ngine::Networking::Backend
{
	Admin::Admin(Plugin& plugin)
		: m_backend(plugin)
	{
	}

	void Admin::RegisterSession(const ConstStringView email, const ConstStringView password)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		writer.Serialize("email", email);
		writer.Serialize("password", password);

		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/session"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"}
			}.GetView(),
			writer.SaveToBuffer<String>(),
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				if (success)
				{
					if (const Optional<ConstStringView> multiFactorKey = responseReader.Read<ConstStringView>("mfa_key"))
					{
						OnRequestTwoFactorCode(*multiFactorKey);
					}
					else
					{
						m_sessionToken = *responseReader.Read<ConstStringView>("auth_token");
						m_sessionTokenHeader = Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"x-auth-token", m_sessionToken};
						m_hasSessionToken = true;
						OnRegisteredSessionInternal();
					}
				}
				else
				{
					String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
					LogError("Couldn't register admin session! Response code: {}, Data: {}", responseCode.m_code, responseData);
				}
			}
		);
	}

	void Admin::VerifyTwoFactorCode(const ConstStringView multiFactorKey, const uint64 secretToken)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		writer.Serialize("mfa_key", multiFactorKey);
		writer.Serialize("secret", secretToken);

		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/2fa"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"}
			}.GetView(),
			writer.SaveToBuffer<String>(),
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				if (success)
				{
					m_sessionToken = *responseReader.Read<ConstStringView>("auth_token");
					m_sessionTokenHeader = Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"x-auth-token", m_sessionToken};
					m_hasSessionToken = true;
					OnRegisteredSessionInternal();
				}
				else
				{
					String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
					LogError("Failed to verify admin session 2FA code! Response code: {}, Data: {}", responseCode.m_code, responseData);
				}
			}
		);
	}

	void Admin::OnRegisteredSessionInternal()
	{
		OnRegisteredSession.ExecuteAndClear();
	}

	void Admin::QueueRequest(
		const HTTP::RequestType requestType, const IO::URIView endpointName, HTTP::RequestData&& requestData, RequestCallback&& callback
	)
	{
		Assert(m_hasSessionToken);
		m_backend.QueueRequestInternal(
			requestType,
			endpointName,
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			Forward<HTTP::RequestData>(requestData),
			Forward<RequestCallback>(callback)
		);
	}

	void Admin::QueueStreamedRequest(
		const HTTP::RequestType requestType,
		const IO::URIView endpointName,
		String&& requestBody,
		StreamResultCallback&& callback,
		StreamingFinishedCallback&& finishedCallback
	)
	{
		Assert(m_hasSessionToken);
		m_backend.QueueStreamedRequestInternal(
			requestType,
			RequestFlags::HighPriority,
			endpointName,
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			Forward<String>(requestBody),
			Forward<StreamResultCallback>(callback),
			Forward<StreamingFinishedCallback>(finishedCallback)
		);
	}

	void Admin::CheckMaintenanceEnabled(CheckMaintenanceModeCallback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		IO::URI::StringType endPoint;
		endPoint.Format(MAKE_URI_LITERAL("admin/maintenance"));

		m_backend.QueueRequestInternal(
			HTTP::RequestType::Get,
			IO::URI(Move(endPoint)),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 1>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"}
			}.GetView(),
			{},
			[callback = Forward<CheckMaintenanceModeCallback>(callback
		   )]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader reader)
			{
				if (responseCode.IsConnectionFailure())
				{
					callback(MaintenanceModeState::NoConnection);
				}
				else if (responseCode.IsSuccessful() && reader.ReadWithDefaultValue<bool>("enabled", false))
				{
					callback(MaintenanceModeState::Enabled);
				}
				else
				{
					callback(MaintenanceModeState::Disabled);
				}
			}
		);
	}

	void Admin::UploadAssetFiles(
		const uint32 assetIdentifier, const Asset::DatabaseEntry& assetEntry, const AssetProgressCallback& progressCallback
	)
	{
		m_backend.UploadAssetFiles(
			[this, assetIdentifier](HTTP::FormDataPart&& part, AssetProgressCallback&& progressCallback)
			{
				progressCallback(true, 1);

				if (IO::Path(part.m_fileName).GetRightMostExtension() == Asset::Asset::FileExtension)
				{
					// Make sure we are publishing minified data
					HTTP::FormDataPart::StoredData& partData = part.m_data.GetExpected<HTTP::FormDataPart::StoredData>();
					Serialization::Data serializedData(ConstStringView{
						reinterpret_cast<const char*>(partData.GetData()),
						static_cast<ConstStringView::SizeType>(partData.GetDataSize())
					});
					Assert(serializedData.IsValid());
					if (LIKELY(serializedData.IsValid()))
					{
						String minifiedData = serializedData.SaveToBuffer<String>(Serialization::SavingFlags{});
						partData.GetView().CopyFrom(
							ArrayView<const ByteType, size>{reinterpret_cast<const ByteType*>(minifiedData.GetData()), minifiedData.GetDataSize()}
						);
						partData.Resize(minifiedData.GetDataSize());
					}
					else
					{
						LogError("Failed to minify asset data!");
						progressCallback(false, -1);
						return;
					}
				}

				LogMessage("Uploading asset");
				UploadAssetFile(
					assetIdentifier,
					Forward<HTTP::FormDataPart>(part),
					[progressCallback = Forward<AssetProgressCallback>(progressCallback
			     )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
					{
						Assert(success);
						if (LIKELY(success))
						{
							LogMessage("Successfully uploaded asset file");
						}
						else
						{
							String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
							LogError("Failed to upload asset! Response code {} data {}", responseCode.m_code, responseData);
						}
						progressCallback(success, -1);
					}
				);
			},
			assetEntry,
			progressCallback
		);
	}

	void Admin::CreateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, AssetProgressCallback&& progressCallback)
	{
		m_backend.CreateAssets(
			IO::URI(MAKE_URI("admin/v1/assets")),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				m_sessionTokenHeader
			}
				.GetView(),
			Forward<IO::Path>(assetDatabasePath),
			Forward<Asset::Database>(assetDatabase),
			Plugin::UploadAssetFilesCallback(*this, &Admin::UploadAssetFiles),
			Forward<AssetProgressCallback>(progressCallback)
		);
	}

	void Admin::ActivateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback)
	{
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

		QueueRequest(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/assets/activate"),
			writer.SaveToBuffer<String>(),
			[callback = Forward<ActivationCallback>(callback)](const bool success, const HTTP::ResponseCode, const Serialization::Reader) mutable
			{
				callback(success);
			}
		);
	}

	void Admin::DeactivateAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback)
	{
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

		QueueRequest(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/assets/deactivate"),
			writer.SaveToBuffer<String>(),
			[callback = Forward<ActivationCallback>(callback)](const bool success, const HTTP::ResponseCode, const Serialization::Reader) mutable
			{
				callback(success);
			}
		);
	}

	void Admin::DeleteAssets(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback)
	{
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

		QueueRequest(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/assets/delete"),
			writer.SaveToBuffer<String>(),
			[callback = Forward<ActivationCallback>(callback)](const bool success, const HTTP::ResponseCode, const Serialization::Reader) mutable
			{
				callback(success);
			}
		);
	}

	void Admin::DeleteAssetFiles(IO::Path&& assetDatabasePath, Asset::Database&& assetDatabase, ActivationCallback&& callback)
	{
		// Make sure the database contains itself
		assetDatabase.RegisterAsset(
			assetDatabase.GetGuid(),
			Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, {}, Move(assetDatabasePath)},
			assetDatabasePath.GetParentPath()
		);

		struct Data
		{
			Threading::Atomic<uint32> remainingDeletions{1};
			ActivationCallback callback;
			Threading::Atomic<bool> success{true};
		};
		SharedPtr<Data> pData = SharedPtr<Data>::Make(Data{0, Forward<ActivationCallback>(callback)});

		assetDatabase.IterateAssets(
			[this, pData](const Asset::Guid assetGuid, const Asset::DatabaseEntry&) mutable
			{
				IO::URI::StringType endPoint;
				endPoint.Format(MAKE_URI_LITERAL("admin/v1/assets/{}/files"), assetGuid);

				pData->remainingDeletions++;
				QueueRequest(
					HTTP::RequestType::Delete,
					IO::URI(Move(endPoint)),
					{},
					[pData](const bool success, const HTTP::ResponseCode, const Serialization::Reader) mutable
					{
						Assert(success);
						if (!success)
						{
							pData->success = false;
						}
						if (pData->remainingDeletions.FetchSubtract(1) == 1)
						{
							pData->callback(pData->success);
						}
					}
				);
				return Memory::CallbackResult::Continue;
			}
		);

		if (pData->remainingDeletions.FetchSubtract(1) == 1)
		{
			pData->callback(pData->success);
		}
	}

	void Admin::UploadAssetFile(const uint32 assetId, HTTP::FormDataPart&& fileData, RequestCallback&& finishedCallback)
	{
		HTTP::FormData formData;
		HTTP::FormDataPart& filePart = formData.m_parts.EmplaceBack(Forward<HTTP::FormDataPart>(fileData));
		filePart.m_name = "file";

		String assetIdString;
		assetIdString.Format("{}", assetId);

		formData.EmplacePart("asset_id", "text/plain", Move(assetIdString));
		formData.EmplacePart("purpose", "text/plain", ConstStringView("FILE"));

		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("admin/v1/upload"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 2>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "multipart/form-data"},
				m_sessionTokenHeader
			}
				.GetView(),
			Move(formData),
			[finishedCallback = Forward<RequestCallback>(finishedCallback
		   )](const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				finishedCallback(success, responseCode, responseReader);
			}
		);
	}
}
