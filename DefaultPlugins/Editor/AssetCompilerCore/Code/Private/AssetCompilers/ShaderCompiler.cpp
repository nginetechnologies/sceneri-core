#include "AssetCompilerCore/AssetCompilers/ShaderCompiler.h"

#include <Common/Asset/Asset.h>
#include <Common/Platform/StartProcess.h>
#include <Common/Memory/Containers/String.h>
#include <Common/IO/Path.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/File.h>
#include <Common/IO/Library.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/EnumFlags.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Time/Timestamp.h>

#include <Renderer/Assets/Shader/ShaderAsset.h>
#include <Renderer/ShaderStage.h>
#include <Renderer/Constants.h>

namespace ngine::AssetCompiler::Compilers
{
	enum class ShaderFeature : uint8
	{
		PushConstants = 1 << 0,
		First = PushConstants,
		Metal = 1 << 1,
		Vulkan = 1 << 2,
		WebGPU = 1 << 3,
		Last = WebGPU,
		End = Last << 1,
		Count = Math::Log2(Last) + 1,
		// Note: Currently converting SPIRV -> MSL at runtime.
		MetalShaderLanguage = Vulkan, // Metal
		SPIRV = Vulkan,
		WGSL = WebGPU,
		All = PushConstants | Metal | Vulkan | WebGPU
	};
	ENUM_FLAG_OPERATORS(ShaderFeature);

	[[nodiscard]] constexpr ConstStringView GetShaderFeatureMacroName(const ShaderFeature shaderFeature)
	{
		switch (shaderFeature)
		{
			case ShaderFeature::PushConstants:
				return "HAS_PUSH_CONSTANTS";
			case ShaderFeature::Metal:
				return "RENDERER_METAL";
			case ShaderFeature::Vulkan:
				return "RENDERER_VULKAN";
			case ShaderFeature::WebGPU:
				return "RENDERER_WEBGPU";
			case ShaderFeature::End:
			// case ShaderFeature::Count:
			case ShaderFeature::All:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	enum class GraphicsAPI : uint8
	{
		Vulkan = 1 << 0,
		Metal = 1 << 1,
		WebGPU = 1 << 2,
		Count = Math::Log2(WebGPU) + 1,
	};

	[[nodiscard]] constexpr GraphicsAPI GetGraphicsAPI(const Platform::Type platform)
	{
		switch (platform)
		{
			case Platform::Type::Windows:
			case Platform::Type::Android:
			case Platform::Type::Linux:
				return GraphicsAPI::Vulkan;
			case Platform::Type::iOS:
			case Platform::Type::macCatalyst:
			case Platform::Type::macOS:
			{
#if RENDERER_USING_METAL
				return GraphicsAPI::Metal;
#else
				return GraphicsAPI::Vulkan;
#endif
			}
			case Platform::Type::visionOS:
				return GraphicsAPI::Metal;
			case Platform::Type::Web:
				return GraphicsAPI::WebGPU;
			// case Platform::Type::Count:
			case Platform::Type::Apple:
			case Platform::Type::All:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	[[nodiscard]] constexpr EnumFlags<ShaderFeature> GetSupportedShaderFeatures(const GraphicsAPI graphicsAPI)
	{
		switch (graphicsAPI)
		{
			case GraphicsAPI::Vulkan:
				return ShaderFeature::Vulkan | ShaderFeature::PushConstants;
			case GraphicsAPI::Metal:
				return ShaderFeature::Metal | ShaderFeature::MetalShaderLanguage | ShaderFeature::PushConstants;
			case GraphicsAPI::WebGPU:
				return ShaderFeature::WebGPU;
			case GraphicsAPI::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	struct ShaderAssetData
	{
		bool Serialize(Serialization::Reader reader)
		{
			if (const Optional<Serialization::Reader> definitionsReader = reader.FindSerializer("definitions"))
			{
				for (const Serialization::Reader definitionReader : definitionsReader->GetArrayView())
				{
					if (const Optional<ConstStringView> definition = definitionReader.ReadInPlace<ConstStringView>())
					{
						m_preprocessorDefines += "-D";
						m_preprocessorDefines += *definition;
						m_preprocessorDefines += "=1 ";
					}
				}
			}
			reader.Serialize("vulkan_version", m_vulkanVersion);
			return true;
		}

		String m_preprocessorDefines;
		String m_vulkanVersion;
	};

	[[nodiscard]] bool CompileSPIRV(
		const IO::PathView sourceFilePath,
		const IO::Path& targetFilePath,
		const Rendering::ShaderStage shaderStage,
		const ConstStringView preprocessorDefines,
		[[maybe_unused]] const ConstStringView vulkanVersion,
		const EnumFlags<ShaderFeature> supportedShaderFeatures
	)
	{
		// TODO: Switch to static libraries instead of separate processes
#if PLATFORM_DESKTOP && !PLATFORM_APPLE_MACCATALYST
		if constexpr (HAS_GLSLANGVALIDATOR)
		{
			const IO::Path glslangValidatorExecutablePath = IO::Path::Combine(
				IO::Path::GetExecutableDirectory(),
				MAKE_PATH("vulkan"),
				IO::Path::Merge(MAKE_PATH("glslangValidator"), IO::Library::ExecutablePostfix)
			);

			String preprocessorMacros{preprocessorDefines};
			for (ShaderFeature shaderFeature = ShaderFeature::First; shaderFeature != ShaderFeature::End;
			     shaderFeature = shaderFeature << uint8(1))
			{
				preprocessorMacros += "-D";
				preprocessorMacros += GetShaderFeatureMacroName(shaderFeature);
				if (supportedShaderFeatures.IsSet(shaderFeature))
				{
					preprocessorMacros += "=1 ";
				}
				else
				{
					preprocessorMacros += "=0 ";
				}
			}

			ConstStringView stage;
			switch (shaderStage)
			{
				case Rendering::ShaderStage::Vertex:
					stage = "vert";
					break;
				case Rendering::ShaderStage::Fragment:
					stage = "frag";
					break;
				case Rendering::ShaderStage::Geometry:
					stage = "geom";
					break;
				case Rendering::ShaderStage::Compute:
					stage = "comp";
					break;
				case Rendering::ShaderStage::TessellationEvaluation:
					stage = "tese";
					break;
				case Rendering::ShaderStage::TessellationControl:
					stage = "tesc";
					break;
				case Rendering::ShaderStage::RayGeneration:
					stage = "rgen";
					break;
				case Rendering::ShaderStage::RayIntersection:
					stage = "rint";
					break;
				case Rendering::ShaderStage::RayAnyHit:
					stage = "rahit";
					break;
				case Rendering::ShaderStage::RayClosestHit:
					stage = "rchit";
					break;
				case Rendering::ShaderStage::RayMiss:
					stage = "rmiss";
					break;
				case Rendering::ShaderStage::RayCallable:
					stage = "rcall";
					break;
				case Rendering::ShaderStage::Invalid:
				case Rendering::ShaderStage::Count:
				case Rendering::ShaderStage::All:
					ExpectUnreachable();
			}

			const ConstStringView targetEnvironment = vulkanVersion.HasElements() ? vulkanVersion : "vulkan1.1";

			if constexpr (HAS_SPIRVOPT)
			{
				const IO::Path tempUnoptimizedFilePath =
					IO::Path::Combine(IO::Path::GetTemporaryDirectory(), MAKE_PATH("AssetCompiler"), Guid::Generate().ToString().GetView());
				{
					IO::Path tempFolderDirectory(tempUnoptimizedFilePath.GetParentPath());
					if (!tempFolderDirectory.Exists())
					{
						tempFolderDirectory.CreateDirectories();
					}
				}

				NativeString glslangValidatorCommandLine(String().Format(
					"{} {} -e main0 --source-entrypoint main --target-env {} -o \"{}\" -V \"{}\" -S {} {}",
					glslangValidatorExecutablePath,
					PROFILE_BUILD ? "-g" : "",
					targetEnvironment,
					tempUnoptimizedFilePath,
					sourceFilePath,
					stage,
					preprocessorMacros
				));
				if (Platform::StartProcessAndWaitForFinish(glslangValidatorExecutablePath.GetZeroTerminated(), glslangValidatorCommandLine))
				{
					const IO::Path spirvOptExecutablePath = IO::Path::Combine(
						IO::Path::GetExecutableDirectory(),
						MAKE_PATH("vulkan"),
						IO::Path::Merge(MAKE_PATH("spirv-opt"), IO::Library::ExecutablePostfix)
					);

					NativeString spirvOptCommandLine(String().Format(
						"{} --preserve-bindings --target-env={} -O \"{}\" -o \"{}\"",
						spirvOptExecutablePath,
						targetEnvironment,
						tempUnoptimizedFilePath,
						targetFilePath
					));
					const bool optimizedSourceFile =
						Platform::StartProcessAndWaitForFinish(spirvOptExecutablePath.GetZeroTerminated(), spirvOptCommandLine);
					if (optimizedSourceFile)
					{
						return true;
					}
					else
					{
						LogWarning("Failed to compile optimize SPIRV for asset {}", sourceFilePath);
						return tempUnoptimizedFilePath.MoveFileTo(targetFilePath);
					}
				}
				else
				{
					LogError("Failed to compile GLSL -> unoptimized SPIRV for asset {}", sourceFilePath);
					return false;
				}
			}
			else
			{
				NativeString glslangValidatorCommandLine(String().Format(
					"{} {} -e main0 --source-entrypoint main --target-env {} -o \"{}\" -V \"{}\" -S {} {}",
					glslangValidatorExecutablePath,
					PROFILE_BUILD ? "-g" : "",
					targetEnvironment,
					targetFilePath,
					sourceFilePath,
					stage,
					preprocessorMacros
				));
				if (Platform::StartProcessAndWaitForFinish(glslangValidatorExecutablePath.GetZeroTerminated(), glslangValidatorCommandLine))
				{
					return true;
				}
				else
				{
					LogError("Failed to compile GLSL -> SPIRV for asset {}", sourceFilePath);
					return false;
				}
			}
		}
		else
		{
			return false;
		}
#else
		UNUSED(sourceFilePath);
		UNUSED(targetFilePath);
		UNUSED(shaderStage);
		UNUSED(preprocessorDefines);
		UNUSED(supportedShaderFeatures);
		return false;
#endif
	}

	[[nodiscard]] bool CompileWGSL(const IO::PathView spirvFilePath, const IO::Path& targetFilePath)
	{
		// TODO: Switch to static libraries instead of separate processes
#if PLATFORM_DESKTOP && !PLATFORM_APPLE_MACCATALYST
		if constexpr (HAS_TINT)
		{
			const IO::Path tintExecutablePath = IO::Path::Combine(
				IO::Path::GetExecutableDirectory(),
				MAKE_PATH("tint"),
				IO::Path::Merge(MAKE_PATH("tint"), IO::Library::ExecutablePostfix)
			);

			NativeString tintCommandLine(
				String().Format("{} --format wgsl --entry-point main0 --output-name {} {}", tintExecutablePath, targetFilePath, spirvFilePath)
			);
			return Platform::StartProcessAndWaitForFinish(tintExecutablePath.GetZeroTerminated(), tintCommandLine);
		}
		else
		{
			return false;
		}
#else
		UNUSED(spirvFilePath);
		UNUSED(targetFilePath);
		return false;
#endif
	}

	Threading::Job* Compile(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin&,
		[[maybe_unused]] EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context&,
		const Rendering::ShaderStage stage
	)
	{
		switch (stage)
		{
			case Rendering::ShaderStage::Geometry:
				platforms.Clear(Platform::Type::Apple | Platform::Type::Web);
				if (platforms.IsEmpty())
				{
					callback(
						flags | CompileFlags::UnsupportedOnPlatform,
						ArrayView<Asset::Asset>{asset},
						ArrayView<const Serialization::Data>{assetData}
					);
					return nullptr;
				}
				break;
			default:
				break;
		}

#if PLATFORM_DESKTOP && !PLATFORM_APPLE_MACCATALYST
		return &Threading::CreateCallback(
			[stage,
		   flags,
		   callback,
		   sourceFilePath,
		   assetData = Move(assetData),
		   asset = Move(asset),
		   platforms](Threading::JobRunnerThread&) mutable
			{
				ShaderAssetData shaderAssetData;
				Serialization::Deserialize(assetData, shaderAssetData);

				EnumFlags<GraphicsAPI> graphicsAPIs;
				for (const Platform::Type platform : platforms)
				{
					graphicsAPIs |= GetGraphicsAPI(platform);
				}

				const EnumFlags<ShaderFeature> supportedShaderFeatures = GetSupportedShaderFeatures(*graphicsAPIs.GetFirstSetFlag());
				for (const GraphicsAPI graphicsAPI : graphicsAPIs)
				{
					const EnumFlags<ShaderFeature> supportedAPIShaderFeatures = GetSupportedShaderFeatures(graphicsAPI);
					if (supportedAPIShaderFeatures != supportedShaderFeatures)
					{
						// TODO: Change shader output so we can compile all at the same time, and output to different binary locations
						Assert(false, "Attempting to compile shader for incompatible platforms at the same time");
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
						return;
					}
				}

				const IO::PathView targetFileExtension = Rendering::ShaderAsset::GetBinaryFileExtensionFromSource(sourceFilePath.GetAllExtensions()
			  );
				const IO::Path targetFilePath = IO::Path::Combine(
					asset.GetDirectory(),
					IO::Path::Merge(asset.GetMetaDataFilePath().GetFileNameWithoutExtensions(), targetFileExtension)
				);

				// Support copying over precompiled MSL if it exists
			  // Temporary workaround for cases where SPIRV-cross generates invalid MSL at runtime
				if (supportedShaderFeatures.IsSet(ShaderFeature::MetalShaderLanguage))
				{
					if (sourceFilePath.GetParentPath() != targetFilePath.GetParentPath())
					{
						const IO::Path sourceMslShaderPath = IO::Path::Combine(
							sourceFilePath.GetParentPath(),
							IO::Path::Merge(targetFilePath.GetFileNameWithoutExtensions(), MAKE_PATH(".msl"))
						);
						if (sourceMslShaderPath.Exists())
						{
							if (!sourceMslShaderPath.CopyFileTo(IO::Path::Merge(targetFilePath.GetWithoutExtensions(), MAKE_PATH(".msl"))))
							{
								Assert(false, "Attempting to copy MSL source");
								callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
							}
						}
					}
				}

				if (supportedShaderFeatures.IsSet(ShaderFeature::SPIRV))
				{
					const bool wasCompiled = CompileSPIRV(
						sourceFilePath,
						targetFilePath,
						stage,
						shaderAssetData.m_preprocessorDefines,
						shaderAssetData.m_vulkanVersion,
						supportedShaderFeatures
					);
					callback(
						flags | (CompileFlags::Compiled * wasCompiled),
						ArrayView<Asset::Asset>{asset},
						ArrayView<const Serialization::Data>{assetData}
					);
				}
				else if (supportedShaderFeatures.IsSet(ShaderFeature::WGSL))
				{
					const IO::Path tempSpirvFilePath = IO::Path::Combine(
						IO::Path::GetTemporaryDirectory(),
						MAKE_PATH("AssetCompiler"),
						Guid::Generate().ToString().GetView(),
						MAKE_PATH(".spv")
					);
					{
						IO::Path tempFolderDirectory(tempSpirvFilePath.GetParentPath());
						if (!tempFolderDirectory.Exists())
						{
							tempFolderDirectory.CreateDirectories();
						}
					}
					bool wasCompiled = CompileSPIRV(
						sourceFilePath,
						tempSpirvFilePath,
						stage,
						shaderAssetData.m_preprocessorDefines,
						shaderAssetData.m_vulkanVersion,
						supportedShaderFeatures
					);
					if (wasCompiled)
					{
						wasCompiled = CompileWGSL(tempSpirvFilePath, targetFilePath);
						// Hacked for now, relying on shader load fail and assertions at runtime
					  // Remove when all of our shaders compile for WGSL
						wasCompiled = true;
						callback(
							flags | (CompileFlags::Compiled * wasCompiled),
							ArrayView<Asset::Asset>{asset},
							ArrayView<const Serialization::Data>{assetData}
						);
					}
					else
					{
						// Hacked for now, relying on shader load fail and assertions at runtime
					  // Remove when all of our shaders compile for WGSL
						wasCompiled = true;
						callback(
							flags | (CompileFlags::Compiled * wasCompiled),
							ArrayView<Asset::Asset>{asset},
							ArrayView<const Serialization::Data>{assetData}
						);
					}
				}
				else
				{
					Assert(false, "Attempting to compile shader for unknown language");
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
			},
			Threading::JobPriority::AssetCompilation
		);
#else
		callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
		return nullptr;
#endif
	}

	bool ShaderCompiler::IsUpToDate(
		const Platform::Type,
		const Serialization::Data&,
		const Asset::Asset& asset,
		const IO::Path& sourceFilePath,
		[[maybe_unused]] const Asset::Context& context
	)
	{
		const Time::Timestamp sourceFileTimestamp = sourceFilePath.GetLastModifiedTime();

		const IO::PathView targetFileExtension = Rendering::ShaderAsset::GetBinaryFileExtensionFromSource(sourceFilePath.GetAllExtensions());
		const IO::Path targetFilePath = IO::Path::Combine(
			asset.GetDirectory(),
			IO::Path::Merge(asset.GetMetaDataFilePath().GetFileNameWithoutExtensions(), targetFileExtension)
		);
		const Time::Timestamp targetFileTimestamp = targetFilePath.GetLastModifiedTime();

		return (targetFileTimestamp >= sourceFileTimestamp) && sourceFileTimestamp.IsValid();
	}

	Threading::Job* ShaderCompiler::CompileFragmentShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::Fragment
		);
	}

	Threading::Job* ShaderCompiler::CompileVertexShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::Vertex
		);
	}

	Threading::Job* ShaderCompiler::CompileGeometryShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::Geometry
		);
	}

	Threading::Job* ShaderCompiler::CompileComputeShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::Compute
		);
	}

	Threading::Job* ShaderCompiler::CompileTessellationEvaluationShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::TessellationEvaluation
		);
	}
	Threading::Job* ShaderCompiler::CompileTessellationControlShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::TessellationControl
		);
	}

	Threading::Job* ShaderCompiler::CompileRaytracingGenerationShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayGeneration
		);
	}
	Threading::Job* ShaderCompiler::CompileRaytracingIntersectionShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayIntersection
		);
	}
	Threading::Job* ShaderCompiler::CompileRaytracingAnyHitShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayAnyHit
		);
	}
	Threading::Job* ShaderCompiler::CompileRaytracingClosestHitShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayClosestHit
		);
	}
	Threading::Job* ShaderCompiler::CompileRaytracingMissShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayMiss
		);
	}
	Threading::Job* ShaderCompiler::CompileRaytracingCallableShader(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		[[maybe_unused]] Threading::JobRunnerThread& currentThread,
		const AssetCompiler::Plugin& plugin,
		[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
		[[maybe_unused]] Serialization::Data&& assetData,
		[[maybe_unused]] Asset::Asset&& asset,
		[[maybe_unused]] const IO::Path& sourceFilePath,
		const Asset::Context& context,
		[[maybe_unused]] const Asset::Context& sourceContext
	)
	{
		return Compile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			plugin,
			platforms,
			Forward<Serialization::Data>(assetData),
			Forward<Asset::Asset>(asset),
			sourceFilePath,
			context,
			Rendering::ShaderStage::RayCallable
		);
	}
}
