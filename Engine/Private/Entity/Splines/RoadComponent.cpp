#include "Entity/Splines/RoadComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/Defaults.h>
#include <Renderer/Scene/SceneView.h>
#include <Common/Math/Tangents.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Math/Mod.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	RoadComponent::RoadComponent(const RoadComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_width(templateComponent.m_width)
		, m_sideSegmentCount(templateComponent.m_sideSegmentCount)
	{
	}

	RoadComponent::RoadComponent(const Deserializer& deserializer)
		: RoadComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<RoadComponent>().ToString().GetView()))
	{
	}

	// TODO: Simplify render mesh editing, shouldn't require a full mesh rebuild (only reupload changed parts)

	RoadComponent::RoadComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_width(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Width", (Math::Lengthf)1_meters)
																			: (Math::Lengthf)1_meters
			)
	{
	}

	RoadComponent::RoadComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_width(initializer.m_width)
		, m_sideSegmentCount(initializer.m_sideSegmentCount)
	{
	}

	void RoadComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();
		if (const Optional<SplineComponent*> pSpline = GetSpline())
		{
			pSpline->OnChanged.Add(
				*this,
				[](RoadComponent& road)
				{
					road.RecreateMesh();
				}
			);
		}

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentSoftReference reference(*this, sceneRegistry);
		SetMeshGeneration(
			[reference = Move(reference), &sceneRegistry](Rendering::StaticObject& object)
			{
				if (const Optional<Entity::Component3D*> pComponent = reference.Find<Entity::Component3D>(sceneRegistry))
				{
					RoadComponent& road = static_cast<RoadComponent&>(*pComponent);
					road.GenerateMesh(object);
				}
			}
		);
	}

	void RoadComponent::GenerateMesh(Rendering::StaticObject& staticObject)
	{
		Optional<const SplineComponent*> pSplineComponent = GetSpline();
		if (pSplineComponent.IsValid())
		{
			pSplineComponent->VisitSpline(
				[this, &staticObject](const Math::Splinef& spline)
				{
					GenerateMesh(staticObject, spline);
					return false;
				}
			);
		}
		else
		{
			Math::Splinef customSpline;
			customSpline.EmplacePoint(Math::Zero, Math::Up, false);
			customSpline.EmplacePoint({0.f, 1.f, 0.f}, Math::Up, false);
			GenerateMesh(staticObject, customSpline);
		}
	}

	void RoadComponent::GenerateMesh(Rendering::StaticObject& staticObject, const Math::Splinef& spline)
	{
		// Update from spline state
		const SegmentCountType numSegments = (SegmentCountType)spline.CalculateSegmentCount();
		const SideSegmentCountType numVerticesInShape = 1 + (m_sideSegmentCount * 2);
		const SideSegmentCountType numSideSegments = numVerticesInShape - 1;
		const Rendering::Index vertexCount = numVerticesInShape * numSegments;
		const Rendering::Index triangleCount = (numSegments - 1) * numSideSegments * 2;
		const Rendering::Index triangleIndexCount = triangleCount * 3;

		staticObject.Resize(Memory::Uninitialized, vertexCount, triangleIndexCount);

		ArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions = staticObject.GetVertexElementView<Rendering::VertexPosition>();
		ArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals = staticObject.GetVertexElementView<Rendering::VertexNormals>();
		ArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates =
			staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>();

		{
			float currentRoadLength = 0.f;

			spline.IterateAdjustedSplinePoints(
				[&currentRoadLength,
			   numVerticesInShape,
			   halfWidth = m_width * 0.5f,
			   vertexIndex = (Rendering::Index)0,
			   vertexPositions = vertexPositions.GetData(),
			   vertexNormals = vertexNormals.GetData(),
			   vertexTextureCoordinates = vertexTextureCoordinates.GetData()](
					const Math::Splinef::Point&,
					const Math::Splinef::Spline::Point&,
					const Math::Vector3f currentBezierPoint,
					const Math::Vector3f nextBezierPoint,
					const Math::Vector3f direction,
					const Math::Vector3f normal
				) mutable
				{
					const Math::LocalTransform::StoredRotationType segmentRotation(
						Math::Matrix3x3f(normal, direction, normal.Cross(direction).GetNormalized())
					);
					const Math::LocalTransform segmentTransform = Math::LocalTransform(segmentRotation, currentBezierPoint);

					constexpr float uvTilingX = 1.f;

					const float uvXSlice = Math::MultiplicativeInverse(float(numVerticesInShape - 1));

					float locationX = -halfWidth.GetMeters();
					const float locationXSlice = halfWidth.GetMeters() / (float)((numVerticesInShape - 1) / 2);

					for (SideSegmentCountType i = 0; i < numVerticesInShape; ++i)
					{
						vertexPositions[vertexIndex] = segmentTransform.TransformLocationWithoutScale({locationX, 0, 0});

						// TODO: Intersect into scene, and offset the point

						const Math::Vector3f segmentNormal = segmentTransform.GetUpColumn();
						vertexNormals[vertexIndex] = {
							segmentNormal,
							Math::CompressedTangent(segmentNormal, segmentTransform.GetForwardColumn(), segmentTransform.GetRightColumn()).m_tangent
						};

						vertexTextureCoordinates[vertexIndex] = {uvXSlice * i * uvTilingX, currentRoadLength};

						vertexIndex++;
						locationX += locationXSlice;
					}

					currentRoadLength += (currentBezierPoint - nextBezierPoint).GetLength();
				}
			);
		}

		ArrayView<Rendering::Index, Rendering::Index> indicesView = staticObject.GetIndices();

		for (SegmentCountType segmentIndex = 0; segmentIndex < (numSegments - 1); ++segmentIndex)
		{
			const Rendering::Index offset = segmentIndex * numVerticesInShape;
			for (SideSegmentCountType l = 0; l < numSideSegments; l++)
			{
				const Rendering::Index backLeft = offset + l;
				const Rendering::Index frontLeft = backLeft + numVerticesInShape;
				const Rendering::Index backRight = offset + l + 1;
				const Rendering::Index frontRight = backRight + numVerticesInShape;

				indicesView[0] = frontLeft;
				indicesView++;
				indicesView[0] = backLeft;
				indicesView++;
				indicesView[0] = backRight;
				indicesView++;
				indicesView[0] = backRight;
				indicesView++;
				indicesView[0] = frontRight;
				indicesView++;
				indicesView[0] = frontLeft;
				indicesView++;
			}
		}

		Assert(indicesView.IsEmpty());
	}

	// TODO: Equivalent to transient properties like UE
	// Allow computing something in Editor, store in a property and save with the level (+ serialize elsewhere)
	// Useful to avoid recomputing stuff, in our case detect the road's spline at edit time, and only update it when the parent changes.
	// This way we don't have to requery where the spline is
	// Property<SplineComponent*, Reflection::PropertyFlags::Transient> Spline;
	// Same can be used to cache the road mesh, avoiding recomputation
	Optional<SplineComponent*> RoadComponent::GetSpline() const
	{
		return FindFirstParentOfType<SplineComponent>();
	}

	[[maybe_unused]] const bool wasRoadRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<RoadComponent>>::Make());
	[[maybe_unused]] const bool wasRoadTypeRegistered = Reflection::Registry::RegisterType<RoadComponent>();
}
