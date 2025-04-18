#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct PyramidComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "fd324834-3dd7-4d38-9a3c-efc7da3a1500"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Math::Radiusf radius = 0.5_meters, const Math::Lengthf height = 1_meters)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_height(height)
			{
			}

			Math::Radiusf m_radius = 0.5_meters;
			Math::Lengthf m_height = 1_meters;
		};

		PyramidComponent(const PyramidComponent& templateComponent, const Cloner& cloner);
		PyramidComponent(const Deserializer& deserializer);
		PyramidComponent(Initializer&& initializer);
		virtual ~PyramidComponent() = default;

		void OnCreated();

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
		friend struct Reflection::ReflectedType<Entity::Primitives::PyramidComponent>;
	private:
		PyramidComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	protected:
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_height = 1_meters;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::PyramidComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::PyramidComponent>(
			Entity::Primitives::PyramidComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Pyramid"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"Radius",
					"{34AA3910-4016-45C3-826C-A622E68F0321}"_guid,
					MAKE_UNICODE_LITERAL("Pyramid"),
					&Entity::Primitives::PyramidComponent::m_radius,
					&Entity::Primitives::PyramidComponent::OnPropertiesChanged
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Height"),
					"Height",
					"{CFD9003D-F511-435E-B552-663C5D547821}"_guid,
					MAKE_UNICODE_LITERAL("Pyramid"),
					&Entity::Primitives::PyramidComponent::m_height,
					&Entity::Primitives::PyramidComponent::OnPropertiesChanged
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"c500f785-c6b3-da15-5866-8d933034d783"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"f6a4ea74-7b33-49d0-893b-4d6cb4410173"_asset
			}}
		);
	};
}
