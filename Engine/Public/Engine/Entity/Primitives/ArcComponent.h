#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct ArcComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "e7a4bda0-5026-4659-ab4e-2a344e0db437"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				const Math::Anglef angle = 90_degrees,
				const Math::Lengthf halfHeight = 0.5_meters,
				const Math::Radiusf outer = 1.0_meters,
				const Math::Radiusf inner = 0.5_meters,
				const uint16 sideCount = 16u
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_angle(angle)
				, m_halfHeight(halfHeight)
				, m_outer(outer)
				, m_inner(inner)
				, m_sideCount(sideCount)
			{
			}

			Math::Anglef m_angle = 90_degrees;
			Math::Lengthf m_halfHeight = 0.5_meters;
			Math::Radiusf m_outer = 1.0_meters;
			Math::Radiusf m_inner = 0.5_meters;
			uint16 m_sideCount = 16u;
		};

		ArcComponent(const ArcComponent& templateComponent, const Cloner& cloner);
		ArcComponent(const Deserializer& deserializer);
		ArcComponent(Initializer&& initializer);
		virtual ~ArcComponent() = default;

		void OnCreated();

		void SetAngle(const Math::Anglef angle)
		{
			m_angle = angle;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Anglef GetAngle() const
		{
			return m_angle;
		}

		void SetHalfHeight(const Math::Lengthf halfHeight)
		{
			m_halfHeight = halfHeight;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Lengthf GetHalfHeight() const
		{
			return m_halfHeight;
		}

		void SetOuterRadius(const Math::Radiusf outerRadius)
		{
			m_outer = outerRadius;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Radiusf GetOuterRadius() const
		{
			return m_outer;
		}

		void SetInnerRadius(const Math::Radiusf innerRadius)
		{
			m_inner = innerRadius;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Radiusf GetInnerRadius() const
		{
			return m_inner;
		}

		void SetSideCount(const uint16 count)
		{
			m_sideCount = count;
			OnPropertiesChanged();
		}
		[[nodiscard]] uint16 GetSideCount() const
		{
			return m_sideCount;
		}
	protected:
		void OnPropertiesChanged()
		{
			RecreateMesh();
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::ArcComponent>;
	private:
		ArcComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		Math::Anglef m_angle = 90_degrees;
		Math::Lengthf m_halfHeight = 0.5_meters;
		Math::Radiusf m_outer = 1.0_meters;
		Math::Radiusf m_inner = 0.5_meters;
		uint16 m_sideCount = 16u;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::ArcComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::ArcComponent>(
			Entity::Primitives::ArcComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Arc"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Angle"),
					"Angle",
					"{31603011-5CBD-461C-8565-1AA2269DEAE1}"_guid,
					MAKE_UNICODE_LITERAL("Arc"),
					&Entity::Primitives::ArcComponent::m_angle,
					&Entity::Primitives::ArcComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Half Height"),
					"HalfHeight",
					"{F8FC14B2-5FB5-4290-8748-849ACBF99AD9}"_guid,
					MAKE_UNICODE_LITERAL("Arc"),
					&Entity::Primitives::ArcComponent::m_halfHeight,
					&Entity::Primitives::ArcComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Outer Radius"),
					"OuterRadius",
					"{B93AB8D1-91CC-4341-8D54-EEDF5089F8F2}"_guid,
					MAKE_UNICODE_LITERAL("Arc"),
					&Entity::Primitives::ArcComponent::m_outer,
					&Entity::Primitives::ArcComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Inner Radius"),
					"InnerRadius",
					"{B29A0DEF-E425-4F0B-BF66-E5619AAEC9D9}"_guid,
					MAKE_UNICODE_LITERAL("Arc"),
					&Entity::Primitives::ArcComponent::m_inner,
					&Entity::Primitives::ArcComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{71E48B50-766F-4F23-B308-DC41D593A159}"_guid,
					MAKE_UNICODE_LITERAL("Arc"),
					&Entity::Primitives::ArcComponent::m_sideCount,
					&Entity::Primitives::ArcComponent::OnPropertiesChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"a7df8f08-85a9-f128-a4b0-d9b1454008a2"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"e245c668-0858-4e12-8600-17d111f1f98a"_asset
			}}
		);
	};
}
