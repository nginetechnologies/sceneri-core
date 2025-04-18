#pragma once

#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetFormat.h>
#include <Common/Math/Color.h>
#include <Common/Guid.h>

#include <Renderer/PushConstants/PushConstant.h>
#include <Renderer/Wrappers/AddressMode.h>
#include <Renderer/Assets/Material/DescriptorContentType.h>

namespace ngine
{
	struct Engine;

	namespace Asset
	{
		struct Guid;
	}

	namespace IO
	{
		struct Path;
	}
}

namespace ngine::Rendering
{
	struct MaterialInstanceDescriptorContent
	{
		using Type = DescriptorContentType;

		MaterialInstanceDescriptorContent()
		{
		}
		MaterialInstanceDescriptorContent(const Type type, const ngine::Asset::Guid assetGuid, const AddressMode addressMode)
			: m_type(type)
			, m_textureInfo(TextureInfo{assetGuid, addressMode})
		{
		}

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] Type GetType() const
		{
			return m_type;
		}

		[[nodiscard]] Asset::Guid GetTextureAssetGuid() const
		{
			switch (m_type)
			{
				case Type::Invalid:
					return {};
				case Type::Texture:
					return m_textureInfo.m_assetGuid;
			}
			ExpectUnreachable();
		}
	protected:
		Type m_type{Type::Invalid};
		struct TextureInfo
		{
			ngine::Asset::Guid m_assetGuid;
			Rendering::AddressMode m_addressMode;
		};

		union
		{
			TextureInfo m_textureInfo;
		};
	};

	struct MaterialInstanceAsset : public Asset::Asset
	{
		using DescriptorContent = MaterialInstanceDescriptorContent;
		using DescriptorContents = Vector<DescriptorContent, uint8>;
		using PushConstants = Vector<PushConstant, uint8>;

		MaterialInstanceAsset() = default;
		MaterialInstanceAsset(Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		void UpdateDependencies()
		{
			ClearDependencies();
			AddDependency(m_materialAssetGuid);
			for (const MaterialInstanceDescriptorContent& descriptorContent : m_descriptorContents)
			{
				const Asset::Guid textureAssetGuid = descriptorContent.GetTextureAssetGuid();
				if (textureAssetGuid.IsValid())
				{
					AddDependency(textureAssetGuid);
				}
			}
		}

		ngine::Asset::Guid m_materialAssetGuid;
		DescriptorContents m_descriptorContents;
		PushConstants m_pushConstants;
	};
}
