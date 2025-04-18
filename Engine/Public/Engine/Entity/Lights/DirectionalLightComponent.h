#pragma once

#include <Engine/Entity/Lights/LightSourceComponent.h>
#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/Color.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Memory/Containers/FlatVector.h>

namespace ngine::Entity
{
	struct DirectionalLightInstanceProperties
	{
		inline static constexpr uint8 MaximumCascadeCount = 4;
		inline static constexpr Array<Math::Lengthf, DirectionalLightInstanceProperties::MaximumCascadeCount> DefaultCascadeDistances{
			6_meters, 12_meters, 32_meters, 60_meters
		};

		Math::Color m_color = "#FFFFFF"_colorf;
		// TODO: More complex ability to restrict
		// i.e. slider range vs actual min / max
		Math::ClampedValuef m_intensity{10.f, 1.f, 50.f};
		Math::ClampedValue<uint16> m_cascadeCount = {4, 1, MaximumCascadeCount};
		FlatVector<Math::Lengthf, MaximumCascadeCount> m_cascadeDistances;

		void DeserializeCustomData(const Optional<Serialization::Reader>);
		bool SerializeCustomData(Serialization::Writer) const;
	};

	struct DirectionalLightComponent : public LightSourceComponent
	{
		using BaseType = LightSourceComponent;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using LightInstanceProperties = DirectionalLightInstanceProperties;

		struct Initializer : public LightSourceComponent::Initializer
		{
			using BaseType = LightSourceComponent::Initializer;
			Initializer(
				BaseType&& initializer,
				const Math::Color color = "#FFFFFF"_colorf,
				const Math::ClampedValue<uint16> cascadeCount = {4, 1, 256},
				const FixedArrayView<const Math::Lengthf, DirectionalLightInstanceProperties::MaximumCascadeCount> cascadeDistances =
					DirectionalLightInstanceProperties::DefaultCascadeDistances
			);

			Math::Color m_color = "#FFFFFF"_colorf;
			Math::ClampedValuef m_intensity{10.f, 1.f, 10000.f};
			Math::ClampedValue<uint16> m_cascadeCount = {4, 1, DirectionalLightInstanceProperties::MaximumCascadeCount};
			FlatVector<Math::Lengthf, DirectionalLightInstanceProperties::MaximumCascadeCount> m_cascadeDistances;
		};

		DirectionalLightComponent(const DirectionalLightComponent& templateComponent, const Cloner& cloner);
		DirectionalLightComponent(const Deserializer&);
		DirectionalLightComponent(Initializer&&);

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

		[[nodiscard]] Math::ClampedValuef GetIntensity() const
		{
			return m_intensity;
		}
		void SetIntensity(const Math::ClampedValuef intensity)
		{
			m_intensity = intensity;
			RefreshLight();
		}

		[[nodiscard]] uint16 GetCascadeCount() const
		{
			return m_cascadeCount;
		}
		void SetCascadeCount(const uint16 cascadeCount)
		{
			m_cascadeCount = cascadeCount;
			RefreshLight();
		}
		[[nodiscard]] ArrayView<const Math::Lengthf> GetCascadeDistances() const
		{
			return m_cascadeDistances.GetView();
		}

		void DeserializeCustomData(const Optional<Serialization::Reader>);
		bool SerializeCustomData(Serialization::Writer) const;
	private:
		friend struct Reflection::ReflectedType<Entity::DirectionalLightComponent>;
		DirectionalLightComponent(const Deserializer&, const Optional<Serialization::Reader> componentSerializer);
		DirectionalLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties);

		Math::Color m_color = "#FFFFFF"_colorf;
		Math::ClampedValuef m_intensity{10.f, 1.f, 10000.f};
		Math::ClampedValue<uint16> m_cascadeCount = {4, 1, DirectionalLightInstanceProperties::MaximumCascadeCount};
		FlatVector<Math::Lengthf, DirectionalLightInstanceProperties::MaximumCascadeCount> m_cascadeDistances;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::DirectionalLightInstanceProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::DirectionalLightInstanceProperties>(
			"{631ED35D-D902-4B20-8F9A-7C43837E1C4D}"_guid,
			MAKE_UNICODE_LITERAL("Directional Light Properties"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{FD63FEAD-2DF9-420E-8CC1-6EB534055BB5}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::DirectionalLightInstanceProperties::m_color
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Cascade Count"),
					"cascades",
					"{4781AE9B-0D9B-4B6A-8908-A23DAD2DF2AD}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::DirectionalLightInstanceProperties::m_cascadeCount
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Intensity"),
					"intensity",
					"{3CB7C05A-EDD0-4A84-A7AF-094AE4BCE91B}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::DirectionalLightInstanceProperties::m_intensity
				)
			}
		);
	};

	template<>
	struct ReflectedType<Entity::DirectionalLightComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::DirectionalLightComponent>(
			"8f32c5fc-bac8-4d50-97df-bdf0be0ad32e"_guid,
			MAKE_UNICODE_LITERAL("Directional Light"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Color"),
					"color",
					"{A4ABC1CD-E2BC-4535-B4F1-78D96E4ED0BA}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::DirectionalLightComponent::SetColor,
					&Entity::DirectionalLightComponent::GetColor
				),
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Cascade Count"),
					"cascades",
					"{596752D6-41AB-4B85-9BE3-8B6DEC61579D}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					Reflection::PropertyFlags::HideFromUI,
					&Entity::DirectionalLightComponent::m_cascadeCount
				},
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Intensity"),
					"intensity",
					"{F1864930-3C3E-4465-A9A8-51483E3FAF60}"_guid,
					MAKE_UNICODE_LITERAL("Light"),
					&Entity::DirectionalLightComponent::SetIntensity,
					&Entity::DirectionalLightComponent::GetIntensity
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "d30d6608-b9bd-f72a-d1a2-b955b13b3649"_asset, "1acb2307-b93b-4e66-b917-dd40212f0a98"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "73674e1d-1f81-2d75-a6ed-68cd36bb7c62"_asset}
			}
		);
	};
}
