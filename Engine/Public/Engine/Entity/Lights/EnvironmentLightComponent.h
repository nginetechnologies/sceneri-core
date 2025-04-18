#pragma once

#include <Engine/Entity/Lights/LightSourceComponent.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Renderer/Assets/Texture/TextureIdentifier.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct EnvironmentLightInstanceProperties
	{
		Math::Radiusf m_radius = 5_meters;
		Rendering::TextureIdentifier m_irradianceTextureIdentifier;
		Rendering::TextureIdentifier m_prefilteredTextureIdentifier;
	};

	struct EnvironmentLightComponent : public LightSourceComponent
	{
		using BaseType = LightSourceComponent;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using LightInstanceProperties = EnvironmentLightInstanceProperties;

		struct Initializer : public LightSourceComponent::Initializer
		{
			using BaseType = LightSourceComponent::Initializer;
			Initializer(BaseType&& initializer);
			Initializer(Entity::Component3D::Initializer&&);
			using BaseType::BaseType;

			LightInstanceProperties m_properties;
		};

		EnvironmentLightComponent(const EnvironmentLightComponent& templateComponent, const Cloner& cloner);
		EnvironmentLightComponent(const Deserializer&);
		EnvironmentLightComponent(Initializer&&);

		[[nodiscard]] Rendering::TextureIdentifier GetIrradianceTexture() const
		{
			return m_irradianceTextureIdentifier;
		}
		[[nodiscard]] Rendering::TextureIdentifier GetPrefilteredEnvironmentTexture() const
		{
			return m_prefilteredTextureIdentifier;
		}

		void SetIrradianceTexture(const Asset::Picker asset);
		[[nodiscard]] Asset::Picker GetIrradianceTextureAsset() const;
		void SetPrefilteredEnvironmentTexture(const Asset::Picker asset);
		[[nodiscard]] Asset::Picker GetPrefilteredEnvironmentTextureAsset() const;
	private:
		EnvironmentLightComponent(const Deserializer&, const Optional<Serialization::Reader> componentSerializer);
		EnvironmentLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties);
	private:
		friend struct Reflection::ReflectedType<Entity::EnvironmentLightComponent>;
		Math::Radiusf m_radius;
		Rendering::TextureIdentifier m_irradianceTextureIdentifier;
		Rendering::TextureIdentifier m_prefilteredTextureIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::EnvironmentLightComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::EnvironmentLightComponent>(
			"7be15c72-f784-4f5e-b895-7cc2fbef6355"_guid,
			MAKE_UNICODE_LITERAL("Environment Light"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"{ADB6E6F6-7EFE-4BC7-B089-0F847EC26857}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::EnvironmentLightComponent::m_radius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Diffuse Lighting Texture"),
					"irradiance",
					"{CE7ECE1B-4C01-4A91-9A75-5A4182FE3174}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::EnvironmentLightComponent::SetIrradianceTexture,
					&Entity::EnvironmentLightComponent::GetIrradianceTextureAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Prefiltered Environment Texture"),
					"prefiltered",
					"{45AE84AC-48A6-4184-AD89-E93B81EF27A0}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::EnvironmentLightComponent::SetPrefilteredEnvironmentTexture,
					&Entity::EnvironmentLightComponent::GetPrefilteredEnvironmentTextureAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "ca037338-26a1-4c09-4ad8-2c8891c38255"_asset, "1acb2307-b93b-4e66-b917-dd40212f0a98"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "5a68b431-cca4-d5f1-5f7a-109c80ac25f8"_asset}
			}
		);
	};
}
