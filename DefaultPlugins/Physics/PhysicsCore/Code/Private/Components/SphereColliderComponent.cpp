#include "PhysicsCore/Components/SphereColliderComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SphereShape.h>

namespace ngine::Physics
{
	[[nodiscard]] JPH::ShapeRef CloneSphereShape(const JPH::SphereShape& templateShape)
	{
		return JPH::SphereShapeSettings{templateShape.GetRadius(), templateShape.GetMaterial()}.Create().Get();
	}

	SphereColliderComponent::SphereColliderComponent(const SphereColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(
				templateComponent,
				ColliderComponent::ClonerInfo{
					Component3D::Cloner{cloner}, CloneSphereShape(static_cast<const JPH::SphereShape&>(*templateComponent.GetShape()))
				}
			)
	{
		Assert(GetWorldScale().IsUniform());
	}

	inline Math::Radiusf ReadRadius(const Optional<Serialization::Reader> serializer)
	{
		return serializer.IsValid() ? serializer->ReadWithDefaultValue<Math::Radiusf>("radius", (Math::Radiusf)0.5_meters)
		                            : (Math::Radiusf)0.5_meters;
	}

	[[nodiscard]] JPH::ShapeRef CreateSphereShape(const Math::Radiusf radius)
	{
		JPH::SphereShapeSettings sphereSettings{radius.GetMeters()};
		JPH::ShapeSettings::ShapeResult result = sphereSettings.Create();
		if (LIKELY(result.IsValid()))
		{
			return result.Get();
		}
		else
		{
			return {};
		}
	}

	SphereColliderComponent::SphereColliderComponent(Initializer&& initializer)
		: ColliderComponent(ColliderComponent::Initializer{
				Forward<Component3D::Initializer>(initializer),
				CreateSphereShape(initializer.m_radius),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
	{
		Assert(GetWorldScale().IsUniform());
	}

	SphereColliderComponent::SphereColliderComponent(const Deserializer& deserializer, const Math::Radiusf radius)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				Entity::Component3D::DeserializerWithBounds{
					Deserializer(deserializer),
					deserializer.GetSceneRegistry(),
					*deserializer.GetParent(),
					Entity::ComponentFlags{},
					Math::BoundingBox(radius)
				},
				CreateSphereShape(radius)
			})
	{
		Assert(GetWorldScale().IsUniform());
	}

	SphereColliderComponent::SphereColliderComponent(const Deserializer& deserializer)
		: SphereColliderComponent(
				deserializer,
				ReadRadius(deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SphereColliderComponent>().ToString().GetView()))
			)
	{
	}

#if ENABLE_ASSERTS
	void SphereColliderComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags)
	{
		Assert(GetWorldScale().IsUniform());
		ColliderComponent::OnWorldTransformChanged(flags);
	}
#endif

	void SphereColliderComponent::SetRadius(const Math::Radiusf radius)
	{
		Assert(GetWorldScale().IsUniform());
		SetShape(WrapShape(CreateSphereShape(radius)));
		OnShapeChanged();
	}

	Math::Radiusf SphereColliderComponent::GetRadius() const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			const JPH::SphereShape& sphere = static_cast<JPH::SphereShape&>(*pShape);
			return Math::Radiusf::FromMeters(sphere.GetRadius());
		}
		return 0_meters;
	}

	[[maybe_unused]] const bool wasSphereColliderRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SphereColliderComponent>>::Make());
	[[maybe_unused]] const bool wasSphereColliderTypeRegistered = Reflection::Registry::RegisterType<SphereColliderComponent>();
}
