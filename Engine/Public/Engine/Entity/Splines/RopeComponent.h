#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

#include <Renderer/Index.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include <Common/Math/ClampedValue.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Entity
{
	struct SplineComponent;

	struct RopeComponent : public StaticMeshComponent
	{
		static constexpr Guid TypeGuid = "6fac4820-a0c3-4e4c-a730-d15a30777f90"_guid;

		using BaseType = StaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using SegmentCountType = uint16;
		using SideCountType = uint16;
		using SideCountPropertyType = Math::ClampedValue<SideCountType>;

		struct Initializer : public StaticMeshComponent::Initializer
		{
			using BaseType = StaticMeshComponent::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, const Math::Radiusf radius = 0.5_meters, const SideCountType sideCount = 2)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_sideCount{sideCount, 2, 1024}
			{
			}

			Math::Radiusf m_radius = 0.5_meters;
			SideCountPropertyType m_sideCount = {8, 2, 1024};
		};

		RopeComponent(const RopeComponent& templateComponent, const Cloner& cloner);
		RopeComponent(const Deserializer& deserializer);
		RopeComponent(Initializer&& initializer);
		virtual ~RopeComponent();

		void OnCreated();

		[[nodiscard]] Optional<SplineComponent*> GetSpline() const;

		void SetRadius(const Math::Radiusf radius)
		{
			m_radius = radius;
		}
		[[nodiscard]] Math::Radiusf GetRadius() const
		{
			return m_radius;
		}

		void SetSideCount(const SideCountType count)
		{
			m_sideCount = count;
		}

		void RecreateMesh();

		using SimulatedSplineType = Math::Splinef;
		[[nodiscard]] SimulatedSplineType& GetSimulatedSpline()
		{
			return m_simulatedSpline;
		}

		struct RopeSide
		{
			Math::Anglef sin;
			Math::Anglef cos;
		};
	protected:
		friend struct Reflection::ReflectedType<Entity::RopeComponent>;
		Math::Radiusf m_radius = 0.5_meters;
		SideCountPropertyType m_sideCount = {8, 2, 1024};
	protected:
		[[nodiscard]] Rendering::StaticMeshIdentifier CreateRopeMesh();
		[[nodiscard]] Threading::JobBatch GenerateMesh();
		void GenerateMesh(
			const Math::Splinef& spline,
			const SideCountType numSides,
			const Rendering::StaticMeshIdentifier meshIdentifier,
			const float sideSlice,
			const ArrayView<const RopeSide, uint16> ropeSides
		);

		[[nodiscard]] static constexpr Rendering::Index GetMainRopeIndexCount(const SegmentCountType numSegments, const SideCountType numSides)
		{
			return (numSegments - 1u) * numSides * 6u;
		}
		[[nodiscard]] static constexpr Rendering::Index GetRopeCapIndexCount(const SideCountType numSides)
		{
			return (numSides - 2u) * 3u;
		}
		[[nodiscard]] static constexpr Rendering::Index GetVertexCount(const SegmentCountType numSegments, const SideCountType numSides)
		{
			return (numSides + 1u) * numSegments;
		}
		[[nodiscard]] static constexpr SegmentCountType GetSegmentCount(const Rendering::Index vertexCount, const SideCountType numSides)
		{
			return SegmentCountType(vertexCount / (numSides + 1u));
		}
	private:
		RopeComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	private:
		inline static constexpr Rendering::Index PrecachedVertexCount = 20000;
		inline static constexpr Rendering::Index PrecachedIndexCount = 20000;

		Threading::Mutex m_meshGenerationMutex;
		SimulatedSplineType m_simulatedSpline;
		Array<Rendering::StaticObject, 2> m_staticObjectCache{
			Rendering::StaticObject{Memory::Reserve, PrecachedVertexCount, PrecachedIndexCount},
			Rendering::StaticObject{Memory::Reserve, PrecachedVertexCount, PrecachedIndexCount}
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RopeComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RopeComponent>(
			Entity::RopeComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Rope"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"Radius",
					"{04919E18-39BB-4CEC-82E7-84BD1B1F2115}"_guid,
					MAKE_UNICODE_LITERAL("Rope"),
					&Entity::RopeComponent::m_radius
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Sides"),
					"Sides",
					"{3E47F191-C883-497A-BB29-67F4F9042FAD}"_guid,
					MAKE_UNICODE_LITERAL("Rope"),
					&Entity::RopeComponent::m_sideCount
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"31b7f11e-05ce-d156-9513-eed1d3978f78"_asset,
				"5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid,
				"f7c8a986-a495-4d74-9f47-84eee0c7f56d"_asset
			}}
		);
	};
}
