#pragma once

#include "FontModifier.h"
#include "FontWeight.h"

#include <Common/Memory/Containers/Vector.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetFormat.h>

struct FT_FaceRec_;

namespace ngine::Font
{
	struct Atlas;
	struct Manager;

	struct Font
	{
		using Modifier = ngine::Font::Modifier;

		struct Variation
		{
			uint16 m_index;
			Weight m_weight;
			EnumFlags<Modifier> m_modifiers;
		};

		inline static constexpr Asset::Format AssetFormat = {
			"{6E905AF0-1B0B-4472-BE32-897FBF6337F4}"_guid, MAKE_PATH(".font.nasset"), MAKE_PATH(".font")
		};

		using Atlas = Atlas;
		using Manager = Manager;

		using MemoryType = FixedSizeVector<ByteType, size>;
		using VariationsContainer = Vector<Variation, uint16>;

		Font() = default;
		Font(FT_FaceRec_* pFont, MemoryType&& memory, VariationsContainer&& variations);
		Font(const Font&) = delete;
		Font& operator=(const Font&) = delete;
		Font(Font&& other)
			: m_pFace(other.m_pFace)
			, m_memory(Move(other.m_memory))
			, m_variations(Move(other.m_variations))
		{
			other.m_pFace = nullptr;
		}
		Font& operator=(Font&& other)
		{
			m_pFace = other.m_pFace;
			other.m_pFace = nullptr;
			m_memory = Move(other.m_memory);
			m_variations = Move(other.m_variations);
			return *this;
		}
		~Font();

		[[nodiscard]] FT_FaceRec_* GetFace() const
		{
			return m_pFace;
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_pFace != nullptr;
		}

		void SwitchVariation(const Weight weight, const EnumFlags<Modifier> modifiers);
	protected:
		FT_FaceRec_* m_pFace = nullptr;
		MemoryType m_memory;

		VariationsContainer m_variations;
	};
}
