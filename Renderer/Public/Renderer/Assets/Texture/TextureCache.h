#pragma once

#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Assets/Texture/RenderTargetTemplateIdentifier.h>
#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Renderer/Wrappers/ImageMappingType.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/ImageAspectFlags.h>
#include <Renderer/ImageLayout.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Constants.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h> // temp

#include <Engine/Asset/AssetType.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Optional.h>
#include <Common/Function/Event.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Vector2.h>
#include <Common/TypeTraits/IsConvertibleTo.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Function/Function.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicPtr.h>

namespace ngine
{
	struct Engine;

	namespace Threading
	{
		struct Job;
		struct JobBatch;
		struct EngineJobRunnerThread;
	}

	namespace IO
	{
		struct LoadingThreadOnly;
		struct LoadingThreadJob;
	}
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandBuffer;
	struct Renderer;
	struct RenderTexture;
	struct ImageMapping;
	struct MipRange;
	struct MipMask;
	struct ArrayRange;

	using TextureLoadingCallback = Function<Optional<Threading::Job*>(const TextureIdentifier identifier, LogicalDevice& logicalDevice), 24>;
	struct TextureInfo
	{
		TextureLoadingCallback m_loadingCallback;
	};

	using RenderTargetCreationCallback = Function<
		Optional<Threading::Job*>(
			TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			LogicalDevice& logicalDevice,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const uint8 numArrayLayers
		),
		24>;
	using RenderTargetResizingCallback = Function<
		Optional<Threading::Job*>(
			TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			LogicalDevice& logicalDevice,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const uint8 numArrayLayers
		),
		24>;
	struct RenderTargetInfo
	{
		RenderTargetCreationCallback m_creationCallback;
		RenderTargetResizingCallback m_resizingCallback;
	};

	enum class TextureLoadFlags : uint8
	{
		//! Assign a dummy texture if the requested one is not loaded yet (1, 1, 1, 0 if RGBA)
		LoadDummy = 1 << 0,
		Default = LoadDummy
	};
	ENUM_FLAG_OPERATORS(TextureLoadFlags);

	struct TextureCache final : public Asset::Type<TextureIdentifier, TextureInfo>
	{
		using BaseType = Type;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& GetRenderer();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& GetRenderer() const;

		TextureCache();
		TextureCache(const TextureCache&) = delete;
		TextureCache& operator=(const TextureCache&) = delete;
		TextureCache(TextureCache&&) = delete;
		TextureCache& operator=(TextureCache&&) = delete;
		virtual ~TextureCache();

		[[nodiscard]] Guid GetRenderTargetAssetGuid(const RenderTargetTemplateIdentifier identifier) const
		{
			return m_renderTargetAssetType.GetAssetGuid(identifier);
		}

		[[nodiscard]] TextureIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] RenderTargetTemplateIdentifier RegisterRenderTargetTemplate(const Asset::Guid guid);
		[[nodiscard]] RenderTargetTemplateIdentifier RegisterProceduralRenderTargetTemplate(
			const Asset::Guid guid, RenderTargetCreationCallback&& creationCallback, RenderTargetResizingCallback&& resizingCallback
		);
		[[nodiscard]] RenderTargetTemplateIdentifier FindOrRegisterRenderTargetTemplate(const Asset::Guid guid);
		[[nodiscard]] RenderTargetTemplateIdentifier FindRenderTargetTemplateIdentifier(const Asset::Guid guid);
		[[nodiscard]] TextureIdentifier RegisterProceduralRenderTargetAsset();

		void Remove(const TextureIdentifier identifier);

		[[nodiscard]] const TextureLoadingCallback& GetLoadingCallback(const TextureIdentifier identifier) const
		{
			return GetAssetData(identifier).m_loadingCallback;
		}

		using TextureLoadEvent = ThreadSafe::Event<
			EventCallbackResult(
				void*,
				LogicalDevice& logicalDevice,
				const TextureIdentifier identifier,
				RenderTexture& texture,
				MipMask changedMips,
				const EnumFlags<LoadedTextureFlags> flags
			),
			96,
			false>;
		using TextureLoadListenerData = TextureLoadEvent::ListenerData;
		using TextureLoadListenerIdentifier = TextureLoadEvent::ListenerIdentifier;

		[[nodiscard]] Optional<Threading::Job*> GetOrLoadRenderTexture(
			const LogicalDeviceIdentifier deviceIdentifier,
			const TextureIdentifier identifier,
			const ImageMappingType type,
			const MipMask requestedMips,
			const EnumFlags<TextureLoadFlags> flags,
			TextureLoadListenerData&& listenerData
		);
		[[nodiscard]] Optional<Threading::Job*>
		ReloadRenderTexture(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier identifier);
		[[nodiscard]] Threading::JobBatch ReloadRenderTexture(const TextureIdentifier identifier);
		bool RemoveRenderTextureListener(
			const LogicalDeviceIdentifier deviceIdentifier,
			const TextureIdentifier identifier,
			const TextureLoadListenerIdentifier listenerIdentifier
		);
		void
		AssignRenderTexture(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier, RenderTexture&& texture, RenderTexture& previousTexture, const EnumFlags<LoadedTextureFlags>);
		void
		ChangeRenderTextureAvailableMips(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier, MipMask loadedMips, const EnumFlags<LoadedTextureFlags>);
		void OnTextureLoadFinished(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier);
		void OnTextureLoadFailed(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier);
		[[nodiscard]] Optional<RenderTexture*>
		GetRenderTexture(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier identifier) const
		{
			return m_perLogicalDeviceData[deviceIdentifier]->m_textures[identifier];
		}
		[[nodiscard]] bool IsRenderTextureLoaded(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier identifier) const
		{
			return m_perLogicalDeviceData[deviceIdentifier]->m_textures[identifier].IsValid();
		}
		[[nodiscard]] bool IsRenderTextureLoading(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier identifier) const
		{
			return m_perLogicalDeviceData[deviceIdentifier]->m_loadingTextures.IsSet(identifier);
		}
		[[nodiscard]] inline MipMask
		GetRequestedTextureMips(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier textureIdentifier) const
		{
			const PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			const PerDeviceTextureData& perDeviceTextureData = *perDeviceData.m_textureData[textureIdentifier];
			return MipMask{perDeviceTextureData.m_requestedMips.Load()};
		}
		[[nodiscard]] DescriptorSetLayoutView GetTexturesDescriptorSetLayout(const LogicalDeviceIdentifier deviceIdentifier) const
		{
			const PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			return perDeviceData.m_texturesDescriptorSetLayout;
		}
		[[nodiscard]] DescriptorSetView GetTexturesDescriptorSet(const LogicalDeviceIdentifier deviceIdentifier) const
		{
			const PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			return perDeviceData.m_texturesDescriptorSet;
		}

		[[nodiscard]] Optional<Threading::Job*> GetOrLoadRenderTarget(
			LogicalDevice& logicalDevice,
			const TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const ArrayRange arrayLayerRange,
			TextureLoadListenerData&& listenerData
		);
		[[nodiscard]] Optional<Threading::Job*> ResizeRenderTarget(
			LogicalDevice& logicalDevice,
			const TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const ArrayRange arrayLayerRange
		);
		void DestroyRenderTarget(LogicalDevice& logicalDevice, const TextureIdentifier identifier);
	protected:
		void OnLogicalDeviceCreated(LogicalDevice& logicalDevice);

		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;

		[[nodiscard]] TextureIdentifier RegisterAsset(const Asset::Guid guid);

		struct PerDeviceTextureData
		{
			TextureLoadEvent m_onLoadedCallback;
			EnumFlags<LoadedTextureFlags> m_flags;
			Threading::Atomic<MipMask::StoredType> m_requestedMips;
			ImageMapping m_mapping;
		};

		struct PerLogicalDeviceData
		{
			TIdentifierArray<UniquePtr<RenderTexture>, TextureIdentifier> m_textures{Memory::Zeroed};
			UniquePtr<RenderTexture> m_pDummyTexture;
			UniquePtr<RenderTexture> m_pDummyTextureCube;

			Threading::EngineJobRunnerThread* m_pTexturesDescriptorPoolLoadingThread{nullptr};
			DescriptorSetLayout m_texturesDescriptorSetLayout;
			DescriptorSet m_texturesDescriptorSet;

			Sampler m_tempTextureSampler;

			Threading::AtomicIdentifierMask<TextureIdentifier> m_loadingTextures;
			TIdentifierArray<Threading::Atomic<PerDeviceTextureData*>, TextureIdentifier> m_textureData{Memory::Zeroed};
		};

		PerDeviceTextureData& GetOrCreatePerDeviceTextureData(PerLogicalDeviceData& perDeviceData, const TextureIdentifier identifier);
		RenderTexture& GetDummyTexture(const LogicalDeviceIdentifier deviceIdentifier, const ImageMappingType type) const;
	protected:
		TIdentifierArray<UniquePtr<PerLogicalDeviceData>, LogicalDeviceIdentifier> m_perLogicalDeviceData;

		Asset::Type<RenderTargetTemplateIdentifier, RenderTargetInfo> m_renderTargetAssetType;
	};
}
