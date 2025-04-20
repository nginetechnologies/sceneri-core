#pragma once

#include <AssetCompilerCore/Plugin.h>

#include <Common/Platform/Type.h>
#include <Common/Serialization/ForwardDeclarations/SerializedData.h>

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

namespace ngine::Scripting::AST
{
	struct Graph;
}

namespace ngine::AssetCompiler::Compilers
{
	struct ScriptCompiler
	{
		[[nodiscard]] static bool IsUpToDate(
			const Platform::Type platform,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context
		);

		[[nodiscard]] static Threading::Job* CompileFromAST(
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
		);
		[[nodiscard]] static Threading::Job* CompileFromLua(
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
		);
	protected:
		[[nodiscard]] static bool CompileFromASTInternal(const Scripting::AST::Graph& astGraph, const IO::PathView assetPackagePath);
	};
}
