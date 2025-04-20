#pragma once

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>

#include <DeferredShading/LightTypes.h>
#include <DeferredShading/VisibleLight.h>

#include <Common/Asset/Guid.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>

namespace ngine::Entity
{
	struct LightSourceComponent;
	struct SpotLightComponent;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
	struct ShadowsStage;

	struct LightGatheringStage final
	{
		LightGatheringStage(SceneView& sceneView);
		virtual ~LightGatheringStage();

		[[nodiscard]] static LightTypes GetLightType(const Entity::LightSourceComponent& component);

		struct Header
		{
			uint32 lightCount;
			uint32 pad[3];
		};

		struct ShadowInfo
		{
			Math::Matrix4x4f viewProjectionMatrix;
		};

		inline static constexpr uint16 MaximumDirectionalLightCount = 1;
		using DirectionalLightIndexType = Memory::NumericSize<MaximumDirectionalLightCount>;

		inline static constexpr uint16 MaximumDirectionalLightCascadeCount = 4;
		inline static constexpr uint16 MaximumShadowmapCount = 8;
		using ShadowMapIndexType = Memory::NumericSize<MaximumShadowmapCount>;

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		struct SDSMLightInfo
		{
			Math::Matrix4x4f ViewMatrix;
			Math::Vector4i BoundMin[MaximumDirectionalLightCascadeCount];
			Math::Vector4i BoundMax[MaximumDirectionalLightCascadeCount];
			Math::Vector4f CascadeSplits;
			Math::UnalignedVector3<float> LightDirection;
			float PSSMLambda;
			float MaxShadowRange;
			float pad[3];
		};

		struct SDSMShadowSamplingInfo
		{
			Math::Matrix4x4f ViewProjMatrix[MaximumDirectionalLightCascadeCount];
			Math::Vector4f CascadeSplits;
		};
#endif

		[[nodiscard]] ArrayView<const uint32, ShadowMapIndexType> GetParallelShadowMapsIndices() const
		{
			return m_parallelShadowMapsIndices.GetView();
		}
		[[nodiscard]] ArrayView<const ShadowInfo, ShadowMapIndexType> GetVisibleShadowCastingLights() const
		{
			return m_visibleShadowCastingLights.GetView();
		}
		[[nodiscard]] bool HasVisibleShadowCastingLights() const
		{
			return m_visibleShadowCastingLights.HasElements();
		}
		[[nodiscard]] DirectionalLightIndexType GetDirectionalShadowingCastingLightCount() const
		{
			return m_numDirectionalShadowingCastingLight;
		}
		[[nodiscard]] ArrayView<const VisibleLight> GetVisibleLights(const LightTypes lightType) const
		{
			return m_visibleLights[(uint8)lightType];
		}
		[[nodiscard]] Optional<const VisibleLight*> GetVisibleLightInfo(const Entity::LightSourceComponent& light) const;

		[[nodiscard]] static constexpr size CalculateLightBufferSize(const ShadowMapIndexType shadowMapCount)
		{
			return sizeof(Header) + sizeof(ShadowInfo) * shadowMapCount;
		}
		[[nodiscard]] bool HasVisibleLights() const
		{
			return m_visibleLights.GetView().Any(
				[](const auto& visibleLights)
				{
					return visibleLights.HasElements();
				}
			);
		}

		void SetShadowsStage(const Optional<ShadowsStage*> pShadowsStage)
		{
			m_pShadowsStage = pShadowsStage;
		}

		bool OnRenderItemsBecomeVisible(const SceneRenderStageIdentifier stageIdentifier, const Entity::RenderItemMask& renderItems);
		void OnVisibleRenderItemsReset(const SceneRenderStageIdentifier stageIdentifier, const Entity::RenderItemMask& renderItems);
		bool OnRenderItemsBecomeHidden(const Entity::RenderItemMask& renderItems);
		void OnVisibleRenderItemTransformsChanged(const Entity::RenderItemMask& renderItems);
		void OnSceneUnloaded();
		void OnActiveCameraPropertiesChanged();
	protected:
		void UpdateLightBuffer();
	protected:
		SceneView& m_sceneView;
		Optional<ShadowsStage*> m_pShadowsStage;

		using LightContainer = Vector<VisibleLight>;
		Array<LightContainer, (uint8)LightTypes::Count> m_visibleLights;
		Entity::RenderItemMask m_visibleLightsMask;

		DirectionalLightIndexType m_numDirectionalShadowingCastingLight = 0;

		FlatVector<ShadowInfo, MaximumShadowmapCount> m_visibleShadowCastingLights;
		FlatVector<Entity::RenderItemIdentifier, MaximumShadowmapCount> m_visibleShadowCastingLightIdentifiers;
		FlatVector<uint32, MaximumShadowmapCount> m_parallelShadowMapsIndices;
	};
}
