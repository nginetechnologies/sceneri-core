#include "Asset/AssetLibrary.h"
#include "Asset/AssetManager.h"

#include <Engine/Engine.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/IO/Filesystem.h>

#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/Asset/FolderAssetType.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Project System/FindEngine.h>
#include <Common/System/Query.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/IO/AsyncLoadFromDiskJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

namespace ngine::Asset
{
	Library::Library()
		: EngineAssetDatabase(DataSourceGuid)
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
		{
			const Tag::Identifier localAssetDatabaseAssetTagIdentifier = tagRegistry.FindOrRegister(LocalAssetDatabaseTagGuid);

			const IO::Path localDatabaseConfigFilePath = LocalDatabase::GetConfigFilePath();
			const Identifier localAssetsRootFolderAssetIdentifier = RegisterAsset(
				Guid::Generate(),
				DatabaseEntry{
					FolderAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path(LocalDatabase::GetDirectoryPath()),
					UnicodeString{MAKE_UNICODE_LITERAL("My Assets")},
					UnicodeString{},
					"a132ceb1-7e68-2c14-c3de-9a5cc001201f"_asset
				},
				Identifier{}
			);
			SetTagAsset(localAssetDatabaseAssetTagIdentifier, localAssetsRootFolderAssetIdentifier);

			if (System::Find<Engine>())
			{
				Threading::Job* pLoadFromDiskJob = IO::CreateAsyncLoadFromDiskJob(
					localDatabaseConfigFilePath,
					Threading::JobPriority::LoadProject,
					[this, localDatabaseConfigFilePath, localAssetsRootFolderAssetIdentifier, localAssetDatabaseAssetTagIdentifier](
						const ConstByteView data
					)
					{
						Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
							ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
						);
						if (UNLIKELY(!rootReader.GetData().IsValid()))
						{
							return;
						}

						const Serialization::Reader reader(rootReader);
						[[maybe_unused]] const bool wasLoaded = Load(
							reader,
							localDatabaseConfigFilePath.GetParentPath(),
							localAssetsRootFolderAssetIdentifier,
							Array{localAssetDatabaseAssetTagIdentifier}
						);
					},
					ByteView{},
					Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
				);
				if (pLoadFromDiskJob != nullptr)
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(*pLoadFromDiskJob);
				}
			}
		}
	}

	Library::~Library()
	{
	}

	Optional<Threading::Job*> Library::LoadPluginDatabase()
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		IO::Path enginePath;
		if (const Optional<IO::Filesystem*> pFilesystem = System::Find<IO::Filesystem>())
		{
			enginePath = pFilesystem->GetEnginePath();
		}
		else
		{
			enginePath = IO::Path(ProjectSystem::FindEngineDirectoryFromExecutableFolder(IO::Path::GetExecutableDirectory()));
		}
		const Identifier engineRootFolderAssetIdentifier = FindOrRegisterFolder(enginePath, Identifier{});

		const Tag::Identifier pluginAssetTagIdentifier = tagRegistry.FindOrRegister(PluginTagGuid);
		const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(EngineManager::EngineAssetsTagGuid);

		const IO::Path enginePluginsAssetDatabasePath = IO::Path::Combine(enginePath, EnginePluginDatabase::FileName);
		return IO::CreateAsyncLoadFromDiskJob(
			enginePluginsAssetDatabasePath,
			Threading::JobPriority::LoadPlugin,
			[this, pluginAssetTagIdentifier, engineAssetTagIdentifier, enginePath, engineRootFolderAssetIdentifier](const ConstByteView data)
			{
				Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
				);
				if (UNLIKELY(!rootReader.GetData().IsValid()))
				{
					return;
				}

				const Serialization::Reader reader(rootReader);
				FlatVector<Tag::Identifier, 2> tags{pluginAssetTagIdentifier, engineAssetTagIdentifier};

				[[maybe_unused]] const bool wasLoaded = Load(reader, enginePath, engineRootFolderAssetIdentifier, tags.GetView());
				Assert(wasLoaded);
			},
			ByteView{},
			Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		);
	}

	Optional<Threading::Job*> Library::LoadLocalPluginDatabase()
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		IO::Path enginePath;
		if (const Optional<IO::Filesystem*> pFilesystem = System::Find<IO::Filesystem>())
		{
			enginePath = pFilesystem->GetEnginePath();
		}
		else
		{
			enginePath = IO::Path(ProjectSystem::FindEngineDirectoryFromExecutableFolder(IO::Path::GetExecutableDirectory()));
		}
		const Identifier engineRootFolderAssetIdentifier = FindOrRegisterFolder(enginePath, Identifier{});

		const Tag::Identifier pluginAssetTagIdentifier = tagRegistry.FindOrRegister(PluginTagGuid);
		const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(EngineManager::EngineAssetsTagGuid);

		const IO::Path localPluginsAssetDatabasePath = LocalPluginDatabase::GetFilePath();
		return IO::CreateAsyncLoadFromDiskJob(
			localPluginsAssetDatabasePath,
			Threading::JobPriority::LoadPlugin,
			[this, pluginAssetTagIdentifier, engineAssetTagIdentifier, enginePath, engineRootFolderAssetIdentifier](const ConstByteView data)
			{
				Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
				);
				if (UNLIKELY(!rootReader.GetData().IsValid()))
				{
					return;
				}

				const Serialization::Reader reader(rootReader);
				FlatVector<Tag::Identifier, 2> tags{pluginAssetTagIdentifier, engineAssetTagIdentifier};

				[[maybe_unused]] const bool wasLoaded = Load(reader, enginePath, engineRootFolderAssetIdentifier, Array{pluginAssetTagIdentifier});
				Assert(wasLoaded);
			},
			ByteView{},
			Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
		);
	}

	Identifier Library::ImportAsset(const Guid assetGuid, IO::Path&& assetPath)
	{
		LocalDatabase localDatabase;
		if (const Optional<DatabaseEntry*> pLibraryDatabaseEntry = localDatabase.RegisterAsset(Forward<IO::Path>(assetPath)))
		{
			if (localDatabase.Save(Serialization::SavingFlags{}))
			{
				return EngineAssetDatabase::RegisterAsset(assetGuid, DatabaseEntry{*pLibraryDatabaseEntry}, {});
			}
		}
		return {};
	}
}
