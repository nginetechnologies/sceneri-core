#pragma once

#include <Common/Asset/Asset.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Math/Color.h>
#include <Common/Guid.h>

#include <Renderer/Constants.h>
#include <Renderer/ShaderStage.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Assets/Texture/TexturePreset.h>
#include <Renderer/Assets/Material/DescriptorContentType.h>
#include <Renderer/PushConstants/PushConstantDefinition.h>
#include <Renderer/Wrappers/AddressMode.h>
#include <Renderer/Wrappers/CompareOperation.h>
#include <Renderer/Wrappers/StencilOperation.h>
#include <Renderer/Wrappers/BlendFactor.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Pipelines/AttachmentBlendState.h>

namespace ngine::Asset
{
	struct Database;
}

namespace ngine::Rendering
{
	struct MaterialAsset : public Asset::Asset
	{
		struct DescriptorBinding
		{
			using Type = DescriptorContentType;

			DescriptorBinding()
			{
			}

			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			uint8 m_location;
			Type m_type;
			struct SamplerInfo
			{
				TexturePreset m_texturePreset;
				Asset::Guid m_defaultTextureGuid;
				AddressMode m_defaultAddressMode;
			};

			union
			{
				SamplerInfo m_samplerInfo;
			};
			EnumFlags<ShaderStage> m_shaderStages;
		};

		using DescriptorBindings = Vector<DescriptorBinding, uint8>;

		struct ColorBlendingSettings
		{
			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			ColorAttachmentBlendState m_colorBlendState;
			AlphaAttachmentBlendState m_alphaBlendState;
		};

		struct StencilTestSettings
		{
			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			bool m_enable = false;
			inline static constexpr StencilOperation DefaultFailureOperation = StencilOperation::Keep;
			StencilOperation m_failureOperation = DefaultFailureOperation;
			inline static constexpr StencilOperation DefaultPassOperation = StencilOperation::Keep;
			StencilOperation m_passOperation = DefaultPassOperation;
			inline static constexpr StencilOperation DefaultDepthFailOperation = StencilOperation::Keep;
			StencilOperation m_depthFailOperation = DefaultDepthFailOperation;
			inline static constexpr CompareOperation DefaultCompareOperation = CompareOperation::AlwaysSucceed;
			CompareOperation m_compareOperation = DefaultCompareOperation;
			inline static constexpr uint32 DefaultCompareMask = 0u;
			uint32 m_compareMask = 0u;
			inline static constexpr uint32 DefaultWriteMask = 0u;
			uint32 m_writeMask = 0u;
			inline static constexpr uint32 DefaultReference = 0u;
			uint32 m_reference = 0u;
		};

		struct Attachment
		{
			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			ngine::Asset::Guid m_renderTargetAssetGuid;
			Math::Color m_clearColor = {0, 0, 0, 1.f};
			AttachmentLoadType m_loadType = AttachmentLoadType::LoadExisting;
			AttachmentStoreType m_storeType = AttachmentStoreType::Store;
			AttachmentLoadType m_stencilLoadType = AttachmentLoadType::Undefined;
			AttachmentStoreType m_stencilStoreType = AttachmentStoreType::Undefined;
			ColorBlendingSettings m_colorBlending;
		};

		struct PushConstants
		{
			using Container = Vector<PushConstantDefinition, uint8>;
			Container m_definitions;
		};

		enum class VertexAttributes : uint8
		{
			Position = 1 << 0,
			Normals = 1 << 1,
			TextureCoordinates = 1 << 2,
			InstanceIdentifier = 1 << 3,
			Count = Math::Log2(InstanceIdentifier) + 1,
			All = Position | Normals | TextureCoordinates | InstanceIdentifier
		};

		MaterialAsset() = default;
		MaterialAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		MaterialAsset(const MaterialAsset&) = delete;
		MaterialAsset(MaterialAsset&& other) = default;
		MaterialAsset& operator=(const MaterialAsset&) = delete;
		MaterialAsset& operator=(MaterialAsset&& other) = default;

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] ngine::Asset::Guid GetVertexShaderAssetGuid() const
		{
			return m_vertexShaderAssetGuid;
		}
		[[nodiscard]] ngine::Asset::Guid GetPixelShaderAssetGuid() const
		{
			return m_pixelShaderAssetGuid;
		}

		[[nodiscard]] ArrayView<const DescriptorBinding, uint8> GetDescriptorBindings() const
		{
			return m_descriptorBindings;
		}
		[[nodiscard]] typename PushConstants::Container::ConstView GetPushConstants() const
		{
			return m_pushConstants.m_definitions;
		}

		[[nodiscard]] ArrayView<const Asset::Guid> GetDependentStages() const
		{
			return m_dependentStages;
		}

		bool Compile()
		{
			// TODO: Use reflection to update the .mtl bindings and locations from the compiled shaders

			m_dependencies.Clear();
			m_dependencies.EmplaceBack(m_vertexShaderAssetGuid);
			m_dependencies.EmplaceBack(m_pixelShaderAssetGuid);

			return true;
		}

		ngine::Asset::Guid m_vertexShaderAssetGuid;
		ngine::Asset::Guid m_pixelShaderAssetGuid;

		DescriptorBindings m_descriptorBindings;
		PushConstants m_pushConstants;

		Vector<Asset::Guid> m_dependentStages;

		EnumFlags<VertexAttributes> m_requiredVertexAttributes{VertexAttributes::All};

		inline static constexpr uint8 DefaultRequiredMatrices = 1ull << (uint8)ViewMatrices::Type::ViewProjection;
		uint8 m_requiredMatrices = DefaultRequiredMatrices;

		// todo: make initial attachment be the main view
		Vector<Attachment, uint8> m_attachments;

		inline static constexpr bool DefaultIsTwoSided = false;
		bool m_twoSided = DefaultIsTwoSided;

		inline static constexpr bool DefaultEnableDepthTest = true;
		bool m_enableDepthTest = DefaultEnableDepthTest;
		inline static constexpr bool DefaultEnableDepthWrite = true;
		bool m_enableDepthWrite = DefaultEnableDepthWrite;
		inline static constexpr CompareOperation DefaultDepthCompareOperation = CompareOperation::Greater;
		inline static constexpr bool DefaultEnableDepthClamp = false;
		bool m_enableDepthClamp = DefaultEnableDepthClamp;
		CompareOperation m_depthCompareOperation = DefaultDepthCompareOperation;
		StencilTestSettings m_stencilTestSettings;
	};
}
