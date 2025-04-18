#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct ConeComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "7ca24768-1e2a-49c1-bb4d-9d3565ccc66d"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer, const Math::Radiusf radius = 0.5_meters, const Math::Lengthf height = 1_meters, const uint16 sideCount = 8
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_height(height)
				, m_sideCount(sideCount)
			{
			}

			Math::Radiusf m_radius = 0.5_meters;
			Math::Lengthf m_height = 1_meters;
			uint16 m_sideCount = 8;
		};

		ConeComponent(const ConeComponent& templateComponent, const Cloner& cloner);
		ConeComponent(const Deserializer& deserializer);
		ConeComponent(Initializer&& initializer);
		virtual ~ConeComponent() = default;

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
		void SetHeight(const Math::Lengthf height)
		{
			m_height = height;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Lengthf GetHeight() const
		{
			return m_height;
		}
	protected:
		void OnPropertiesChanged()
		{
			RecreateMesh();
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::ConeComponent>;
	private:
		ConeComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_height = 1_meters;
		uint16 m_sideCount = 8;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::ConeComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::ConeComponent>(
			Entity::Primitives::ConeComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Cone"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"Radius",
					"{04EE99C2-255F-438B-8BA7-4DF75594BDF8}"_guid,
					MAKE_UNICODE_LITERAL("Cone"),
					&Entity::Primitives::ConeComponent::m_radius,
					&Entity::Primitives::ConeComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Height"),
					"Height",
					"{F96CF1B0-FE4D-41E8-A0FE-AD6E39619DA2}"_guid,
					MAKE_UNICODE_LITERAL("Cone"),
					&Entity::Primitives::ConeComponent::m_height,
					&Entity::Primitives::ConeComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{A3F5A102-02EF-46C7-895E-86317839B517}"_guid,
					MAKE_UNICODE_LITERAL("Cone"),
					&Entity::Primitives::ConeComponent::m_sideCount,
					&Entity::Primitives::ConeComponent::OnPropertiesChanged
				),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"bfc2359a-896f-01b0-16e7-e8bde8874068"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"58851447-cf0b-4971-8f86-931297306517"_asset
			}}
		);
	};
}
