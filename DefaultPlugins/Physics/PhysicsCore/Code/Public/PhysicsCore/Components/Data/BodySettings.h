#pragma once

#include <Common/Math/Mass.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Math/Angle.h>

#include <PhysicsCore/Layer.h>
#include <PhysicsCore/Components/Data/BodyType.h>

namespace ngine::Physics
{
	struct BodySettings
	{
		BodySettings(
			const BodyType type = BodyType::Static,
			const Layer layer = Layer::Static,
			const Math::Anglef maximumAngularVelocity = DefaultMaximumAngularVelocity,
			const Math::Massf overriddenMass = 0_kilograms,
			const Math::Ratiof gravityScale = 100_percent,
			const EnumFlags<BodyFlags> flags = {}
		)
			: m_type(type)
			, m_layer(layer)
			, m_maximumAngularVelocity(maximumAngularVelocity)
			, m_overriddenMass(overriddenMass)
			, m_gravityScale(gravityScale)
			, m_flags(flags | BodyFlags::HasOverriddenMass * (overriddenMass > 0_grams))
		{
		}

		//! Body type, determines if body should react to external forces or not
		BodyType m_type = BodyType::Static;
		//! Layer is used to filter collisions between different object
		Layer m_layer = Layer::Static;

		inline static constexpr Math::Anglef DefaultMaximumAngularVelocity = Math::PI * 60.0f * 0.25f;
		//! Maximum angular velocity that this body can reach
		Math::Anglef m_maximumAngularVelocity = DefaultMaximumAngularVelocity;
		//! Mass override in kilograms
		Math::Massf m_overriddenMass;
		//! Gravity scale
		Math::Ratiof m_gravityScale{100_percent};
		EnumFlags<BodyFlags> m_flags;
	};
}
