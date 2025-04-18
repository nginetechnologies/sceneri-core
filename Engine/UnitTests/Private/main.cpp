#include "gtest/gtest.h"

#include <Common/Memory/UniquePtr.h>

#include <Engine/EngineSystems.h>

namespace ngine
{
	namespace CommandLine
	{
		struct InitializationParameters;
	}
}

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters&)
{
	return {};
}

int __cdecl main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
