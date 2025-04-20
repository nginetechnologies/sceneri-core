#pragma once

#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Constants.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Storage/Identifier.h>

#include "Font.h"
#include "Point.h"
#include "FontWeight.h"

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct ToolWindow;
	struct TextureCache;
}

namespace ngine::Threading
{
	struct Job;
}

struct FT_FaceRec_;

namespace ngine::Font
{
	struct Atlas
	{
		Atlas(Rendering::TextureCache& textureCache);

		using OnReadyForUseCallback = Function<void(), 24>;
		void LoadGlyphs(
			Font&& font,
			const Point size,
			const float devicePixelRatio,
			const EnumFlags<Modifier> fontModifiers,
			const Weight fontWeight,
			const ConstUnicodeStringView characters = {}
		);

		[[nodiscard]] bool HasLoadedGlyphs() const
		{
			return m_maximumGlyphSize.GetLengthSquared() > 0;
		}

		[[nodiscard]] Rendering::TextureIdentifier GetTextureIdentifier() const
		{
			return m_textureIdentifier;
		}

		struct GlyphInfo
		{
			Math::Vector2ui m_pixelSize;
			Math::Vector2i m_offset;
			Math::Vector2ui m_advance;
			Math::Vector2f m_atlasCoordinates;
			Math::Vector2f m_atlasScale;
		};

		[[nodiscard]] bool HasGlyph(const UnicodeCharType character) const
		{
			return m_glyphInfoMap.Contains(character);
		}
		[[nodiscard]] Optional<const GlyphInfo*> GetGlyphInfo(const UnicodeCharType character) const
		{
			auto it = m_glyphInfoMap.Find(character);
			return Optional<const GlyphInfo*>(&it->second, it != m_glyphInfoMap.end());
		}

		// Gets the maximum size of a glyph in this atlas
		[[nodiscard]] Math::Vector2ui GetMaximumGlyphWidth() const
		{
			return m_maximumGlyphSize;
		}

		[[nodiscard]] uint32 CalculateWidth(const ConstUnicodeStringView text) const;
		[[nodiscard]] Math::Vector2ui CalculateSize(const ConstUnicodeStringView text) const;
		[[nodiscard]] UnicodeStringView TrimStringToFit(UnicodeStringView string, const uint32 maximumWidth) const;
	protected:
		Optional<Threading::Job*> LoadRenderTexture(const Rendering::TextureIdentifier identifier, Rendering::LogicalDevice& logicalDevice);
	protected:
		// TODO: Move this out to the FontCache?
		Font m_font;
		Math::Vector2ui m_maximumGlyphSize = Math::Zero;
		UnicodeString m_characters;
		UnorderedMap<UnicodeCharType, GlyphInfo> m_glyphInfoMap;
		Rendering::TextureIdentifier m_textureIdentifier;
	};
}
