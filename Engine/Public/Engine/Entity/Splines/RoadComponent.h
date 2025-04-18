#pragma once

#include <Engine/Entity/ProceduralStaticMeshComponent.h>

#include <Common/Math/Primitives/ForwardDeclarations/Spline.h>

namespace ngine::Entity
{
	struct SplineComponent;

	struct RoadComponent : public ProceduralStaticMeshComponent
	{
		static constexpr Guid TypeGuid = "b2949dcf-7d3e-4ac7-962a-29685a44d92c"_guid;

		using BaseType = ProceduralStaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using SegmentCountType = uint16;
		using SideSegmentCountType = uint16;

		struct Initializer : public ProceduralStaticMeshComponent::Initializer
		{
			using BaseType = ProceduralStaticMeshComponent::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, Math::Lengthf width = 1_meters, SideSegmentCountType sideSegmentCount = 2)
				: BaseType(Forward<BaseType>(initializer))
				, m_width(width)
				, m_sideSegmentCount(sideSegmentCount)
			{
			}

			Math::Lengthf m_width = 1_meters;
			SideSegmentCountType m_sideSegmentCount = 2;
		};

		RoadComponent(const RoadComponent& templateComponent, const Cloner& cloner);
		RoadComponent(const Deserializer& deserializer);
		RoadComponent(Initializer&& initializer);
		virtual ~RoadComponent() = default;

		void OnCreated();

		[[nodiscard]] Optional<SplineComponent*> GetSpline() const;

		void SetWidth(const Math::Lengthf width)
		{
			m_width = width;
		}
		[[nodiscard]] Math::Lengthf GetWidth() const
		{
			return m_width;
		}

		void SetSideSegmentCount(const SideSegmentCountType count)
		{
			m_sideSegmentCount = count;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::RoadComponent>;
		// TODO: Expose these per segment
		Math::Lengthf m_width = 1.0_meters;
		SideSegmentCountType m_sideSegmentCount = 2;
	protected:
		void GenerateMesh(Rendering::StaticObject& staticObjectOut);
		void GenerateMesh(Rendering::StaticObject& staticObjectOut, const Math::Splinef& spline);
	private:
		RoadComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RoadComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RoadComponent>(
			Entity::RoadComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Road"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Width"),
					"Width",
					"{36337514-3670-4123-ADA9-5A476B6E1FEE}"_guid,
					MAKE_UNICODE_LITERAL("Road"),
					&Entity::RoadComponent::m_width
				) /*,
		     Reflection::MakeProperty(
		       MAKE_UNICODE_LITERAL("SegmentCount"),
		       MAKE_UNICODE_LITERAL("SegmentCount"),
		       MAKE_UNICODE_LITERAL("Road"),
		       &Entity::RoadComponent::m_sideSegmentCount
		     )*/
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"31b7f11e-05ce-d156-9513-eed1d3978f78"_asset,
				"5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid,
				"d6cb4275-90f9-4cd2-85db-baa351c7ee11"_asset
			}}
		);
	};
}
