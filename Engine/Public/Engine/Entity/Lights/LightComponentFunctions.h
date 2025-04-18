#pragma once

#include <Engine/Entity/Component3D.h>

#include <Common/Math/Color.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Angle.h>

namespace ngine::Entity
{
	void SetLightColor(Entity::Component3D& component, const Math::Color color);
	[[nodiscard]] Math::Color GetLightColor(Entity::Component3D& component);

	void SetLightRadius(Entity::Component3D& component, const Math::Radiusf radius);
	[[nodiscard]] Math::Radiusf GetLightRadius(Entity::Component3D& component);

	void SetLightFieldOfView(Entity::Component3D& component, const Math::Anglef angle);
	[[nodiscard]] Math::Anglef GetLightFieldOfView(Entity::Component3D& component);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Entity::SetLightColor>
	{
		static constexpr auto Function = Reflection::Function{
			"fe8d2c0c-5f8d-4ead-a031-1faa595a6de8"_guid,
			MAKE_UNICODE_LITERAL("Set Light Color"),
			&Entity::SetLightColor,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::GetLightColor>
	{
		static constexpr auto Function = Reflection::Function{
			"936fc165-25d9-4e4d-9451-ea04c41bde0b"_guid,
			MAKE_UNICODE_LITERAL("Get Light Color"),
			&Entity::GetLightColor,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Entity::SetLightRadius>
	{
		static constexpr auto Function = Reflection::Function{
			"01842bee-ab71-4888-9452-18801059f501"_guid,
			MAKE_UNICODE_LITERAL("Set Light Radius"),
			&Entity::SetLightRadius,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::GetLightRadius>
	{
		static constexpr auto Function = Reflection::Function{
			"04e09f91-7ed6-4efc-b499-d3bffd0f6174"_guid,
			MAKE_UNICODE_LITERAL("Get Light Radius"),
			&Entity::GetLightRadius,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Entity::SetLightFieldOfView>
	{
		static constexpr auto Function = Reflection::Function{
			"b1656792-85d2-460f-925a-10543277b9ef"_guid,
			MAKE_UNICODE_LITERAL("Set Light Radius"),
			&Entity::SetLightFieldOfView,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")}
		};
	};
	template<>
	struct ReflectedFunction<&Entity::GetLightFieldOfView>
	{
		static constexpr auto Function = Reflection::Function{
			"d9589403-9ab5-4227-b8c1-4f670e41851c"_guid,
			MAKE_UNICODE_LITERAL("Get Light Radius"),
			&Entity::GetLightFieldOfView,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"b0c43134-e9f1-4ad0-928d-64bcc2a35478"_guid, MAKE_UNICODE_LITERAL("Color")},
			Reflection::Argument{
				"ef1a924a-49ea-4a6e-82a5-3fbbe60497d2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};
}
