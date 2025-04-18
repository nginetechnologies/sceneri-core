#pragma once

#include <Common/IO/Path.h>

#include <Common/System/SystemType.h>

namespace ngine
{
	struct Engine;
	struct Log;
	struct Project;
}

namespace ngine::IO
{
	struct File;
	struct Library;

	struct Filesystem final
	{
		inline static constexpr System::Type SystemType = System::Type::FileSystem;

		Filesystem();
		~Filesystem();

		[[nodiscard]] const Path& GetExecutablePath() const
		{
			return m_executablePath;
		}
		[[nodiscard]] const Path& GetEnginePath() const
		{
			return m_enginePath;
		}
	protected:
		void FindEngineRootFolder();
	protected:
		Path m_executablePath;
		Path m_enginePath;
	};
}
