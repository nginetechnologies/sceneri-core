#pragma once

namespace ngine::Physics
{
	enum class BodyFlags : uint8
	{
		//! Sensor body is used to only detect collisions, turns off collision response
		IsSensorOnly = 1 << 0,
		//! Prevents body from going inactive/to sleep
		KeepAwake = 1 << 1,
		//! If a body has a mass override
		HasOverriddenMass = 1 << 2,
		//! Prevents the body from being rotated by physical forces
		DisableRotation = 1 << 3,
		//! Whether the body should be updated with linear casting, casting from start to destination to avoid tunneling
		LinearCast = 1 << 4
	};
	ENUM_FLAG_OPERATORS(BodyFlags);
}
