#include "PhysicsCore/Components/BoxColliderComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/BoxShape.h>

namespace ngine::Physics
{
	[[nodiscard]] JPH::ShapeRef CloneBoxShape(const JPH::BoxShape& templateShape)
	{
		return JPH::BoxShapeSettings{templateShape.GetHalfExtent(), templateShape.GetConvexRadius(), templateShape.GetMaterial()}.Create().Get(
		);
	}

	BoxColliderComponent::BoxColliderComponent(const BoxColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(
				templateComponent,
				ColliderComponent::ClonerInfo{
					Component3D::Cloner{cloner}, CloneBoxShape(static_cast<JPH::BoxShape&>(*templateComponent.GetShape()))
				}
			)
	{
	}

	Math::Vector3f ReadBoxHalfExtent(const Optional<Serialization::Reader> serializer)
	{
		Math::Vector3f halfExtent{0.5f};
		if (serializer.IsValid())
		{
			serializer->Serialize("half_extent", halfExtent);
		}
		return halfExtent;
	}

	[[nodiscard]] JPH::ShapeRef CreateBoxShape(const Math::Vector3f halfExtent)
	{
		const Math::Vector3f clampedExtent = Math::Max(halfExtent, Math::Vector3f(JPH::cDefaultConvexRadius * 2.f));
		JPH::BoxShapeSettings boxSettings{JPH::Vec3{clampedExtent.x, clampedExtent.y, clampedExtent.z}};
		JPH::ShapeSettings::ShapeResult result = boxSettings.Create();
		if (LIKELY(result.IsValid()))
		{
			return result.Get();
		}
		else
		{
			return {};
		}
	}

	BoxColliderComponent::BoxColliderComponent(Initializer&& initializer)
		: ColliderComponent(ColliderComponent::Initializer{
				Entity::Component3D::Initializer{
					*initializer.GetParent(), initializer.m_localTransform, Math::BoundingBox(-initializer.m_halfExtent, initializer.m_halfExtent)
				},
				CreateBoxShape(initializer.m_halfExtent),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
	{
	}

	BoxColliderComponent::BoxColliderComponent(const Deserializer& deserializer, const Math::Vector3f halfExtent)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				Entity::Component3D::DeserializerWithBounds{
					Deserializer(deserializer),
					deserializer.GetSceneRegistry(),
					*deserializer.GetParent(),
					Entity::ComponentFlags{},
					Math::BoundingBox(-halfExtent, halfExtent)
				},
				CreateBoxShape(halfExtent)
			})
	{
	}

	BoxColliderComponent::BoxColliderComponent(const Deserializer& deserializer)
		: BoxColliderComponent(
				deserializer,
				ReadBoxHalfExtent(deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<BoxColliderComponent>().ToString().GetView()))
			)
	{
	}

	void BoxColliderComponent::SetHalfExtent(const Math::Vector3f halfExtent)
	{
		SetShape(WrapShape(CreateBoxShape(halfExtent)));
		OnShapeChanged();
	}

	Math::Vector3f BoxColliderComponent::GetHalfExtent() const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			const JPH::BoxShape& box = static_cast<JPH::BoxShape&>(*pShape);
			const JPH::Vec3 halfExtent = box.GetHalfExtent();
			return Math::Vector3f(halfExtent.GetX(), halfExtent.GetY(), halfExtent.GetZ());
		}
		else
		{
			return Math::Zero;
		}
	}

	[[maybe_unused]] const bool wasBoxColliderRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<BoxColliderComponent>>::Make());
	[[maybe_unused]] const bool wasBoxColliderTypeRegistered = Reflection::Registry::RegisterType<BoxColliderComponent>();
}
