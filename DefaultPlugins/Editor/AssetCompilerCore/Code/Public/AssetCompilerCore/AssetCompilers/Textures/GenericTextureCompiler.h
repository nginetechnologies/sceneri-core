#pragma once

#include <AssetCompilerCore/Plugin.h>

#include <Renderer/Format.h>
#include <Renderer/Assets/Texture/TextureAsset.h>

#include <Common/Serialization/ForwardDeclarations/SerializedData.h>
#include <Common/Platform/Type.h>

namespace ngine::Asset
{
	struct Asset;
	struct Database;
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
	struct GenericTextureCompiler
	{
		[[nodiscard]] static bool IsUpToDate(
			const Platform::Type platforms,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context
		);

		static Threading::Job* CompileJPG(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompilePNG(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		[[nodiscard]] static Vector<ByteType> ExportPNG(
			const Rendering::Format sourceFormat,
			const Rendering::TextureAsset::BinaryType binaryType,
			const Math::Vector2ui resolution,
			const Rendering::Format targetFormat,
			const Optional<Math::Ratiof> decompressionQuality,
			const ConstByteView data
		);
		[[nodiscard]] static Threading::Job*
		ExportPNG(ExportedCallback&&, const EnumFlags<Platform::Type> platforms, Serialization::Data&& assetData, Asset::Asset&& asset, const IO::PathView targetFormat, const Asset::Context&);

		static Threading::Job* CompilePSD(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompileTGA(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompileBMP(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompileGIF(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompileHDR(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		static Threading::Job* CompileEXR(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		static Threading::Job* CompileBRDF(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);

		static Threading::Job* CompilePIC(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompilePNM(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			return CompileStbi(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				plugin,
				platforms,
				Forward<Serialization::Data>(assetData),
				Forward<Asset::Asset>(asset),
				sourceFilePath,
				context,
				sourceContext
			);
		}

		static Threading::Job* CompileTIFF(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		[[nodiscard]] static Threading::Job*
		ExportTIFF(ExportedCallback&&, const EnumFlags<Platform::Type> platforms, Serialization::Data&& assetData, Asset::Asset&& asset, const IO::PathView targetFormat, const Asset::Context&);

		static Threading::Job* CompileDDS(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		static Threading::Job* CompileKTX(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		static Threading::Job* CompileKTX2(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
		static Threading::Job* CompileSVG(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);

		//! Returns whether the source texture contains an alpha channel
		//! Does not check the contents of pixels.
		[[nodiscard]] static bool HasAlphaChannel(const IO::Path& sourceFilePath);

		enum class AlphaChannelUsageType : uint8
		{
			//! The texture either has no alpha channel, or has all pixels set to 100% alpha
			None,
			//! Whether the texture has an alpha channel and has any pixels set to any value other than 0% and 100% alpha
			Transparency,
			//! Whether the texture has an alpha channel and has all pixels set to 0% or 100% alpha
			Mask
		};

		//! Inspects all pixels in a texture to find out what type of alpha is used
		[[nodiscard]] static AlphaChannelUsageType GetAlphaChannelUsageType(const IO::Path& sourceFilePath);
	protected:
		static Threading::Job* CompileStbi(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& plugin,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
	};
}
