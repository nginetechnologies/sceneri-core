#include <Common/Memory/New.h>

#include <Common/Tests/UnitTest.h>

#include <Common/Scripting/VirtualMachine/DynamicFunction/NativeFunction.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicEvent.h>

#include <Common/Math/Vector3.h>
#include <Common/Math/Vector4.h>

namespace ngine::Tests
{
	UNIT_TEST(Register, Compare)
	{
		using namespace Scripting::VM;
		const Register a = DynamicInvoke::LoadArgumentZeroed(Math::Vector4f{1, 2, 3, 4});
		const Register b = DynamicInvoke::LoadArgumentZeroed(Math::Vector4f{1, 2, 3, 4});
		const Register c = DynamicInvoke::LoadArgumentZeroed(Math::Vector4f{0, 2, 3, 4});
		EXPECT_TRUE(DynamicEvent::CompareRegisters(a, b));
		EXPECT_FALSE(DynamicEvent::CompareRegisters(a, c));
	}

	UNIT_TEST(DynamicFunction, BindAndCall)
	{
		static constexpr auto testNode = +[](const Math::Vector3f a, const Math::Vector3f b, Math::Vector3f c) -> Math::Vector3f
		{
			return a + b + c;
		};

		Scripting::VM::NativeFunction<Math::Vector3f(Math::Vector3f, Math::Vector3f, Math::Vector3f)> dynamicFunction;
		EXPECT_FALSE(dynamicFunction.IsValid());
		dynamicFunction = decltype(dynamicFunction)::Make<testNode>();
		EXPECT_TRUE(dynamicFunction.IsValid());
		const Math::Vector3f returnValue = dynamicFunction(Math::Vector3f{1, 2, 3}, Math::Vector3f{0, 4, 6}, Math::Vector3f{3, 2, 1});
		EXPECT_TRUE(returnValue == Math::Vector3f(4, 8, 10));
	}

	UNIT_TEST(DynamicEvent, BindAndUnbindLambda)
	{
		struct Foo
		{
			void Test()
			{
			}
		};
		{
			Foo foo;

			Scripting::VM::DynamicEvent dynamicEvent;
			dynamicEvent.Emplace(Scripting::VM::DynamicDelegate::Make<&Foo::Test>(foo));
			EXPECT_TRUE(dynamicEvent.Remove(&foo));
		}
		{
			Foo foo;

			Scripting::VM::DynamicEvent dynamicEvent;
			dynamicEvent.Emplace(Scripting::VM::DynamicDelegate::Make<&Foo::Test>(foo));
			EXPECT_TRUE(dynamicEvent.Remove(foo));
		}
	}

	UNIT_TEST(DynamicEvent, BindAndNotifyMember)
	{
		Scripting::VM::DynamicEvent dynamicEvent;
		EXPECT_FALSE(dynamicEvent.HasCallbacks());
		// Make sure we can still call an empty event
		dynamicEvent(Math::Vector3f{2, 4, 5}, Math::Vector3f{3, 6, 7});

		struct Foo
		{
			void Member(const Math::Vector3f a, const Math::Vector3f b)
			{
				static uint8 counter = 0;
				switch (counter++)
				{
					case 0:
						EXPECT_TRUE((a == Math::Vector3f{4, 3, 1}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{1, 3, 5}).AreAllSet());
						break;
					case 1:
						EXPECT_TRUE((a == Math::Vector3f{6, 2, 5}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{3, 7, 8}).AreAllSet());
						break;
					default:
						// Should never be reached
						EXPECT_TRUE(false);
				}
			}
		};

		Foo foo;
		dynamicEvent.Emplace(Scripting::VM::DynamicDelegate::Make<&Foo::Member>(foo));
		EXPECT_TRUE(dynamicEvent.HasCallbacks());
		EXPECT_TRUE(dynamicEvent.Contains(foo));

		dynamicEvent(Math::Vector3f{4, 3, 1}, Math::Vector3f{1, 3, 5});
		dynamicEvent(Math::Vector3f{6, 2, 5}, Math::Vector3f{3, 7, 8});

		const bool wasRemoved = dynamicEvent.Remove(foo);
		EXPECT_TRUE(wasRemoved);

		EXPECT_FALSE(dynamicEvent.HasCallbacks());
	}

	UNIT_TEST(DynamicEvent, BindAndNotifyLambda)
	{
		Scripting::VM::DynamicEvent dynamicEvent;
		EXPECT_FALSE(dynamicEvent.HasCallbacks());
		// Make sure we can still call an empty event
		dynamicEvent(Math::Vector3f{2, 4, 5}, Math::Vector3f{3, 6, 7});

		void* identifier = reinterpret_cast<void*>(0xDEDEDE);
		int capturedValue = 1337;
		dynamicEvent.Emplace(Scripting::VM::DynamicDelegate::Make(
			identifier,
			[capturedValue](const Math::Vector3f a, const Math::Vector3f b)
			{
				EXPECT_EQ(capturedValue, 1337);

				static uint8 counter = 0;
				switch (counter++)
				{
					case 0:
						EXPECT_TRUE((a == Math::Vector3f{4, 3, 1}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{1, 3, 5}).AreAllSet());
						break;
					case 1:
						EXPECT_TRUE((a == Math::Vector3f{6, 2, 5}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{3, 7, 8}).AreAllSet());
						break;
					default:
						// Should never be reached
						EXPECT_TRUE(false);
				}
			}
		));
		EXPECT_TRUE(dynamicEvent.HasCallbacks());
		EXPECT_TRUE(dynamicEvent.Contains(identifier));

		dynamicEvent(Math::Vector3f{4, 3, 1}, Math::Vector3f{1, 3, 5});
		dynamicEvent(Math::Vector3f{6, 2, 5}, Math::Vector3f{3, 7, 8});

		const bool wasRemoved = dynamicEvent.Remove(identifier);
		EXPECT_TRUE(wasRemoved);

		EXPECT_FALSE(dynamicEvent.HasCallbacks());
	}
}
