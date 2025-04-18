#pragma once

#include "BodyComponent.h"

#include <Common/Memory/UniqueRef.h>
#include <Common/Asset/Picker.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>

namespace ngine::Physics
{
	struct ColliderComponent;

	namespace Data
	{
		struct Scene3D;
		struct Body;
	}

	struct CharacterBase : public BodyComponent
	{
		static constexpr Guid TypeGuid = "a413cecf-a566-413d-bb01-bd1317cd4bb2"_guid;

		using BaseType = BodyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		struct Initializer : public BodyComponent::Initializer
		{
			using BaseType = BodyComponent::Initializer;

			Initializer(BaseType&& initializer)
				: BaseType(Forward<BaseType>(initializer))
			{
			}
			Initializer(DynamicInitializer&& initializer)
				: BaseType{Forward<DynamicInitializer>(initializer), BodySettings{BodyType::Dynamic, Layer::Dynamic}}
			{
			}
		};

		CharacterBase(Initializer&& initializer);
		CharacterBase(const CharacterBase& templateComponent, const Cloner& cloner);
		CharacterBase(const Deserializer& deserializer, Settings&& defaultSettings = {});
		virtual ~CharacterBase();

		virtual void AddImpulse(const Math::Vector3f impulse)
		{
			m_movementRequest.impulse += impulse;
		}

		// TODO: Naming
		virtual void SetVelocity(const Math::Vector3f velocity)
		{
			m_movementRequest.velocity = velocity;
		}

		virtual Math::Vector3f GetLinearVelocity();

		virtual void Jump(const Math::Vector3f acceleration);

		enum class GroundState : uint8
		{
			OnGround,
			Sliding,
			InAir
		};

		[[nodiscard]] virtual GroundState GetGroundState() const
		{
			return GroundState::OnGround;
		}

		[[nodiscard]] bool IsOnGround() const
		{
			return GetGroundState() == GroundState::OnGround;
		}
		[[nodiscard]] bool IsSliding() const
		{
			return GetGroundState() == GroundState::Sliding;
		}
		[[nodiscard]] bool IsInAir() const
		{
			return GetGroundState() == GroundState::InAir;
		}

		[[nodiscard]] virtual Math::Vector3f GetGroundNormal() const
		{
			return Math::Up;
		}

		[[nodiscard]] virtual Math::WorldCoordinate GetFootLocation() const;

		[[nodiscard]] virtual Optional<Entity::Component3D*> GetGroundComponent() const;
	protected:
		friend struct Reflection::ReflectedType<CharacterBase>;
		friend struct Data::Scene;
	protected:
		struct MovementRequest
		{
			Math::Vector3f impulse = Math::Zero;
			Math::Vector3f velocity = Math::Zero;
		};
		MovementRequest m_movementRequest;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::CharacterBase>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::CharacterBase>(
			Physics::CharacterBase::TypeGuid,
			MAKE_UNICODE_LITERAL("Character Physics Base"),
			Reflection::TypeFlags::IsAbstract,
			Reflection::Tags{}
		);
	};
}
