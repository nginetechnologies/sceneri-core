#include "AssetCompilerCore/AssetCompilers/ScriptCompiler.h"

#include <Engine/Scripting/ScriptAssetType.h>
#include <Engine/Scripting/Parser/Lexer.h>
#include <Engine/Scripting/Parser/Parser.h>
#include <Engine/Scripting/Parser/AST/Graph.h>
#include <Engine/Scripting/Compiler/Compiler.h>

#include <Common/Asset/Asset.h>
#include <Common/Asset/Context.h>
#include <Common/Memory/Containers/String.h>
#include <Common/IO/Path.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/File.h>
#include <Common/IO/Library.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/EnumFlags.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Time/Timestamp.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Guid.h>

namespace ngine::AssetCompiler::Compilers
{
	bool ScriptCompiler::IsUpToDate(
		const Platform::Type,
		const Serialization::Data& assetData,
		const Asset::Asset& asset,
		const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context
	)
	{
		const Time::Timestamp astTimestamp = asset.GetMetaDataFilePath().GetLastModifiedTime();
		if (sourceFilePath != asset.GetMetaDataFilePath())
		{
			if (!astTimestamp.IsValid())
			{
				return false;
			}

			// Compiling from an external script (i.a. .lua)
			// We'll do two-part "compilation", first from Lua -> AST
			// Then AST -> compiled function
			const Time::Timestamp sourceFileTimestamp = sourceFilePath.GetLastModifiedTime();
			Assert(sourceFileTimestamp.IsValid());
			if (astTimestamp < sourceFileTimestamp)
			{
				return false;
			}
		}
		else
		{
			Assert(astTimestamp.IsValid());
		}

		// Validate that all compiled functions are up to date
		Scripting::AST::Graph astGraph;
		if (!astGraph.Serialize(Serialization::Reader(assetData)))
		{
			return false;
		}

		if (!astGraph.IsValid())
		{
			return false;
		}

		const IO::PathView assetPackagePath = asset.GetDirectory();
		Assert(assetPackagePath.GetAllExtensions() == Scripting::ScriptAssetType::AssetFormat.metadataFileExtension);

		// Find all assignments that declare function variables
		bool failedAny = false;
		astGraph.IterateFunctions(
			[astTimestamp,
		   assetPackagePath,
		   &failedAny](const Scripting::AST::Expression::Base& variableBase, const Scripting::AST::Expression::Function&)
			{
				Guid functionGuid;
				switch (variableBase.GetType())
				{
					case Scripting::AST::NodeType::VariableDeclaration:
					{
						functionGuid = static_cast<const Scripting::AST::Expression::VariableDeclaration&>(variableBase).GetIdentifier().identifier;
					}
					break;
					case Scripting::AST::NodeType::Variable:
					{
						functionGuid = static_cast<const Scripting::AST::Expression::Variable&>(variableBase).GetIdentifier().identifier;
					}
					break;
					default:
						ExpectUnreachable();
				}

				// Each type of function entry point is stored as a compiled binary inside the script's asset package
			  // Validate that it is up to date
				const IO::Path targetBinaryFilePath = IO::Path::Combine(
					assetPackagePath,
					IO::Path::Merge(functionGuid.ToString().GetView(), Scripting::ScriptAssetType::FunctionExtension)
				);
				const Time::Timestamp targetBinaryFileTimestamp = targetBinaryFilePath.GetLastModifiedTime();
				if (!targetBinaryFileTimestamp.IsValid())
				{
					failedAny = true;
					return;
				}

				if (targetBinaryFileTimestamp < astTimestamp)
				{
					failedAny = true;
					return;
				}
			}
		);

		return !failedAny;
	}

	bool ScriptCompiler::CompileFromASTInternal(const Scripting::AST::Graph& astGraph, const IO::PathView assetPackagePath)
	{
		if (astGraph.IsInvalid())
		{
			return false;
		}

		Assert(assetPackagePath.GetAllExtensions() == Scripting::ScriptAssetType::AssetFormat.metadataFileExtension);
		bool failedAny = false;
		astGraph.IterateFunctions(
			[assetPackagePath,
		   &failedAny](const Scripting::AST::Expression::Base& variableBase, const Scripting::AST::Expression::Function& functionExpression)
			{
				Guid functionGuid;
				switch (variableBase.GetType())
				{
					case Scripting::AST::NodeType::VariableDeclaration:
					{
						functionGuid = static_cast<const Scripting::AST::Expression::VariableDeclaration&>(variableBase).GetIdentifier().identifier;
					}
					break;
					case Scripting::AST::NodeType::Variable:
					{
						functionGuid = static_cast<const Scripting::AST::Expression::Variable&>(variableBase).GetIdentifier().identifier;
					}
					break;
					default:
						ExpectUnreachable();
				}

				// Each type of function entry point is stored as a compiled binary inside the script's asset package
			  // Validate that it is up to date
				const IO::Path targetBinaryFilePath = IO::Path::Combine(
					assetPackagePath,
					IO::Path::Merge(functionGuid.ToString().GetView(), Scripting::ScriptAssetType::FunctionExtension)
				);
				Scripting::Compiler compiler;
				UniquePtr<Scripting::FunctionObject> pFunction = compiler.Compile(functionExpression);
				if (pFunction.IsInvalid())
				{
					failedAny = true;
					return;
				}

				IO::File outputFile(targetBinaryFilePath, IO::AccessModeFlags::WriteBinary);
				if (!outputFile.IsValid())
				{
					failedAny = true;
					return;
				}

				Vector<ByteType> code;
				if (!compiler.Save(*pFunction, code))
				{
					failedAny = true;
					return;
				}
				outputFile.Write(code.GetView());
			}
		);

		return !failedAny;
	}

	Threading::Job* ScriptCompiler::
		CompileFromAST(const EnumFlags<CompileFlags> flags, CompileCallback&& callback, Threading::JobRunnerThread&, const AssetCompiler::Plugin&, const EnumFlags<Platform::Type>, Serialization::Data&& assetData, Asset::Asset&& asset, const IO::Path&, const Asset::Context&, const Asset::Context&)
	{
		return &Threading::CreateCallback(
			[callback = Forward<CompileCallback>(callback),
		   flags,
		   assetData = Forward<Serialization::Data>(assetData),
		   asset = Forward<Asset::Asset>(asset)](Threading::JobRunnerThread&) mutable
			{
				const IO::Path sourceFilePath = *asset.ComputeAbsoluteSourceFilePath();
				Serialization::Data data(sourceFilePath);
				if (!data.IsValid())
				{
					LogError("Failed to open script asset '{}' from file '{}'", asset.GetGuid().ToString(), sourceFilePath);
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				Scripting::AST::Graph astGraph;
				if (!astGraph.Serialize(Serialization::Reader(data)))
				{
					LogError("Failed to read AST graph from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				const IO::PathView assetPackagePath = asset.GetDirectory();
				if (!CompileFromASTInternal(astGraph, assetPackagePath))
				{
					LogError("Failed to compile AST graph from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			},
			Threading::JobPriority::AssetCompilation
		);
	}

	Threading::Job* ScriptCompiler::
		CompileFromLua(const EnumFlags<CompileFlags> flags, CompileCallback&& callback, Threading::JobRunnerThread&, const AssetCompiler::Plugin&, const EnumFlags<Platform::Type>, Serialization::Data&& assetData, Asset::Asset&& asset, const IO::Path&, const Asset::Context&, const Asset::Context&)
	{
		return &Threading::CreateCallback(
			[callback = Forward<CompileCallback>(callback),
		   flags,
		   assetData = Forward<Serialization::Data>(assetData),
		   asset = Forward<Asset::Asset>(asset)](Threading::JobRunnerThread&) mutable
			{
				const IO::Path sourceFilePath = *asset.ComputeAbsoluteSourceFilePath();

				IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
				if (!sourceFile.IsValid())
				{
					LogError("Failed to open script asset '{}' from file '{}'", asset.GetGuid().ToString(), sourceFilePath);
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				const uint32 size = uint32(sourceFile.GetSize());
				Scripting::StringType sourceText;
				sourceText.Resize(size);
				if (!sourceFile.ReadIntoView(sourceText.GetView()))
				{
					LogError("Failed to read file '{}'", sourceFilePath);
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				Scripting::Lexer lexer;
				lexer.SetSourceFilePath(sourceFilePath.GetView());
				Scripting::TokenListType tokens;
				if (!lexer.ScanTokens(sourceText, tokens))
				{
					LogError("Failed to run lexer on script from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				Scripting::Parser parser;
				const Optional<Scripting::AST::Graph> astGraph = parser.Parse(tokens);
				if (astGraph.IsInvalid())
				{
					LogError("Failed to generate AST graph from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				Serialization::Writer writer(assetData);
				if (!astGraph->Serialize(writer))
				{
					LogError("Failed to write AST graph from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				const IO::PathView assetPackagePath = asset.GetDirectory();
				if (!CompileFromASTInternal(*astGraph, assetPackagePath))
				{
					LogError("Failed to compile AST graph from asset '{}''", asset.GetGuid().ToString());
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					return;
				}

				asset.SetTypeGuid(Scripting::ScriptAssetType::AssetFormat.assetTypeGuid);
				asset.Serialize(writer);

				callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			},
			Threading::JobPriority::AssetCompilation
		);
	}
}
