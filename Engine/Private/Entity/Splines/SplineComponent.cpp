#include "Entity/Splines/SplineComponent.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Math/Primitives/Serialization/Spline.h>
#include <Common/Math/Primitives/Serialization/BoundingBox.h>
#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>

namespace ngine::Entity
{
	SplineComponent::SplineComponent(const SplineComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_spline(templateComponent.m_spline)
	{
	}

	SplineComponent::SplineComponent(const Deserializer& deserializer)
		: SplineComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SplineComponent>().ToString().GetView()))
	{
	}

	SplineComponent::SplineComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: Component3D(Component3D::DeserializerWithBounds(
				Deserializer(deserializer),
				deserializer.GetSceneRegistry(),
				*deserializer.GetParent(),
				deserializer.GetFlags(),
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::BoundingBox>("bounds", Math::BoundingBox(1_meters))
																			: Math::BoundingBox(1_meters)
			))
		, m_spline(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<SplineType>("spline", SplineType()) : SplineType{})
	{
	}

	SplineComponent::SplineComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
	{
		// A spline needs at least two points to be valid.
		m_spline.EmplacePoint(Math::Zero, Math::Up);
		m_spline.EmplacePoint(Math::Forward, Math::Up);
	}

	bool SplineComponent::SerializeCustomData(Serialization::Writer serializer) const
	{
		Threading::SharedLock lock(m_splineMutex);
		bool serializedAny = serializer.Serialize("spline", m_spline);
		serializedAny |= serializer.Serialize("bounds", GetBoundingBox());
		return serializedAny;
	}

	[[maybe_unused]] const bool wasSplineRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<SplineComponent>>::Make());
	[[maybe_unused]] const bool wasSplineTypeRegistered = Reflection::Registry::RegisterType<SplineComponent>();
}
