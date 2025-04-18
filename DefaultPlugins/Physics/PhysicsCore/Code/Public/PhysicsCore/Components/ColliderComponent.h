#pragma once

#include <Engine/Entity/Component3D.h>

#include <Common/Asset/Picker.h>
#include <Common/Math/Mass.h>
#include <Common/Math/Density.h>
#include <Common/Memory/Variant.h>
#include <Common/Function/Event.h>

#include <PhysicsCore/Components/ColliderIdentifier.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/Shape.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/Shape/SubShapeID.h>

namespace ngine::Physics
{
	struct Material;
	struct Manager;
	struct ContactInfo;

	namespace Data
	{
		struct Scene;
		struct Body;
		struct PhysicsCommandStage;
	}

	struct ColliderComponent : public Entity::Component3D
	{
		using ShapeIndex = uint32;
		inline static constexpr ShapeIndex InvalidShapeIndex = Math::NumericLimits<ShapeIndex>::Max;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		struct Initializer : public Component3D::Initializer
		{
			using BaseType = Component3D::Initializer;
			Initializer(
				BaseType&& initializer,
				JPH::ShapeRef&& pShape,
				Optional<const Material*> pPhysicalMaterial,
				Optional<Data::Body*> pTargetBody = {},
				Optional<Entity::Component3D*> pTargetBodyOwner = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_pShape(Forward<JPH::ShapeRef>(pShape))
				, m_pPhysicalMaterial(pPhysicalMaterial)
				, m_pTargetBody(pTargetBody)
				, m_pTargetBodyOwner(pTargetBodyOwner)
			{
			}
			Initializer(
				BaseType&& initializer,
				Optional<const Material*> pPhysicalMaterial = Invalid,
				Optional<Data::Body*> pTargetBody = {},
				Optional<Entity::Component3D*> pTargetBodyOwner = {}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_pPhysicalMaterial(pPhysicalMaterial)
				, m_pTargetBody(pTargetBody)
				, m_pTargetBodyOwner(pTargetBodyOwner)
			{
			}

			JPH::ShapeRef m_pShape;
			Optional<const Material*> m_pPhysicalMaterial;
			Optional<Data::Body*> m_pTargetBody;
			Optional<Entity::Component3D*> m_pTargetBodyOwner;
		};

		struct DeserializerInfo : public Component3D::DeserializerWithBounds
		{
			using BaseType = Component3D::DeserializerWithBounds;
			DeserializerInfo(BaseType&& deserializer, JPH::ShapeRef&& pShape)
				: BaseType(Forward<BaseType>(deserializer))
				, m_pShape(Forward<JPH::ShapeRef>(pShape))
			{
			}

			[[nodiscard]] DeserializerInfo operator|(const Math::BoundingBox boundingBox) const
			{
				DeserializerInfo deserializer = *this;
				deserializer.m_localBoundingBox = boundingBox;
				return deserializer;
			}

			JPH::ShapeRef m_pShape;
		};

		struct ClonerInfo : public Component3D::Cloner
		{
			using BaseType = Component3D::Cloner;
			ClonerInfo(BaseType&& cloner, JPH::ShapeRef&& pShape)
				: BaseType(Forward<BaseType>(cloner))
				, m_pShape(Forward<JPH::ShapeRef>(pShape))
			{
			}

			JPH::ShapeRef m_pShape;
		};

		ColliderComponent(const ColliderComponent& templateComponent, ClonerInfo&& cloner);
		ColliderComponent(DeserializerInfo&& deserializer);
		ColliderComponent(Initializer&& initializer);
		virtual ~ColliderComponent();

		void OnCreated();
		void OnDestroying();
		void OnEnable();
		void OnDisable();

		void DeserializeCustomData(const Optional<Serialization::Reader> serializer);
		bool SerializeCustomData(Serialization::Writer serializer) const;

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		Event<void(void*), 24> OnShapeChanged;
	protected:
		// Component3D
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;
		virtual void OnAttachedToNewParent() override;
		virtual void OnBeforeDetachFromParent() override;
		// ~Component3D

		[[nodiscard]] DataComponentResult<Data::Body> GetNearestBody();

		/*[[nodiscard]] static uint8 GetShapeFlags(const Type type);
		[[nodiscard]] uint8 GetShapeFlags() const;*/

		[[nodiscard]] JPH::ShapeRef WrapShape(JPH::ShapeRef&& pShapeIn);
		[[nodiscard]] JPH::Shape* GetShape() const;
		void SetShape(JPH::ShapeRef&& pShape);
		[[nodiscard]] const Material& GetMaterial() const
		{
			return m_material;
		}

		void SetMassFromProperty(const Math::Massf mass);
		[[nodiscard]] Math::Massf GetMassFromProperty() const;
		void SetDensityFromProperty(const Math::Densityf mass);
		[[nodiscard]] Math::Densityf GetDensityFromProperty() const;

		[[nodiscard]] JPH::MassProperties GetMassProperties() const;
	private:
		ColliderComponent(DeserializerInfo&& deserializer, const Optional<Serialization::Reader> typeSerializer);

		[[nodiscard]] bool HasBody();

		friend struct Reflection::ReflectedType<Physics::ColliderComponent>;
		friend ContactInfo;
		friend Data::Scene;
		friend Data::Body;
		friend Data::PhysicsCommandStage;

		ReferenceWrapper<const Material> m_material;

		// TODO: How to indicate this?
		Math::Vector3f m_centerOfMassOffset = Math::Zero;

		JPH::ShapeRef m_pShape;
		JPH::BodyID m_attachedBodyIdentifier;
		// Identifier of the shape attached to the body. This should never be used outside of the physics command stage!
		uint32 m_subShapeIndex = Math::NumericLimits<uint32>::Max;

		Optional<Data::Body*> m_pAttachedBody;
		Optional<Entity::Component3D*> m_pAttachedBodyOwner;
		ColliderIdentifier m_colliderIdentifier;

		void SetMaterial(const Asset::Picker asset);
		Asset::Picker GetMaterialAsset() const;

		//! Defines how we calculate the mass and shape of this body
		//! If set ot mass, explicitly overrides mass
		//! If set to density, mass is calculated from volume
		//! If not set, automatically calculated from physical material's density and shape volume
		Variant<Math::Massf, Math::Densityf> m_massOrDensity;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::ColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::ColliderComponent>(
			"10DCEBA7-BE9F-4A9D-9902-6F51BED634AB"_guid,
			MAKE_UNICODE_LITERAL("Collider"),
			TypeFlags::IsAbstract,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Physical Material"),
					"physical_material",
					"{65DD1CA2-731B-4D0F-994E-9F9CB8191ACF}"_guid,
					MAKE_UNICODE_LITERAL("Physics Collider"),
					&Physics::ColliderComponent::SetMaterial,
					&Physics::ColliderComponent::GetMaterialAsset
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Center Of Mass Offset"),
					"comOffset",
					"{11A7F994-2C27-45B7-8653-60EE3D14ABA2}"_guid,
					MAKE_UNICODE_LITERAL("Physics Collider"),
					&Physics::ColliderComponent::m_centerOfMassOffset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mass"),
					"mass",
					"{A8E28A11-2561-435F-87C0-F0EFF9CDCF7B}"_guid,
					MAKE_UNICODE_LITERAL("Physics Collider"),
					Reflection::PropertyFlags::Transient,
					&Physics::ColliderComponent::SetMassFromProperty,
					&Physics::ColliderComponent::GetMassFromProperty
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Density"),
					"density",
					"762aa409-a92c-4064-99bc-9ab7630d40a7"_guid,
					MAKE_UNICODE_LITERAL("Physics Collider"),
					Reflection::PropertyFlags::Transient,
					&Physics::ColliderComponent::SetDensityFromProperty,
					&Physics::ColliderComponent::GetDensityFromProperty
				),
			}
		);
	};
}
