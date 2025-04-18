#pragma once

#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetFormat.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Log2.h>

#include <Renderer/Format.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/UsageFlags.h>
#include <Renderer/Assets/Texture/TexturePreset.h>
#include <Renderer/Assets/Texture/MipRange.h>
#include <Renderer/Assets/Texture/ArrayRange.h>
#include <Renderer/Wrappers/ImageFlags.h>

namespace ngine::Rendering
{
	struct TextureAsset : public Asset::Asset
	{
		TextureAsset() = default;
		TextureAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		void SetPreset(const TexturePreset preset)
		{
			m_preset = preset;
		}
		[[nodiscard]] TexturePreset GetPreset() const
		{
			return m_preset;
		}

		void SetArraySize(const ArrayRange::UnitType size)
		{
			m_arraySize = size;
		}
		[[nodiscard]] ArrayRange::UnitType GetArraySize() const
		{
			return m_arraySize;
		}

		void SetResolution(const Math::Vector2ui size)
		{
			m_resolution = size;
		}
		[[nodiscard]] Math::Vector2ui GetResolution() const
		{
			return m_resolution;
		}

		[[nodiscard]] EnumFlags<UsageFlags> GetUsageFlags() const
		{
			return m_usageFlags;
		}

		void SetUsageFlags(const EnumFlags<UsageFlags> flags)
		{
			m_usageFlags = flags;
		}

		[[nodiscard]] EnumFlags<ImageFlags> GetFlags() const
		{
			return m_flags;
		}

		void SetFlags(const EnumFlags<ImageFlags> flags)
		{
			m_flags = flags;
		}

		enum class BinaryType : uint8
		{
			BC = 1 << 0,
			First = BC,
			ASTC = 1 << 1,
			Uncompressed = 1 << 2,
			Last = Uncompressed,
			End = Last << 1,
			Count = Math::Log2(Last) + 1,
		};

		[[nodiscard]] static BinaryType GetBinaryType(const Format format)
		{
			const EnumFlags<Rendering::FormatFlags> formatFlags = Rendering::GetFormatInfo(format).m_flags;
			if (formatFlags.IsSet(Rendering::FormatFlags::BC))
			{
				return BinaryType::BC;
			}
			else if (formatFlags.IsSet(Rendering::FormatFlags::ASTC))
			{
				return BinaryType::ASTC;
			}
			else
			{
				return BinaryType::Uncompressed;
			}
		}

		[[nodiscard]] static IO::PathView GetBinaryFileExtension(const BinaryType type)
		{
			switch (type)
			{
				case Rendering::TextureAsset::BinaryType::BC:
					return MAKE_PATH(".bc");
					break;
				case Rendering::TextureAsset::BinaryType::ASTC:
					return MAKE_PATH(".astc");
					break;
				case Rendering::TextureAsset::BinaryType::Uncompressed:
					return MAKE_PATH(".uncompressed");
					break;
				case Rendering::TextureAsset::BinaryType::Count:
				case Rendering::TextureAsset::BinaryType::End:
					ExpectUnreachable();
					break;
			}
			ExpectUnreachable();
		}
		[[nodiscard]] IO::Path GetBinaryFilePath(const BinaryType type) const;

		struct MipInfo
		{
			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			uint16 level;
			size m_offset;
			size m_size;
		};

		void EnableMipGeneration()
		{
			m_generateMips = true;
		}
		void DisableMipGeneration()
		{
			m_generateMips = false;
		}
		[[nodiscard]] bool ShouldGenerateMips() const
		{
			return m_generateMips;
		}

		struct BinaryInfo
		{
			void SetMipmapCount(const MipRange::UnitType count)
			{
				m_mipInfo.Resize(count, Memory::Uninitialized);
			}

			bool Serialize(const Serialization::Reader serializer);
			bool Serialize(Serialization::Writer serializer) const;

			void StoreMipInfo(MipInfo&& info, const MipRange::UnitType index)
			{
				m_mipInfo[index] = Forward<MipInfo>(info);
			}

			void ClearMipmaps()
			{
				m_mipInfo.Clear();
			}

			using MipInfoView = ArrayView<const MipInfo, MipRange::UnitType>;
			[[nodiscard]] MipInfoView GetMipInfoView() const
			{
				return m_mipInfo;
			}

			void SetFormat(const Format format)
			{
				m_format = format;
			}
			[[nodiscard]] Format GetFormat() const
			{
				return m_format;
			}

			[[nodiscard]] Optional<Math::Ratiof> GetCompressionQuality() const
			{
				return m_compressionQuality;
			}
		protected:
			Format m_format = Format::Invalid;
			InlineVector<MipInfo, 12, MipRange::UnitType> m_mipInfo;
			Optional<Math::Ratiof> m_compressionQuality;
		};

		void SetBinaryAssetInfo(const BinaryType type, BinaryInfo&& info)
		{
			m_binaryInfo[Math::Log2((uint8)type)] = Forward<BinaryInfo>(info);
		}
		[[nodiscard]] const BinaryInfo& GetBinaryAssetInfo(const BinaryType type) const
		{
			return m_binaryInfo[Math::Log2((uint8)type)];
		}
		[[nodiscard]] BinaryInfo& GetBinaryAssetInfo(const BinaryType type)
		{
			return m_binaryInfo[Math::Log2((uint8)type)];
		}
	protected:
		TexturePreset m_preset = TexturePreset::Unknown;
		EnumFlags<UsageFlags> m_usageFlags = UsageFlags::Sampled | UsageFlags::TransferDestination;
		EnumFlags<ImageFlags> m_flags;
		ArrayRange::UnitType m_arraySize = 1;
		Math::Vector2ui m_resolution = Math::Zero;
		bool m_generateMips{true};

		Array<BinaryInfo, (uint8)BinaryType::Count> m_binaryInfo;
	};

	ENUM_FLAG_OPERATORS(TextureAsset::BinaryType);
}
