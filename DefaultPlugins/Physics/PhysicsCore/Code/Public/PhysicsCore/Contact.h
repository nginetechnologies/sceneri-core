#pragma once

#include <Common/Math/Vector3.h>
#include <Common/Memory/ReferenceWrapper.h>

namespace physx
{
	class PxShape;
}

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::Physics
{
	namespace Data
	{
		struct Body;
	}

	struct ColliderComponent;
	struct Material;

	struct Contact
	{
		[[nodiscard]] Contact GetInverted() const
		{
			return Contact{
				otherComponent,
				otherBody,
				otherCollider,
				otherContactPoints,
				thisComponent,
				thisBody,
				thisCollider,
				thisContactPoints,
				-normal
			};
		}

		Material& GetMaterial() const;
		Material& GetOtherMaterial() const;

		Optional<Entity::Component3D*> thisComponent;
		Optional<Data::Body*> thisBody;
		Optional<ColliderComponent*> thisCollider;
		ArrayView<const Math::WorldCoordinate, uint8> thisContactPoints;

		Optional<Entity::Component3D*> otherComponent;
		Optional<Data::Body*> otherBody;
		Optional<ColliderComponent*> otherCollider;
		ArrayView<const Math::WorldCoordinate, uint8> otherContactPoints;

		Math::Vector3f normal;
	};
}
