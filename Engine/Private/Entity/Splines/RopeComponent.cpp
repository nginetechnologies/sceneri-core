#include "Entity/Splines/RopeComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/HierarchyComponent.inl>

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
#include <Common/Memory/AddressOf.h>

namespace ngine::Entity
{
	RopeComponent::RopeComponent(const RopeComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_sideCount(templateComponent.m_sideCount)
	{
		SetMesh(*System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(CreateRopeMesh()).m_pMesh);
		RecreateMesh();
	}

	RopeComponent::RopeComponent(const Deserializer& deserializer)
		: RopeComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<RopeComponent>().ToString().GetView()))
	{
	}

	// TODO: Simplify render mesh editing, shouldn't require a full mesh rebuild (only reupload changed parts)

	inline Rendering::StaticMeshIdentifier RopeComponent::CreateRopeMesh()
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();

		return meshCache.Create(
			[this]([[maybe_unused]] const Rendering::StaticMeshIdentifier identifier) -> Threading::JobBatch
			{
				Assert(GetMeshIdentifier() == identifier);
				return GenerateMesh();
			}
		);
	}

	RopeComponent::RopeComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: StaticMeshComponent(deserializer)
		, m_radius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Radiusf>("Radius", 0.5_meters)
																			: (Math::Radiusf)0.5_meters
			)
		, m_sideCount(
				componentSerializer.IsValid()
					? componentSerializer->ReadWithDefaultValue<SideCountPropertyType>("Sides", SideCountPropertyType{8, 2, 1024})
					: SideCountPropertyType{8, 2, 1024}
			)
	{
		SetMesh(*System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(CreateRopeMesh()).m_pMesh);
	}

	[[nodiscard]] inline Rendering::MaterialInstanceIdentifier GetDefaultMaterialInstanceIdentifier()
	{
		return System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(
			Rendering::Constants::DefaultMaterialInstanceAssetGuid
		);
	}

	RopeComponent::RopeComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{initializer}, CreateRopeMesh(), initializer.m_materialInstanceIdentifier
			})
		, m_radius(initializer.m_radius)
		, m_sideCount(initializer.m_sideCount)
	{
	}

	RopeComponent::~RopeComponent()
	{
		System::Get<Rendering::Renderer>().GetMeshCache().Remove(GetMeshIdentifier());

		Threading::UniqueLock lock(m_meshGenerationMutex);
	}

	void RopeComponent::OnCreated()
	{
		StaticMeshComponent::OnCreated();
		GetSpline()->OnChanged.Add(
			*this,
			[](RopeComponent& rope)
			{
				rope.RecreateMesh();
			}
		);
	}

	inline void CreateVerticesForSegment(
		const float textureCoordinateY,
		Rendering::Index& vertexIndex,
		Rendering::VertexPosition* const vertexPositions,
		Rendering::VertexNormals* const vertexNormals,
		Rendering::VertexTextureCoordinate* const vertexTextureCoordinates,
		const Math::LocalTransform& __restrict segmentTransform,
		const float sideSlice,
		const ArrayView<const RopeComponent::RopeSide, uint16> ropeSides
	)
	{
		constexpr float uvTilingX = 1.f;

		const Rendering::VertexPosition& firstVertexPosition = vertexPositions[vertexIndex];
		const Rendering::VertexNormals& firstVertexNormals = vertexNormals[vertexIndex];
		const Rendering::VertexTextureCoordinate& firstVertexTextureCoordinate = vertexTextureCoordinates[vertexIndex];

		for (const RopeComponent::RopeSide& __restrict ropeSide : ropeSides)
		{
			const Math::Vector3f vertexOffset =
				segmentTransform.TransformDirectionWithoutScale({ropeSide.cos.GetRadians(), 0, ropeSide.sin.GetRadians()});

			vertexPositions[vertexIndex] = segmentTransform.GetLocation() + vertexOffset;

			const Math::Vector3f sideNormal = vertexOffset.GetNormalized();
			vertexNormals[vertexIndex] = {
				sideNormal,
				Math::CompressedTangent(sideNormal, segmentTransform.GetUpColumn(), segmentTransform.GetRightColumn()).m_tangent
			};

			const uint16 sideIndex = ropeSides.GetIteratorIndex(Memory::GetAddressOf(ropeSide));
			const float sideRatio = static_cast<float>(sideIndex) * sideSlice;

			vertexTextureCoordinates[vertexIndex] = {sideRatio * uvTilingX, textureCoordinateY};

			vertexIndex++;
		}

		vertexPositions[vertexIndex] = firstVertexPosition;
		vertexNormals[vertexIndex] = firstVertexNormals;
		vertexTextureCoordinates[vertexIndex] = firstVertexTextureCoordinate;
		vertexTextureCoordinates[vertexIndex].x = ropeSides.GetSize() * sideSlice * uvTilingX;
		vertexIndex++;
	}

	Threading::JobBatch RopeComponent::GenerateMesh()
	{
		Threading::UniqueLock lock(Threading::TryLock, m_meshGenerationMutex);
		if (lock.IsLocked())
		{
			Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
			Entity::ComponentSoftReference reference(*this, sceneRegistry);
			return Threading::CreateCallback(
				[reference = Move(reference), &sceneRegistry, lock = Move(lock)](Threading::JobRunnerThread&)
				{
					if (const Optional<Entity::Component3D*> pComponent = reference.Find<Entity::Component3D>(sceneRegistry))
					{
						RopeComponent& __restrict rope = static_cast<RopeComponent&>(*pComponent);
						const Optional<const Rendering::StaticMesh*> pMesh = rope.GetMesh();
						Assert(pMesh.IsValid());
						if (UNLIKELY_ERROR(pMesh.IsInvalid()))
						{
							return;
						}

						const Math::Radiusf radius = rope.m_radius;
						const SideCountType sideCount = rope.m_sideCount;
						SimulatedSplineType& __restrict simulatedSpline = rope.GetSimulatedSpline();

						const Rendering::StaticMesh& __restrict staticMesh = *pMesh;
						const Rendering::StaticMeshIdentifier meshIdentifier = staticMesh.GetIdentifier();
						const Rendering::Index meshVertexCount = staticMesh.GetVertexCount();

						const float sideSlice = Math::MultiplicativeInverse((float)sideCount);

						FlatVector<RopeSide, 256> ropeSides(Memory::ConstructWithSize, Memory::Uninitialized, sideCount);
						for (auto it = ropeSides.begin(), end = ropeSides.end(); it != end; ++it)
						{
							const Math::Anglef angle = Math::PI * 2.f * sideSlice * static_cast<float>(ropeSides.GetIteratorIndex(it));
							it->sin = angle.Sin() * radius.GetMeters();
							it->cos = angle.Cos() * radius.GetMeters();
						}

						if (simulatedSpline.HasPoints())
						{
							const SegmentCountType segmentCount = GetSegmentCount(meshVertexCount, sideCount);
							const uint32 numSubdivisions = 32;

							Assert(GetVertexCount(segmentCount, sideCount) == meshVertexCount);

							float currentRopeLength = 0.f;

							Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
							Rendering::StaticMesh& mesh = *meshCache.GetAssetData(meshIdentifier).m_pMesh;

							const float interpolatedSplineLength = simulatedSpline.CalculateSplineLength();
							const float interpolatedSegmentLength = interpolatedSplineLength / (float)segmentCount;

							float uvLength = 0.f;
							Math::Vector3f lastPointPosition = Math::Zero;

							simulatedSpline.IterateAdjustedSplinePoints(
								[&currentRopeLength,
						     &uvLength,
						     &lastPointPosition,
						     interpolatedSegmentLength,
						     &ropeSides,
						     sideSlice,
						     vertexIndex = (Rendering::Index)0,
						     vertexPositions = mesh.GetVertexPositions().GetData(),
						     vertexNormals = mesh.GetVertexNormals().GetData(),
						     vertexTextureCoordinates = mesh.GetVertexTextureCoordinates().GetData()](
									const SimulatedSplineType::Spline::Point&,
									const SimulatedSplineType::Spline::Point&,
									const Math::Vector3f currentBezierPoint,
									const Math::Vector3f nextBezierPoint,
									const Math::Vector3f direction,
									const Math::Vector3f normal
								) mutable
								{
									const Math::Vector3f currentBezierDistance = nextBezierPoint - currentBezierPoint;

									const float currentBezierSegmentLength = currentBezierDistance.GetLength();
									currentRopeLength += currentBezierSegmentLength;
									while (currentRopeLength - interpolatedSegmentLength > 0)
									{
										const float remainingLength = currentRopeLength - interpolatedSegmentLength;
										const float remainingRatio = remainingLength / currentBezierSegmentLength;
										const float usedRatio = 1.f - remainingRatio;

										const Math::Vector3f currentSegmentLocation = currentBezierPoint + currentBezierDistance * usedRatio;

										const Math::LocalTransform::StoredRotationType segmentRotation(
											Math::Matrix3x3f(normal, direction, normal.Cross(direction).GetNormalized())
										);
										const Math::LocalTransform segmentTransform = Math::LocalTransform(segmentRotation, currentSegmentLocation);

										CreateVerticesForSegment(
											uvLength,
											vertexIndex,
											vertexPositions,
											vertexNormals,
											vertexTextureCoordinates,
											segmentTransform,
											sideSlice,
											ropeSides
										);
										uvLength += (lastPointPosition - segmentTransform.GetLocation()).GetLength();
										lastPointPosition = segmentTransform.GetLocation();

										currentRopeLength -= interpolatedSegmentLength;
									}
								},
								numSubdivisions
							);

							mesh.GetStaticObjectData().CalculateAndSetBoundingBox();

							meshCache.OnMeshLoaded(meshIdentifier);
						}
						else
						{
							Optional<const SplineComponent*> pSplineComponent = rope.GetSpline();
							if (pSplineComponent.IsValid())
							{
								pSplineComponent->VisitSpline(
									[&rope, sideCount, meshIdentifier, sideSlice, ropeSides = ropeSides.GetView()](const Math::Splinef& spline)
									{
										rope.GenerateMesh(spline, sideCount, meshIdentifier, sideSlice, ropeSides);
										return false;
									}
								);
							}
							else
							{
								Math::Splinef customSpline;
								customSpline.EmplacePoint(Math::Zero, Math::Up);
								customSpline.EmplacePoint({0.f, 1.f, 0.f}, Math::Up);
								rope.GenerateMesh(customSpline, sideCount, meshIdentifier, sideSlice, ropeSides);
							}
						}
					}
				},
				Threading::JobPriority::CreateProceduralMesh
			);
		}
		else
		{
			return {};
		}
	}

	void RopeComponent::GenerateMesh(
		const Math::Splinef& spline,
		const SideCountType sideCount,
		const Rendering::StaticMeshIdentifier meshIdentifier,
		const float sideSlice,
		const ArrayView<const RopeSide, uint16> ropeSides
	)
	{
		// Update from spline state
		const SegmentCountType numSegments = (SegmentCountType)spline.CalculateSegmentCount();

		const uint32 vertexCount = GetVertexCount(numSegments, sideCount);
		const Rendering::Index indexCount = GetMainRopeIndexCount(numSegments, sideCount) + GetRopeCapIndexCount(sideCount) * 2u;

		const FixedArrayView<Rendering::StaticObject, 2> staticObjectCache = m_staticObjectCache.GetView();
		Rendering::StaticObject staticObject = staticObjectCache[0].GetVertexCount() > 0 ? Move(staticObjectCache[0])
		                                                                                 : Move(staticObjectCache[1]);
		staticObject.Resize(Memory::Uninitialized, vertexCount, indexCount);

		{
			float currentRopeLength = 0.f;

			Math::WorldCoordinate lastLocation;

			spline.IterateAdjustedSplinePoints(
				[&currentRopeLength,
			   &ropeSides,
			   sideSlice,
			   vertexIndex = (Rendering::Index)0,
			   vertexPositions = staticObject.GetVertexElementView<Rendering::VertexPosition>().GetData(),
			   vertexNormals = staticObject.GetVertexElementView<Rendering::VertexNormals>().GetData(),
			   vertexTextureCoordinates = staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>().GetData()](
					const Math::Splinef::Point&,
					const Math::Splinef::Point&,
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

					CreateVerticesForSegment(
						currentRopeLength,
						vertexIndex,
						vertexPositions,
						vertexNormals,
						vertexTextureCoordinates,
						segmentTransform,
						sideSlice,
						ropeSides
					);

					currentRopeLength += (currentBezierPoint - nextBezierPoint).GetLength();
				}
			);
		}

		ArrayView<Rendering::Index, Rendering::Index> indicesView = staticObject.GetIndices();
		const Rendering::Index endVertexIndex = GetVertexCount(numSegments - 1u, sideCount);

		auto createCap = [sideCount, &indicesView](Rendering::Index firstIndex, Rendering::Index secondIndex, const Rendering::Index thirdIndex)
		{
			for (uint32 i = 0u, n = sideCount - 2u; i < n; i++, indicesView += 3u)
			{
				indicesView[0] = firstIndex++;
				indicesView[1] = secondIndex++;
				indicesView[2] = thirdIndex;
			}
		};

		// Create a cap that covers up the start
		createCap(1u, 2u, 0u);

		for (Rendering::Index startIndex = 0u; startIndex < endVertexIndex; startIndex += sideCount + 1u)
		{
			for (Rendering::Index i = startIndex, n = startIndex + sideCount; i < n; ++i, indicesView += 6u)
			{
				indicesView[0] = i;
				indicesView[2] = indicesView[3] = indicesView[0] + 1u;
				indicesView[1] = indicesView[4] = indicesView[2] + sideCount;
				indicesView[5] = indicesView[1] + 1u;
			}
		}

		// Do the same for the end
		createCap(endVertexIndex + 2u, endVertexIndex + 1u, endVertexIndex);

		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		Rendering::StaticMesh& mesh = *meshCache.GetAssetData(meshIdentifier).m_pMesh;

		staticObject.CalculateAndSetBoundingBox();
		Rendering::StaticObject previousObject = mesh.SetStaticObjectData(Move(staticObject));
		if (staticObjectCache[0].GetVertexCount() == 0)
		{
			staticObjectCache[0] = Move(previousObject);
		}
		else
		{
			Assert(staticObjectCache[1].GetVertexCount() > 0);
			staticObjectCache[1] = Move(previousObject);
		}

		meshCache.OnMeshLoaded(meshIdentifier);
	}

	void RopeComponent::RecreateMesh()
	{
		if (const Optional<const Rendering::StaticMesh*> pMesh = GetMesh())
		{
			Threading::JobBatch jobBatch = System::Get<Rendering::Renderer>().GetMeshCache().ReloadMesh(pMesh->GetIdentifier());
			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
		}
	}

	// TODO: Equivalent to transient properties like UE
	// Allow computing something in Editor, store in a property and save with the level (+ serialize elsewhere)
	// Useful to avoid recomputing stuff, in our case detect the rope's spline at edit time, and only update it when the parent changes.
	// This way we don't have to requery where the spline is
	// Property<SplineComponent*, Reflection::PropertyFlags::Transient> Spline;
	// Same can be used to cache the rope mesh, avoiding recomputation
	Optional<SplineComponent*> RopeComponent::GetSpline() const
	{
		return FindFirstParentOfType<SplineComponent>();
	}

	[[maybe_unused]] const bool wasRopeRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<RopeComponent>>::Make());
	[[maybe_unused]] const bool wasRopeTypeRegistered = Reflection::Registry::RegisterType<RopeComponent>();
}
