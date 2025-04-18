#include "Entity/Splines/ProceduralSplineMeshComponent.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/StaticMeshComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/ProceduralStaticMeshComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Stages/MaterialsStage.h>
#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/MeshCache.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>
#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/NumericLimits.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Memory/InlineDynamicBitset.h>

#include <Common/Serialization/Guid.h>
#include <Common/Math/Primitives/Spline.h>

namespace ngine::Entity
{
	ProceduralSplineMeshComponent::ProceduralSplineMeshComponent(const ProceduralSplineMeshComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_assetGuid(templateComponent.m_assetGuid)
	{
	}

	ProceduralSplineMeshComponent::ProceduralSplineMeshComponent(const Deserializer& deserializer)
		: ProceduralSplineMeshComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ProceduralSplineMeshComponent>().ToString().GetView())
			)
	{
	}

	ProceduralSplineMeshComponent::ProceduralSplineMeshComponent(
		const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer
	)
		: Component3D(deserializer)
	{
	}

	ProceduralSplineMeshComponent::ProceduralSplineMeshComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_assetGuid(initializer.m_assetGuid)
	{
	}

	ProceduralSplineMeshComponent::~ProceduralSplineMeshComponent()
	{
	}

	void ProceduralSplineMeshComponent::OnCreated()
	{
		LoadAsset(
			[this]()
			{
				CreateMesh();
			}
		);
	}

	Optional<SplineComponent*> ProceduralSplineMeshComponent::GetSpline() const
	{
		return FindFirstParentOfType<SplineComponent>();
	}

	void ProceduralSplineMeshComponent::RecreateMesh()
	{
		// TODO: should be better!
		for (Entity::Component3D& component : GetChildren())
		{
			Entity::ProceduralStaticMeshComponent& meshComponent = static_cast<Entity::ProceduralStaticMeshComponent&>(component);
			meshComponent.RecreateMesh();
		}
	}

	void ProceduralSplineMeshComponent::LoadAsset(OnLoadedCallback&& callback)
	{
		Assert(m_meshIdentifiers.IsEmpty());
		Assert(m_assetGuid.IsValid());

		Guid assetTypeGuid = System::Get<Asset::Manager>().GetAssetTypeGuid(m_assetGuid);
		Assert(assetTypeGuid.IsValid(), "Invalid asset was selected!");

		if (SupportedAssetTypes.GetView().Contains(assetTypeGuid))
		{
			LoadSceneAsset(m_assetGuid, Forward<OnLoadedCallback>(callback));
		}
		else
		{
			Assert(false, "Asset type not supported!");
		}
	}

	void ProceduralSplineMeshComponent::GetStaticMeshesRecursive(const Entity::Component3D& component, Vector<MeshInfo>& meshIdentifiers)
	{
		for (const Entity::Component3D& componentChild : component.GetChildren())
		{
			if (componentChild.IsStaticMesh(component.GetSceneRegistry()))
			{
				const Entity::StaticMeshComponent& meshComponent = componentChild.AsExpected<Entity::StaticMeshComponent>();

				ProceduralSplineMeshComponent::MeshInfo info;
				info.materialInstanceIdentifier = meshComponent.GetMaterialInstanceIdentifier();
				info.meshIdentifier = meshComponent.GetMeshIdentifier();
				info.stageMask = meshComponent.GetStageMask();
				meshIdentifiers.EmplaceBack(Move(info));
			}
			else
			{
				GetStaticMeshesRecursive(componentChild, meshIdentifiers);
			}
		}
	}

	void ProceduralSplineMeshComponent::LoadSceneAsset(Asset::Guid guid, OnLoadedCallback&& callback)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		Threading::JobBatch sceneLoadBatch = sceneTemplateCache.TryLoadScene(
			sceneTemplateCache.FindOrRegister(guid),
			Entity::ComponentTemplateCache::LoadListenerData(
				*this,
				[&sceneTemplateCache,
		     callback = Forward<OnLoadedCallback>(callback
		     )](ProceduralSplineMeshComponent& meshComponent, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier) mutable
				{
					Optional<Entity::Component3D*> pTemplateComponent = sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;
					Assert(pTemplateComponent.IsValid());
					if (LIKELY(pTemplateComponent.IsValid()))
					{
						GetStaticMeshesRecursive(*pTemplateComponent, meshComponent.m_meshIdentifiers);

						// Counter used to check if all meshes have been loaded.
						Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
						Threading::JobBatch loadMeshesBatch{
							Threading::CreateIntermediateStage("Load Procedural Meshes Start Stage"),
							Threading::CreateIntermediateStage("Load Procedural Meshes Finish Stage")
						};

						for (const MeshInfo& meshInfo : meshComponent.m_meshIdentifiers)
						{
							Rendering::StaticMeshInfo& info = meshCache.GetAssetData(meshInfo.meshIdentifier);
							if (info.m_pMesh == nullptr || !info.m_pMesh->IsLoaded())
							{
								Threading::IntermediateStage& finishedLoadingMeshStage = Threading::CreateIntermediateStage("Finish Mesh Loading Stage");
								finishedLoadingMeshStage.AddSubsequentStage(loadMeshesBatch.GetFinishedStage());

								Threading::JobBatch loadJobBatch = meshCache.TryLoadStaticMesh(
									meshInfo.meshIdentifier,
									Rendering::MeshCache::MeshLoadListenerData{
										&meshComponent,
										[&finishedLoadingMeshStage](ProceduralSplineMeshComponent&, const Rendering::StaticMeshIdentifier)
										{
											finishedLoadingMeshStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
											return EventCallbackResult::Remove;
										}
									}
								);
								loadMeshesBatch.QueueAfterStartStage(loadJobBatch);
							}
						}

						loadMeshesBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
							[callback = Move(callback)](Threading::JobRunnerThread&)
							{
								callback();
							},
							Threading::JobPriority::CreateProceduralMesh
						));
						Threading::JobRunnerThread::GetCurrent()->Queue(loadMeshesBatch);
						return EventCallbackResult::Remove;
					}
					else
					{
						callback();
						return EventCallbackResult::Remove;
					}
				}
			)
		);
		if (sceneLoadBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(sceneLoadBatch);
		}
	}

	bool ProceduralSplineMeshComponent::AreAssetsLoaded() const
	{
		if (m_meshIdentifiers.IsEmpty())
			return false;
		else
		{
			Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
			for (const MeshInfo& meshInfo : m_meshIdentifiers)
			{
				Rendering::StaticMeshInfo& info = meshCache.GetAssetData(meshInfo.meshIdentifier);
				if (info.m_pMesh == nullptr || info.m_pMesh->IsLoaded() == false)
				{
					return false;
				}
			}

			return true;
		}
	}

	struct MergePosition
	{
		Rendering::VertexPosition position;
		Rendering::Index vertexIndex;
	};

	struct PairPosition
	{
		Rendering::Index startIndex;
		Rendering::Index endIndex;
	};

	float ProceduralSplineMeshComponent::FindStartYPosition(const Vector<MeshInfo>& meshes)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		float startY = Math::NumericLimits<float>::Max;
		for (const MeshInfo& meshInfo : meshes)
		{
			Rendering::StaticMeshInfo& staticMeshInfo = meshCache.GetAssetData(meshInfo.meshIdentifier);
			const Rendering::StaticMesh& baseMesh = *staticMeshInfo.m_pMesh;

			const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions = baseMesh.GetVertexPositions();

			for (Rendering::VertexPosition position : vertexPositions)
			{
				startY = Math::Min(position.y, startY);
			}
		}

		return startY;
	}

	float ProceduralSplineMeshComponent::FindEndYPosition(const Vector<MeshInfo>& meshes)
	{
		float endY = Math::NumericLimits<float>::Min;

		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		for (const MeshInfo& meshInfo : meshes)
		{
			Rendering::StaticMeshInfo& staticMeshInfo = meshCache.GetAssetData(meshInfo.meshIdentifier);
			const Rendering::StaticMesh& baseMesh = *staticMeshInfo.m_pMesh;

			const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions = baseMesh.GetVertexPositions();

			for (Rendering::VertexPosition position : vertexPositions)
			{
				endY = Math::Max(position.y, endY);
			}
		}

		return endY;
	}

	void CollectStartVertices(
		ArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions, float startYPosition, Vector<MergePosition>& startOut
	)
	{
		startOut.Reserve(16u);
		for (uint32 i = 0; i < vertexPositions.GetSize(); ++i)
		{
			if (Math::IsEquivalentTo(vertexPositions[i].y, startYPosition, 0.001f))
			{
				MergePosition m;
				m.position = vertexPositions[i];
				m.vertexIndex = i;
				startOut.EmplaceBack(m);
			}
		}
	}

	void CollectEndVertices(ArrayView<Rendering::VertexPosition, Rendering::Index> mesh, float endYPosition, Vector<MergePosition>& endOut)
	{
		endOut.Reserve(16u);
		for (uint32 i = 0; i < mesh.GetSize(); ++i)
		{
			if (Math::IsEquivalentTo(mesh[i].y, endYPosition, 0.001f))
			{
				MergePosition m;
				m.position = mesh[i];
				m.vertexIndex = i;
				endOut.EmplaceBack(m);
			}
		}
	}

	void FindPairVertex(
		Vector<MergePosition>& startPositions, Vector<MergePosition>& endPositions, Vector<PairPosition>& out, Rendering::Index vertexCount
	)
	{
		DynamicInlineBitset<20000, Rendering::Index> used;
		used.Reserve(vertexCount);

		out.Reserve(Math::Max(startPositions.GetSize(), endPositions.GetSize()));
		for (MergePosition startPosition : startPositions)
		{
			for (MergePosition endPosition : endPositions)
			{
				startPosition.position.y = 0.f;
				endPosition.position.y = 0.f;

				if (startPosition.position.IsEquivalentTo(endPosition.position, 0.001f))
				{
					// Do not allow any duplicates
					if (used.IsSet(startPosition.vertexIndex) || used.IsSet(endPosition.vertexIndex))
						continue;

					PairPosition pair;
					pair.startIndex = startPosition.vertexIndex;
					pair.endIndex = endPosition.vertexIndex;
					out.EmplaceBack(pair);
					used.Set(startPosition.vertexIndex);
					used.Set(endPosition.vertexIndex);
					break;
				}
			}
		}
	}

	void GenerateMesh(
		const Rendering::StaticMesh& baseMesh,
		const Vector<PairPosition>& vertexPairs,
		const Entity::SplineComponent& splineComponent,
		Rendering::StaticObject& finalObject
	)
	{
		splineComponent.VisitSpline(
			[&baseMesh, &vertexPairs, &finalObject](const Math::Splinef& spline)
			{
				const uint32 subdivisions = 1u;
				const uint32 segmentCount = spline.CalculateSegmentCount(subdivisions);

				Rendering::Index vertexCount = segmentCount * baseMesh.GetVertexCount();
				Rendering::Index indexCount = segmentCount * baseMesh.GetIndices().GetSize();

				Rendering::StaticObject staticObject;
				staticObject.Resize(Memory::Uninitialized, vertexCount, indexCount);

				Rendering::Index positionIndex = 0;
				Rendering::Index indexIndex = 0;

				uint32 counter = 0;

				spline.IterateAdjustedSplinePoints(
					[&positionIndex,
			     &baseMesh,
			     &counter,
			     &vertexPairs,
			     &indexIndex,
			     pPositions = staticObject.GetVertexElementView<Rendering::VertexPosition>().GetData(),
			     pNormals = staticObject.GetVertexElementView<Rendering::VertexNormals>().GetData(),
			     pIndices = staticObject.GetIndices().GetData(),
			     pTextureCoordinates = staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>().GetData()](
						const Entity::SplineComponent::SplineType::Spline::Point&,
						const Entity::SplineComponent::SplineType::Spline::Point&,
						const Math::Vector3f currentBezierPoint,
						const Math::Vector3f,
						const Math::Vector3f direction,
						const Math::Vector3f normal
					) mutable
					{
						const Math::LocalTransform::StoredRotationType segmentRotation(
							Math::Matrix3x3f(normal, direction, normal.Cross(direction).GetNormalized()).GetOrthonormalized()
						);

						const Math::LocalTransform segmentTransform = Math::LocalTransform(segmentRotation, currentBezierPoint);
						auto baseTextureCoordinates = baseMesh.GetVertexTextureCoordinates();
						auto basePositions = baseMesh.GetVertexPositions();
						auto baseNormals = baseMesh.GetVertexNormals();
						for (uint32 i = 0; i < basePositions.GetSize(); ++i, ++positionIndex)
						{
							pPositions[positionIndex] = segmentTransform.TransformLocationWithoutScale(basePositions[i]);

							if (counter != 0)
							{
								for (uint32 b = 0; b < vertexPairs.GetSize(); ++b)
								{
									if (vertexPairs[b].startIndex == i)
									{
										// Find the previous end vertex and merge it with the new start vertex
										const uint32 index = (positionIndex - baseMesh.GetVertexCount()) +
								                         (vertexPairs[b].endIndex - vertexPairs[b].startIndex);
										pPositions[index] = pPositions[positionIndex];
									}
								}
							}

							pTextureCoordinates[positionIndex] = baseTextureCoordinates[i];
							Math::Vector3f transformedNormal = segmentTransform.TransformLocationWithoutScale(baseNormals[i].normal);
							pNormals[positionIndex] = Rendering::VertexNormals{transformedNormal, Math::Vector3f(0.0f)};
						}

						auto baseIndices = baseMesh.GetIndices();
						for (uint32 i = 0; i < baseIndices.GetSize(); ++i, ++indexIndex)
						{
							pIndices[indexIndex] = baseIndices[i] + baseMesh.GetVertexCount() * counter;
						}

						counter++;
					},
					subdivisions
				);

				Assert(staticObject.GetVertexCount() == positionIndex);
				Assert(staticObject.GetIndexCount() == indexCount);

				Rendering::VertexTangents::Generate(
					staticObject.GetIndices(),
					staticObject.GetVertexElementView<Rendering::VertexPosition>(),
					staticObject.GetVertexElementView<Rendering::VertexNormals>(),
					staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>()
				);

				staticObject.CalculateAndSetBoundingBox();

				finalObject = staticObject;
			}
		);
	}

	void ProceduralSplineMeshComponent::CreateMesh()
	{
		Optional<SplineComponent*> pSplineComponent = GetSpline();
		Assert(pSplineComponent != nullptr && AreAssetsLoaded());
		if (UNLIKELY_ERROR(!pSplineComponent || !AreAssetsLoaded()))
		{
			return;
		}

		Threading::JobBatch jobBatch = Threading::CreateCallback(
			[this, &splineComponent = *pSplineComponent](Threading::JobRunnerThread&)
			{
				const float startYPosition = FindStartYPosition(m_meshIdentifiers);
				const float endYPosition = FindEndYPosition(m_meshIdentifiers);

				Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
				Entity::ComponentTypeSceneData<Entity::ProceduralStaticMeshComponent>& renderItemSceneData =
					*splineComponent.GetSceneRegistry().GetOrCreateComponentTypeData<Entity::ProceduralStaticMeshComponent>();
				for (const MeshInfo& meshInfo : m_meshIdentifiers)
				{
					Optional<Entity::ProceduralStaticMeshComponent*> pComponent =
						renderItemSceneData.CreateInstance(Entity::ProceduralStaticMeshComponent::Initializer{
							Entity::RenderItemComponent::Initializer{
								Entity::Component3D::Initializer{*this, splineComponent.GetWorldTransform()},
								meshInfo.stageMask
							},
							meshInfo.materialInstanceIdentifier
						});

					Rendering::StaticMeshInfo& staticMeshInfo = meshCache.GetAssetData(meshInfo.meshIdentifier);
					Rendering::StaticMesh& baseMesh = *staticMeshInfo.m_pMesh;
					Assert(baseMesh.IsLoaded(), "Mesh was not loaded and can not be used for the spline generation!");

					auto basePositions = baseMesh.GetVertexPositions();
					Vector<PairPosition> pairs;
					Vector<MergePosition> startVertices;
					Vector<MergePosition> endVertices;
					CollectStartVertices(basePositions, startYPosition, startVertices);
					CollectEndVertices(basePositions, endYPosition, endVertices);
					FindPairVertex(startVertices, endVertices, pairs, baseMesh.GetVertexCount());

					pComponent->SetMeshGeneration(
						[&baseMesh, pairs, &splineComponent](Rendering::StaticObject& finalObject)
						{
							GenerateMesh(baseMesh, pairs, splineComponent, finalObject);
						}
					);
				}
			},
			Threading::JobPriority::CreateProceduralMesh
		);

		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	void ProceduralSplineMeshComponent::SetComponentType(const ComponentTypePicker asset)
	{
		m_assetGuid = asset.GetAssetGuid();
	}

	ProceduralSplineMeshComponent::ComponentTypePicker ProceduralSplineMeshComponent::GetComponentType() const
	{
		const Guid assetTypeGuid = System::Get<Asset::Manager>().GetAssetTypeGuid(m_assetGuid);
		return ComponentTypePicker{
			Asset::Reference{m_assetGuid, assetTypeGuid},
			Asset::Types{Array<const Asset::TypeGuid, 2>{
				Reflection::GetTypeGuid<Entity::SceneComponent>(),
				Reflection::GetTypeGuid<Entity::StaticMeshComponent>()
			}}
		};
	}

	bool ProceduralSplineMeshComponent::
		CanApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>) const
	{
		return false;
	}

	bool ProceduralSplineMeshComponent::
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags>)
	{
		return false;
	}

	void ProceduralSplineMeshComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Picker componentType = GetComponentType();
		if (callback(componentType.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}
	}

	[[maybe_unused]] const bool wasSplineMeshRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ProceduralSplineMeshComponent>>::Make());
	[[maybe_unused]] const bool wasSplineMeshTypeRegistered = Reflection::Registry::RegisterType<ProceduralSplineMeshComponent>();
}
