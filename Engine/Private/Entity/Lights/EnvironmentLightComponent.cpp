#include "Entity/Lights/EnvironmentLightComponent.h"

#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"
#include <Common/System/Query.h>

#include <Common/Memory/Move.h>
#include <Common/Memory/Forward.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Defaults.h>

namespace ngine::Entity
{
	EnvironmentLightComponent::EnvironmentLightComponent(const EnvironmentLightComponent& templateComponent, const Cloner& cloner)
		: LightSourceComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_irradianceTextureIdentifier(templateComponent.m_irradianceTextureIdentifier)
		, m_prefilteredTextureIdentifier(templateComponent.m_prefilteredTextureIdentifier)
	{
	}

	EnvironmentLightComponent::EnvironmentLightComponent(const Deserializer& deserializer)
		: EnvironmentLightComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<EnvironmentLightComponent>().ToString().GetView())
			)
	{
	}

	EnvironmentLightComponent::EnvironmentLightComponent(
		const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer
	)
		: EnvironmentLightComponent(
				deserializer,
				LightInstanceProperties{
					componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Radiusf>("radius", 1_meters)
																				: Math::Radiusf{1_meters},
					System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(
						componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Asset::Guid>(
																							"irradiance", Asset::Guid{Rendering::Constants::DefaultCubemapIrradianceAssetGuid}
																						)
																					: Asset::Guid{Rendering::Constants::DefaultCubemapIrradianceAssetGuid}
					),
					System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(
						componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Asset::Guid>(
																							"prefiltered", Asset::Guid{Rendering::Constants::DefaultCubemapPrefilteredAssetGuid}
																						)
																					: Asset::Guid{Rendering::Constants::DefaultCubemapPrefilteredAssetGuid}
					),
				}
			)
	{
	}

	EnvironmentLightComponent::EnvironmentLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties)
		: LightSourceComponent(DeserializerWithBounds{deserializer} | Math::BoundingBox(properties.m_radius))
		, m_radius(properties.m_radius)
		, m_irradianceTextureIdentifier(properties.m_irradianceTextureIdentifier)
		, m_prefilteredTextureIdentifier(properties.m_prefilteredTextureIdentifier)
	{
	}

	EnvironmentLightComponent::Initializer::Initializer(BaseType&& initializer)
		: BaseType(Forward<BaseType>(initializer))
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		// Temp, hardcoded to deferred PBR
		m_stageMask.Set(stageCache.FindIdentifier("F141B823-5844-4FBC-B106-0635FF52199C"_asset));
	}

	EnvironmentLightComponent::Initializer::Initializer(Entity::Component3D::Initializer&& initializer)
		: BaseType(Forward<Entity::Component3D::Initializer>(initializer))
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		// Temp, hardcoded to deferred PBR
		m_stageMask.Set(stageCache.FindIdentifier("F141B823-5844-4FBC-B106-0635FF52199C"_asset));

		Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		m_properties.m_irradianceTextureIdentifier = textureCache.FindOrRegisterAsset(Rendering::Constants::DefaultCubemapIrradianceAssetGuid);
		m_properties.m_prefilteredTextureIdentifier = textureCache.FindOrRegisterAsset(Rendering::Constants::DefaultCubemapPrefilteredAssetGuid
		);
	}

	EnvironmentLightComponent::EnvironmentLightComponent(Initializer&& initializer)
		: LightSourceComponent(LightSourceComponent::Initializer(initializer) | Math::BoundingBox(initializer.m_properties.m_radius))
		, m_radius(initializer.m_properties.m_radius)
		, m_irradianceTextureIdentifier(initializer.m_properties.m_irradianceTextureIdentifier)
		, m_prefilteredTextureIdentifier(initializer.m_properties.m_prefilteredTextureIdentifier)
	{
	}

	void EnvironmentLightComponent::SetIrradianceTexture(const Asset::Picker asset)
	{
		m_irradianceTextureIdentifier = System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(asset.GetAssetGuid());
		RefreshLight();
	}

	Asset::Picker EnvironmentLightComponent::GetIrradianceTextureAsset() const
	{
		const Rendering::TextureIdentifier irradianceTextureIdentifier = m_irradianceTextureIdentifier;
		return Asset::Reference{
			irradianceTextureIdentifier.IsValid() ? System::Get<Rendering::Renderer>().GetTextureCache().GetAssetGuid(irradianceTextureIdentifier)
																						: Asset::Guid{},
			TextureAssetType::AssetFormat.assetTypeGuid
		};
	}

	void EnvironmentLightComponent::SetPrefilteredEnvironmentTexture(const Asset::Picker asset)
	{
		m_prefilteredTextureIdentifier = System::Get<Rendering::Renderer>().GetTextureCache().FindOrRegisterAsset(asset.GetAssetGuid());
		RefreshLight();
	}

	Asset::Picker EnvironmentLightComponent::GetPrefilteredEnvironmentTextureAsset() const
	{
		const Rendering::TextureIdentifier prefilteredTextureIdentifier = m_prefilteredTextureIdentifier;
		return Asset::Reference{
			prefilteredTextureIdentifier.IsValid()
				? System::Get<Rendering::Renderer>().GetTextureCache().GetAssetGuid(m_prefilteredTextureIdentifier)
				: Asset::Guid{},
			TextureAssetType::AssetFormat.assetTypeGuid
		};
	}

	[[maybe_unused]] const bool wasEnvironmentLightRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<EnvironmentLightComponent>>::Make());
	[[maybe_unused]] const bool wasEnvironmentLightTypeRegistered = Reflection::Registry::RegisterType<EnvironmentLightComponent>();
}
