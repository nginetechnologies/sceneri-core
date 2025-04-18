#pragma once

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Asset/Guid.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Assets/Shader/ShaderIdentifier.h>
#include <Renderer/Assets/Shader/Shader.h>
#include <Renderer/ShaderStage.h>
#include <Renderer/Wrappers/RenderPassView.h>

#include <Engine/Asset/AssetType.h>

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Memory/Variant.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::IO
{
	struct FileView;
	struct Path;
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct GraphicsPipelineBase;
	struct PipelineLayoutView;
	struct VertexInputBindingDescription;
	struct VertexInputAttributeDescription;

	struct ShaderCache final : public Asset::Type<ShaderIdentifier, Shader>
	{
		using BaseType = Asset::Type<ShaderIdentifier, Shader>;

		ShaderCache();
		ShaderCache(const ShaderCache&) = delete;
		ShaderCache(ShaderCache&&) = delete;
		ShaderCache& operator=(const ShaderCache&) = delete;
		ShaderCache& operator=(ShaderCache&&) = delete;
		~ShaderCache();

		void Destroy(const LogicalDeviceView logicalDevice);

		[[nodiscard]] LogicalDevice& GetLogicalDevice();
		[[nodiscard]] const LogicalDevice& GetLogicalDevice() const;

		using ShaderRequesters = ThreadSafe::Event<EventCallbackResult(void*, const ShaderView), 24, false>;

		struct VertexInfo
		{
			ArrayView<const VertexInputBindingDescription, uint16> m_inputBindings;
			ArrayView<const VertexInputAttributeDescription, uint16> m_inputAttributes;
		};
		struct FragmentInfo
		{
			RenderPassView m_renderPass;
			uint8 m_subpassIndex;
		};
		using StageInfo = Variant<VertexInfo, FragmentInfo>;

		[[nodiscard]] Threading::Job* FindOrLoad(
			const Asset::Guid guid,
			const ShaderStage stage,
			const ConstZeroTerminatedStringView entryPointName,
			const PipelineLayoutView pipelineLayout,
			const StageInfo stageInfo,
			ShaderRequesters::ListenerData&& listenerData
		);

		void SaveToDisk() const;
#if RENDERER_VULKAN
		[[nodiscard]] VkPipelineCache GetPipelineCache() const
		{
			return m_pPipelineCache;
		}
#endif
	protected:
		[[nodiscard]] Shader CreateShaderInternal(
			const ConstByteView shaderFileContents,
			const ShaderStage stage,
			const ConstZeroTerminatedStringView entryPointName,
			const PipelineLayoutView pipelineLayout,
			const StageInfo stageInfo
		);

		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;

		[[nodiscard]] IO::Path GetShaderCacheFilePath() const;
	protected:
#if RENDERER_VULKAN
		VkPipelineCache m_pPipelineCache = 0;
#endif

		Threading::SharedMutex m_shaderRequesterMutex;
		UnorderedMap<ShaderIdentifier, UniqueRef<ShaderRequesters>, ShaderIdentifier::Hash> m_shaderRequesterMap;
		Threading::AtomicIdentifierMask<ShaderIdentifier> m_loadingShaders;
	};
}
