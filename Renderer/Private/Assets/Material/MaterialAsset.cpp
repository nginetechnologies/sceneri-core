#include <Renderer/Assets/Material/MaterialAsset.h>
#include <Renderer/Assets/Material/MaterialAssetType.h>

#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Asset/AssetDatabase.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Align.h>

namespace ngine::Rendering
{
	MaterialAsset::MaterialAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath)
	{
		m_metaDataFilePath = Forward<IO::Path>(assetMetaFilePath);
		Serialization::Deserialize(assetData, *this);
		SetTypeGuid(MaterialAssetType::AssetFormat.assetTypeGuid);
	}

	bool MaterialAsset::Serialize(const Serialization::Reader serializer)
	{
		Asset::Asset::Serialize(serializer);

		serializer.Serialize("vertex_shader", m_vertexShaderAssetGuid);
		serializer.Serialize("pixel_shader", m_pixelShaderAssetGuid);

		serializer.Serialize("descriptor_bindings", m_descriptorBindings);
		serializer.Serialize("push_constants", m_pushConstants.m_definitions);

		serializer.Serialize("dependent_stages", m_dependentStages);

		serializer.Serialize("vertex_attributes", m_requiredVertexAttributes.GetUnderlyingValue());
		serializer.Serialize("required_matrices", m_requiredMatrices);

		if (!serializer.Serialize("attachments", m_attachments))
		{
			m_attachments.Clear();
			m_attachments.EmplaceBack(Attachment());
		}

		serializer.Serialize("two_sided", m_twoSided);
		serializer.Serialize("depth_test", m_enableDepthTest);
		serializer.Serialize("depth_write", m_enableDepthWrite);
		serializer.Serialize("depth_compare_operation", m_depthCompareOperation);
		serializer.Serialize("depth_clamp", m_enableDepthClamp);
		serializer.Serialize("stencil_test", m_stencilTestSettings);

		return true;
	}

	bool MaterialAsset::Serialize(Serialization::Writer serializer) const
	{
		Assert(m_typeGuid == MaterialAssetType::AssetFormat.assetTypeGuid);

		Asset::Asset::Serialize(serializer);

		serializer.Serialize("vertex_shader", m_vertexShaderAssetGuid);
		serializer.Serialize("pixel_shader", m_pixelShaderAssetGuid);

		serializer.Serialize("descriptor_bindings", m_descriptorBindings);
		serializer.Serialize("push_constants", m_pushConstants.m_definitions);

		serializer.Serialize("dependent_stages", m_dependentStages);

		serializer.SerializeWithDefaultValue(
			"vertex_attributes",
			m_requiredVertexAttributes.GetUnderlyingValue(),
			(UNDERLYING_TYPE(VertexAttributes))VertexAttributes::All
		);
		serializer.SerializeWithDefaultValue("required_matrices", m_requiredMatrices, DefaultRequiredMatrices);

		serializer.Serialize("attachments", m_attachments);

		serializer.SerializeWithDefaultValue("depth_test", m_enableDepthTest, DefaultEnableDepthTest);
		serializer.SerializeWithDefaultValue("depth_write", m_enableDepthWrite, DefaultEnableDepthWrite);
		serializer.SerializeWithDefaultValue("two_sided", m_twoSided, DefaultIsTwoSided);
		serializer.SerializeWithDefaultValue("depth_compare_operation", m_depthCompareOperation, DefaultDepthCompareOperation);
		serializer.SerializeWithDefaultValue("depth_clamp", m_enableDepthClamp, DefaultEnableDepthClamp);

		serializer.Serialize("stencil_test", m_stencilTestSettings);

		return true;
	}

	bool MaterialAsset::DescriptorBinding::Serialize(const Serialization::Reader serializer)
	{
		serializer.Serialize("location", m_location);
		serializer.Serialize("type", m_type);
		switch (m_type)
		{
			case Type::Invalid:
				ExpectUnreachable();
			case Type::Texture:
			{
				serializer.Serialize("preset", m_samplerInfo.m_texturePreset);
				serializer.Serialize("default_texture", m_samplerInfo.m_defaultTextureGuid);
				if (!serializer.Serialize("default_address_mode", m_samplerInfo.m_defaultAddressMode))
				{
					m_samplerInfo.m_defaultAddressMode = Rendering::AddressMode::Repeat;
				}
			}
			break;
		}

		serializer.Serialize("stages", m_shaderStages.GetUnderlyingValue());
		return true;
	}

	bool MaterialAsset::DescriptorBinding::Serialize(Serialization::Writer serializer) const
	{
		bool wroteAny = serializer.Serialize("location", m_location);
		wroteAny |= serializer.Serialize("type", m_type);
		switch (m_type)
		{
			case Type::Invalid:
				ExpectUnreachable();
			case Type::Texture:
			{
				wroteAny |= serializer.Serialize("preset", m_samplerInfo.m_texturePreset);
				wroteAny |= serializer.Serialize("default_texture", m_samplerInfo.m_defaultTextureGuid);

				if (m_samplerInfo.m_defaultAddressMode != AddressMode::Repeat)
				{
					wroteAny |= serializer.Serialize("default_address_mode", m_samplerInfo.m_defaultAddressMode);
				}
			}
			break;
		}

		wroteAny |= serializer.Serialize("stages", m_shaderStages.GetUnderlyingValue());
		return wroteAny;
	}

	bool MaterialAsset::ColorBlendingSettings::Serialize(const Serialization::Reader serializer)
	{
		serializer.Serialize("source_blend_factor", m_colorBlendState.sourceBlendFactor);
		serializer.Serialize("target_blend_factor", m_colorBlendState.targetBlendFactor);
		serializer.Serialize("source_alpha_blend_factor", m_alphaBlendState.sourceBlendFactor);
		serializer.Serialize("target_alpha_blend_factor", m_alphaBlendState.targetBlendFactor);
		return true;
	}

	bool MaterialAsset::ColorBlendingSettings::Serialize(Serialization::Writer serializer) const
	{
		bool wroteAny = serializer.SerializeWithDefaultValue("source_blend_factor", m_colorBlendState.sourceBlendFactor, BlendFactor::One);
		wroteAny |= serializer.SerializeWithDefaultValue("target_blend_factor", m_colorBlendState.targetBlendFactor, BlendFactor::Zero);
		wroteAny |= serializer.SerializeWithDefaultValue("source_alpha_blend_factor", m_alphaBlendState.sourceBlendFactor, BlendFactor::One);
		wroteAny |= serializer.SerializeWithDefaultValue("target_alpha_blend_factor", m_alphaBlendState.targetBlendFactor, BlendFactor::Zero);
		return wroteAny;
	}

	bool MaterialAsset::StencilTestSettings::Serialize(const Serialization::Reader serializer)
	{
		m_enable = true;
		serializer.Serialize("failure_operation", m_failureOperation);
		serializer.Serialize("pass_operation", m_passOperation);
		serializer.Serialize("depth_fail_operation", m_depthFailOperation);
		serializer.Serialize("compare_operation", m_compareOperation);
		serializer.Serialize("compare_mask", m_compareMask);
		serializer.Serialize("write_mask", m_writeMask);
		serializer.Serialize("reference", m_reference);
		return true;
	}

	bool MaterialAsset::StencilTestSettings::Serialize(Serialization::Writer serializer) const
	{
		bool wroteAny = serializer.SerializeWithDefaultValue("failure_operation", m_failureOperation, DefaultFailureOperation);
		wroteAny |= serializer.SerializeWithDefaultValue("pass_operation", m_passOperation, DefaultPassOperation);
		wroteAny |= serializer.SerializeWithDefaultValue("depth_fail_operation", m_depthFailOperation, DefaultDepthFailOperation);
		wroteAny |= serializer.SerializeWithDefaultValue("compare_operation", m_compareOperation, DefaultCompareOperation);
		wroteAny |= serializer.SerializeWithDefaultValue("compare_mask", m_compareMask, DefaultCompareMask);
		wroteAny |= serializer.SerializeWithDefaultValue("write_mask", m_writeMask, DefaultWriteMask);
		wroteAny |= serializer.SerializeWithDefaultValue("reference", m_reference, DefaultReference);
		return wroteAny;
	}

	bool MaterialAsset::Attachment::Serialize(const Serialization::Reader serializer)
	{
		bool readAny = serializer.Serialize("render_target", m_renderTargetAssetGuid);

		readAny |= serializer.Serialize("load_type", m_loadType);
		readAny |= serializer.Serialize("store_type", m_storeType);
		readAny |= serializer.Serialize("stencil_load_type", m_stencilLoadType);
		readAny |= serializer.Serialize("stencil_store_type", m_stencilStoreType);

		if (serializer.Serialize("clear_color", m_clearColor))
		{
			m_loadType = AttachmentLoadType::Clear;
			readAny = true;
		}

		readAny |= serializer.Serialize("color_blending", m_colorBlending);
		return readAny;
	}

	bool MaterialAsset::Attachment::Serialize(Serialization::Writer serializer) const
	{
		bool wroteAny = serializer.Serialize("render_target", m_renderTargetAssetGuid);

		wroteAny |= serializer.SerializeWithDefaultValue("load_type", m_loadType, AttachmentLoadType::LoadExisting);
		wroteAny |= serializer.SerializeWithDefaultValue("store_type", m_storeType, AttachmentStoreType::Store);
		wroteAny |= serializer.SerializeWithDefaultValue("stencil_load_type", m_stencilLoadType, AttachmentLoadType::Undefined);
		wroteAny |= serializer.SerializeWithDefaultValue("stencil_store_type", m_stencilStoreType, AttachmentStoreType::Undefined);

		if (m_loadType == AttachmentLoadType::Clear)
		{
			wroteAny |= serializer.Serialize("clear_color", m_clearColor);
		}

		if (m_colorBlending.m_colorBlendState.IsEnabled() | m_colorBlending.m_alphaBlendState.IsEnabled())
		{
			wroteAny |= serializer.Serialize("color_blending", m_colorBlending);
		}

		return wroteAny;
	}
}
