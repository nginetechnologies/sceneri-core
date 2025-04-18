#include "PhysicsCore/Components/PlaneColliderComponent.h"
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
	[[nodiscard]] JPH::ShapeRef ClonePlaneShape(const JPH::BoxShape& templateShape)
	{
		return JPH::BoxShapeSettings{templateShape.GetHalfExtent(), templateShape.GetConvexRadius(), templateShape.GetMaterial()}.Create().Get(
		);
	}

	PlaneColliderComponent::PlaneColliderComponent(const PlaneColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(
				templateComponent,
				ColliderComponent::ClonerInfo{
					Component3D::Cloner{cloner}, ClonePlaneShape(static_cast<const JPH::BoxShape&>(*templateComponent.GetShape()))
				}
			)
	{
	}

	Math::Vector2f ReadPlaneHalfExtent(Optional<Serialization::Reader> serializer)
	{
		Math::Vector2f halfExtent;

		if (serializer.IsValid())
		{
			serializer.Get().Serialize("half_extent", halfExtent);
		}
		else
		{
			halfExtent = Math::Vector2f(0.5f);
		}

		return halfExtent;
	}

	inline static constexpr float PlaneHalfHeight = 0.1f;
	inline static constexpr float PlaneHeight = PlaneHalfHeight * 2.f;

	[[nodiscard]] JPH::ShapeRef CreatePlaneShape(const Math::Vector2f halfExtent)
	{
		JPH::BoxShapeSettings boxSettings{JPH::Vec3{halfExtent.x, halfExtent.y, PlaneHeight}};
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

	PlaneColliderComponent::PlaneColliderComponent(Initializer&& initializer)
		: ColliderComponent(ColliderComponent::Initializer{
				Entity::Component3D::Initializer{
					*initializer.GetParent(),
					initializer.m_localTransform,
					Math::BoundingBox(
						-Math::Vector3f{initializer.m_halfExtent.x, initializer.m_halfExtent.y, PlaneHalfHeight},
						Math::Vector3f{initializer.m_halfExtent.x, initializer.m_halfExtent.y, PlaneHalfHeight}
					)
				},
				CreatePlaneShape(initializer.m_halfExtent),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
	{
	}

	PlaneColliderComponent::PlaneColliderComponent(const Deserializer& deserializer, const Math::Vector2f halfExtent)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				Entity::Component3D::DeserializerWithBounds{
					Deserializer(deserializer),
					deserializer.GetSceneRegistry(),
					*deserializer.GetParent(),
					Entity::ComponentFlags{},
					Math::BoundingBox(
						-Math::Vector3f{halfExtent.x, halfExtent.y, PlaneHalfHeight}, Math::Vector3f{halfExtent.x, halfExtent.y, PlaneHalfHeight}
					)
				},
				CreatePlaneShape(halfExtent)
			})
	{
	}

	PlaneColliderComponent::PlaneColliderComponent(const Deserializer& deserializer)
		: PlaneColliderComponent(
				deserializer,
				ReadPlaneHalfExtent(deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<PlaneColliderComponent>().ToString().GetView()))
			)
	{
	}

	void PlaneColliderComponent::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		if (serializer.IsValid())
		{
			const Math::Vector2f halfExtent = ReadPlaneHalfExtent(serializer.Get());
			SetShape(WrapShape(CreatePlaneShape(halfExtent)));
		}
	}

	bool PlaneColliderComponent::SerializeCustomData(Serialization::Writer serializer) const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			const JPH::BoxShape& box = static_cast<JPH::BoxShape&>(*pShape);
			const JPH::Vec3 joltHalfExtent = box.GetHalfExtent();

			const Math::Vector2f halfExtent = Math::Vector2f{joltHalfExtent.GetX(), joltHalfExtent.GetY()};
			serializer.Serialize("half_extent", halfExtent);
			return true;
		}
		else
		{
			return false;
		}
	}

	void PlaneColliderComponent::SetHalfExtent(const Math::Vector2f halfExtent)
	{
		SetShape(WrapShape(CreatePlaneShape(halfExtent)));
		OnShapeChanged();
	}

	Math::Vector2f PlaneColliderComponent::GetHalfExtent() const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			const JPH::BoxShape& box = static_cast<JPH::BoxShape&>(*pShape);
			const JPH::Vec3 joltHalfExtent = box.GetHalfExtent();
			return Math::Vector2f(joltHalfExtent.GetX(), joltHalfExtent.GetY());
		}
		return Math::Vector2f(0.f);
	}

	[[maybe_unused]] const bool wasPlaneColliderRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<PlaneColliderComponent>>::Make());
	[[maybe_unused]] const bool wasPlaneColliderTypeRegistered = Reflection::Registry::RegisterType<PlaneColliderComponent>();
}
