#pragma once

#include "BodyComponent.h"

#include <Common/Memory/UniqueRef.h>
#include <Common/Asset/Picker.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>

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
		struct Scene;
		struct Body;
	}

	struct CharacterComponent : public BodyComponent
	{
		static constexpr Guid TypeGuid = "25e514d3-26ed-4b6d-b3e2-351e3ed403fc"_guid;

		using BaseType = BodyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = BodyComponent::Initializer;

			using BaseType::BaseType;
			Initializer(BaseType&& initializer, Optional<const Material*> pPhysicalMaterial = Invalid)
				: BaseType(Forward<BaseType>(initializer))
				, m_pPhysicalMaterial(pPhysicalMaterial)
			{
			}

			Optional<const Material*> m_pPhysicalMaterial;
		};

		CharacterComponent(Initializer&& initializer);
		CharacterComponent(const CharacterComponent& templateComponent, const Cloner& cloner);
		CharacterComponent(const Deserializer& deserializer);
		virtual ~CharacterComponent();

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		friend struct Reflection::ReflectedType<Physics::CharacterComponent>;

		void FixedPhysicsUpdate();
		void AfterPhysicsUpdate();
		void OnCreated();
		void OnEnable();
		void OnDisable();

		void AddAcceleration(const Math::Vector3f acceleration)
		{
			if (m_isOnGround)
			{
				const Math::Vector3f upVector = GetWorldUpDirection();
				const float upDot = upVector.Dot(m_requestedImpulse);
				if (upDot < 0.f)
				{
					m_requestedImpulse -= upVector * upDot;
				}
			}

			m_requestedImpulse += acceleration;
		}

		void AddImpulse(const Math::Vector3f acceleration)
		{
			m_requestedImpulse += acceleration;
		}

		void Jump(const Math::Vector3f acceleration);
		void SetVelocity(const Math::Vector3f velocity)
		{
			m_velocity = velocity;
		}

		enum class GroundState : uint8
		{
			OnGround,
			Sliding,
			InAir
		};

		[[nodiscard]] GroundState GetGroundState() const;
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

		[[nodiscard]] Math::Vector3f GetGroundLocation() const;
		[[nodiscard]] Math::Vector3f GetGroundNormal() const;
		[[nodiscard]] Math::Vector3f GetGroundVelocity() const;
		[[nodiscard]] Optional<Entity::Component3D*> GetGroundComponent() const;

		[[nodiscard]] Math::WorldCoordinate GetFootLocation() const;
	protected:
		CharacterComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);
		friend struct Reflection::ReflectedType<CharacterComponent>;
		friend struct Data::Scene;

		using PhysicalMaterialPicker = Asset::Picker;

		void SetMaterialAsset(const PhysicalMaterialPicker asset);
		PhysicalMaterialPicker GetMaterialAsset() const;
	protected:
		ReferenceWrapper<const Material> m_material;

		Math::Vector3f m_velocity = Math::Zero;
		Math::Vector3f m_requestedImpulse = Math::Zero;
		bool m_isOnGround = false;
		Math::Vector3f m_groundNormal;

		GroundState m_groundState = GroundState::OnGround;
		float m_timeInAir = 0.f;

		//! Maximum angle the character can walk on, past this they will slide
		Math::Anglef m_maximumWalkableAngle = 50_degrees;
		//! Maximum ground angle the character can be considered to stand on, past this we will consider them in air
		Math::Anglef m_maximumGroundAngle = 50_degrees;
	private:
		// Ground properties
		JPH::BodyID mGroundBodyID;
		JPH::SubShapeID mGroundBodySubShapeID;
		JPH::Vec3 mGroundPosition = JPH::Vec3::sZero();
		JPH::Vec3 mGroundNormal = JPH::Vec3::sZero();
		JPH::RefConst<JPH::PhysicsMaterial> mGroundMaterial;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::CharacterComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::CharacterComponent>(
			Physics::CharacterComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Character Physics"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Physical Material"),
				"physical_material",
				"{A792B441-2BF7-4500-8252-8F9B1C38F810}"_guid,
				MAKE_UNICODE_LITERAL("Character"),
				&Physics::CharacterComponent::SetMaterialAsset,
				&Physics::CharacterComponent::GetMaterialAsset
			)}
		);
	};
}
