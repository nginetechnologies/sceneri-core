#pragma once

#include <Common/Math/ForwardDeclarations/Matrix4x4.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Threading/Mutexes/Mutex.h>

#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Constants.h>
#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Threading
{
	struct StageBase;
	struct JobBatch;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;

	struct RenderPassView;
	struct RenderMeshView;
	struct BufferView;
	struct RenderMaterialCache;
	struct SceneView;

	struct RuntimeMaterial;
	struct RenderMaterialInstance;
	struct ViewMatrices;

	struct RenderMaterial final : public DescriptorSetLayout, Rendering::GraphicsPipeline
	{
		RenderMaterial(RenderMaterialCache& cache, const RuntimeMaterial& material);
		RenderMaterial(const RenderMaterial&) = delete;
		RenderMaterial& operator=(const RenderMaterial&) = delete;
		RenderMaterial(RenderMaterial&&) = delete;
		RenderMaterial& operator=(RenderMaterial&&) = delete;
		~RenderMaterial();

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);

		void Draw(
			const uint32 firstInstanceIndex,
			const uint32 instanceCount,
			const Rendering::RenderMeshView mesh,
			const BufferView instanceBuffer,
			const RenderMaterialInstance& materialInstance,
			const Rendering::RenderCommandEncoderView renderCommandEncoder
		) const;

		[[nodiscard]] const RuntimeMaterial& GetMaterial() const
		{
			return m_material;
		}

		const RenderMaterialInstance& GetOrLoadMaterialInstance(SceneView& sceneView, const MaterialInstanceIdentifier identifier);
		Threading::JobBatch LoadRenderMaterialInstanceResources(SceneView& sceneView, const MaterialInstanceIdentifier identifier);

		void OnMaterialInstanceLoaded(const MaterialInstanceIdentifier identifier)
		{
			[[maybe_unused]] const bool wasCleared = m_loadingMaterialInstances.Clear(identifier);
			Assert(wasCleared);
		}

		void CreateDescriptorSetLayout(Rendering::LogicalDevice& logicalDevice);
		void CreateBasePipeline(
			Rendering::LogicalDevice& logicalDevice,
			const DescriptorSetLayoutView viewInfoDescriptorSetLayout,
			const DescriptorSetLayoutView transformBufferDescriptorSetLayout
		);
		[[nodiscard]] Threading::JobBatch CreatePipeline(
			Rendering::LogicalDevice& logicalDevice,
			Rendering::ShaderCache& shaderCache,
			const Rendering::RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);

		[[nodiscard]] ArrayView<const PushConstantRange> GetPushConstantRanges() const
		{
			return m_pushConstantRanges.GetView();
		}
	protected:
		using PushConstantRangeContainer = FixedCapacityVector<PushConstantRange, uint8>;
	protected:
		RenderMaterialCache& m_cache;
		ReferenceWrapper<const RuntimeMaterial> m_material;
		Threading::Mutex m_descriptorLoadingMutex;
		PushConstantRangeContainer m_pushConstantRanges;
		TIdentifierArray<UniquePtr<RenderMaterialInstance>, MaterialInstanceIdentifier> m_materialInstances{Memory::Zeroed};
		Threading::AtomicIdentifierMask<MaterialInstanceIdentifier> m_loadingMaterialInstances;

#if RENDERER_OBJECT_DEBUG_NAMES
		String m_debugName;
#endif
	};
}
