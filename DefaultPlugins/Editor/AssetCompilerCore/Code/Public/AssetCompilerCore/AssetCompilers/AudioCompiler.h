#pragma once

#include <Common/Serialization/ForwardDeclarations/SerializedData.h>
#include <Common/Platform/Type.h>
#include <AssetCompilerCore/Plugin.h>

namespace ngine::Asset
{
	struct Asset;
}

namespace ngine::IO
{
	struct Path;
}

namespace ngine::Threading
{
	struct JobRunnerThread;
	struct Job;
}

namespace ngine::AssetCompiler::Compilers
{
	struct AudioCompiler
	{
		[[nodiscard]] static bool IsUpToDate(
			const Platform::Type platform,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context
		);

		static Threading::Job* CompileAudio(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& assetCompiler,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
	};
}
