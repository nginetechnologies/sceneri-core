#include "Entity/Lights/DirectionalLightComponent.h"

#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/RootSceneComponent.h"

#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Renderer.h>

#include <Common/Memory/Move.h>
#include <Common/Memory/Forward.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Serialization/Type.h>
#include <Common/Reflection/Serialization/Property.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Serialization/Vector.h>

namespace ngine::Entity
{
	DirectionalLightComponent::DirectionalLightComponent(const DirectionalLightComponent& templateComponent, const Cloner& cloner)
		: LightSourceComponent(templateComponent, cloner)
		, m_color(templateComponent.m_color)
		, m_intensity(templateComponent.m_intensity)
		, m_cascadeCount(templateComponent.m_cascadeCount)
		, m_cascadeDistances(templateComponent.m_cascadeDistances)
	{
	}

	DirectionalLightComponent::DirectionalLightComponent(const Deserializer& deserializer)
		: DirectionalLightComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<DirectionalLightComponent>().ToString().GetView())
			)
	{
	}

	DirectionalLightComponent::DirectionalLightComponent(
		const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer
	)
		: DirectionalLightComponent(
				deserializer,
				componentSerializer.IsValid() ? Reflection::GetType<LightInstanceProperties>().DeserializeType(Reflection::TypeDeserializer{
																					*componentSerializer, System::Get<Reflection::Registry>(), *deserializer.m_pJobBatch
																				})
																			: LightInstanceProperties{}
			)
	{
	}

	DirectionalLightComponent::DirectionalLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties)
		: LightSourceComponent(
				DeserializerWithBounds{deserializer} |
				Math::BoundingBox(deserializer.GetParent()->GetRootSceneComponent().GetBoundingBox().GetRadius())
			)
		, m_color(properties.m_color)
		, m_intensity(properties.m_intensity)
		, m_cascadeCount(properties.m_cascadeCount)
		, m_cascadeDistances(properties.m_cascadeDistances)
	{
	}

	DirectionalLightComponent::Initializer::Initializer(
		BaseType&& initializer,
		const Math::Color color,
		const Math::ClampedValue<uint16> cascadeCount,
		const FixedArrayView<const Math::Lengthf, DirectionalLightInstanceProperties::MaximumCascadeCount> cascadeDistances
	)
		: BaseType(Forward<BaseType>(initializer))
		, m_color(color)
		, m_cascadeCount(cascadeCount)
		, m_cascadeDistances(cascadeDistances.GetDynamicView())
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		// Enable tile population and shadows stage by default
		m_stageMask.Set(stageCache.FindIdentifier("F141B823-5844-4FBC-B106-0635FF52199C"_asset));
		m_stageMask.Set(stageCache.FindIdentifier("dfca0ef8-eded-4660-8adb-43c0aa8f4e60"_asset));
	}

	DirectionalLightComponent::DirectionalLightComponent(Initializer&& initializer)
		: LightSourceComponent(
				LightSourceComponent::Initializer(initializer) |
				Math::BoundingBox(initializer.GetParent()->GetRootSceneComponent().GetBoundingBox().GetRadius())
			)
		, m_color(initializer.m_color)
		, m_intensity(initializer.m_intensity)
		, m_cascadeCount(initializer.m_cascadeCount)
		, m_cascadeDistances(initializer.m_cascadeDistances)
	{
	}

	void DirectionalLightInstanceProperties::DeserializeCustomData(const Optional<Serialization::Reader> reader)
	{
		if (reader.IsValid())
		{
			if (!reader->Serialize("cascade_distances", m_cascadeDistances))
			{
				m_cascadeDistances.Clear();
				m_cascadeDistances.CopyEmplaceRangeBack(DefaultCascadeDistances.GetDynamicView());
			}
		}
	}

	bool DirectionalLightInstanceProperties::SerializeCustomData(Serialization::Writer writer) const
	{
		writer.Serialize("cascade_distances", m_cascadeDistances);
		return true;
	}

	void DirectionalLightComponent::DeserializeCustomData(const Optional<Serialization::Reader> reader)
	{
		if (reader.IsValid())
		{
			m_cascadeDistances.Clear();
			if (!reader->Serialize("cascade_distances", m_cascadeDistances))
			{
				m_cascadeDistances.CopyEmplaceRangeBack(DirectionalLightInstanceProperties::DefaultCascadeDistances.GetDynamicView());
			}
		}
	}

	bool DirectionalLightComponent::SerializeCustomData(Serialization::Writer writer) const
	{
		writer.Serialize("cascade_distances", m_cascadeDistances);
		return true;
	}

	[[maybe_unused]] const bool wasDirectionalLightRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<DirectionalLightComponent>>::Make());
	[[maybe_unused]] const bool wasDirectionalLightTypeRegistered = Reflection::Registry::RegisterType<DirectionalLightComponent>();
}
