#include "PhysicsCore/Components/MeshColliderComponent.h"
#include "PhysicsCore/Plugin.h"
#include "Components/BodyComponent.h"
#include "Components/Data/SceneComponent.h"
#include "Components/Data/BodyComponent.h"
#include "Material.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Defaults.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/MeshShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SphereShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ConvexHullShape.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Log.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Physics
{
	[[nodiscard]] JPH::ShapeRef CreateDummyShape()
	{
		const float radius = 0.01f;
		JPH::SphereShapeSettings sphereSettings(radius);
		JPH::ShapeSettings::ShapeResult result = sphereSettings.Create();
		if (LIKELY(result.IsValid()))
		{
			return result.Get();
		}
		else
		{
			return {};
		}
	}

	MeshColliderComponent::MeshColliderComponent(Initializer&& initializer)
		: MeshColliderComponent(
				Forward<Initializer>(initializer),
				initializer.m_pMesh.IsValid() ? *initializer.m_pMesh
																			: System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterMesh(Primitives::PlaneMeshAssetGuid)
			)
	{
	}

	MeshColliderComponent::MeshColliderComponent(Initializer&& initializer, const Rendering::StaticMesh& mesh)
		: ColliderComponent(ColliderComponent::Initializer{
				Entity::Component3D::Initializer{
					*initializer.GetParent(),
					initializer.m_localTransform,
					mesh.GetBoundingBox().IsZero() ? Math::BoundingBox(0.5_meters) : Math::BoundingBox(mesh.GetBoundingBox())
				},
				CreateDummyShape(),
				initializer.m_pPhysicalMaterial,
				initializer.m_pTargetBody,
				initializer.m_pTargetBodyOwner
			})
		, m_mesh(mesh)
	{
		ColliderComponent::DataComponentResult<Data::Body> nearestBody = GetNearestBody();
		Threading::JobBatch jobBatch = TryLoadOrCreateShape(nearestBody.m_pDataComponent);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	MeshColliderComponent::MeshColliderComponent(const MeshColliderComponent& templateComponent, const Cloner& cloner)
		: ColliderComponent(templateComponent, ColliderComponent::ClonerInfo{Component3D::Cloner{cloner}, CreateDummyShape()})
		, m_mesh(templateComponent.m_mesh)
	{
		ColliderComponent::DataComponentResult<Data::Body> nearestBody = GetNearestBody();
		Threading::JobBatch jobBatch = TryLoadOrCreateShape(nearestBody.m_pDataComponent);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	MeshColliderComponent::MeshColliderComponent(const Deserializer& deserializer, const Rendering::StaticMesh& mesh)
		: ColliderComponent(ColliderComponent::DeserializerInfo{
				Entity::Component3D::DeserializerWithBounds{
					Deserializer(deserializer),
					deserializer.GetSceneRegistry(),
					*deserializer.GetParent(),
					Entity::ComponentFlags{},
					mesh.GetBoundingBox().IsZero() ? Math::BoundingBox(0.5_meters) : Math::BoundingBox(mesh.GetBoundingBox())
				},
				CreateDummyShape()
			})
		, m_mesh(mesh)
	{
	}

	inline const Rendering::StaticMesh& ReadMesh(const Optional<Serialization::Reader> serializer)
	{
		return System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterMesh(
			serializer.IsValid() ? serializer->ReadWithDefaultValue<Asset::Guid>("mesh", Asset::Guid(Rendering::Constants::DefaultMeshAssetGuid))
													 : Asset::Guid(Rendering::Constants::DefaultMeshAssetGuid)
		);
	}

	MeshColliderComponent::MeshColliderComponent(const Deserializer& deserializer)
		: MeshColliderComponent(
				deserializer, ReadMesh(deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<MeshColliderComponent>().ToString().GetView()))
			)
	{
	}

	Threading::JobBatch MeshColliderComponent::TryLoadOrCreateShape(Data::Body* pBody)
	{
		const Rendering::StaticMeshIdentifier renderMeshIdentifier = m_mesh->GetIdentifier();
		MeshCache& meshCache = System::FindPlugin<Plugin>()->GetMeshCache();
		const MeshIdentifier meshIdentifier = meshCache.FindOrRegisterAsset(renderMeshIdentifier);

		switch (pBody ? pBody->GetType() : BodyComponent::Type::Static)
		{
			case BodyComponent::Type::Static:
			case BodyComponent::Type::Kinematic:
			{
				return meshCache.TryLoadOrCreateTriangleMesh(
					meshIdentifier,
					renderMeshIdentifier,
					MeshLoadListenerData{
						this,
						[meshIdentifier, renderMeshIdentifier](MeshColliderComponent& collider, JPH::ShapeRef pShape)
						{
							if (LIKELY(pShape.IsValid()))
							{
								// TODO: Add a MaterialOverrideShape wrapper, that way we can save memory on cloning the shape info itself.
								JPH::MeshShape* pNewShape = new JPH::MeshShape{static_cast<JPH::MeshShape&>(*pShape)};
								pNewShape->SetMaterial(JPH::SubShapeID{}, &collider.GetMaterial());

								Assert(!collider.IsDestroying());
								collider.SetShape(collider.WrapShape(pShape.GetPtr()));
							}
							else
							{
								LogWarning("Failed to create mesh shape for Mesh Collider, falling back to convex mesh!");
								MeshCache& meshCache = System::FindPlugin<Plugin>()->GetMeshCache();
								Threading::JobBatch jobBatch = meshCache.TryLoadOrCreateConvexMesh(
									meshIdentifier,
									renderMeshIdentifier,
									MeshLoadListenerData{
										&collider,
										[](MeshColliderComponent& collider, JPH::ShapeRef pShape)
										{
											if (LIKELY(pShape.IsValid()))
											{
												// TODO: Add a MaterialOverrideShape wrapper, that way we can save memory on cloning the shape info itself.
												JPH::ConvexHullShape* pNewShape = new JPH::ConvexHullShape{static_cast<JPH::ConvexHullShape&>(*pShape)};
												pNewShape->SetMaterial(JPH::SubShapeID{}, &collider.GetMaterial());

												Assert(!collider.IsDestroying());
												collider.SetShape(collider.WrapShape(pShape.GetPtr()));
											}
											else
											{
												LogError("Failed to create fallback convex shape for Mesh Collider!");
											}
										}
									}
								);
								if (jobBatch.IsValid())
								{
									Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
								}
							}
						}
					}
				);
			}
			case BodyComponent::Type::Dynamic:
			{
				return meshCache.TryLoadOrCreateConvexMesh(
					meshIdentifier,
					renderMeshIdentifier,
					MeshLoadListenerData{
						this,
						[](MeshColliderComponent& collider, JPH::ShapeRef pShape)
						{
							if (LIKELY(pShape.IsValid()))
							{
								// TODO: Add a MaterialOverrideShape wrapper, that way we can save memory on cloning the shape info itself.
								JPH::ConvexHullShape* pNewShape = new JPH::ConvexHullShape{static_cast<JPH::ConvexHullShape&>(*pShape)};
								pNewShape->SetMaterial(JPH::SubShapeID{}, &collider.GetMaterial());

								Assert(!collider.IsDestroying());
								collider.SetShape(collider.WrapShape(pShape.GetPtr()));
							}
							else
							{
								LogError("Failed to create convex shape for Mesh Collider!");
							}
						}
					}
				);
			}
		}
		ExpectUnreachable();
	}

	void MeshColliderComponent::CreateStaticMeshShape(Data::Body* pBody)
	{
		Threading::JobBatch jobBatch = TryLoadOrCreateShape(pBody);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	void MeshColliderComponent::SetStaticMesh(const Asset::Picker asset)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		Rendering::StaticMesh& newMesh = meshCache.FindOrRegisterMesh(asset.GetAssetGuid());
		const bool changed = &newMesh != &*m_mesh;
		m_mesh = newMesh;
		if (changed)
		{
			ColliderComponent::DataComponentResult<Data::Body> nearestBody = GetNearestBody();
			Threading::JobBatch jobBatch = TryLoadOrCreateShape(nearestBody.m_pDataComponent);
			if (jobBatch.IsValid())
			{
				if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
				{
					pThread->Queue(jobBatch);
				}
				else
				{
					System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadMeshCollider);
				}
			}
		}
	}

	Asset::Picker MeshColliderComponent::GetStaticMesh() const
	{
		return {
			System::Get<Rendering::Renderer>().GetMeshCache().GetAssetGuid(m_mesh->GetIdentifier()),
			MeshPartAssetType::AssetFormat.assetTypeGuid
		};
	}

	Threading::JobBatch MeshColliderComponent::SetDeserializedStaticMesh(
		const Asset::Picker asset, [[maybe_unused]] const Serialization::Reader objectReader, const Serialization::Reader
	)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		m_mesh = meshCache.FindOrRegisterMesh(asset.GetAssetGuid());

		ColliderComponent::DataComponentResult<Data::Body> nearestBody = GetNearestBody();
		return TryLoadOrCreateShape(nearestBody.m_pDataComponent);
	}

	bool MeshColliderComponent::CanApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	) const
	{
		if (ColliderComponent::CanApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			return pAssetReference->GetTypeGuid() == MeshPartAssetType::AssetFormat.assetTypeGuid;
		}
		return false;
	}

	bool MeshColliderComponent::ApplyAtPoint(
		const Entity::ApplicableData& applicableData,
		const Math::WorldCoordinate coordinate,
		const EnumFlags<Entity::ApplyAssetFlags> applyFlags
	)
	{
		if (ColliderComponent::ApplyAtPoint(applicableData, coordinate, applyFlags))
		{
			return true;
		}

		if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
		{
			if (pAssetReference->GetTypeGuid() == MeshPartAssetType::AssetFormat.assetTypeGuid)
			{
				SetStaticMesh(*pAssetReference);
				return true;
			}
		}
		return false;
	}

	void MeshColliderComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Picker mesh = GetStaticMesh();
		if (callback(mesh.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}

		ColliderComponent::IterateAttachedItems(allowedTypes, callback);
	}

	void MeshColliderComponent::OnAttachedToNewParent()
	{
		if (DataComponentResult<Data::Body> nearestBody = GetNearestBody())
		{
			Threading::JobBatch jobBatch = TryLoadOrCreateShape(nearestBody.m_pDataComponent);
			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
			return;
		}

		ColliderComponent::OnAttachedToNewParent();
	}

	[[maybe_unused]] const bool wasMeshColliderComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<MeshColliderComponent>>::Make());
	[[maybe_unused]] const bool wasMeshColliderComponentTypeRegistered = Reflection::Registry::RegisterType<MeshColliderComponent>();
}
