#include "PhysicsCore/Components/CylinderColliderComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "Material.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Math/IsEquivalentTo.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/CylinderShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ScaledShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ngine::Physics
{
	[[nodiscard]] JPH::ShapeRef CloneCylinderShape(const JPH::CylinderShape& templateShape)
	{
		JPH::CylinderShapeSettings* cylinderSettings = new JPH::CylinderShapeSettings{
			templateShape.GetHalfHeight(),
			templateShape.GetRadius(),
			JPH::cDefaultConvexRadius,
			templateShape.GetMaterial()
		};

		const Math::Anglef xAxisRotation = 90.0_degrees;
		JPH::ShapeSettings::ShapeResult result = JPH::RotatedTranslatedShapeSettings(
																							 JPH::Vec3::sZero(),
																							 JPH::Quat::sRotation(JPH::Vec3::sAxisX(), xAxisRotation.GetRadians()),
																							 cylinderSettings
		)
		                                           .Create();

		if (LIKELY(result.IsValid()))
		{
			return result.Get();
		}
		else
		{
			return {};
		}
	}

	CylinderColliderComponent::CylinderColliderComponent(const CylinderColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(
				templateComponent,
				ColliderComponent::ClonerInfo{
					Component3D::Cloner{cloner}, CloneCylinderShape(static_cast<JPH::CylinderShape&>(*templateComponent.GetShape()))
				}
			)
	{
	}

	CylinderInitializationParametersBase ReadCylinderInitializationParameters(const Optional<Serialization::Reader> serializer)
	{
		if (serializer.IsValid())
		{
			return {
				serializer->ReadWithDefaultValue<Math::Radiusf>("radius", (Math::Radiusf)0.5_meters),
				serializer->ReadWithDefaultValue<Math::Radiusf>("half_height", (Math::Radiusf)0.1_meters)
			};
		}
		else
		{
			return {(Math::Radiusf)0.5_meters, (Math::Radiusf)0.1_meters};
		}
	}

	[[nodiscard]] JPH::ShapeRef CreateCylinderShape(Math::Lengthf halfHeight, const Math::Radiusf radius)
	{
		JPH::CylinderShapeSettings* capsuleSettings = new JPH::CylinderShapeSettings(halfHeight.GetMeters(), radius.GetMeters());
		const Math::Anglef xAxisRotation = 90.0_degrees;
		JPH::ShapeSettings::ShapeResult result = JPH::RotatedTranslatedShapeSettings(
																							 JPH::Vec3::sZero(),
																							 JPH::Quat::sRotation(JPH::Vec3::sAxisX(), xAxisRotation.GetRadians()),
																							 capsuleSettings
		)
		                                           .Create();
		if (LIKELY(result.IsValid()))
		{
			return result.Get();
		}
		else
		{
			return {};
		}
	}

	CylinderColliderComponent::CylinderColliderComponent(Initializer&& initializer)
		: ColliderComponent(ColliderComponent::Initializer{
				Entity::Component3D::Initializer{
					*initializer.GetParent(), initializer.m_localTransform, Math::BoundingBox(initializer.m_halfHeight * 2.f)
				},
				CreateCylinderShape(initializer.m_halfHeight, initializer.m_radius),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
	{
		Assert(Math::IsEquivalentTo(GetWorldScale().x, GetWorldScale().y));
	}

	CylinderColliderComponent::CylinderColliderComponent(
		const Deserializer& deserializer, const CylinderInitializationParametersBase& initializationParameters
	)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				DeserializerWithBounds{deserializer} | Math::BoundingBox(initializationParameters.m_halfHeight * 2.f),
				CreateCylinderShape(initializationParameters.m_halfHeight, initializationParameters.m_radius)
			})
	{
		Assert(Math::IsEquivalentTo(GetWorldScale().x, GetWorldScale().y));
	}

	CylinderColliderComponent::CylinderColliderComponent(const Deserializer& deserializer)
		: CylinderColliderComponent(
				deserializer,
				ReadCylinderInitializationParameters(
					deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<CylinderColliderComponent>().ToString().GetView())
				)
			)
	{
	}

	void CylinderColliderComponent::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		if (serializer.IsValid())
		{
			const CylinderInitializationParametersBase initParameters = ReadCylinderInitializationParameters(serializer.Get());
			SetShape(WrapShape(CreateCylinderShape(initParameters.m_halfHeight, initParameters.m_radius)));
		}
	}

	bool CylinderColliderComponent::SerializeCustomData(Serialization::Writer serializer) const
	{
		Math::Radiusf radius;
		Math::Lengthf halfHeight;
		if (GetHeightAndRadius(radius, halfHeight))
		{
			serializer.Serialize("radius", radius);
			serializer.Serialize("half_height", radius);
			return true;
		}
		else
		{
			return false;
		}
	}

	void CylinderColliderComponent::SetRadius(const Math::Radiusf radius)
	{
		SetShape(WrapShape(CreateCylinderShape(GetHalfHeight(), radius)));
		OnShapeChanged();
	}

	Math::Radiusf CylinderColliderComponent::GetRadius() const
	{
		Math::Radiusf radius;
		Math::Lengthf halfHeight;
		GetHeightAndRadius(radius, halfHeight);
		return radius;
	}

	void CylinderColliderComponent::SetHalfHeight(const Math::Lengthf height)
	{
		SetShape(WrapShape(CreateCylinderShape(height, GetRadius())));
		OnShapeChanged();
	}

	Math::Lengthf CylinderColliderComponent::GetHalfHeight() const
	{
		Math::Radiusf radius;
		Math::Lengthf halfHeight;
		GetHeightAndRadius(radius, halfHeight);
		return halfHeight;
	}

	bool CylinderColliderComponent::GetHeightAndRadius(Math::Radiusf& radiusOut, Math::Lengthf& heightOut) const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			const JPH::CylinderShape& cylinder = static_cast<JPH::CylinderShape&>(*pShape);
			radiusOut = Math::Radiusf::FromMeters(cylinder.GetRadius());
			heightOut = Math::Lengthf::FromMeters(cylinder.GetHalfHeight());

			return true;
		}
		else
		{
			radiusOut = Math::Radiusf(Math::Zero);
			heightOut = Math::Lengthf(Math::Zero);
			return false;
		}
	}

	[[maybe_unused]] const bool wasCylinderColliderRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CylinderColliderComponent>>::Make());
	[[maybe_unused]] const bool wasCylinderColliderTypeRegistered = Reflection::Registry::RegisterType<CylinderColliderComponent>();
}
