#include <Engine/Entity/RenderItemComponentFunctions.h>
#include <Engine/Entity/Data/RenderItem/StageMask.h>

#include <Engine/Entity/Component3D.inl>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	void EnableStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(component.GetIdentifier()))
		{
			pStageMask->EnableStage(component, stageIdentifier);
		}
		else if (component.IsMeshScene())
		{
			using Recurse = void (*)(
				Entity::Component3D& component,
				const Rendering::SceneRenderStageIdentifier stageIdentifier,
				ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
			);
			static Recurse recurse = [](
																 Entity::Component3D& component,
																 const Rendering::SceneRenderStageIdentifier stageIdentifier,
																 ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
															 )
			{
				for (Entity::Component3D& child : component.GetChildren())
				{
					if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(child.GetIdentifier()))
					{
						pStageMask->EnableStage(child, stageIdentifier);
					}

					recurse(child, stageIdentifier, stageMaskSceneData);
				}
			};

			recurse(component, stageIdentifier, stageMaskSceneData);
		}
	}

	void DisableStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(component.GetIdentifier()))
		{
			pStageMask->DisableStage(component, stageIdentifier);
		}
		else if (component.IsMeshScene())
		{
			using Recurse = void (*)(
				Entity::Component3D& component,
				const Rendering::SceneRenderStageIdentifier stageIdentifier,
				ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
			);
			static Recurse recurse = [](
																 Entity::Component3D& component,
																 const Rendering::SceneRenderStageIdentifier stageIdentifier,
																 ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
															 )
			{
				for (Entity::Component3D& child : component.GetChildren())
				{
					if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(child.GetIdentifier()))
					{
						pStageMask->DisableStage(child, stageIdentifier);
					}

					recurse(child, stageIdentifier, stageMaskSceneData);
				}
			};

			recurse(component, stageIdentifier, stageMaskSceneData);
		}
	}

	void ResetStage(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(component.GetIdentifier()))
		{
			pStageMask->ResetStage(component, stageIdentifier);
		}
		else if (component.IsMeshScene())
		{
			using Recurse = void (*)(
				Entity::Component3D& component,
				const Rendering::SceneRenderStageIdentifier stageIdentifier,
				ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
			);
			static Recurse recurse = [](
																 Entity::Component3D& component,
																 const Rendering::SceneRenderStageIdentifier stageIdentifier,
																 ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
															 )
			{
				for (Entity::Component3D& child : component.GetChildren())
				{
					if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(child.GetIdentifier()))
					{
						pStageMask->ResetStage(child, stageIdentifier);
					}

					recurse(child, stageIdentifier, stageMaskSceneData);
				}
			};

			recurse(component, stageIdentifier, stageMaskSceneData);
		}
	}

	[[nodiscard]] bool IsStageEnabled(Entity::Component3D& component, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(component.GetIdentifier()))
		{
			return pStageMask->IsStageEnabled(stageIdentifier);
		}
		else if (component.IsMeshScene())
		{
			using Recurse = bool (*)(
				Entity::Component3D& component,
				const Rendering::SceneRenderStageIdentifier stageIdentifier,
				ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
			);
			static Recurse recurse = [](
																 Entity::Component3D& component,
																 const Rendering::SceneRenderStageIdentifier stageIdentifier,
																 ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData
															 )
			{
				for (Entity::Component3D& child : component.GetChildren())
				{
					if (const Optional<Data::RenderItem::StageMask*> pStageMask = stageMaskSceneData.GetComponentImplementation(child.GetIdentifier()))
					{
						if (pStageMask->IsStageEnabled(stageIdentifier))
						{
							return true;
						}
					}

					if (recurse(child, stageIdentifier, stageMaskSceneData))
					{
						return true;
					}
				}

				return false;
			};

			return recurse(component, stageIdentifier, stageMaskSceneData);
		}
		else
		{
			return false;
		}
	}

	[[maybe_unused]] inline static const bool wasEnableStageReflected = Reflection::Registry::RegisterGlobalFunction<&EnableStage>();
	[[maybe_unused]] inline static const bool wasDisableStageReflected = Reflection::Registry::RegisterGlobalFunction<&DisableStage>();
	[[maybe_unused]] inline static const bool wasResetStageReflected = Reflection::Registry::RegisterGlobalFunction<&ResetStage>();
	[[maybe_unused]] inline static const bool wasIsStageEnabledReflected = Reflection::Registry::RegisterGlobalFunction<&IsStageEnabled>();
}
