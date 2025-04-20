#include "Plugin.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Math/Primitives/WorldLine.h>

#include <Common/System/Query.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/StaticMeshComponent.h>
#include <Engine/Entity/Primitives/ArrowComponent.h>
#include <Engine/Entity/Primitives/BoxComponent.h>
#include <Engine/Entity/Primitives/CapsuleComponent.h>
#include <Engine/Entity/Primitives/CylinderComponent.h>
#include <Engine/Entity/Primitives/ArcComponent.h>
#include <Engine/Entity/Primitives/ConeComponent.h>
#include <Engine/Entity/Primitives/PyramidComponent.h>
#include <Engine/Entity/Primitives/PlaneComponent.h>
#include <Engine/Entity/Primitives/SphereComponent.h>
#include <Engine/Entity/Primitives/TorusComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Threading/JobManager.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Defaults.h>

#include <FontRendering/Components/TextComponent.h>
#include <FontRendering/Manager.h>

namespace ngine::VisualDebug
{
	inline static constexpr Array<Asset::Guid, (uint8)Color::Count> colorMaterialAssets{
		"35674db1-56c6-4c21-8c2c-5bdd7c86c452"_asset, // red
		"6ddaa95a-1c7f-46d7-b50a-df85bab4808a"_asset, // green
		"36ee4991-8e7b-467b-b057-dcb11af6d754"_asset, // blue
		"565287cd-e2da-4379-811f-1068e117a052"_asset, // purple
		"d096ab0b-3721-4029-8167-ff73b5e73f1c"_asset  // orange
	};

	void Plugin::OnLoaded(Application&)
	{
	}

	void Plugin::AddSphere(
		Scene& scene, const Math::WorldCoordinate coordinate, const Time::Durationf duration, const Color color, const Math::Radiusf radius
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::SphereComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::SphereComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "bba6ad40-e1ee-4f1e-baf8-608ff9e7d77f"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::SphereComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::SphereComponent::Initializer{
				Entity::RenderItemComponent::Initializer{
					Entity::Component3D::Initializer{
						scene.GetRootComponent(),
						Math::WorldTransform(Math::Identity, coordinate, Math::Vector3f(radius.GetMeters()))
					},
					stageMask
				},
				materialInstanceId
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddBox(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Vector3f dimensions
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::BoxComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::BoxComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::BoxComponent*> component = primitiveSceneData.CreateInstance(Entity::Primitives::BoxComponent::Initializer{
			Entity::RenderItemComponent::Initializer{
				Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate, dimensions)},
				stageMask
			},
			materialInstanceId
		});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddTorus(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius,
		const Math::Lengthf thickness,
		const uint16 sideCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::TorusComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::TorusComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::TorusComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::TorusComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
						stageMask
					},
					materialInstanceId
				},
				radius,
				thickness,
				sideCount
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddPlane(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::PlaneComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::PlaneComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::PlaneComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::PlaneComponent::Initializer{
				Entity::RenderItemComponent::Initializer{
					Entity::Component3D::Initializer{
						scene.GetRootComponent(),
						Math::WorldTransform(rotation, coordinate, Math::Vector3f(radius.GetMeters() * 2.f, radius.GetMeters() * 2.f, 1.f))
					},
					stageMask
				},
				materialInstanceId
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void
	Plugin::AddLine(Scene& scene, const Math::WorldLine line, const Time::Durationf duration, const Color color, const Math::Radiusf radius)
	{
		const Math::Quaternionf rotation = Math::Quaternionf(Math::CreateRotationAroundYAxis, -90_degrees);
		AddCylinder(
			scene,
			line.GetStart(),
			rotation.TransformRotation(Math::Quaternionf(line.GetDirection())),
			duration,
			color,
			radius,
			Math::Lengthf::FromMeters(line.GetLength())
		);
	}

	void Plugin::AddDirection(
		Scene& scene, const Math::WorldCoordinate coordinate, const Math::Vector3f direction, const Time::Durationf duration, const Color color
	)
	{
		constexpr Math::Lengthf shaftLength = 0.75_meters;
		constexpr Math::Radiusf shaftRadius = 0.02_meters;
		constexpr Math::Lengthf tipLength = 0.25_meters;
		constexpr Math::Radiusf tipRadius = 0.06_meters;

		AddArrow(
			scene,
			coordinate,
			Math::Quaternionf(direction.GetNormalized()),
			duration,
			color,
			shaftLength,
			shaftRadius,
			tipRadius,
			tipLength
		);
	}

	void Plugin::AddCylinder(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius,
		const Math::Lengthf height,
		const uint16 sideCount,
		const uint16 segmentCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::CylinderComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::CylinderComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::CylinderComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::CylinderComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
						stageMask
					},
					materialInstanceId
				},
				radius,
				height,
				sideCount,
				segmentCount
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddArc(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Anglef angle,
		const Math::Lengthf halfHeight,
		const Math::Radiusf outerRadius,
		const Math::Radiusf innerRadius,
		const uint16 sideCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::ArcComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::ArcComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::ArcComponent*> component = primitiveSceneData.CreateInstance(Entity::Primitives::ArcComponent::Initializer{
			Entity::ProceduralStaticMeshComponent::Initializer{
				Entity::RenderItemComponent::Initializer{
					Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
					stageMask
				},
				materialInstanceId
			},
			angle,
			halfHeight,
			outerRadius,
			innerRadius,
			sideCount
		});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddCone(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius,
		const Math::Lengthf height,
		const uint16 sideCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::ConeComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::ConeComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::ConeComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::ConeComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
						stageMask
					},
					materialInstanceId
				},
				radius,
				height,
				sideCount
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddPyramid(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius,
		const Math::Lengthf height
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::PyramidComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::PyramidComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::PyramidComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::PyramidComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
						stageMask
					},
					materialInstanceId
				},
				radius,
				height
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddCapsule(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Radiusf radius,
		const Math::Lengthf height,
		const uint16 sideCount,
		const uint16 segmentCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::CapsuleComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::CapsuleComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::CapsuleComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::CapsuleComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{scene.GetRootComponent(), Math::WorldTransform(rotation, coordinate)},
						stageMask
					},
					materialInstanceId
				},
				radius,
				height,
				sideCount,
				segmentCount
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddArrow(
		Scene& scene,
		const Math::WorldCoordinate coordinate,
		const Math::WorldQuaternion rotation,
		const Time::Durationf duration,
		const Color color,
		const Math::Lengthf shaftHeight,
		const Math::Radiusf shaftRadius,
		const Math::Radiusf tipRadius,
		const Math::Lengthf tipHeight,
		const uint16 sideCount
	)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Primitives::ArrowComponent>& primitiveSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::ArrowComponent>();
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		const Rendering::MaterialInstanceIdentifier materialInstanceId =
			materialInstanceCache.FindOrRegisterAsset(colorMaterialAssets[(uint8)color]);

		const Asset::Guid materialsStageId = "BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F"_asset;
		Rendering::RenderItemStageMask stageMask;

		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(materialsStageId);
		stageMask.Set(stageIdentifier);

		Optional<Entity::Primitives::ArrowComponent*> component =
			primitiveSceneData.CreateInstance(Entity::Primitives::ArrowComponent::Initializer{
				Entity::ProceduralStaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{
							scene.GetRootComponent(),
							Math::WorldTransform(rotation.TransformRotation(Math::Quaternionf(Math::Down, Math::Forward)), coordinate)
						},
						stageMask
					},
					materialInstanceId
				},
				shaftRadius,
				shaftHeight,
				tipRadius,
				tipHeight,
				sideCount
			});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}

	void Plugin::AddText3D(
		Scene& scene,
		const Math::WorldTransform transform,
		const ConstUnicodeStringView text,
		const Math::Color color,
		const Time::Durationf duration
	)
	{
		using namespace Font::Literals;

		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Font::TextComponent>& textSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Font::TextComponent>();
		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();
		Optional<Font::TextComponent*> component = textSceneData.CreateInstance(Font::TextComponent::Initializer{
			Entity::StaticMeshComponent::Initializer{
				Entity::RenderItemComponent::Initializer{Entity::Component3D::Initializer{scene.GetRootComponent(), transform}}
			},
			UnicodeString(text),
			color,
			20_pt,
			fontManager.GetCache().FindOrRegisterAsset("73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset)
		});
		if (LIKELY(component.IsValid()))
		{
			System::Get<Threading::JobManager>().ScheduleAsync(
				duration,
				[&component = *component, &sceneRegistry](Threading::JobRunnerThread&)
				{
					component.Destroy(sceneRegistry);
				},
				Threading::JobPriority::QueuedComponentDestructions
			);
		}
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::VisualDebug::Plugin>();
#else
extern "C" VISUALDEBUG_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::VisualDebug::Plugin(application);
}
#endif
