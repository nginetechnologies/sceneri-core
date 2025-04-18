#include "Entity/ProceduralStaticMeshComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/Primitives/Box.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/Defaults.h>
#include <Renderer/Scene/SceneView.h>
#include <Common/Math/Tangents.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Math/Mod.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	ProceduralStaticMeshComponent::ProceduralStaticMeshComponent(const ProceduralStaticMeshComponent& templateComponent, const Cloner& cloner)
		: StaticMeshComponent(templateComponent, cloner)
	{
		SetMesh(*System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(CreateMesh()).m_pMesh);
	}

	ProceduralStaticMeshComponent::ProceduralStaticMeshComponent(const Deserializer& deserializer)
		: ProceduralStaticMeshComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ProceduralStaticMeshComponent>().ToString().GetView())
			)
	{
	}

	ProceduralStaticMeshComponent::~ProceduralStaticMeshComponent()
	{
		if (m_generateMeshCallback.IsValid())
		{
			System::Get<Rendering::Renderer>().GetMeshCache().Remove(GetMeshIdentifier());
		}

		Threading::UniqueLock lock(m_meshGenerationMutex);
	}

	[[nodiscard]] inline Rendering::StaticMeshIdentifier ProceduralStaticMeshComponent::CreateMesh()
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

	ProceduralStaticMeshComponent::ProceduralStaticMeshComponent(
		const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer
	)
		: StaticMeshComponent(
				deserializer,
				CreateMesh(),
				deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<StaticMeshComponent>().ToString().GetView())
			)
	{
	}

	[[nodiscard]] inline Rendering::MaterialInstanceIdentifier GetDefaultMaterialInstanceIdentifier()
	{
		return System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(
			Rendering::Constants::DefaultMaterialInstanceAssetGuid
		);
	}

	ProceduralStaticMeshComponent::ProceduralStaticMeshComponent(Initializer&& initializer)
		: StaticMeshComponent(StaticMeshComponent::Initializer{
				RenderItemComponent::Initializer{initializer}, CreateMesh(), initializer.m_materialInstanceIdentifier
			})
	{
	}

	Threading::JobBatch ProceduralStaticMeshComponent::GenerateMesh()
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
						ProceduralStaticMeshComponent& __restrict component = static_cast<ProceduralStaticMeshComponent&>(*pComponent);

						const FixedArrayView<Rendering::StaticObject, 2> staticObjectCache = component.m_staticObjectCache.GetView();

						const Optional<const Rendering::StaticMesh*> pMesh = component.GetMesh();
						if (UNLIKELY_ERROR(pMesh.IsInvalid()))
						{
							return;
						}
						const Rendering::StaticMeshIdentifier meshIdentifier = pMesh->GetIdentifier();

						Rendering::StaticObject staticObject = staticObjectCache[0].GetVertexCount() > 0 ? Move(staticObjectCache[0])
					                                                                                   : Move(staticObjectCache[1]);
						if (component.m_generateMeshCallback.IsValid())
						{
							component.m_generateMeshCallback(staticObject);
						}
						else
						{
							// Generate a default mesh.
							staticObject = Rendering::Primitives::Box::CreateUniform(Math::Vector3f(0.5f));
						}

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
				},
				Threading::JobPriority::CreateProceduralMesh
			);
		}
		else
		{
			return {};
		}
	}

	void ProceduralStaticMeshComponent::SetMeshGeneration(GenerateMeshFunction&& mesh)
	{
		m_generateMeshCallback = Move(mesh);

		RecreateMesh();
	}

	void ProceduralStaticMeshComponent::RecreateMesh()
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

	[[maybe_unused]] const bool wasProceduralStaticMeshComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ProceduralStaticMeshComponent>>::Make());
	[[maybe_unused]] const bool wasProceduralStaticMeshComponentTypeRegistered =
		Reflection::Registry::RegisterType<ProceduralStaticMeshComponent>();
}
