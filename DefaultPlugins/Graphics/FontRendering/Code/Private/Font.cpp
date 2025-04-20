#include "Font.h"

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wlanguage-extension-token");

#include "ft2build.h"
#include "freetype/ftmm.h"
#include FT_FREETYPE_H

POP_CLANG_WARNINGS

namespace ngine::Font
{
	Font::Font(FT_FaceRec_* pFont, MemoryType&& memory, VariationsContainer&& variations)
		: m_pFace(pFont)
		, m_memory(Forward<MemoryType>(memory))
		, m_variations(Forward<VariationsContainer>(variations))
	{
	}

	Font::~Font()
	{
		if (m_pFace != nullptr)
		{
			FT_Done_Face(m_pFace);
		}
	}

	void Font::SwitchVariation(const Weight weight, const EnumFlags<Modifier> modifiers)
	{
		if (m_variations.HasElements())
		{
			Optional<const Variation*> pBestVariation;
			uint8 bestScore = 0;

			for (const Variation& __restrict variation : m_variations)
			{
				const uint8 modifierCount = (modifiers & variation.m_modifiers).GetNumberOfSetFlags();
				const uint8 score = (variation.m_weight == weight) * 50 + modifierCount;

				if (score > bestScore)
				{
					pBestVariation = &variation;
					bestScore = score;
				}
			}

			if (pBestVariation != nullptr)
			{
				[[maybe_unused]] const FT_Error error = FT_Set_Named_Instance(m_pFace, pBestVariation->m_index);
				Assert(error == 0);
			}
			else
			{
				[[maybe_unused]] const FT_Error error = FT_Set_Named_Instance(m_pFace, 0);
				Assert(error == 0);
			}
		}
	}
}
