#pragma once

#include <Engine/Entity/Lights/LightSourceComponent.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Color.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Function/Event.h>

namespace ngine::Entity
{
	struct PointLightInstanceProperties
	{
		Math::Radiusf m_influenceRadius = 20_meters;
		Math::Color m_color = "#FFFFFF"_colorf;

		[[nodiscard]] Math::Radiusf GetInfluenceRadius() const
		{
			return m_influenceRadius;
		}
	};

	struct PointLightComponent : public LightSourceComponent
	{
		using BaseType = LightSourceComponent;

		using LightInstanceProperties = PointLightInstanceProperties;
		inline static constexpr LightInstanceProperties DefaultProperties = LightInstanceProperties{};

		struct Initializer : public LightSourceComponent::Initializer, public PointLightInstanceProperties
		{
			using BaseType = LightSourceComponent::Initializer;
			Initializer(BaseType&& initializer, const Math::Radiusf radius = 20_meters, const Math::Color color = "#FFFFFF"_colorf);
		};

		PointLightComponent(const PointLightComponent& templateComponent, const Cloner& cloner);
		PointLightComponent(const Deserializer&);
		PointLightComponent(Initializer&&);

		[[nodiscard]] Math::Radiusf GetInfluenceRadius() const
		{
			return m_influenceRadius;
		}
		[[nodiscard]] Math::Radiusf GetGizmoRadius() const
		{
			return m_gizmoRadius;
		}
		void SetInfluenceRadius(const Math::Radiusf radius)
		{
			Assert(radius.GetMeters() > 0.0f);
			m_influenceRadius = radius;

			ComputeDerivedQuantitiesFromInfluenceRadius();

			RefreshLight();

			OnInfluenceRadiusChanged();
		}
		void SetGizmoRadius(const Math::Radiusf radius)
		{
			Assert(radius.GetMeters() > 0.0f);
			m_gizmoRadius = radius;

			ComputeDerivedQuantitiesFromGizmoRadius();

			RefreshLight();

			OnInfluenceRadiusChanged();
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

		Event<void(void*), 24> OnInfluenceRadiusChanged;
	private:
		void ComputeDerivedQuantitiesFromInfluenceRadius();
		void ComputeDerivedQuantitiesFromGizmoRadius();

		[[nodiscard]] float ComputeIntensity() const;
		[[nodiscard]] Math::Radiusf ComputeGizmoRadius() const;
	private:
		friend struct Reflection::ReflectedType<Entity::PointLightComponent>;
		PointLightComponent(const Deserializer&, const Optional<Serialization::Reader> componentSerializer);
		PointLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties);
		Math::Radiusf m_influenceRadius =
			5_meters; // radius at which light intensity falloff to LightSourceComponent::m_intensityCutoff (and faded to zero)
		Math::Color m_color = "#FFFFFF"_colorf;

		// derived quantities
		float m_intensity = 1.f;
		Math::Radiusf m_gizmoRadius = 10_meters; // radius at which light falloff to LightSourceComponent::m_gizmoIntensityCutoff
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::PointLightInstanceProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::PointLightInstanceProperties>(
			"{66EB5F4C-514D-40FA-BDFC-E27A6B36314B}"_guid,
			MAKE_UNICODE_LITERAL("Point Light Properties"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"influenceRadius",
					"{7C6EBF23-2596-4E75-8892-AD5D99701C60}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::PointLightInstanceProperties::m_influenceRadius
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{7C6EBF23-2596-4E75-8892-AD5D99701C60}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::PointLightInstanceProperties::m_color
				)
			}
		);
	};

	template<>
	struct ReflectedType<Entity::PointLightComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::PointLightComponent>(
			"c720a57c-40c6-48a4-8f18-12d41e093616"_guid,
			MAKE_UNICODE_LITERAL("Point Light"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"influenceRadius",
					"{EAAEE631-4091-4BDC-8837-119A2B93EFF6}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::PointLightComponent::SetInfluenceRadius,
					&Entity::PointLightComponent::GetInfluenceRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{52F44D30-C5F3-482F-81E3-76AFF4DE940F}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::PointLightComponent::SetColor,
					&Entity::PointLightComponent::GetColor
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "99c93ad5-0c9b-30b8-f883-aa43fd2ec6ad"_asset, "1acb2307-b93b-4e66-b917-dd40212f0a98"_guid
				},
				Entity::IndicatorTypeExtension{"bfe742cc-f140-4616-9749-f9524463fe00"_guid, "dbaeaa7c-2d95-a908-967a-6a8dbe49bedd"_asset}
			}
		);
	};
}
