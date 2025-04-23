#pragma once

#include <Renderer/Framegraph/Framegraph.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Assets/Texture/RenderTargetTemplateIdentifier.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Rendering
{
	struct RenderOutput;
	struct MaterialStage;
	struct Stage;
	struct SceneView;
	struct FramegraphBuilder;

	struct BuildAccelerationStructureStage;
	struct MaterialIdentifiersStage;
	struct TilePopulationStage;
	struct ShadowsStage;
	struct DepthMinMaxPyramidStage;
	struct PBRLightingStage;
	struct SSAOStage;
	struct SSRStage;
	struct PostProcessStage;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Widgets
{
	struct CopyToScreenStage;
}

namespace ngine::Font
{
	struct DrawTextStage;
}

namespace ngine::App::UI
{
	struct SceneFramegraphBuilder
	{
		SceneFramegraphBuilder();
		SceneFramegraphBuilder(const SceneFramegraphBuilder&) = delete;
		SceneFramegraphBuilder(SceneFramegraphBuilder&&) = delete;
		SceneFramegraphBuilder& operator=(const SceneFramegraphBuilder&) = delete;
		SceneFramegraphBuilder& operator=(SceneFramegraphBuilder&&) = delete;
		~SceneFramegraphBuilder();

		void Create(Rendering::SceneView& sceneView, Threading::JobBatch& jobBatchOut);
		void Build(Rendering::FramegraphBuilder& framegraphBuilder, Rendering::SceneView& sceneView);
	protected:
		Optional<Rendering::MaterialStage*> EmplaceMaterialStage(
			Rendering::SceneView& sceneView,
			const Asset::Guid guid,
			const float renderAreaFactor,
			const EnumFlags<Rendering::Stage::Flags> stageFlags = Rendering::Stage::Flags::Enabled
		);

		template<typename Type, typename... Args>
		Type& EmplaceRenderStage(Args&&... args)
		{
			return static_cast<Type&>(*m_renderStages.EmplaceBack(UniquePtr<Type>::Make(Forward<Args>(args)...)));
		}
	protected:
		Threading::Mutex m_mutex;

		Vector<UniquePtr<Rendering::MaterialStage>, uint16> m_materialStages;
		ArrayView<ReferenceWrapper<Rendering::Stage>, uint16> m_deferredGBufferPopulationStages;
		ArrayView<ReferenceWrapper<Rendering::Stage>, uint16> m_deferredSkyboxStages;
		FlatVector<ReferenceWrapper<Rendering::Stage>, 1> m_deferredLightingStages;

		ArrayView<ReferenceWrapper<Rendering::Stage>, uint16> m_commonMaterialStages;

		Vector<UniquePtr<Rendering::Stage>> m_renderStages;
	private:
		Optional<Rendering::BuildAccelerationStructureStage*> m_pBuildAccelerationStructureStage = nullptr;
		Optional<Rendering::MaterialIdentifiersStage*> m_pMaterialIdentifiersStage = nullptr;
		Optional<Rendering::TilePopulationStage*> m_pTilePopulationStage = nullptr;
		Optional<Rendering::ShadowsStage*> m_pShadowsStage = nullptr;
		Optional<Rendering::DepthMinMaxPyramidStage*> m_pDepthMinMaxPyramidStage = nullptr;
		Optional<Rendering::PBRLightingStage*> m_pPBRLightingStage = nullptr;
		Optional<Rendering::SSAOStage*> m_pSSAOStage = nullptr;
		Optional<Rendering::SSRStage*> m_pSSRStage = nullptr;
		Optional<Rendering::PostProcessStage*> m_pPostProcessStage = nullptr;

		Optional<Font::DrawTextStage*> m_pDrawTextStage = nullptr;
		UniquePtr<Widgets::CopyToScreenStage> m_pCopyToScreenStage;
		UniquePtr<Widgets::CopyToScreenStage> m_pCopyTemporalAAHistoryStage;
	};

	//! Graph of all stages that make up a frame
	//! Currently hardcoded, but will in the future be exposed to UI and compile out
	struct SceneFramegraph : public Rendering::Framegraph, public SceneFramegraphBuilder
	{
		SceneFramegraph(Rendering::LogicalDevice& logicalDevice, Rendering::RenderOutput& renderOutput);
		SceneFramegraph(const SceneFramegraph&) = delete;
		SceneFramegraph(SceneFramegraph&&) = delete;
		SceneFramegraph& operator=(const SceneFramegraph&) = delete;
		SceneFramegraph& operator=(SceneFramegraph&&) = delete;
		virtual ~SceneFramegraph();

		void Build(Rendering::SceneView& sceneView, Threading::JobBatch& jobBatchOut);
	};
}
