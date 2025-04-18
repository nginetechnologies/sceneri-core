#include <Common/Memory/New.h>

#include <Common/Tests/UnitTest.h>
#include <Common/Version.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/EnumFlags.h>

#include <Renderer/Instance.h>

namespace ngine::Rendering::Tests
{
	UNIT_TEST(Instance, Instance)
	{
		Instance instance("Sceneri unit test app", Version(1, 0, 0), "Sceneri unit test engine", Version(1, 0, 0), Instance::CreationFlags{});
		EXPECT_TRUE(instance.IsValid());

		const InstanceView instanceView = instance;
		EXPECT_TRUE(instanceView.IsValid());
	}
}
