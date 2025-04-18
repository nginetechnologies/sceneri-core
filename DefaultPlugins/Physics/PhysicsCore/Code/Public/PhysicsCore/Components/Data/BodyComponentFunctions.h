#pragma once

#include "PhysicsCore/Components/Data/BodyComponent.h"

#include <Engine/Entity/Component3D.h>

#include <Common/Math/Vector3.h>

namespace ngine::Physics
{
	void AddForce(Entity::Component3D& component, const Math::Vector3f force);
	void AddForceAtLocation(Entity::Component3D& component, const Math::Vector3f force, const Math::WorldCoordinate location);
	void AddImpulse(Entity::Component3D& component, const Math::Vector3f force);
	void AddImpulseAtLocation(Entity::Component3D& component, const Math::Vector3f force, const Math::WorldCoordinate location);
	void AddTorque(Entity::Component3D& component, const Math::Vector3f torque);
	void AddAngularImpulse(Entity::Component3D& component, const Math::Vector3f angularVelocity);
	void SetVelocity(Entity::Component3D& component, const Math::Vector3f velocity);
	[[nodiscard]] Math::Vector3f GetVelocity(Entity::Component3D& component);
	[[nodiscard]] Math::Vector3f GetVelocityAtPoint(Entity::Component3D& component, const Math::WorldCoordinate coordinate);
	void SetAngularVelocity(Entity::Component3D& component, const Math::Angle3f angularVelocity);
	[[nodiscard]] Math::Angle3f GetAngularVelocity(Entity::Component3D& component);
	void Wake(Entity::Component3D& component);
	void Sleep(Entity::Component3D& component);
	[[nodiscard]] bool IsAwake(Entity::Component3D& component);
	[[nodiscard]] bool IsSleeping(Entity::Component3D& component);
	[[nodiscard]] TypeTraits::MemberType<decltype(&Physics::Data::Body::OnContactFound)>*
	GetOnContactFoundEvent(Entity::Component3D& component);
	[[nodiscard]] TypeTraits::MemberType<decltype(&Physics::Data::Body::OnContactFound)>* GetOnContactLostEvent(Entity::Component3D& component
	);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Physics::AddForce>
	{
		static constexpr auto Function = Reflection::Function{
			"435abf8a-25bf-4c44-a715-5578a919e974"_guid,
			MAKE_UNICODE_LITERAL("Add Force"),
			&Physics::AddForce,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"bd6904c9-5e60-4d9f-bbbc-fe7507e05e5c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"946772ca-6c08-4701-8a37-7326120d89ee"_guid, MAKE_UNICODE_LITERAL("Force")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::AddForceAtLocation>
	{
		static constexpr auto Function = Reflection::Function{
			"e245a963-776f-4a7f-9ef1-ae43c9240018"_guid,
			MAKE_UNICODE_LITERAL("Add Force At Location"),
			&Physics::AddForceAtLocation,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"7ddc01d9-136e-4fc9-ace6-92709aa6e3ae"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"0672778a-b6c2-40b1-92d8-9f9b8d472609"_guid, MAKE_UNICODE_LITERAL("Force")},
			Reflection::Argument{"bc83cb40-97cd-4a9f-b7d7-2c7e7d7070f4"_guid, MAKE_UNICODE_LITERAL("Location")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::AddImpulse>
	{
		static constexpr auto Function = Reflection::Function{
			"6fccfd67-8090-4ce5-bd2a-1b55ec77381d"_guid,
			MAKE_UNICODE_LITERAL("Add Impulse"),
			&Physics::AddImpulse,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"250f8b07-c138-418f-a4b3-52bd73a01bd2"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"da670a10-3a3c-41c4-81dd-5e510ae23c3a"_guid, MAKE_UNICODE_LITERAL("Force")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::AddImpulseAtLocation>
	{
		static constexpr auto Function = Reflection::Function{
			"65c1e757-1a5c-4ae4-8dd7-124a2e279ac7"_guid,
			MAKE_UNICODE_LITERAL("Add Impulse At Location"),
			&Physics::AddImpulseAtLocation,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"44d432fb-596f-4c3d-8fc6-1b36b1d8d272"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"20d3a582-c468-49d1-9f71-617e5bbb52fc"_guid, MAKE_UNICODE_LITERAL("Force")},
			Reflection::Argument{"2cfddf53-5711-41c3-9cdd-08652a28f1e3"_guid, MAKE_UNICODE_LITERAL("Location")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::AddTorque>
	{
		static constexpr auto Function = Reflection::Function{
			"8c5e0c98-5e31-4ae5-8181-42afb78d956f"_guid,
			MAKE_UNICODE_LITERAL("Add Torque"),
			&Physics::AddTorque,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"711c555c-e4c2-4fdf-b861-4323fc3451fc"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bbe53066-c562-4c55-bca9-4c4c7baa435f"_guid, MAKE_UNICODE_LITERAL("Torque")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::AddAngularImpulse>
	{
		static constexpr auto Function = Reflection::Function{
			"6104e068-70fd-4823-8173-4d08b37b2b62"_guid,
			MAKE_UNICODE_LITERAL("Add Angular Impulse"),
			&Physics::AddTorque,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"31cfc9f4-d3fb-4264-b907-6cb23fc1e14a"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"bbe53066-c562-4c55-bca9-4c4c7baa435f"_guid, MAKE_UNICODE_LITERAL("Angular Impulse")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::SetVelocity>
	{
		static constexpr auto Function = Reflection::Function{
			"727f4def-be98-4c7e-b946-290b0167f1a0"_guid,
			MAKE_UNICODE_LITERAL("Set Velocity"),
			&Physics::SetVelocity,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"aedd720d-2576-41dc-a339-a1b9bb0ad012"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"39682578-25e5-4f78-9972-ea8f7f5984a5"_guid, MAKE_UNICODE_LITERAL("Velocity")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::GetVelocity>
	{
		static constexpr auto Function = Reflection::Function{
			"4b0a01a1-bcfc-450f-8bca-9106451084e5"_guid,
			MAKE_UNICODE_LITERAL("Get Velocity"),
			&Physics::GetVelocity,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"3dd7d5bb-c65d-4c54-a932-34b90cf9561f"_guid, MAKE_UNICODE_LITERAL("Velocity")},
			Reflection::Argument{
				"0390371d-c597-428b-9d51-f1e14cc7934c"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::GetVelocityAtPoint>
	{
		static constexpr auto Function = Reflection::Function{
			"6c4e2346-a5b7-4e8d-b99c-a40443450f31"_guid,
			MAKE_UNICODE_LITERAL("Get Velocity At Point"),
			&Physics::GetVelocityAtPoint,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"52167e11-8554-4332-9877-22c01caa6689"_guid, MAKE_UNICODE_LITERAL("Velocity")},
			Reflection::Argument{
				"08f3b5c0-b6c2-4e18-bca6-58984c3621e6"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"ed925e77-85ee-4f3e-a31e-4e50cbc0a85b"_guid, MAKE_UNICODE_LITERAL("Point")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::SetAngularVelocity>
	{
		static constexpr auto Function = Reflection::Function{
			"ef4383fd-c0a6-4e84-a18f-6f3109ac994b"_guid,
			MAKE_UNICODE_LITERAL("Set Angular Velocity"),
			&Physics::SetAngularVelocity,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"c326924d-47f1-4f43-8c16-40b5da9b6c79"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			},
			Reflection::Argument{"1bc2e3fa-989d-4c61-b664-8e44a6237229"_guid, MAKE_UNICODE_LITERAL("Angular Velocity")}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::GetAngularVelocity>
	{
		static constexpr auto Function = Reflection::Function{
			"7ea72c33-fbae-4304-a1b3-3d70be23dec0"_guid,
			MAKE_UNICODE_LITERAL("Get Angular Velocity"),
			&Physics::GetAngularVelocity,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"d6988579-40d6-4225-9da5-170dd4b3705d"_guid, MAKE_UNICODE_LITERAL("Angular Velocity")},
			Reflection::Argument{
				"0ec6c800-9835-455c-a140-4394f6fe88c7"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::Wake>
	{
		static constexpr auto Function = Reflection::Function{
			"99d44bdf-8db9-4316-b369-8c2b3121e824"_guid,
			MAKE_UNICODE_LITERAL("Wake"),
			&Physics::Wake,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"9a2ba6b2-c9dc-4155-8912-1f6f1a3bad77"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::Sleep>
	{
		static constexpr auto Function = Reflection::Function{
			"b1be0d76-f698-4fa6-816f-8bb18938c828"_guid,
			MAKE_UNICODE_LITERAL("Sleep"),
			&Physics::Sleep,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{},
			Reflection::Argument{
				"db6725de-a99a-4dcc-ba61-78232ea227ab"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::IsAwake>
	{
		static constexpr auto Function = Reflection::Function{
			"824dc030-885c-4427-aa43-12a045406d9c"_guid,
			MAKE_UNICODE_LITERAL("Is Awake"),
			&Physics::IsAwake,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"b818a9e9-a0fa-4491-a6a0-10ba0b9cf20e"_guid, MAKE_UNICODE_LITERAL("Result")},
			Reflection::Argument{
				"d9c66ae1-adab-458a-b2bb-888c944c93b3"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedFunction<&Physics::IsSleeping>
	{
		static constexpr auto Function = Reflection::Function{
			"95a5a102-ce5b-4429-8fec-ad7f4a3fc10b"_guid,
			MAKE_UNICODE_LITERAL("Is Sleeping"),
			&Physics::IsSleeping,
			Reflection::FunctionFlags::VisibleToUI,
			Reflection::Argument{"e76fc8d0-ba50-4f40-a647-820d494ebd8b"_guid, MAKE_UNICODE_LITERAL("Result")},
			Reflection::Argument{
				"9bfd8d82-bbed-4f97-b568-4a18a11fbc97"_guid, MAKE_UNICODE_LITERAL("Component"), Reflection::ArgumentFlags::IsSelf
			}
		};
	};

	template<>
	struct ReflectedEvent<&Physics::GetOnContactFoundEvent>
	{
		static constexpr auto Event = Reflection::Event{
			"bdf8eb1b-b0ea-427b-baf5-f54f2aa61f9c"_guid,
			MAKE_UNICODE_LITERAL("Contact Begin"),
			&Physics::GetOnContactFoundEvent,
			Argument{"feb392e4-44f3-474d-a8f2-6c2be535879f"_guid, MAKE_UNICODE_LITERAL("Contact")}
		};
	};

	template<>
	struct ReflectedEvent<&Physics::GetOnContactLostEvent>
	{
		static constexpr auto Event = Reflection::Event{
			"8db976c2-8b19-4b52-8566-d79d8b6e43a7"_guid,
			MAKE_UNICODE_LITERAL("Contact End"),
			&Physics::GetOnContactLostEvent,
			Argument{"bd36c62d-63c2-4bef-8676-0770b06ecf94"_guid, MAKE_UNICODE_LITERAL("Contact")}
		};
	};
}
