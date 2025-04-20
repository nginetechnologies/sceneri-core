#include <Common/Tests/UnitTest.h>

#include <Engine/EngineSystems.h>

#include <Common/CommandLine/CommandLineInitializationParameters.h>

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters&)
{
	return {};
}

int __cdecl main(int argc, char** argv)
{
	::testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
