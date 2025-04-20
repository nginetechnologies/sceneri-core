#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Platform/Type.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Function/FunctionPointer.h>
#include <Common/Function/CopyableFunction.h>
#include <Common/Serialization/ForwardDeclarations/SerializedData.h>
#include <Common/Serialization/SavingFlags.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Containers/UnorderedSet.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/IO/Path.h>
#include <Common/Asset/Guid.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/ForwardDeclarations/Atomic.h>
#include <Common/Function/Function.h>
#include <Common/Threading/AtomicBool.h>
#include <Engine/Asset/Mask.h>
#include <Engine/Asset/ImportingFlags.h>

namespace ngine::Threading
{
	struct JobRunnerThread;
	struct Job;
	struct JobManager;
}

namespace ngine::Asset
{
	struct Asset;
	struct Guid;
	struct Context;
	struct Owners;
	struct Database;
	struct DatabaseEntry;
	struct LocalDatabase;
	struct Format;
}

namespace ngine::CommandLine
{
	struct Arguments;
}

namespace ngine::AssetCompiler
{
	enum class CompileFlags : uint8
	{
		Compiled = 1 << 0,
		UpToDate = 1 << 1,
		WasDirectlyRequested = 1 << 2,
		UnsupportedOnPlatform = 1 << 3,
		//! Whether the data saved to disk should be formatted intended to be readable by a human
		SaveHumanReadable = 1 << 4,
		//! Always recompiles regardless of timestamps
		ForceRecompile = 1 << 5,
		//! Whether the provided asset is a collection AKA folder asset
		IsCollection = 1 << 6
	};

	using CompileCallback = CopyableFunction<
		void(const EnumFlags<CompileFlags> flags, ArrayView<Asset::Asset> assets, ArrayView<const Serialization::Data> assetsData),
		24>;
	using ExportedCallback = CopyableFunction<void(const ConstByteView exportedData, const IO::PathView exportFileExtension), 24>;
	struct Plugin;

	struct SourceFileFormat
	{
		IO::PathView sourceFileExtension;
		const Asset::Format& m_assetFormat;

		using CompileFunction = FunctionPointer<Threading::Job*(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& assetCompiler,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			const Asset::Context& sourceContext
		)>;
		CompileFunction compileFunction;
		using IsUpToDateFunction = FunctionPointer<bool(
			const Platform::Type platform,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context
		)>;
		IsUpToDateFunction isUpToDateFunction;
		using ExportFunction = FunctionPointer<Threading::Job*(
			ExportedCallback&& callback,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::PathView targetFormat,
			const Asset::Context& context
		)>;
		IO::PathView m_exportedFileExtension;
		ExportFunction exportFunction;
	};

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "72D83E65-B5D9-4CCD-8B8B-8227C805570E"_guid;

		Plugin(Application&)
		{
		}
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		void RunCommandLineOptions(
			const CommandLine::Arguments& commandLineArgs,
			const Asset::Context& context,
			Threading::JobManager& jobManager,
			CompileCallback&& callback,
			Threading::Atomic<bool>& failedAnyTasks
		);

		[[nodiscard]] Threading::Job* CompileAnyAsset(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const EnumFlags<Platform::Type> platforms,
			IO::Path&& filePath,
			const Asset::Context& context,
			const Asset::Context& sourceContext,
			const IO::PathView targetDirectory = {}
		) const;
		[[nodiscard]] Threading::Job* CompileMetaDataAsset(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const EnumFlags<Platform::Type> platforms,
			IO::Path&& filePath,
			const Asset::Context& context,
			const Asset::Context& sourceContext
		) const;
		[[nodiscard]] Threading::Job* CompileAssetSourceFile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const EnumFlags<Platform::Type> platforms,
			IO::Path&& filePath,
			const IO::PathView targetDirectory,
			const Asset::Context& context,
			const Asset::Context& sourceContext
		) const;
		[[nodiscard]] Threading::Job* CompileAsset(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const SourceFileFormat& sourceFileFormat,
			const IO::Path& absoluteSourceFilePath,
			const Asset::Context& context,
			const Asset::Context& sourceContext
		) const;
		[[nodiscard]] Threading::Job* CompileAsset(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const Asset::Context& context,
			const Asset::Context& sourceContext
		) const;

		[[nodiscard]] Threading::Job* ExportAsset(
			ExportedCallback&& callback,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const Asset::Context& context
		) const;

		[[nodiscard]] bool
		AddNewAssetToDatabase(Asset::DatabaseEntry&& assetEntry, const Asset::Guid assetGuid, const Asset::Owners& assetOwners, const EnumFlags<Serialization::SavingFlags>);

		[[nodiscard]] bool HasAssetInDatabase(const Asset::Guid assetGuid, const Asset::Owners& assetOwners);

		[[nodiscard]] ngine::Guid
		CreateAsset(const IO::Path& newAssetFilePath, const ngine::Guid assetTypeGuid, const Asset::Context context, const EnumFlags<Serialization::SavingFlags> flags, Function<void(Serialization::Data& assetData, Asset::Asset&), 64>&&);
		[[nodiscard]] bool
		ModifyAsset(const Asset::Guid assetGuid, const Asset::Context context, const EnumFlags<Serialization::SavingFlags> flags, Function<void(Serialization::Data& assetData, Asset::DatabaseEntry& entry), 64>&&);

		[[nodiscard]] bool
		AddImportedAssetToDatabase(const Asset::Guid assetGuid, const Asset::Owners& assetOwners, const EnumFlags<Serialization::SavingFlags>);

		bool
		CopyAsset(const IO::Path& existingMetaDataPath, const Asset::Context& sourceContext, const IO::Path& newMetaDataPath, Asset::Context&& targetContext, const EnumFlags<Serialization::SavingFlags>);
		bool
		MoveAsset(const IO::Path& existingMetaDataPath, const IO::Path& newMetaDataPath, const Asset::Context& context, const EnumFlags<Serialization::SavingFlags>);
		ngine::Guid
		DuplicateAsset(const IO::Path& existingMetaDataPath, const IO::Path& newMetaDataPath, const Asset::Context& newContext, const EnumFlags<Serialization::SavingFlags>, Function<void(Serialization::Data& assetData, Asset::Asset&), 64>&&);
	protected:
		[[nodiscard]] bool
		RemoveAssetFromDatabase(const Asset::Guid assetGuid, const Asset::Owners& assetOwners, const EnumFlags<Serialization::SavingFlags>);
		[[nodiscard]] bool
		CopyAssetInternal(const IO::Path& existingMetaDataPath, const Asset::Context& sourceContext, const IO::Path& newMetaDataPath, Asset::Owners& newAssetOwners, const EnumFlags<Serialization::SavingFlags>);
		[[nodiscard]] bool
		CopySingleAssetInternal(const IO::Path& existingMetaDataPath, const Asset::Context& sourceContext, const IO::Path& newMetaDataPath, Asset::Owners& newAssetOwners, const EnumFlags<Serialization::SavingFlags>);
		[[nodiscard]] bool
		MoveAssetInternal(const IO::Path& existingMetaDataPath, const Asset::Owners& existingAssetOwners, const IO::Path& newMetaDataPath, const Asset::Owners& newAssetOwners, const EnumFlags<Serialization::SavingFlags>);

		void OnAssetsImported(const Asset::Mask& assets, const EnumFlags<Asset::ImportingFlags> flags);
		void OnAssetFinishedCompilingInternal(const Asset::Guid assetGuid);

		void RegisterRuntimeAsset(const Asset::Asset& asset, const Asset::Owners& assetOwners);
	protected:
		struct DatabaseLock
		{
			DatabaseLock(const IO::PathView pathView)
				: path(pathView)
			{
			}

			IO::Path path;
			Threading::Mutex lock;
		};

		Threading::Mutex m_databaseLocksLock;
		FlatVector<DatabaseLock, 64> m_databaseLocks;

		mutable Threading::Mutex m_currentlyCompilingAssetsLock;
		mutable UnorderedSet<Asset::Guid, Asset::Guid::Hash> m_currentlyCompilingAssets;
	};

	ENUM_FLAG_OPERATORS(CompileFlags);
}
