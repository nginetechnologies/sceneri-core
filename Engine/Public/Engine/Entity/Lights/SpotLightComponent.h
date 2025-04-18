#pragma once

#include <Engine/Entity/Lights/LightSourceComponent.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Color.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/ClampedValue.h>

namespace ngine::Entity
{
	struct SpotLightInstanceProperties
	{
		Math::Lengthf m_nearPlane = 0.1_meters;
		Math::Radiusf m_influenceRadius = 20_meters;
		Math::Anglef m_fieldOfView = 100_degrees;
		Math::Color m_color = "#FFFFFF"_colorf;
	};

	struct SpotLightComponent : public LightSourceComponent
	{
		using BaseType = LightSourceComponent;

		using LightInstanceProperties = SpotLightInstanceProperties;
		inline static constexpr LightInstanceProperties DefaultProperties = LightInstanceProperties{};

		struct Initializer : public LightSourceComponent::Initializer, public SpotLightInstanceProperties
		{
			using BaseType = LightSourceComponent::Initializer;

			Initializer(
				BaseType&& initializer,
				const Math::Lengthf nearPlane = 0.1_meters,
				const Math::Radiusf influenceRadius = 20_meters,
				const Math::Anglef fieldOfView = 100_degrees,
				const Math::Color color = "#FFFFFF"_colorf
			);
		};

		SpotLightComponent(const SpotLightComponent& templateComponent, const Cloner& cloner);
		SpotLightComponent(const Deserializer&);
		SpotLightComponent(Initializer&&);

		[[nodiscard]] Math::Lengthf GetNearPlane() const
		{
			return m_nearPlane;
		}
		void SetNearPlane(const Math::Lengthf nearPlane)
		{
			m_nearPlane = nearPlane;
			RefreshLight();
		}
		void DeriveIntensityFromInfluenceRadius()
		{
			// derive light intensity based on influence radius, based on https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/

			float a = 1.0f + (m_influenceRadius - m_lightRadius) / m_lightRadius;

			m_intensity = m_intensityScaling * m_intensityCutoff * a * a;
		}
		[[nodiscard]] Math::Radiusf GetInfluenceRadius() const
		{
			return m_influenceRadius;
		}
		void SetInfluenceRadius(const Math::Radiusf influenceRadius)
		{
			m_influenceRadius = influenceRadius;

			DeriveIntensityFromInfluenceRadius();

			RefreshLight();
		}
		[[nodiscard]] Math::Anglef GetFieldOfView() const
		{
			return m_fieldOfView;
		}
		void SetFieldOfView(const Math::Anglef fieldOfView)
		{
			m_fieldOfView = fieldOfView;
			RefreshLight();
		}
		[[nodiscard]] Math::Color GetColorWithIntensity() const
		{
			return m_color * m_intensity;
		}
		[[nodiscard]] Math::Color GetColor() const
		{
			return m_color;
		}
		void SetColor(const Math::Color color)
		{
			m_color = color;
			RefreshLight();
		}
	private:
		friend struct Reflection::ReflectedType<Entity::SpotLightComponent>;
		SpotLightComponent(const Deserializer&, const Optional<Serialization::Reader> componentSerializer);
		SpotLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties);

		void OnInfluenceRadiusChanged()
		{
			RefreshLight();
		}

		void OnNearPlaneChanged()
		{
			RefreshLight();
		}

		void OnFieldOfViewChanged()
		{
			RefreshLight();
		}

		void OnColorChanged()
		{
			RefreshLight();
		}

		Math::Lengthf m_nearPlane = 0.1_meters;
		Math::Radiusf m_influenceRadius = 5_meters;
		Math::Anglef m_fieldOfView = 100_degrees;
		Math::Color m_color = "#FFFFFF"_colorf;
		Math::ClampedValuef m_intensity{1.f, 1.f, 10000.f};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::SpotLightInstanceProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::SpotLightInstanceProperties>(
			"{FEA6F288-44E7-4457-8CCA-421616AE2FDB}"_guid,
			MAKE_UNICODE_LITERAL("Spot Light Properties"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Near Plane"),
					"near",
					"{A455B88C-E4D9-4F4A-9574-8D1257344C41}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightInstanceProperties::m_nearPlane
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("InfluenceRadius"),
					"influenceRadius",
					"{D7A8929A-48EA-476E-93BF-1258C8705978}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightInstanceProperties::m_influenceRadius
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Field of View"),
					"fov",
					"{DDEB9E05-5C86-44B6-92A5-3B7FE51BBF9D}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightInstanceProperties::m_fieldOfView
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{7B55E357-B529-45F2-8405-7B0E8E35C11F}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightInstanceProperties::m_color
				)
			}
		);
	};

	template<>
	struct ReflectedType<Entity::SpotLightComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::SpotLightComponent>(
			"9c14016a-5bab-4719-9213-5ff28d05fe1e"_guid,
			MAKE_UNICODE_LITERAL("Spot Light"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Near Plane"),
					"near",
					"{BBF291F1-B30C-4268-A25F-BE728D07B9E3}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightComponent::m_nearPlane,
					&Entity::SpotLightComponent::OnNearPlaneChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("InfluenceRadius"),
					"influenceRadius",
					"{F298E1CA-B7DA-4D43-8013-11A666F5BA26}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightComponent::m_influenceRadius,
					&Entity::SpotLightComponent::OnInfluenceRadiusChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Field of View"),
					"fov",
					"{8A0E874F-C1D6-4969-B295-E45095BBA7F2}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightComponent::m_fieldOfView,
					&Entity::SpotLightComponent::OnFieldOfViewChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{7FE8A639-B888-4C1A-B743-64AA0799E991}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::SpotLightComponent::m_color,
					&Entity::SpotLightComponent::OnColorChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "a80503fb-45bb-e12d-158d-a7ad84912564"_asset, "1acb2307-b93b-4e66-b917-dd40212f0a98"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "18c5240b-001d-d521-527b-2c5189572579"_asset}
			}
		);
	};
}
