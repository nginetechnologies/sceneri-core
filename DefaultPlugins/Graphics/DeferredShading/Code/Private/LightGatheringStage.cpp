#include "LightGatheringStage.h"
#include "ShadowsStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Power.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/System/Query.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/EnvironmentLightComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>

namespace ngine::Rendering
{
	[[nodiscard]] /* static */ LightTypes LightGatheringStage::GetLightType(const Entity::LightSourceComponent& component)
	{
		if (component.Is<Entity::PointLightComponent>())
		{
			return LightTypes::PointLight;
		}
		else if (component.Is<Entity::SpotLightComponent>())
		{
			return LightTypes::SpotLight;
		}
		else if (component.Is<Entity::DirectionalLightComponent>())
		{
			return LightTypes::DirectionalLight;
		}
		else if (component.Is<Entity::EnvironmentLightComponent>())
		{
			return LightTypes::EnvironmentLight;
		}

		ExpectUnreachable();
	}

	LightGatheringStage::LightGatheringStage(SceneView& sceneView)
		: m_sceneView(sceneView)
	{
	}

	LightGatheringStage::~LightGatheringStage()
	{
	}

	bool LightGatheringStage::OnRenderItemsBecomeVisible(
		const SceneRenderStageIdentifier stageIdentifier, const Entity::RenderItemMask& renderItems
	)
	{
		Entity::RenderItemMask lights;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsLight())
			{
				lights.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
		}

		if (lights.AreAnySet())
		{
			m_visibleLightsMask |= lights;

			for (const uint32 renderItemIndex : lights.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
					m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
				Assert(pVisibleComponent.IsValid());
				if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
				{
					continue;
				}

				const Entity::LightSourceComponent& light = pVisibleComponent->AsExpected<Entity::LightSourceComponent>();
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
				m_visibleLights[(uint8)GetLightType(light)].EmplaceBack(VisibleLight{-1, 0, 0, light});
#else
				m_visibleLights[(uint8)GetLightType(light)].EmplaceBack(VisibleLight{-1, 0, light});
#endif
				m_sceneView.GetSubmittedRenderItemStageMask(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)).Set(stageIdentifier);
			}

			UpdateLightBuffer();
			return true;
		}
		else
		{
			return false;
		}
	}

	void LightGatheringStage::OnVisibleRenderItemsReset(
		const SceneRenderStageIdentifier stageIdentifier, const Entity::RenderItemMask& renderItems
	)
	{
		Entity::RenderItemMask lights;
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsLight())
			{
				lights.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			}
		}

		if (lights.AreAnySet())
		{
			// TODO: Resetting
			OnRenderItemsBecomeHidden(lights);

			for (const uint32 renderItemIndex : lights.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
				m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				m_sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
			}

			OnRenderItemsBecomeVisible(stageIdentifier, lights);
		}
	}

	bool LightGatheringStage::OnRenderItemsBecomeHidden(const Entity::RenderItemMask& renderItems)
	{
		Entity::RenderItemMask removedLightsMask = m_visibleLightsMask & renderItems;
		if (removedLightsMask.AreAnySet())
		{
			const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
				m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
			for (const uint32 renderItemIndex : removedLightsMask.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				for (LightContainer& typeVisibleLights : m_visibleLights)
				{
					const LightTypes lightType = (LightTypes)m_visibleLights.GetIteratorIndex(&typeVisibleLights);
					for (auto it = typeVisibleLights.begin(), endIt = typeVisibleLights.end(); it != endIt; ++it)
					{
						if (it->light->GetRenderItemIdentifier().GetFirstValidIndex() == renderItemIndex)
						{
							const int32 shadowMapIndex = it->shadowMapIndex;
							if (shadowMapIndex != -1)
							{
								const ShadowMapIndexType shadowMapCount = [](const LightTypes lightType, const VisibleLight& light) -> ShadowMapIndexType
								{
									switch (lightType)
									{
										case LightTypes::SpotLight:
										case LightTypes::DirectionalLight:
											return light.cascadeCount;
										case LightTypes::PointLight:
											return 6;
										case LightTypes::EnvironmentLight:
										case LightTypes::Count:
											ExpectUnreachable();
									}
									ExpectUnreachable();
								}(lightType, *it);
								m_visibleShadowCastingLights.Remove(
									m_visibleShadowCastingLights.GetSubView((ShadowMapIndexType)shadowMapIndex, shadowMapCount)
								);
								m_visibleShadowCastingLightIdentifiers.Remove(
									m_visibleShadowCastingLightIdentifiers.GetSubView((ShadowMapIndexType)shadowMapIndex, shadowMapCount)
								);

								// Update shadow map indices if we end up shifting the array
								for (LightContainer& lights : m_visibleLights)
								{
									for (VisibleLight& light : lights)
									{
										if (light.shadowMapIndex > shadowMapIndex)
										{
											light.shadowMapIndex--;
										}
									}
								}

								if (lightType == LightTypes::DirectionalLight)
								{
									m_parallelShadowMapsIndices.Remove(
										m_parallelShadowMapsIndices.GetSubView((ShadowMapIndexType)it->directionalShadowMatrixIndex, it->cascadeCount)
									);
								}
							}

							typeVisibleLights.Remove(it);

							break;
						}
					}
				}
			}

			m_visibleLightsMask.Clear(renderItems);

			UpdateLightBuffer();
			return true;
		}
		else
		{
			return false;
		}
	}

	void LightGatheringStage::OnVisibleRenderItemTransformsChanged(const Entity::RenderItemMask& renderItems)
	{
		bool encounteredLights = false;

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			encounteredLights |= pVisibleComponent->IsLight();
		}

		if (encounteredLights)
		{
			UpdateLightBuffer();
		}
	}

	void LightGatheringStage::OnSceneUnloaded()
	{
		for (Vector<VisibleLight>& lightContainer : m_visibleLights)
		{
			lightContainer.Clear();
		}
		m_visibleLightsMask.ClearAll();
		m_numDirectionalShadowingCastingLight = 0;
		m_visibleShadowCastingLights.Clear();
		m_parallelShadowMapsIndices.Clear();
	}

	void LightGatheringStage::OnActiveCameraPropertiesChanged()
	{
		UpdateLightBuffer();
	}

	Optional<const VisibleLight*> LightGatheringStage::GetVisibleLightInfo(const Entity::LightSourceComponent& light) const
	{
		if(OptionalIterator<const VisibleLight> pVisibleLight = m_visibleLights[(uint8)GetLightType(light)].FindIf(
		       [&light](const VisibleLight& visibleLight)
		       {
			       return visibleLight.light == light;
		       }
		   ))
		{
			return *pVisibleLight;
		}

		return Invalid;
	}

	void LightGatheringStage::UpdateLightBuffer()
	{
		if (m_pShadowsStage.IsValid() && m_pShadowsStage->GetState() == ShadowsStage::State::Rasterized)
		{
			// TODO: Detect which light or camera changed, and only update that.

			constexpr Math::Matrix4x4f biasMatrix =
				Math::Matrix4x4f{0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f, 1.0f};

			for (VisibleLight& light : m_visibleLights[(uint8)LightTypes::SpotLight])
			{
				const Entity::SpotLightComponent& spotLight = static_cast<const Entity::SpotLightComponent&>(*light.light);

				const Math::WorldTransform worldTransform = spotLight.GetWorldTransform();
				const Math::Matrix4x4f projection = Math::Matrix4x4f::CreatePerspective(
					spotLight.GetFieldOfView(),
					1.f,
					spotLight.GetNearPlane().GetMeters(),
					spotLight.GetInfluenceRadius().GetMeters()
				);
				const Math::Matrix4x4f lookAt = Math::Matrix4x4f::CreateLookAt(
					worldTransform.GetLocation(),
					worldTransform.GetLocation() + worldTransform.GetForwardColumn(),
					worldTransform.GetUpColumn()
				);
				const Math::Matrix4x4f viewProjectionMatrix = lookAt * projection;

				light.shadowSampleViewProjectionMatrices[0] = viewProjectionMatrix * biasMatrix;

				if (light.shadowMapIndex == -1)
				{
					if (m_visibleShadowCastingLights.ReachedCapacity())
					{
						continue;
					}

					light.cascadeCount = 1;
					light.shadowMapIndex = m_visibleShadowCastingLights.GetNextAvailableIndex();
					m_visibleShadowCastingLights.EmplaceBack(ShadowInfo{viewProjectionMatrix});
					m_visibleShadowCastingLightIdentifiers.EmplaceBack(spotLight.GetRenderItemIdentifier());
				}
				else
				{
					m_visibleShadowCastingLights[(ShadowMapIndexType)light.shadowMapIndex] = ShadowInfo{viewProjectionMatrix};
				}
			}

			for (VisibleLight& light : m_visibleLights[(uint8)LightTypes::PointLight])
			{
				const Entity::PointLightComponent& pointLight = static_cast<const Entity::PointLightComponent&>(*light.light);
				const Math::Matrix4x4f projection =
					Math::Matrix4x4f::CreatePerspective(90_degrees, 1.f, 0.1f, pointLight.GetInfluenceRadius().GetMeters());
				const Math::WorldCoordinate position = pointLight.GetWorldLocation();

				auto getLookAtMatrix = [position](const uint8 cubemapIndex)
				{
					switch (cubemapIndex)
					{
						case 0:
							return Math::Matrix4x4f::CreateLookAt(position, position + Math::Vector3f{Math::Right}, Math::Vector3f{Math::Up});
						case 1:
							return Math::Matrix4x4f::CreateLookAt(position, position - Math::Vector3f{Math::Right}, Math::Vector3f{Math::Up});
						case 2:
							return Math::Matrix4x4f::CreateLookAt(position, position + Math::Vector3f{Math::Forward}, Math::Vector3f{Math::Up});
						case 3:
							return Math::Matrix4x4f::CreateLookAt(position, position - Math::Vector3f{Math::Forward}, Math::Vector3f{Math::Up});
						case 4:
							return Math::Matrix4x4f::CreateLookAt(position, position + Math::Vector3f{Math::Up}, Math::Vector3f{Math::Backward});
						case 5:
							return Math::Matrix4x4f::CreateLookAt(position, position - Math::Vector3f{Math::Up}, Math::Vector3f{Math::Forward});
						default:
							ExpectUnreachable();
					}
				};

				if (light.shadowMapIndex == -1)
				{
					if (m_visibleShadowCastingLights.GetRemainingCapacity() < 6)
					{
						continue;
					}

					light.cascadeCount = 1;
					light.shadowMapIndex = m_visibleShadowCastingLights.GetNextAvailableIndex();

					const Entity::RenderItemIdentifier renderItemIdentifier = pointLight.GetRenderItemIdentifier();
					for (uint8 i = 0; i < 6; ++i)
					{
						Math::Matrix4x4f viewProjection = getLookAtMatrix(i) * projection;
						light.shadowSampleViewProjectionMatrices[i] = viewProjection * biasMatrix;
						m_visibleShadowCastingLights.EmplaceBack(ShadowInfo{viewProjection});
						m_visibleShadowCastingLightIdentifiers.EmplaceBack(renderItemIdentifier);
					}
				}
				else
				{
					for (uint8 i = 0; i < 6; ++i)
					{
						Math::Matrix4x4f viewProjection = getLookAtMatrix(i) * projection;
						light.shadowSampleViewProjectionMatrices[i] = viewProjection * biasMatrix;
						m_visibleShadowCastingLights[ShadowMapIndexType(light.shadowMapIndex + i)] = ShadowInfo{viewProjection};
					}
				}
			}

#if !ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
			const Entity::CameraComponent& activeCamera = m_sceneView.GetActiveCameraComponent();
			const float nearPlane = activeCamera.GetNearPlane().GetUnits();
			const float clipRange = activeCamera.GetDepthRange().GetUnits();

			Array<Math::Lengthf, VisibleLight::MaximumCascadeCount> cascadeSplits;
#endif
			DirectionalLightIndexType numDirectionalShadowingCastingLight = 0;
			for (VisibleLight& light : m_visibleLights[(uint8)LightTypes::DirectionalLight])
			{
				numDirectionalShadowingCastingLight += light.shadowMapIndex != -1;
			}

			for (VisibleLight& light : m_visibleLights[(uint8)LightTypes::DirectionalLight])
			{
				const Entity::DirectionalLightComponent& directionalLight = static_cast<const Entity::DirectionalLightComponent&>(*light.light);

				if (light.shadowMapIndex == -1)
				{
					const uint16 numCascades = directionalLight.GetCascadeCount();
					if (m_visibleShadowCastingLights.GetRemainingCapacity() < numCascades || numDirectionalShadowingCastingLight >= MaximumDirectionalLightCount)
					{
						continue;
					}

					++numDirectionalShadowingCastingLight;

					light.shadowMapIndex = m_visibleShadowCastingLights.GetNextAvailableIndex();
					Assert(numCascades <= MaximumCascadeCount);
					light.cascadeCount = (VisibleLight::CascadeIndexType)numCascades;

					const Entity::RenderItemIdentifier renderItemIdentifier = directionalLight.GetRenderItemIdentifier();

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
					light.directionalShadowMatrixIndex = m_parallelShadowMapsIndices.GetSize();

					for (uint8 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
					{
						m_parallelShadowMapsIndices.EmplaceBack(m_visibleShadowCastingLights.GetSize());
						m_visibleShadowCastingLights.EmplaceBack(ShadowInfo{}); // reserve space, will be filled by GPU
						m_visibleShadowCastingLightIdentifiers.EmplaceBack(renderItemIdentifier);
					}
#else
					cascadeSplits.GetView().CopyFrom(directionalLight.GetCascadeDistances());
					// Change the cascade split from meters to ratio
					for (uint8 i = 0; i < numCascades; i++)
					{
						cascadeSplits[i] = Math::Lengthf::FromMeters((cascadeSplits[i].GetMeters() - nearPlane) / clipRange);
					}

					Array<Math::Vector3f, 8> frustumCorners = {
						Math::Vector3f{-1.0f, 1.0f, 1.0f},  // near bottom left
						Math::Vector3f{-1.0f, -1.0f, 1.0f}, // near top left
						Math::Vector3f{1.0f, -1.0f, 1.0f},  // near top right
						Math::Vector3f{1.0f, 1.0f, 1.0f},   // near bottom right,
						Math::Vector3f{-1.0f, 1.0f, 0.0f},  // far bottom left
						Math::Vector3f{-1.0f, -1.0f, 0.0f}, // far top left
						Math::Vector3f{1.0f, -1.0f, 0.0f},  // far top right
						Math::Vector3f{1.0f, 1.0f, 0.0f},   // far bottom right,
					};

					// Project frustum corners into world space
					for (Math::Vector3f& frustumCorner : frustumCorners)
					{
						frustumCorner = m_sceneView.ViewToWorld(Math::Vector2f{frustumCorner.x, frustumCorner.y}, frustumCorner.z);
					}

					// Calculate orthographic projection matrix for each cascade

					const Math::Vector3f lightDir = light.light->GetWorldForwardDirection();

					Math::Lengthf lastSplitDist = 0_meters;
					for (uint8 cascadeIndex = 0; cascadeIndex < numCascades; cascadeIndex++)
					{
						const Math::Lengthf splitDist = cascadeSplits[cascadeIndex];

						Array<Math::Vector3f, 8> cascadeFrustumCorners;
						for (uint8 i = 0; i < 4; i++)
						{
							const Math::Vector3f dist = frustumCorners[i + 4] - frustumCorners[i];
							cascadeFrustumCorners[i + 4] = frustumCorners[i] + (dist * splitDist.GetMeters());
							cascadeFrustumCorners[i] = frustumCorners[i] + (dist * lastSplitDist.GetMeters());
						}

						// Get frustum center
						Math::Vector3f frustumCenter = Math::Zero;
						for (const Math::Vector3f frustumCorner : cascadeFrustumCorners)
						{
							frustumCenter += frustumCorner;
						}
						constexpr float inverseCornerCount = 1.f / cascadeFrustumCorners.GetSize();
						frustumCenter *= inverseCornerCount;

						float radius = 0.0f;
						for (const Math::Vector3f frustumCorner : cascadeFrustumCorners)
						{
							const float distance = (frustumCorner - frustumCenter).GetLength();
							radius = Math::Max(radius, distance);
						}
						radius = Math::Ceil(radius * 16.0f) / 16.0f;

						Math::Matrix4x4f lightViewMatrix =
							Math::Matrix4x4f::CreateLookAt(frustumCenter - lightDir * radius, frustumCenter, light.light->GetWorldUpDirection());

						// Snap frustumCenter to texel size in order to stabilize the shadow map (ie. no shimering)

						Math::Vector3f S(lightViewMatrix.m_rows[0].x, lightViewMatrix.m_rows[1].x, lightViewMatrix.m_rows[2].x);
						Math::Vector3f T(lightViewMatrix.m_rows[0].y, lightViewMatrix.m_rows[1].y, lightViewMatrix.m_rows[2].y);
						Math::Vector3f U(lightViewMatrix.m_rows[0].z, lightViewMatrix.m_rows[1].z, lightViewMatrix.m_rows[2].z);

						float s = S.Dot(frustumCenter);
						float t = T.Dot(frustumCenter);
						float u = U.Dot(frustumCenter);

						float texelSize = 2.0f * radius / (float)ShadowMapSize;

						s = (Math::Round(s / texelSize) + 0.5f) * texelSize; // TODO not sure about this half pixel offset
						t = (Math::Round(t / texelSize) + 0.5f) * texelSize;

						frustumCenter = S * s + T * t + U * u;

						// Compute matrices
						lightViewMatrix =
							Math::Matrix4x4f::CreateLookAt(frustumCenter - lightDir * radius, frustumCenter, light.light->GetWorldUpDirection());

						const Math::Matrix4x4f lightOrthoMatrix =
							Math::Matrix4x4f::CreateOrthographic(-radius, radius, -radius, radius, 0.0f, radius * 2.0f);

						const Math::Matrix4x4f viewProjection = lightViewMatrix * lightOrthoMatrix;
						light.shadowSampleViewProjectionMatrices[cascadeIndex] = viewProjection * biasMatrix;

						m_visibleShadowCastingLights.EmplaceBack(ShadowInfo{viewProjection});
						m_visibleShadowCastingLightIdentifiers.EmplaceBack(renderItemIdentifier);
						light.cascadeSplitDepths[cascadeIndex] = (nearPlane + splitDist.GetMeters() * clipRange);

						lastSplitDist = cascadeSplits[cascadeIndex];
					}
#endif
				}
			}

			m_numDirectionalShadowingCastingLight = numDirectionalShadowingCastingLight;
		}
	}
}
