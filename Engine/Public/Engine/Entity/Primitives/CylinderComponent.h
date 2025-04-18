#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct CylinderComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "968abbdf-84eb-40c3-bff7-79635e33fdc3"_guid;

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

		CylinderComponent(const CylinderComponent& templateComponent, const Cloner& cloner);
		CylinderComponent(const Deserializer& deserializer);
		CylinderComponent(Initializer&& initializer);
		virtual ~CylinderComponent() = default;

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
		friend struct Reflection::ReflectedType<Entity::Primitives::CylinderComponent>;
	private:
		CylinderComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
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
	struct ReflectedType<Entity::Primitives::CylinderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::CylinderComponent>(
			Entity::Primitives::CylinderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Cylinder"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"Radius",
					"{C393516A-F022-48DD-AE19-0E0F72B326F6}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder"),
					&Entity::Primitives::CylinderComponent::m_radius,
					&Entity::Primitives::CylinderComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Height"),
					"Height",
					"{D8F6790B-2C89-45EA-9013-A02C23A489F5}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder"),
					&Entity::Primitives::CylinderComponent::m_height,
					&Entity::Primitives::CylinderComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{D0C85F33-B967-46BB-AB29-9E726AC3163F}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder"),
					&Entity::Primitives::CylinderComponent::m_sideCount,
					&Entity::Primitives::CylinderComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Segment Count"),
					"SegmentCount",
					"{CA1DCCAA-5BB7-47E9-8EE4-F1E59518303D}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder"),
					&Entity::Primitives::CylinderComponent::m_segmentCount,
					&Entity::Primitives::CylinderComponent::OnPropertiesChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"62a86996-661f-715a-f9ca-75d38106bbc3"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"80c0d946-8019-4b12-976f-1bbe6502c32d"_asset
			}}
		);
	};
}
