#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct ArrowComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "468a0ab0-1279-450e-bf8a-c9af3bf93921"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer,
				const Math::Radiusf shaftRadius = 0.05_meters,
				const Math::Lengthf shaftHeight = 0.75_meters,
				const Math::Radiusf tipRadius = 0.3_meters,
				const Math::Lengthf tipHeight = 0.25_meters,
				const uint16 sideCount = 8u
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_shaftRadius(shaftRadius)
				, m_shaftHeight(shaftHeight)
				, m_tipRadius(tipRadius)
				, m_tipHeight(tipHeight)
				, m_sideCount(sideCount)
			{
			}

			// Radius of the arrow shaft.
			Math::Radiusf m_shaftRadius = 0.05_meters;
			// The height of the arrow shaft.
			Math::Lengthf m_shaftHeight = 0.75_meters;
			// Radius of the arrow head.
			Math::Radiusf m_tipRadius = 0.3_meters;
			// The height of the arrow head.
			Math::Lengthf m_tipHeight = 0.25_meters;
			// Determines how many sides/polygons the arrow has.
			uint16 m_sideCount = 8u;
		};

		ArrowComponent(const ArrowComponent& templateComponent, const Cloner& cloner);
		ArrowComponent(const Deserializer& deserializer);
		ArrowComponent(Initializer&& initializer);
		virtual ~ArrowComponent() = default;

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

		void SetShaftRadius(const Math::Radiusf radius)
		{
			m_shaftRadius = radius;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Radiusf GetShaftRadius() const
		{
			return m_shaftRadius;
		}

		void SetTipRadius(const Math::Radiusf radius)
		{
			m_tipRadius = radius;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Radiusf GetTipRadius() const
		{
			return m_tipRadius;
		}

		void SetShaftLength(const Math::Lengthf height)
		{
			m_shaftHeight = height;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Lengthf GetShaftLength() const
		{
			return m_shaftHeight;
		}

		void SetTipLength(const Math::Lengthf height)
		{
			m_tipHeight = height;
			OnPropertiesChanged();
		}
		[[nodiscard]] Math::Lengthf GetTipLength() const
		{
			return m_tipHeight;
		}
	protected:
		void OnPropertiesChanged()
		{
			RecreateMesh();
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::ArrowComponent>;
	private:
		ArrowComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		// Radius of the arrow shaft.
		Math::Radiusf m_shaftRadius = 0.05_meters;
		// The height of the arrow shaft.
		Math::Lengthf m_shaftHeight = 0.75_meters;
		// Radius of the arrow head.
		Math::Radiusf m_tipRadius = 0.3_meters;
		// The height of the arrow head.
		Math::Lengthf m_tipHeight = 0.25_meters;
		// Determines how many sides/polygons the arrow has.
		uint16 m_sideCount = 8u;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::ArrowComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::ArrowComponent>(
			Entity::Primitives::ArrowComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Arrow"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Side Count"),
					"SideCount",
					"{8925C7EB-31F0-4090-8FCD-278E26511DAA}"_guid,
					MAKE_UNICODE_LITERAL("Arrow"),
					&Entity::Primitives::ArrowComponent::m_sideCount,
					&Entity::Primitives::ArrowComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Shaft Radius"),
					"ShaftRadius",
					"{88270863-3FC8-4EFD-8520-D58E1A3B26BD}"_guid,
					MAKE_UNICODE_LITERAL("Arrow"),
					&Entity::Primitives::ArrowComponent::m_shaftRadius,
					&Entity::Primitives::ArrowComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Tip Radius"),
					"TipRadius",
					"{25E4E167-0E11-41E9-A1A0-4EDBC70F94A2}"_guid,
					MAKE_UNICODE_LITERAL("Arrow"),
					&Entity::Primitives::ArrowComponent::m_tipRadius,
					&Entity::Primitives::ArrowComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Shaft Height"),
					"ShaftHeight",
					"{E1DDA2C3-340A-4AD0-8E6C-93FE22B34057}"_guid,
					MAKE_UNICODE_LITERAL("Arrow"),
					&Entity::Primitives::ArrowComponent::m_shaftHeight,
					&Entity::Primitives::ArrowComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Tip Height"),
					"TipHeight",
					"{8E219423-CC51-4266-AB06-82A93E4071FB}"_guid,
					MAKE_UNICODE_LITERAL("Arrow"),
					&Entity::Primitives::ArrowComponent::m_tipHeight,
					&Entity::Primitives::ArrowComponent::OnPropertiesChanged
				),
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"d464f2b3-87bb-7433-dcd3-789b045402b6"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"a86b211f-6098-4b67-9228-f29856375e9f"_asset
			}}
		);
	};
}
