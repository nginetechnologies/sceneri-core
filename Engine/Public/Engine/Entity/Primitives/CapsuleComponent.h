#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct CapsuleComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "5fa8c0ab-b2ea-4fe0-b630-cbbab19f0472"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				const Math::Radiusf radius = 0.5_meters,
				const Math::Lengthf height = 1_meters,
				const uint16 sideCount = 8,
				const uint16 segmentCount = 2
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_height(height)
				, m_sideCount(sideCount)
				, m_segmentCount(segmentCount)
			{
			}

			Math::Radiusf m_radius = 0.5_meters;
			Math::Lengthf m_height = 1_meters;
			uint16 m_sideCount = 8;
			uint16 m_segmentCount = 2;
		};

		CapsuleComponent(const CapsuleComponent& templateComponent, const Cloner& cloner);
		CapsuleComponent(const Deserializer& deserializer);
		CapsuleComponent(Initializer&& initializer);
		virtual ~CapsuleComponent() = default;

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
		void SetSegmentCount(const uint16 count)
		{
			m_segmentCount = count;
			OnPropertiesChanged();
		}
		[[nodiscard]] uint16 GetSegmentCount() const
		{
			return m_segmentCount;
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
		friend struct Reflection::ReflectedType<Entity::Primitives::CapsuleComponent>;
	private:
		CapsuleComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_height = 1_meters;
		uint16 m_sideCount = 8;
		uint16 m_segmentCount = 2;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::CapsuleComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::CapsuleComponent>(
			Entity::Primitives::CapsuleComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Capsule"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"Radius",
					"{607F7A67-84EE-46DF-9CE7-E623DA8A0D4C}"_guid,
					MAKE_UNICODE_LITERAL("Capsule"),
					&Entity::Primitives::CapsuleComponent::m_radius,
					&Entity::Primitives::CapsuleComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Height"),
					"Height",
					"{BBE428BA-3A51-48D8-A33D-ED90EDAD968F}"_guid,
					MAKE_UNICODE_LITERAL("Capsule"),
					&Entity::Primitives::CapsuleComponent::m_height,
					&Entity::Primitives::CapsuleComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{6B11C381-15A3-4E75-8DC6-7BFD81B3336E}"_guid,
					MAKE_UNICODE_LITERAL("Capsule"),
					&Entity::Primitives::CapsuleComponent::m_sideCount,
					&Entity::Primitives::CapsuleComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Segment Count"),
					"SegmentCount",
					"{5B614F06-BA22-4BBF-9729-6A5A0C71812B}"_guid,
					MAKE_UNICODE_LITERAL("Capsule"),
					&Entity::Primitives::CapsuleComponent::m_segmentCount,
					&Entity::Primitives::CapsuleComponent::OnPropertiesChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"10f8e621-370c-85e3-3f6d-b3e46eaa6f91"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"fa74dcfc-a979-48bd-8143-a291f2e3e74f"_asset
			}}
		);
	};
}
