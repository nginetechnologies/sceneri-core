#include "PhysicsCore/Components/CapsuleColliderComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "Material.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Math/IsEquivalentTo.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/CapsuleShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SphereShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ScaledShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>

namespace ngine::Physics
{
	[[nodiscard]] JPH::ShapeRef CloneCapsuleShape(const JPH::Shape& templateShape)
	{
		switch (templateShape.GetSubType())
		{
			case JPH::EShapeSubType::Capsule:
			{

				JPH::CapsuleShape& templateCapsuleShape = const_cast<JPH::CapsuleShape&>(static_cast<const JPH::CapsuleShape&>(templateShape));
				JPH::CapsuleShapeSettings* capsuleShapeSetting = new JPH::CapsuleShapeSettings(
					templateCapsuleShape.GetHalfHeightOfCylinder(),
					templateCapsuleShape.GetRadius(),
					templateCapsuleShape.GetMaterial()
				);
				const Math::Anglef xAxisRotation = 90.0_degrees;
				JPH::ShapeSettings::ShapeResult result = JPH::RotatedTranslatedShapeSettings(
																									 JPH::Vec3::sZero(),
																									 JPH::Quat::sRotation(JPH::Vec3::sAxisX(), xAxisRotation.GetRadians()),
																									 capsuleShapeSetting
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
			case JPH::EShapeSubType::Sphere:
			{
				JPH::SphereShape& templateSphereShape = const_cast<JPH::SphereShape&>(static_cast<const JPH::SphereShape&>(templateShape));
				JPH::SphereShapeSettings sphereShapeSetting =
					JPH::SphereShapeSettings(templateSphereShape.GetRadius(), templateSphereShape.GetMaterial());
				JPH::ShapeSettings::ShapeResult res = sphereShapeSetting.Create();
				if (LIKELY(res.IsValid()))
				{
					return res.Get();
				}
				else
				{
					return {};
				}
			}
			default:
				ExpectUnreachable();
		}
	}

	CapsuleColliderComponent::CapsuleColliderComponent(const CapsuleColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(
				templateComponent, ColliderComponent::ClonerInfo{Component3D::Cloner{cloner}, CloneCapsuleShape(*templateComponent.GetShape())}
			)
	{
	}

	CapsuleInitializationParametersBase ReadCapsuleInitializationParameters(const Optional<Serialization::Reader> serializer)
	{
		CapsuleInitializationParametersBase parameters;
		serializer->Serialize("radius", parameters.m_radius);
		serializer->Serialize("half_height", parameters.m_halfHeight);
		return parameters;
	}

	[[nodiscard]] JPH::ShapeRef CreateCapsuleShape(Math::Lengthf halfHeight, const Math::Radiusf radius)
	{
		JPH::CapsuleShapeSettings* capsuleSettings = new JPH::CapsuleShapeSettings(halfHeight.GetMeters(), radius.GetMeters());
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

	CapsuleColliderComponent::CapsuleColliderComponent(Initializer&& initializer)
		: ColliderComponent(ColliderComponent::Initializer{
				Entity::Component3D::Initializer{
					*initializer.GetParent(), initializer.m_localTransform, Math::BoundingBox(initializer.m_halfHeight * 2.f)
				},
				CreateCapsuleShape(initializer.m_halfHeight, initializer.m_radius),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
	{
		Assert(Math::IsEquivalentTo(GetWorldScale().x, GetWorldScale().y));
	}

	CapsuleColliderComponent::CapsuleColliderComponent(
		const Deserializer& deserializer, const CapsuleInitializationParametersBase& initializationParameters
	)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				DeserializerWithBounds{deserializer} | Math::BoundingBox(initializationParameters.m_halfHeight * 2.f),
				CreateCapsuleShape(initializationParameters.m_halfHeight, initializationParameters.m_radius)
			})
	{
		Assert(Math::IsEquivalentTo(GetWorldScale().x, GetWorldScale().y));
	}

	CapsuleColliderComponent::CapsuleColliderComponent(const Deserializer& deserializer)
		: CapsuleColliderComponent(
				deserializer,
				ReadCapsuleInitializationParameters(
					deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<CapsuleColliderComponent>().ToString().GetView())
				)
			)
	{
	}

	void CapsuleColliderComponent::SetRadius(const Math::Radiusf radius)
	{
		SetShape(WrapShape(CreateCapsuleShape(GetHalfHeight(), radius)));
		OnShapeChanged();
	}

	Math::Radiusf CapsuleColliderComponent::GetRadius() const
	{
		Math::Radiusf radius;
		Math::Lengthf halfHeight;
		GetHalfHeightAndRadius(radius, halfHeight);
		return radius;
	}

	void CapsuleColliderComponent::SetHalfHeight(const Math::Lengthf height)
	{
		SetShape(WrapShape(CreateCapsuleShape(height, GetRadius())));
		OnShapeChanged();
	}

	Math::Lengthf CapsuleColliderComponent::GetHalfHeight() const
	{
		Math::Radiusf radius;
		Math::Lengthf halfHeight;
		GetHalfHeightAndRadius(radius, halfHeight);
		return halfHeight;
	}

	bool CapsuleColliderComponent::GetHalfHeightAndRadius(Math::Radiusf& radiusOut, Math::Lengthf& heightOut) const
	{
		JPH::Shape* pShape = GetShape();
		if (LIKELY(pShape != nullptr))
		{
			Assert(Math::IsEquivalentTo(GetWorldScale().x, GetWorldScale().y));
			switch (pShape->GetSubType())
			{
				case JPH::EShapeSubType::Capsule:
				{
					const JPH::CapsuleShape& capsule = static_cast<const JPH::CapsuleShape&>(*pShape);
					radiusOut = Math::Radiusf::FromMeters(capsule.GetRadius());
					heightOut = Math::Lengthf::FromMeters(capsule.GetHalfHeightOfCylinder());
				}
				break;
				case JPH::EShapeSubType::Sphere:
				{
					const JPH::SphereShape& sphere = static_cast<const JPH::SphereShape&>(*pShape);
					radiusOut = Math::Radiusf::FromMeters(sphere.GetRadius());
					heightOut = radiusOut;
				}
				break;
				default:
					ExpectUnreachable();
			}

			return true;
		}
		else
		{
			radiusOut = Math::Radiusf(Math::Zero);
			heightOut = Math::Lengthf(Math::Zero);
			return false;
		}
	}

	[[maybe_unused]] const bool wasCapsuleColliderRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CapsuleColliderComponent>>::Make());
	[[maybe_unused]] const bool wasCapsuleColliderTypeRegistered = Reflection::Registry::RegisterType<CapsuleColliderComponent>();
}
