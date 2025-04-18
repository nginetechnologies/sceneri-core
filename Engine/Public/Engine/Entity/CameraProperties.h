#pragma once

#include <Common/Math/Angle.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Reflection/Type.h>

namespace ngine::Entity
{
	struct CameraProperties
	{
		Math::Anglef m_fieldOfView = 60.0_degrees;
		Math::ClampedValuef m_nearPlane = {0.1f, 0.0001f, 4096};
		Math::ClampedValuef m_farPlane = {128.f, 1.f, 65536};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::CameraProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::CameraProperties>(
			"{9AE0E145-86B4-4E47-AFDA-697BBD008922}"_guid,
			MAKE_UNICODE_LITERAL("Camera Properties"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Field of View"),
					"fieldOfView",
					"{6EA8A41E-DAC7-445B-8A24-02D6575B586F}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraProperties::m_fieldOfView
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Near Plane"),
					"nearPlane",
					"{FC43D5B6-2E4C-4986-AFCE-8409CD32FC0A}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraProperties::m_nearPlane
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Far Plane"),
					"farPlane",
					"{244DC701-4E83-4234-B923-6306DDD60CC0}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraProperties::m_farPlane
				}
			}
		);
	};
}
