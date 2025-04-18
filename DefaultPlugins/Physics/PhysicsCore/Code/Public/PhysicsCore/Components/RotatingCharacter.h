#pragma once

#include "BodyComponent.h"

#include <Common/Memory/UniqueRef.h>
#include <Common/Asset/Picker.h>
#include <Common/Reflection/CoreTypes.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>
#include <PhysicsCore/Components/CharacterBase.h>

namespace JPH
{
	class PhysicsMaterial;
}

namespace ngine::Physics
{
	struct Material;
	struct ColliderComponent;

	namespace Data
	{
		struct Scene3D;
		struct Body;
	}

	struct RotatingCharacter : public CharacterBase
	{
		static constexpr Guid TypeGuid = "f8ad93e2-e8d0-4d15-b2a7-371e57a9bb67"_guid;
		using BaseType = CharacterBase;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		RotatingCharacter(Initializer&& initializer);
		RotatingCharacter(const RotatingCharacter& templateComponent, const Cloner& cloner);
		RotatingCharacter(const Deserializer& deserializer);
		virtual ~RotatingCharacter();

		void FixedPhysicsUpdate();
		void AfterPhysicsUpdate();
		void OnCreated();
		void OnEnable();
		void OnDisable();

		[[nodiscard]] virtual GroundState GetGroundState() const override;
		[[nodiscard]] virtual Math::Vector3f GetGroundNormal() const override;
	protected:
		friend struct Reflection::ReflectedType<RotatingCharacter>;
		friend struct Data::Scene;
	protected:
		// Amount of in air controls
		float m_inAirControl = 0.05f;

		// Minimum distance to ground to be considered "on ground" state
		Math::Lengthf m_minimumGroundDistance = 0.5_meters;
	private:
		GroundState m_groundState = GroundState::OnGround;
		Math::Vector3f m_groundNormal = Math::Zero;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::RotatingCharacter>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::RotatingCharacter>(
			Physics::RotatingCharacter::TypeGuid,
			MAKE_UNICODE_LITERAL("Rotating Character"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("In Air Control Amount"),
					"inAirControl",
					"{F45FC00E-D8FD-4FEE-AEBE-5D4DBBB7A8B8}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Character"),
					&Physics::RotatingCharacter::m_inAirControl
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Minimum Ground Distance"),
					"minGroundDistance",
					"{E8540A8B-37F4-4F45-8BB1-84CC57A218D2}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Character"),
					&Physics::RotatingCharacter::m_minimumGroundDistance
				)
			}
		);
	};
}
