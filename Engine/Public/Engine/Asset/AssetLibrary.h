#pragma once

#include <Engine/Asset/AssetDatabase.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Asset
{
	struct Manager;

	//! Represents te library of assets that are available for usage, but not necessarily imported into the current context
	struct Library final : public EngineAssetDatabase
	{
		inline static constexpr Guid DataSourceGuid = "f085a0ca-d068-497c-b6d6-721ef72b5ab3"_guid;
		//! Assets that came from the local asset database
		inline static constexpr Guid LocalAssetDatabaseTagGuid = "4c869146-520f-4330-8059-9ed05a66a3be"_guid;
		//! Assets that come from a plug-in
		inline static constexpr Guid PluginTagGuid = "fd89c526-f129-4248-bf00-67b7bb25632c"_guid;
		//! Assets that were imported from the asset library
		inline static constexpr Guid ImportedTagGuid = "417FBCBE-0367-4D61-B608-8C2F303E9C0D"_guid;
		inline static constexpr Guid CloudAssetTagGuid = "36aa4f13-bbc6-413d-a0d7-e51798a4a24b"_guid;
		inline static constexpr Guid OwnedAssetTagGuid = "d3d60603-e9d1-40f0-9db9-8f90eb2389bd"_guid;
		inline static constexpr Guid IsInCartTagGuid = "c3512352-8b8c-43d3-a9ed-6036cb57bd3e"_guid;
		inline static constexpr Guid FeaturedAssetTagGuid = "81c920f5-06dd-4006-8d31-4c616398ed54"_guid;
		inline static constexpr Guid PurchasableAssetTagGuid = "ebb05eaf-3951-4cbd-926f-d6f9188f9a1e"_guid;
		inline static constexpr Guid AvatarAssetTagGuid = "8d00f0ca-170e-47b9-83ae-274492f7be7e"_guid;
		inline static constexpr Guid ThumbnailAssetTagGuid = "ba6059e7-3017-4b62-909f-804fcbe56c31"_guid;

		Library();
		virtual ~Library();

		// Delete the save functionality as we don't want the library saved back to disk due to how it contains backend assets
		bool Save(const IO::ConstZeroTerminatedPathView databaseFile) = delete;
		bool SaveMinimal(const IO::ConstZeroTerminatedPathView databaseFile) = delete;

		using EngineAssetDatabase::RegisterAsset;
		Identifier ImportAsset(const Guid assetGuid, IO::Path&& assetPath);

		[[nodiscard]] Optional<Threading::Job*> LoadPluginDatabase();
		[[nodiscard]] Optional<Threading::Job*> LoadLocalPluginDatabase();
	protected:
		friend Manager;
	};
}
