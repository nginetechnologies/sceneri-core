#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Math/Log2.h>

namespace ngine::Input
{
	namespace GamepadInput
	{
		enum class Button : uint32
		{
			Invalid,

			// A on Apple
			// Cross for PlayStation
			A = 1 << 0,                  // 1
			                             // B on Apple
			                             // Circle on PlayStation
			B = 1 << 1,                  // 2
			                             // X on Apple
			                             // Square on PlayStation
			X = 1 << 2,                  // 4
			                             // Y on Apple
			                             // Triangle on PlayStation
			Y = 1 << 3,                  // 8
			LeftShoulder = 1 << 4,       // 16
			RightShoulder = 1 << 5,      // 32
			LeftThumbstick = 1 << 6,     // 64
			RightThumbstick = 1 << 7,    // 128
			                             // Menu on Apple
			                             // Options on PlayStation
			Menu = 1 << 8,               // 256
			                             // Home on Apple
			                             // PS / Home button on PlayStation
			Home = 1 << 9,               // 512
			                             // Options on Apple
			                             // Share on PlayStation
			Options = 1 << 10,           // 1024
			Share = 1 << 11,             // 2048
			DirectionPadLeft = 1 << 12,  // 4096
			DirectionPadRight = 1 << 13, // 8192
			DirectionPadUp = 1 << 14,    // 16384
			DirectionPadDown = 1 << 15,  // 32768
			Touchpad = 1 << 16,          // 65536
			PaddleOne = 1 << 17,         // 131072
			PaddleTwo = 1 << 18,         // 262144
			PaddleThree = 1 << 19,       // 524288
			PaddleFour = 1 << 20,        // 1048576
			Last = PaddleFour,
			Count = Math::Log2((uint32)Last) + 1,
		};

		ENUM_FLAG_OPERATORS(Button);

		enum class Axis : uint8
		{
			LeftThumbstick,  // 0
			RightThumbstick, // 1
			Count,
		};

		enum class Analog : uint8
		{
			LeftTrigger,  // 0
			RightTrigger, // 1
			Count,
		};
	}
}
