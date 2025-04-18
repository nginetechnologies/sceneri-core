#pragma once

#include <Renderer/Constants.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Assets/Material/DescriptorContentType.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Descriptors/DescriptorType.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>

#include <Common/Function/EventCallbackResult.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Jobs/CallbackResult.h>

#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
	struct JobBatch;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct SceneView;

	struct RuntimeMaterialInstance;
	struct RuntimeDescriptorContent;
	struct RenderMaterial;
	struct RenderMaterialCache;
	struct RenderTexture;
	struct MipMask;

	struct RenderMaterialInstance final
	{
		struct DescriptorContent
		{
			using Type = DescriptorContentType;

			DescriptorContent(const Rendering::TextureIdentifier identifier, Rendering::Sampler&& sampler)
				: m_type(Type::Texture)
				, m_texture{identifier, Forward<Rendering::Sampler>(sampler)}
			{
			}
			~DescriptorContent()
			{
			}

			Type m_type;
			struct TextureData
			{
				Rendering::TextureIdentifier m_identifier;
				Rendering::Sampler m_sampler;
				Rendering::ImageMapping m_textureView;
			};

			union
			{
				TextureData m_texture;
			};
		};

		RenderMaterialInstance(RenderMaterialCache& renderMaterialCache, RenderMaterial& renderMaterial);
		RenderMaterialInstance(const RenderMaterialInstance&) = delete;
		RenderMaterialInstance& operator=(const RenderMaterialInstance&) = delete;
		RenderMaterialInstance(RenderMaterialInstance&&) = delete;
		RenderMaterialInstance& operator=(RenderMaterialInstance&&) = delete;

		[[nodiscard]] Threading::JobBatch Load(SceneView& sceneView, const MaterialInstanceIdentifier identifier);
		void Destroy(const Rendering::LogicalDevice& logicalDevice);

		[[nodiscard]] RenderMaterial& GetMaterial()
		{
			return m_material;
		}
		[[nodiscard]] const RenderMaterial& GetMaterial() const
		{
			return m_material;
		}
		[[nodiscard]] const RuntimeMaterialInstance& GetMaterialInstance() const
		{
			Expect(m_pMaterialInstance != nullptr);
			return *m_pMaterialInstance;
		}
		[[nodiscard]] Rendering::DescriptorSetView GetDescriptorSet() const
		{
			return m_descriptorSet.AtomicLoad();
		}

		[[nodiscard]] bool IsValid() const;
	protected:
		EventCallbackResult OnTextureLoadedAsync(
			LogicalDevice& logicalDevice,
			const TextureIdentifier identifier,
			RenderTexture& texture,
			const MipMask loadedMipValues,
			const EnumFlags<LoadedTextureFlags> loadFlags
		);
		void
		OnDescriptorContentChanged(const uint8 index, const RuntimeDescriptorContent& previousDescriptorContent, LogicalDevice& logicalDevice);
		void OnParentMaterialChanged(SceneView& sceneView);

		void LoadMaterialDescriptors(LogicalDevice& logicalDevice, RuntimeMaterialInstance& materialInstance);
	protected:
		RenderMaterialCache& m_renderMaterialCache;
		ReferenceWrapper<RenderMaterial> m_material;
		RuntimeMaterialInstance* m_pMaterialInstance = nullptr;
		Threading::Atomic<uint8> m_loadingTextureCount = 0;

		Rendering::DescriptorSet m_descriptorSet;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		Vector<DescriptorContent, uint8> m_descriptorContents;
		Threading::Mutex m_textureLoadMutex;
	};
}
