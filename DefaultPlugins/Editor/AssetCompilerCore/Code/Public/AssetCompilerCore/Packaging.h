#pragma once

#include <Common/Platform/Type.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine
{
	struct ProjectInfo;
	struct EngineInfo;

	namespace Asset
	{
		struct Context;
	}
}

namespace ngine::IO
{
	struct Path;
}

namespace ngine::AssetCompiler
{
	struct Plugin;
}

namespace ngine::Threading
{
	struct JobRunnerThread;
	struct Job;
}

namespace ngine::ProjectSystem
{
	enum class PackagingFlags : uint8
	{
		// Whether to create a fully clean build by first clearing the target directory
		CleanBuild = 1 << 0,
	};
	ENUM_FLAG_OPERATORS(PackagingFlags);

	void PackageProjectLauncher(
		AssetCompiler::Plugin& assetcompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		const Asset::Context& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageProjectEditor(
		AssetCompiler::Plugin& assetcompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageStandaloneEditor(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageStandaloneLauncher(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageStandaloneProjectSystem(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageStandaloneAssetCompiler(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
	void PackageStandaloneTools(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		const EnumFlags<PackagingFlags> flags = {}
	);
}
