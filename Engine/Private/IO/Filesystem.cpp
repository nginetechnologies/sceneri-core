#include "IO/Filesystem.h"

#include <Engine/Engine.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/Assert/Assert.h>
#include <Common/IO/File.h>
#include <Common/IO/FileIterator.h>
#include <Common/IO/Library.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Project System/FindEngine.h>
#include <Common/Memory/Containers/Format/StringView.h>

#include <Common/System/Query.h>

namespace ngine::IO
{
	Filesystem::Filesystem()
		: m_executablePath(IO::Path::GetExecutablePath())
	{
		FindEngineRootFolder();

		IO::Path::InitializeDataDirectories();

		System::Get<Log>().OnInitialized.Add(
			*this,
			[](Filesystem& filesystem, Log& log)
			{
				log.Message("Starting {}", System::Get<Engine>().GetInfo().GetName());
				log.Message("Application executable: {}", filesystem.m_executablePath);
				log.Message("Engine folder: {}", filesystem.m_enginePath);
				log.Message("");
			}
		);
	}

	Filesystem::~Filesystem()
	{
		System::Get<Log>().OnInitialized.Remove(this);
	}

	void Filesystem::FindEngineRootFolder()
	{
		if (System::Get<Engine>().GetCommandLineArguments().HasArgument(MAKE_NATIVE_LITERAL("engine"), CommandLine::Prefix::Minus))
		{
			m_enginePath = IO::Path(
				System::Get<Engine>().GetCommandLineArguments().GetArgumentValue(MAKE_NATIVE_LITERAL("engine"), CommandLine::Prefix::Minus)
			);
			m_enginePath = IO::Path(m_enginePath.GetParentPath());
			return;
		}

#if !PLATFORM_ANDROID
		m_enginePath = IO::Path(ProjectSystem::FindEngineDirectoryFromExecutableFolder(IO::Path::GetExecutableDirectory()));
		IO::Path::SetWorkingDirectory(m_enginePath);
		CheckFatalError(
			SOURCE_LOCATION,
			m_enginePath.IsEmpty(),
			System::Get<Log>(),
			"Failed to locate engine configuration file with extension {} in directory tree!",
			EngineAssetFormat.metadataFileExtension
		);
#endif
	}
}
