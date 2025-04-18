#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct TorusComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "948597c4-dc45-406e-962e-350ed2a93fc8"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				const Math::Radiusf radius = 0.5_meters,
				const Math::Lengthf thickness = 0.2_meters,
				const uint16 sideCount = 8
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_thickness(thickness)
				, m_sideCount(sideCount)
			{
			}

			Math::Radiusf m_radius = 0.5_meters;
			Math::Lengthf m_thickness = 0.2_meters;
			uint16 m_sideCount = 8;
		};

		TorusComponent(const TorusComponent& templateComponent, const Cloner& cloner);
		TorusComponent(const Deserializer& deserializer);
		TorusComponent(Initializer&& initializer);
		virtual ~TorusComponent() = default;

		void OnCreated();

		void SetSideCount(const uint16 count)
		{
			m_sideCount = count;
			OnPropertiesChanged();
		}
		[[nodiscard]] uint16 GetSideCount() const
		{
			return m_sideCount;
		}

		void SetRadius(const Math::Radiusf radius)
		{
			m_radius = radius;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Radiusf GetRadius() const
		{
			return m_radius;
		}

		void SetThickness(const Math::Lengthf height)
		{
			m_thickness = height;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Lengthf GetThickness() const
		{
			return m_thickness;
		}
	protected:
		void OnPropertiesChanged()
		{
			RecreateMesh();
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::TorusComponent>;
	private:
		TorusComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_thickness = 0.2_meters;
		uint16 m_sideCount = 8;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::TorusComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::TorusComponent>(
			Entity::Primitives::TorusComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Torus"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Shaft Radius"),
					"Radius",
					"{66941D28-267E-42B1-B884-3832DDC2899B}"_guid,
					MAKE_UNICODE_LITERAL("Torus"),
					&Entity::Primitives::TorusComponent::m_radius,
					&Entity::Primitives::TorusComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Thickness"),
					"Thickness",
					"{042C0318-06BD-49BA-9E33-F340E72BC55B}"_guid,
					MAKE_UNICODE_LITERAL("Torus"),
					&Entity::Primitives::TorusComponent::m_thickness,
					&Entity::Primitives::TorusComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{0A8BF151-EAF3-400B-B1A4-3D227FFE8EB2}"_guid,
					MAKE_UNICODE_LITERAL("Torus"),
					&Entity::Primitives::TorusComponent::m_sideCount,
					&Entity::Primitives::TorusComponent::OnPropertiesChanged
				),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"5a9e9562-1a5c-0d57-ea1b-257cbab3480a"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"f64d7c9f-93ba-49a9-99cc-c905f28a8866"_asset
			}}
		);
	};
}
