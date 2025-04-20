#include <AssetCompilerCore/SolutionGenerator.h>
#if SUPPORT_GENERATE_SOLUTION

#include <Common/Project System/PluginInfo.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/Project System/EngineDatabase.h>
#include <Common/Platform/StartProcess.h>
#include <Common/IO/File.h>
#include <Common/IO/FileIterator.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Format/Guid.h>
#include <Common/IO/Format/Path.h>
#include <Common/Serialization/Writer.h>
#include <Common/Platform/GetName.h>

namespace ngine::AssetCompiler
{
	constexpr IO::PathView GetGenerator(const Platform::Type platform)
	{
		switch (platform)
		{
			case Platform::Type::Windows:
				return IO::PathView(MAKE_PATH("Visual Studio 16 2019"));
			case Platform::Type::iOS:
			case Platform::Type::macOS:
			case Platform::Type::macCatalyst:
			case Platform::Type::visionOS:
				return IO::PathView(MAKE_PATH("Xcode"));
			default:
				ExpectUnreachable();
		}
	}

	constexpr IO::PathView GetSystemName(const Platform::Type platform)
	{
		switch (platform)
		{
			case Platform::Type::Windows:
				return IO::PathView(MAKE_PATH("Windows"));
			case Platform::Type::macOS:
				return IO::PathView(MAKE_PATH("Darwin"));
			case Platform::Type::iOS:
			case Platform::Type::macCatalyst:
				return IO::PathView(MAKE_PATH("iOS"));
			case Platform::Type::visionOS:
				return IO::PathView(MAKE_PATH("visionOS"));
			default:
				ExpectUnreachable();
		}
	}

	constexpr IO::PathView GetAdditionalCMakeOptions(const Platform::Type platform)
	{
		switch (platform)
		{
			case Platform::Type::iOS:
				return IO::PathView(MAKE_PATH("-DCMAKE_OSX_ARCHITECTURES=arm64"));
			default:
				return IO::PathView();
		}
	}

	enum class LauncherFlags : uint8
	{
		ProjectLauncher = 1 << 0,
		Editor = 1 << 1
	};

	ENUM_FLAG_OPERATORS(LauncherFlags);

	void ParsePlugin(
		PluginInfo&& plugin,
		String& cmakeListsContents,
		const EnginePluginDatabase& availablePluginDatabase,
		Vector<Asset::Guid>& parsedPluginsOut,
		const EnumFlags<LauncherFlags> launcherFlags
	)
	{
		FlatString<37> pluginGuidString = plugin.GetGuid().ToString();
		pluginGuidString.MakeUpper();

		const bool hasBeenParsed = parsedPluginsOut.Contains(plugin.GetGuid());
		if (!hasBeenParsed)
		{
			parsedPluginsOut.EmplaceBack(plugin.GetGuid());

			if (plugin.HasSourceDirectory())
			{
				IO::Path pluginSourceFolder = plugin.GetSourceDirectory();
				pluginSourceFolder.MakeForwardSlashes();

				cmakeListsContents += String().Format(
					R"(

if(NOT TARGET {1})
	add_subdirectory("{0}" "${{CMAKE_CURRENT_BINARY_DIR}}/{1}")
endif())",
					pluginSourceFolder,
					pluginGuidString
				);

				if (launcherFlags.IsSet(LauncherFlags::ProjectLauncher))
				{
					cmakeListsContents += String().Format("\nLinkPlugin(ProjectLauncher {0})", pluginGuidString);
				}

				if (launcherFlags.IsSet(LauncherFlags::Editor))
				{
					cmakeListsContents += String().Format("\nLinkPlugin(Editor {0})", pluginGuidString);
				}
			}
		}

		for (const Asset::Guid pluginDependencyGuid : plugin.GetDependencies())
		{
			IO::Path pluginConfigPath = availablePluginDatabase.FindPlugin(pluginDependencyGuid);
			Assert(!pluginConfigPath.IsEmpty());

			PluginInfo dependentPlugin(Move(pluginConfigPath));

			if (!hasBeenParsed)
			{
				ParsePlugin(Move(dependentPlugin), cmakeListsContents, availablePluginDatabase, parsedPluginsOut, launcherFlags);
			}

			if (plugin.HasSourceDirectory() & dependentPlugin.HasSourceDirectory())
			{
				FlatString<37> dependentPluginGuidString = dependentPlugin.GetGuid().ToString();
				dependentPluginGuidString.MakeUpper();

				cmakeListsContents += String().Format("\ntarget_link_libraries({0} PRIVATE {1})", pluginGuidString, dependentPluginGuidString);
			}
		}
	}

	bool GenerateProjectCMakeLists(const ProjectInfo& projectInfo, const EngineInfo& engineInfo)
	{
		const IO::PathView projectDirectoryPath = projectInfo.GetDirectory();

		const IO::Path codeDirectory = IO::Path::Combine(projectDirectoryPath, MAKE_PATH("Code"));

		Assert(engineInfo.IsValid());
		const IO::PathView engineFolder = engineInfo.GetDirectory();
		const IO::Path engineCodeFolder = IO::Path::Combine(engineFolder, engineInfo.GetRelativeSourceDirectory());

		if (!codeDirectory.Exists())
		{
			codeDirectory.CreateDirectories();
		}

		IO::Path configFilePathAdjusted = IO::Path(projectInfo.GetConfigFilePath());
		configFilePathAdjusted.MakeForwardSlashes();

		const IO::Path cmakeListsFilePath = IO::Path::Combine(codeDirectory, MAKE_PATH("CMakeLists.txt"));
		String cmakeListsContents;
		cmakeListsContents.Format(
			R"(cmake_minimum_required (VERSION 3.14)

if(NOT DEFINED ENGINE_CODE_DIRECTORY)
	if(NOT DEFINED ENGINE_CODE_DIRECTORY_IN)
		message(FATAL_ERROR "ENGINE_CODE_DIRECTORY_IN must be set to generate cache!")
	endif()

	set(ENGINE_CODE_DIRECTORY "${{ENGINE_CODE_DIRECTORY_IN}}")
endif()

set(ENGINE_CMAKE_DIRECTORY "${{ENGINE_CODE_DIRECTORY}}/CMake")
include("${{ENGINE_CMAKE_DIRECTORY}}/InitialSettings.cmake")
include("${{ENGINE_CMAKE_DIRECTORY}}/json-cmake/JSONParser.cmake")

set(PROJECT_BUILD TRUE CACHE BOOL "Project build" FORCE)
set(PROJECT_FILE "{0}" CACHE FILEPATH "Project file" FORCE)

get_filename_component(PROJECT_CONFIG_FILEPATH "${{PROJECT_FILE}}" REALPATH BASE_DIR "${{CMAKE_CURRENT_LIST_DIR}}")

file (STRINGS "${{PROJECT_CONFIG_FILEPATH}}" PROJECT_CONFIG_CONTENTS)
sbeParseJson(PROJECT_CONFIG_JSON PROJECT_CONFIG_CONTENTS)

message("Project: ${{PROJECT_CONFIG_JSON.name}} ${{PROJECT_CONFIG_JSON.version}}")
project("${{PROJECT_CONFIG_JSON.name}}" VERSION ${{PROJECT_CONFIG_JSON.version}} LANGUAGES CXX))",
			configFilePathAdjusted
		);

		const EnginePluginDatabase availablePluginDatabase(IO::Path::Combine(engineFolder, EnginePluginDatabase::FileName));

		IO::Path adjustedEngineCodeFolder = IO::Path(engineCodeFolder);
		adjustedEngineCodeFolder.MakeForwardSlashes();

		cmakeListsContents += R"(

option(OPTION_BUILD_ENGINE "Build engine" OFF)
if(OPTION_BUILD_ENGINE)
add_subdirectory("${ENGINE_CODE_DIRECTORY}" "${CMAKE_CURRENT_BINARY_DIR}/Engine")

if ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)

set_target_properties(Editor PROPERTIES
	VS_DEBUGGER_COMMAND_ARGUMENTS "-project ${PROJECT_FILE}")
set_target_properties(ProjectLauncher PROPERTIES
	VS_DEBUGGER_COMMAND_ARGUMENTS "-project ${PROJECT_FILE}")
endif()
else()
add_library(ProjectLauncher STATIC "${ENGINE_CODE_DIRECTORY}/Common/Public/Common/Platform/Type.h")
set_target_properties(ProjectLauncher PROPERTIES LINKER_LANGUAGE CXX)

add_library(Editor STATIC "${ENGINE_CODE_DIRECTORY}/Common/Public/Common/Platform/Type.h")
set_target_properties(Editor PROPERTIES LINKER_LANGUAGE CXX)

if ("${CMAKE_GENERATOR}" MATCHES "^Visual Studio")
set_property(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT Editor)

set_target_properties(Editor PROPERTIES
	VS_DEBUGGER_COMMAND "${ENGINE_CODE_DIRECTORY}/bin/$(Configuration)/Editor.exe"
	VS_DEBUGGER_COMMAND_ARGUMENTS "-project ${PROJECT_FILE}")
set_target_properties(ProjectLauncher PROPERTIES
	VS_DEBUGGER_COMMAND "${ENGINE_CODE_DIRECTORY}/bin/$(Configuration)/ProjectLauncher.exe"
	VS_DEBUGGER_COMMAND_ARGUMENTS "-project ${PROJECT_FILE}")
endif()
endif())";

		Vector<Asset::Guid> parsedPluginGuids;
		for (const Asset::Guid pluginGuid : projectInfo.GetPluginGuids())
		{
			IO::Path pluginConfigPath = availablePluginDatabase.FindPlugin(pluginGuid);
			Assert(!pluginConfigPath.IsEmpty());

			ParsePlugin(
				PluginInfo(Move(pluginConfigPath)),
				cmakeListsContents,
				availablePluginDatabase,
				parsedPluginGuids,
				LauncherFlags::Editor | LauncherFlags::ProjectLauncher
			);
		}

		constexpr bool includeEditor = true;
		constexpr Asset::Guid editorCoreGuid = "1B7A9E37-7221-4F7E-A266-DF5CAD4CEF1E"_asset;
		if constexpr (includeEditor)
		{
			IO::Path pluginConfigPath = availablePluginDatabase.FindPlugin(editorCoreGuid);
			Assert(!pluginConfigPath.IsEmpty());

			ParsePlugin(
				PluginInfo(Move(pluginConfigPath)),
				cmakeListsContents,
				availablePluginDatabase,
				parsedPluginGuids,
				LauncherFlags::Editor
			);
		}

		const IO::File cmakeListsFile(cmakeListsFilePath, IO::AccessModeFlags::Write);
		cmakeListsFile.Write(cmakeListsContents);
		return true;
	}

	bool GenerateSolution(
		const Platform::Type platform,
		const IO::PathView sourceDirectory,
		const IO::PathView buildDirectory,
		const IO::PathView engineCodeDirectory,
		const IO::PathView binaryDirectory,
		const IO::PathView libraryDirectory,
		const bool includeEngineSource
	)
	{
		const IO::Path cmakeExecutablePath(MAKE_PATH("cmake"));

		const IO::PathView generator(GetGenerator(platform));
		const IO::PathView systemName(GetSystemName(platform));
		const IO::PathView additionalOptions = GetAdditionalCMakeOptions(platform);

		NativeString commandLine(String().Format(
			"{0} -G \"{1}\" -S \"{2}\" -B \"{3}\" -D ENGINE_CODE_DIRECTORY_IN=\"{4}\" -D BINARY_DIRECTORY=\"{5}\" -D LIBRARY_DIRECTORY=\"{6}\" "
			"-D "
			"OPTION_BUILD_ENGINE={7} -DCMAKE_SYSTEM_NAME=\"{8}\" {9}",
			cmakeExecutablePath,
			generator,
			sourceDirectory,
			buildDirectory,
			engineCodeDirectory,
			binaryDirectory,
			libraryDirectory,
			includeEngineSource ? "ON" : "OFF",
			systemName,
			additionalOptions
		));
		return Platform::StartProcess(cmakeExecutablePath, commandLine);
	}

	bool GenerateEngineSolution(const EngineInfo& engineInfo, const Platform::Type platform)
	{
		const IO::PathView engineRootDirectory = engineInfo.GetDirectory();

		const IO::Path sourceCodeDirectory = IO::Path::Combine(engineRootDirectory, engineInfo.GetRelativeSourceDirectory());
		const IO::Path solutionDirectory =
			IO::Path::Combine(sourceCodeDirectory, IO::Path::Merge(MAKE_PATH("Intermediate"), Platform::GetName(platform)));

		const IO::Path binaryDirectory = IO::Path::Combine(engineRootDirectory, engineInfo.GetRelativeBinaryDirectory());
		const IO::Path libraryDirectory = IO::Path::Combine(engineRootDirectory, engineInfo.GetRelativeLibraryDirectory());

		return GenerateSolution(platform, sourceCodeDirectory, solutionDirectory, sourceCodeDirectory, binaryDirectory, libraryDirectory, true);
	}

	bool GenerateProjectSolution(
		const ProjectInfo& projectInfo, const EngineInfo& engineInfo, const bool includeEngineSource, const Platform::Type platform
	)
	{
		const IO::PathView projectDirectoryPath = projectInfo.GetDirectory();

		const IO::Path codeDirectory = IO::Path::Combine(projectDirectoryPath, MAKE_PATH("Code"));
		const IO::Path solutionDirectory =
			IO::Path::Combine(projectDirectoryPath, IO::Path::Merge(MAKE_PATH("Intermediate"), Platform::GetName(platform)));

		Assert(engineInfo.IsValid());
		const IO::PathView engineFolder = engineInfo.GetDirectory();
		const IO::Path engineCodeFolder = IO::Path::Combine(engineFolder, engineInfo.GetRelativeSourceDirectory());

		if (!IO::Path::Combine(codeDirectory, MAKE_PATH("CMakeLists.txt")).Exists())
		{
			if (!GenerateProjectCMakeLists(projectInfo, engineInfo))
			{
				return false;
			}
		}

		const IO::Path binaryDirectory = IO::Path::Combine(projectDirectoryPath, projectInfo.GetRelativeBinaryDirectory());
		const IO::Path libraryDirectory = IO::Path::Combine(projectDirectoryPath, projectInfo.GetRelativeLibraryDirectory());

		return GenerateSolution(
			platform,
			codeDirectory,
			solutionDirectory,
			engineCodeFolder,
			binaryDirectory,
			libraryDirectory,
			includeEngineSource
		);
	}

	bool GeneratePluginCMakeLists(const PluginInfo& pluginDefinition)
	{
		const IO::Path engineFolder = IO::Path::Combine(IO::Path::GetExecutableDirectory(), MAKE_PATH(".."), MAKE_PATH(".."));

		const IO::Path sourceCodeDirectory = pluginDefinition.GetSourceDirectory();
		const IO::Path cmakeListsFilePath = IO::Path::Combine(sourceCodeDirectory, MAKE_PATH("CMakeLists.txt"));

		const IO::PathView pluginName = pluginDefinition.GetConfigFilePath().GetFileNameWithoutExtensions();

		IO::Path libraryDirectory(pluginDefinition.GetRelativeLibraryDirectory());
		libraryDirectory.MakeForwardSlashes();
		IO::Path binaryDirectory(pluginDefinition.GetRelativeBinaryDirectory());
		binaryDirectory.MakeForwardSlashes();

		IO::Path relativeConfigFilePath = pluginDefinition.GetConfigFilePath();
		relativeConfigFilePath.MakeRelativeTo(sourceCodeDirectory);

		String cmakeListsContents;
		cmakeListsContents.Format(
			R"(cmake_minimum_required (VERSION 3.14)

include("${{ENGINE_CMAKE_DIRECTORY}}/MakePlugin.cmake")

MakePlugin({0} ${{CMAKE_CURRENT_LIST_DIR}} "{1}" "{2}" "{3}" "{4}")
)",
			pluginDefinition.GetGuid().ToString(),
			pluginName,
			libraryDirectory,
			binaryDirectory,
			relativeConfigFilePath
		);

		for (const Guid pluginDependencyGuid : pluginDefinition.GetDependencies())
		{
			cmakeListsContents += String().Format("LinkPlugin({0} {1})", pluginDefinition.GetGuid().ToString(), pluginDependencyGuid);
		}

		const IO::File cmakeListsFile(cmakeListsFilePath, IO::AccessModeFlags::Write);
		cmakeListsFile.Write(cmakeListsContents.GetView());
		return true;
	}

	bool GeneratePluginSolution(const PluginInfo& pluginDefinition, const EngineInfo& engineInfo, const Platform::Type platform)
	{
		const IO::Path sourceCodeDirectory = pluginDefinition.GetSourceDirectory();
		if (!sourceCodeDirectory.Exists())
		{
			sourceCodeDirectory.CreateDirectories();
		}

		if (!IO::Path::Combine(sourceCodeDirectory, MAKE_PATH("CMakeLists.txt")).Exists())
		{
			if (!GeneratePluginCMakeLists(pluginDefinition))
			{
				return false;
			}
		}

		Assert(engineInfo.IsValid());
		const IO::Path engineCodeFolder = IO::Path::Combine(engineInfo.GetDirectory(), engineInfo.GetRelativeSourceDirectory());

		const IO::Path solutionDirectory =
			IO::Path::Combine(sourceCodeDirectory, IO::Path::Merge(MAKE_PATH("Intermediate"), Platform::GetName(platform)));

		const IO::Path binaryDirectory = pluginDefinition.GetBinaryDirectory();
		const IO::Path libraryDirectory = pluginDefinition.GetLibraryDirectory();

		return GenerateSolution(platform, sourceCodeDirectory, solutionDirectory, engineCodeFolder, binaryDirectory, libraryDirectory, false);
	}
}
#endif
