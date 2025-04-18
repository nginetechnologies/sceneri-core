#pragma once

#include "Data/Component3D.h"

namespace ngine::Entity
{
	struct CameraComponent;
	struct CameraController : public Data::Component3D
	{
		using BaseType = Component3D;
	private:
		friend CameraComponent;
		virtual void OnBecomeActive()
		{
		}
		virtual void OnBecomeInactive()
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::CameraController>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::CameraController>(
			"{885CC8B7-9DC0-4947-BB05-E409A383489D}"_guid, MAKE_UNICODE_LITERAL("Camera Controller"), Reflection::TypeFlags::IsAbstract
		);
	};
}
