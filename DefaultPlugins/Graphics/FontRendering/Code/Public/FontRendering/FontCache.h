#pragma once

#include "FontIdentifier.h"
#include "FontInstanceIdentifier.h"
#include "FontModifier.h"
#include "Point.h"
#include "FontWeight.h"

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Function/Function.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/EnumFlags.h>

#include <Engine/Asset/AssetType.h>

struct FT_LibraryRec_;

namespace ngine::Font
{
	struct Atlas;

	struct InstanceProperties
	{
		Identifier m_fontIdentifier;
		Point m_fontSize;
		float m_devicePixelRatio;
		EnumFlags<Modifier> m_fontModifiers;
		Weight m_fontWeight;
		ConstUnicodeStringView m_characters;

		using Hash = size;
		[[nodiscard]] Hash CalculateHash() const;
	};
}

namespace ngine
{
	struct Engine;

	extern template struct UnorderedMap<Font::InstanceProperties::Hash, Font::InstanceIdentifier>;
	extern template struct TSaltedIdentifierStorage<Font::InstanceIdentifier>;

	extern template struct Threading::AtomicIdentifierMask<Font::InstanceIdentifier>;
	extern template struct TIdentifierArray<UniquePtr<Font::Atlas>, Font::InstanceIdentifier>;
}

namespace ngine::Asset
{
	struct Guid;
	struct Manager;
}

namespace ngine::Threading
{
	struct Job;
	struct JobBatch;
}

namespace ngine::Font
{
	struct Font;
	struct FontPipeline;
	struct Manager;

	inline static constexpr ConstUnicodeStringView DefaultCharacters = MAKE_UNICODE_LITERAL(
		"abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ-−‐+=$@~`_:;.,()[]{}*/\\!?&^'’\"<>|$@#%°1234567890 ▶→↓←↓↔↕⋅²"
	);

	struct FontInfo
	{
		Vector<ByteType, size> m_fontData;
	};

	inline static constexpr Weight DefaultFontWeight = Weight(400);

	struct Cache final : public Asset::Type<Identifier, FontInfo>
	{
		using BaseType = Asset::Type<InstanceIdentifier, FontInfo>;

		Cache(Asset::Manager& assetManager);
		~Cache();

		[[nodiscard]] Identifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] Identifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] InstanceIdentifier FindOrRegisterInstance(const InstanceProperties properties);
		[[nodiscard]] InstanceIdentifier FindInstance(const InstanceProperties properties) const;
		void RemoveInstance(const InstanceIdentifier identifier);

		using OnLoadedCallback = Function<void(const Identifier), 24>;
		[[nodiscard]] Optional<Threading::Job*> TryLoad(const Identifier identifier, Asset::Manager& assetManager, OnLoadedCallback&& callback);

		using OnInstanceLoadedCallback = Function<void(const InstanceIdentifier), 24>;
		[[nodiscard]] Threading::JobBatch TryLoadInstance(
			const InstanceIdentifier identifier,
			const InstanceProperties properties,
			Asset::Manager& assetManager,
			OnInstanceLoadedCallback&& callback
		);

		[[nodiscard]] Optional<const Atlas*> GetInstanceAtlas(const InstanceIdentifier identifier) const
		{
			return m_atlases[identifier].Get();
		}

		[[nodiscard]] const TSaltedIdentifierStorage<InstanceIdentifier>& GetInstanceIdentifiers() const
		{
			return m_instanceIdentifiers;
		}
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const Identifier identifier, const IO::PathView filePath) override;
#endif

		[[nodiscard]] Font LoadFont(const ConstByteView data) const;
	protected:
		FT_LibraryRec_* m_pLibrary;

		Threading::AtomicIdentifierMask<Identifier> m_loadingFonts;
		struct FontRequesters
		{
			Threading::SharedMutex m_mutex;
			InlineVector<OnLoadedCallback, 2> m_callbacks;
		};
		Threading::SharedMutex m_fontRequesterMutex;
		UnorderedMap<Identifier, UniquePtr<FontRequesters>, Identifier::Hash> m_fontRequesterMap;

		mutable Threading::SharedMutex m_instanceLookupMutex;
		UnorderedMap<InstanceProperties::Hash, InstanceIdentifier> m_instanceLookupMap;
		TSaltedIdentifierStorage<InstanceIdentifier> m_instanceIdentifiers;

		Threading::AtomicIdentifierMask<InstanceIdentifier> m_loadingInstances;
		struct InstanceRequesters
		{
			Threading::SharedMutex m_mutex;
			InlineVector<OnInstanceLoadedCallback, 2> m_callbacks;
		};
		Threading::SharedMutex m_instanceRequesterMutex;
		UnorderedMap<InstanceIdentifier, UniquePtr<InstanceRequesters>, InstanceIdentifier::Hash> m_instanceRequesterMap;
		TIdentifierArray<UniquePtr<Atlas>, InstanceIdentifier> m_atlases;
	};
}
