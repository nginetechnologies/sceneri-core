#include "FontCache.h"
#include "FontAtlas.h"
#include "Manager.h"
#include "Font.h"

#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Endian.h>
#include <Common/Math/Hash.h>
#include <Common/Math/Vector2/Hash.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Engine/Asset/AssetType.inl>

#include <Common/System/Query.h>
#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>

#include "ft2build.h"
#include "freetype/ftmm.h"
#include "freetype/ftsnames.h"
#include "freetype/ttnameid.h"
#include "freetype/tttables.h"
#include FT_FREETYPE_H

// TODO: Shared resource pool
// Creation / query returns a shared ref
// When low on memory, we query objects with shared ref count == 0, release that
// This way we don't always release stuff, but will if we have to
// Apply to fonts, meshes, textures etc.

namespace ngine::Font
{
	Cache::Cache(Asset::Manager& assetManager)
	{
		[[maybe_unused]] const FT_Error result = FT_Init_FreeType(&m_pLibrary);
		Assert(result == 0, "Failed to initialize FreeType");

		BaseType::RegisterAssetModifiedCallback(assetManager);
	}

	Cache::~Cache()
	{
		{
			Threading::UniqueLock lock(m_instanceLookupMutex);
			m_instanceLookupMap.Clear();
		}

		for (UniquePtr<Atlas>& pAtlas : m_instanceIdentifiers.GetValidElementView(m_atlases.GetView()))
		{
			pAtlas.DestroyElement();
		}

		{
			Threading::UniqueLock lock(m_instanceRequesterMutex);
			m_instanceRequesterMap.Clear();
		}

		{
			Threading::UniqueLock lock(m_fontRequesterMutex);
			m_fontRequesterMap.Clear();
		}

		FT_Done_FreeType(m_pLibrary);
	}

	Identifier Cache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const Identifier, const Asset::Guid)
			{
				return FontInfo{};
			}
		);
	}

	Identifier Cache::RegisterAsset(const Asset::Guid guid)
	{
		const Identifier identifier = BaseType::RegisterAsset(
			guid,
			[](const Identifier, const Guid)
			{
				return FontInfo{};
			}
		);
		return identifier;
	}

	InstanceProperties::Hash InstanceProperties::CalculateHash() const
	{
		return Math::Hash(
			m_fontIdentifier.GetValue(),
			m_fontSize.GetPoints(),
			m_devicePixelRatio,
			m_fontModifiers.GetUnderlyingValue(),
			m_fontWeight.GetValue(),
			m_characters
		);
	}

	InstanceIdentifier Cache::FindOrRegisterInstance(const InstanceProperties properties)
	{
		const InstanceProperties::Hash instanceHash = properties.CalculateHash();
		{
			Threading::SharedLock sharedLock(m_instanceLookupMutex);
			decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(instanceHash);
			if (it != m_instanceLookupMap.end())
			{
				return it->second;
			}
		}

		InstanceIdentifier identifier;
		{
			Threading::UniqueLock lock(m_instanceLookupMutex);
			{
				decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(instanceHash);
				if (it != m_instanceLookupMap.end())
				{
					return it->second;
				}
			}

			identifier = m_instanceIdentifiers.AcquireIdentifier();
			m_instanceLookupMap.Emplace(InstanceProperties::Hash(instanceHash), InstanceIdentifier(identifier));
		}

		m_atlases[identifier] = UniquePtr<Atlas>::Make(System::Get<Rendering::Renderer>().GetTextureCache());
		return identifier;
	}

	InstanceIdentifier Cache::FindInstance(const InstanceProperties properties) const
	{
		Threading::SharedLock sharedLock(m_instanceLookupMutex);
		decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(properties.CalculateHash());
		if (it != m_instanceLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	void Cache::RemoveInstance(const InstanceIdentifier identifier)
	{
		m_atlases[identifier].DestroyElement();
		m_instanceIdentifiers.ReturnIdentifier(identifier);
	}

	Optional<Threading::Job*> Cache::TryLoad(const Identifier identifier, Asset::Manager& assetManager, OnLoadedCallback&& callback)
	{
		FontRequesters* pFontRequesters;
		uint32 callbackIndex;
		{
			Threading::SharedLock readLock(m_fontRequesterMutex);
			decltype(m_fontRequesterMap)::iterator it = m_fontRequesterMap.Find(identifier);
			if (it != m_fontRequesterMap.end())
			{
				pFontRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_fontRequesterMutex);
				it = m_fontRequesterMap.Find(identifier);
				if (it != m_fontRequesterMap.end())
				{
					pFontRequesters = it->second.Get();
				}
				else
				{
					pFontRequesters = m_fontRequesterMap.Emplace(Identifier(identifier), UniquePtr<FontRequesters>::Make())->second.Get();
				}
			}

			Threading::UniqueLock requestersLock(pFontRequesters->m_mutex);
			OnInstanceLoadedCallback& emplacedCallback = pFontRequesters->m_callbacks.EmplaceBack(Forward<OnLoadedCallback>(callback));
			callbackIndex = pFontRequesters->m_callbacks.GetIteratorIndex(&emplacedCallback);
		}

		FontInfo& fontInfo = GetAssetData(identifier);
		if (fontInfo.m_fontData.IsEmpty())
		{
			if (m_loadingFonts.Set(identifier))
			{
				if (fontInfo.m_fontData.IsEmpty())
				{
					Assert(m_loadingFonts.IsSet(identifier));
					const Guid fontAssetGuid = GetAssetGuid(identifier);
					return assetManager.RequestAsyncLoadAssetBinary(
						fontAssetGuid,
						Threading::JobPriority::LoadFont,
						[this, identifier](const ConstByteView data)
						{
							auto finishedLoad = [this, identifier]()
							{
								[[maybe_unused]] const bool wasCleared = m_loadingFonts.Clear(identifier);
								Assert(wasCleared);

								{
									Threading::SharedLock readLock(m_fontRequesterMutex);
									const decltype(m_fontRequesterMap)::const_iterator it = m_fontRequesterMap.Find(identifier);
									if (it != m_fontRequesterMap.end())
									{
										FontRequesters& __restrict requesters = *it->second;
										Threading::SharedLock requestersReadLock(requesters.m_mutex);
										for (const OnLoadedCallback& callback : requesters.m_callbacks)
										{
											callback(identifier);
										}
									}
								}
							};

							Assert(data.HasElements());
							if (UNLIKELY(!data.HasElements()))
							{
								finishedLoad();
								return;
							}

							Vector<ByteType, size> fontData;
							fontData.Resize(data.GetDataSize(), Memory::Uninitialized);
							ByteView(fontData.GetView()).CopyFrom(data);
							GetAssetData(identifier).m_fontData = Move(fontData);

							finishedLoad();
						}
					);
				}
			}
		}

		if (fontInfo.m_fontData.HasElements())
		{
			Threading::SharedLock requestersReadLock(pFontRequesters->m_mutex);
			pFontRequesters->m_callbacks[callbackIndex](identifier);
		}
		return nullptr;
	}

	Threading::JobBatch Cache::TryLoadInstance(
		const InstanceIdentifier identifier,
		const InstanceProperties properties,
		Asset::Manager& assetManager,
		OnInstanceLoadedCallback&& callback
	)
	{
		InstanceRequesters* pInstanceRequesters;
		uint32 callbackIndex;
		{
			Threading::SharedLock readLock(m_instanceRequesterMutex);
			decltype(m_instanceRequesterMap)::iterator it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				pInstanceRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_instanceRequesterMutex);
				it = m_instanceRequesterMap.Find(identifier);
				if (it != m_instanceRequesterMap.end())
				{
					pInstanceRequesters = it->second.Get();
				}
				else
				{
					pInstanceRequesters = m_instanceRequesterMap.Emplace(Identifier(identifier), UniquePtr<InstanceRequesters>::Make())->second.Get();
				}
			}

			Threading::UniqueLock requestersLock(pInstanceRequesters->m_mutex);
			OnInstanceLoadedCallback& emplacedCallback = pInstanceRequesters->m_callbacks.EmplaceBack(Forward<OnInstanceLoadedCallback>(callback)
			);
			callbackIndex = pInstanceRequesters->m_callbacks.GetIteratorIndex(&emplacedCallback);
		}

		Atlas& fontAtlas = *m_atlases[identifier];
		if (!fontAtlas.HasLoadedGlyphs())
		{
			if (m_loadingInstances.Set(identifier))
			{
				if (!fontAtlas.HasLoadedGlyphs())
				{
					Assert(m_loadingInstances.IsSet(identifier));
					Threading::Job& createInstanceJob = Threading::CreateCallback(
						[this, identifier, properties, &fontAtlas](Threading::JobRunnerThread&)
						{
							auto finishedLoad = [this, identifier]()
							{
								[[maybe_unused]] const bool wasCleared = m_loadingInstances.Clear(identifier);
								Assert(wasCleared);

								{
									Threading::SharedLock readLock(m_instanceRequesterMutex);
									const decltype(m_instanceRequesterMap)::const_iterator it = m_instanceRequesterMap.Find(identifier);
									if (it != m_instanceRequesterMap.end())
									{
										InstanceRequesters& __restrict requesters = *it->second;
										Threading::SharedLock requestersReadLock(requesters.m_mutex);
										for (const OnInstanceLoadedCallback& callback : requesters.m_callbacks)
										{
											callback(identifier);
										}
									}
								}
							};

							const ConstByteView fontData = GetAssetData(properties.m_fontIdentifier).m_fontData.GetView();
							Assert(fontData.HasElements());
							if (UNLIKELY(fontData.IsEmpty()))
							{
								finishedLoad();
								return;
							}

							Font font = LoadFont(fontData);
							Assert(font.IsValid());
							if (UNLIKELY(!font.IsValid()))
							{
								finishedLoad();
								return;
							}

							fontAtlas.LoadGlyphs(
								Move(font),
								properties.m_fontSize,
								properties.m_devicePixelRatio,
								properties.m_fontModifiers,
								properties.m_fontWeight,
								properties.m_characters
							);
							finishedLoad();
						},
						Threading::JobPriority::LoadFont,
						"Load Font"
					);
					Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
					createInstanceJob.AddSubsequentStage(jobBatch.GetFinishedStage());
					Threading::Job* pLoadFontJob = TryLoad(
						properties.m_fontIdentifier,
						assetManager,
						[&createInstanceJob](const Identifier)
						{
							if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
							{
								createInstanceJob.Queue(*pThread);
							}
							else
							{
								createInstanceJob.Queue(System::Get<Threading::JobManager>());
							}
						}
					);
					if (pLoadFontJob != nullptr)
					{
						jobBatch.QueueAfterStartStage(*pLoadFontJob);
					}
					return jobBatch;
				}
			}
		}

		if (fontAtlas.HasLoadedGlyphs())
		{
			Threading::SharedLock requestersReadLock(pInstanceRequesters->m_mutex);
			pInstanceRequesters->m_callbacks[callbackIndex](identifier);
		}
		return {};
	}

	struct FontVariationInfo
	{
		size m_nameHash;
		Weight m_weight;
		EnumFlags<Font::Modifier> m_modifiers;
	};
	inline static constexpr Array FontVariations{
		FontVariationInfo{Math::Hash(ConstStringView("Extra Light")), Weight{100}},
		FontVariationInfo{Math::Hash(ConstStringView("Extra Light Italic")), Weight{100}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraLight")), Weight{100}},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraLight Italic")), Weight{100}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Ultra Light")), Weight{100}},
		FontVariationInfo{Math::Hash(ConstStringView("Ultra Light Italic")), Weight{100}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("UltraLight")), Weight{100}},
		FontVariationInfo{Math::Hash(ConstStringView("UltraLight Italic")), Weight{100}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Light")), Weight{200}},
		FontVariationInfo{Math::Hash(ConstStringView("Light Italic")), Weight{200}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Thin")), Weight{200}},
		FontVariationInfo{Math::Hash(ConstStringView("Thin Italic")), Weight{200}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Book")), Weight{300}},
		FontVariationInfo{Math::Hash(ConstStringView("Book Italic")), Weight{300}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Demi")), Weight{300}},
		FontVariationInfo{Math::Hash(ConstStringView("Demi Italic")), Weight{300}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Normal")), Weight{400}},
		FontVariationInfo{Math::Hash(ConstStringView("Normal Italic")), Weight{400}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Regular")), Weight{400}},
		FontVariationInfo{Math::Hash(ConstStringView("Regular Italic")), Weight{400}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Italic")), Weight{400}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Medium")), Weight{500}},
		FontVariationInfo{Math::Hash(ConstStringView("Medium Italic")), Weight{500}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Semibold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("Semibold Italic")), Weight{600}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("SemiBold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("Semi Bold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("SemiBold Italic")), Weight{600}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Semi Bold Italic")), Weight{600}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Demibold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("Demibold Italic")), Weight{600}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("DemiBold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("Demi Bold")), Weight{600}},
		FontVariationInfo{Math::Hash(ConstStringView("DemiBold Italic")), Weight{600}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Demi Bold Italic")), Weight{600}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Bold")), Weight{700}},
		FontVariationInfo{Math::Hash(ConstStringView("Bold Italic")), Weight{700}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Black")), Weight{800}},
		FontVariationInfo{Math::Hash(ConstStringView("Black Italic")), Weight{800}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Extrabold")), Weight{800}},
		FontVariationInfo{Math::Hash(ConstStringView("Extrabold Italic")), Weight{800}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraBold")), Weight{800}},
		FontVariationInfo{Math::Hash(ConstStringView("Extra Bold")), Weight{800}},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraBold Italic")), Weight{800}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Extra Bold Italic")), Weight{800}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Heavy")), Weight{800}},
		FontVariationInfo{Math::Hash(ConstStringView("Heavy Italic")), Weight{800}, Font::Modifier::Italic},

		FontVariationInfo{Math::Hash(ConstStringView("Extrablack")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Extrablack Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraBlack")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Extra Black")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("ExtraBlack Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Extra Black Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Fat")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Fat Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Poster")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Poster Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Ultrablack")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Ultrablack Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("UltraBlack")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("Ultra Black")), Weight{900}},
		FontVariationInfo{Math::Hash(ConstStringView("UltraBlack Italic")), Weight{900}, Font::Modifier::Italic},
		FontVariationInfo{Math::Hash(ConstStringView("Ultra Black Italic")), Weight{900}, Font::Modifier::Italic}

	};

	UnicodeString ConvertMacRomanCharacterToUtf8(const unsigned char c)
	{
		static constexpr Array<ConstUnicodeStringView, 128> strings{
			/* case 0x80: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x84")), // A umlaut
			/* case 0x81: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x85")), // A circle
			/* case 0x82: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x87")), // C cedilla
			/* case 0x83: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x89")), // E accent
			/* case 0x84: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x91")), // N tilde
			/* case 0x85: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x96")), // O umlaut
			/* case 0x86: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x9C")), // U umlaut
			/* case 0x87: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA1")), // a accent
			/* case 0x88: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA0")), // a grave
			/* case 0x89: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA2")), // a circumflex
			/* case 0x8A: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA4")), // a umlaut
			/* case 0x8B: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA3")), // a tilde
			/* case 0x8C: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA5")), // a circle
			/* case 0x8D: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA7")), // c cedilla
			/* case 0x8E: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA9")), // e accent
			/* case 0x8F: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA8")), // e grave

			/* case 0x90: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAA")), // e circumflex
			/* case 0x91: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAB")), // e umlaut
			/* case 0x92: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAD")), // i accent
			/* case 0x93: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAC")), // i grave
			/* case 0x94: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAE")), // i circumflex
			/* case 0x95: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xAF")), // i umlaut
			/* case 0x96: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB1")), // n tilde
			/* case 0x97: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB3")), // o accent
			/* case 0x98: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB2")), // o grave
			/* case 0x99: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB4")), // o circumflex
			/* case 0x9A: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB6")), // o umlaut
			/* case 0x9B: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB5")), // o tilde
			/* case 0x9C: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xBA")), // u accent
			/* case 0x9D: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB9")), // u grave
			/* case 0x9E: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xBB")), // u circumflex
			/* case 0x9F: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xBC")), // u tilde

			/* case 0xA0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xA0")), // cross
			/* case 0xA1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB0")),     // degree
			/* case 0xA2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA2")),     // cents
			/* case 0xA3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA3")),     // pounds
			/* case 0xA4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA7")),     // section
			/* case 0xA5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xA2")), // bullet
			/* case 0xA6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB6")),     // pilcrow
			/* case 0xA7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x9F")),     // german sharp S
			/* case 0xA8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xAE")),     // registered
			/* case 0xA9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA9")),     // copyright
			/* case 0xAA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x84\xA2")), // TM
			/* case 0xAB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB4")),     // back tick
			/* case 0xAC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA8")),     // umlaut
			/* case 0xAD: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x89\xA0")), // not equal (not in Windows 1252)
			/* case 0xAE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x86")),     // AE
			/* case 0xAF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x98")),     // O slash

			/* case 0xB0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x9E")), // infinity (not in Windows 1252)
			/* case 0xB1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB1")),     // plus or minus
			/* case 0xB2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x89\xA4")), // less than or equal (not in Windows 1252)
			/* case 0xB3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x89\xA5")), // greater than or equal (not in Windows 1252)
			/* case 0xB4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA5")),     // yen
			/* case 0xB5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB5")),     // mu
			/* case 0xB6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x82")), // derivative (not in Windows 1252)
			/* case 0xB7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x91")), // large sigma (not in Windows 1252)
			/* case 0xB8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x8F")), // large pi (not in Windows 1252)
			/* case 0xB9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCF\x80")),     // small pi (not in Windows 1252)
			/* case 0xBA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\xAB")), // integral (not in Windows 1252)
			/* case 0xBB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xAA")),     // feminine ordinal
			/* case 0xBC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xBA")),     // masculine ordinal
			/* case 0xBD: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCE\xA9")),     // large ohm (not in Windows 1252)
			/* case 0xBE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xA6")),     // ae
			/* case 0xBF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB8")),     // o slash

			/* case 0xC0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xBF")),     // inverted question mark
			/* case 0xC1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA1")),     // inverted exclamation mark
			/* case 0xC2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xAC")),     // not
			/* case 0xC3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x9A")), // root (not in Windows 1252)
			/* case 0xC4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC6\x92")),     // function
			/* case 0xC5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x89\x88")), // approximately equal (not in Windows 1252)
			/* case 0xC6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x88\x86")), // large delta (not in Windows 1252)
			/* case 0xC7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xAB")),     // open angle quotation mark
			/* case 0xC8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xBB")),     // close angle quotation mark
			/* case 0xC9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xA6")), // ellipsis
			/* case 0xCA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xA0")),     // NBSP
			/* case 0xCB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x80")),     // A grave
			/* case 0xCC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x83")),     // A tilde
			/* case 0xCD: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x95")),     // O tilde
			/* case 0xCE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC5\x92")),     // OE
			/* case 0xCF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC5\x93")),     // oe

			/* case 0xD0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x93")), // en dash
			/* case 0xD1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x94")), // em dash
			/* case 0xD2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x9C")), // open smart double quote
			/* case 0xD3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x9D")), // close smart double quote
			/* case 0xD4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x98")), // open smart single quote
			/* case 0xD5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x99")), // close smart single quote
			/* case 0xD6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xB7")),     // divided
			/* case 0xD7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x97\x8A")), // diamond (not in Windows 1252)
			/* case 0xD8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\xBF")),     // y umlaut
			/* case 0xD9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC5\xB8")),     // Y umlaut
			/* case 0xDA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x81\x84")), // big slash (not in Windows 1252)
			/* case 0xDB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x82\xAC")), // euro (not in Windows 1252)
			/* case 0xDC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xB9")), // open angle single quote
			/* case 0xDD: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xBA")), // close angle single quote
			/* case 0xDE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xEF\xAC\x81")), // fi ligature (not in Windows 1252)
			/* case 0xDF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xEF\xAC\x82")), // fl ligature (not in Windows 1252)

			/* case 0xE0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xA1")), // double dagger
			/* case 0xE1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB7")),     // interpunct
			/* case 0xE2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x9A")), // inverted smart single quote
			/* case 0xE3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\x9E")), // inverted smart double quote
			/* case 0xE4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xE2\x80\xB0")), // per mille
			/* case 0xE5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x82")),     // A circumflex
			/* case 0xE6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8A")),     // E circumflex
			/* case 0xE7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x81")),     // A accent
			/* case 0xE8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8B")),     // E umlaut
			/* case 0xE9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x88")),     // E grave
			/* case 0xEA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8D")),     // I accent
			/* case 0xEB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8E")),     // I circumflex
			/* case 0xEC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8F")),     // I umlaut
			/* case 0xED: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x8C")),     // I grave
			/* case 0xEE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x93")),     // O accent
			/* case 0xEF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x94")),     // O circumflex

			/* case 0xF0: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xEF\xA3\xBF")), // box (not in Windows 1252)
			/* case 0xF1: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x92")),     // O grave
			/* case 0xF2: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x9A")),     // U accent
			/* case 0xF3: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x9B")),     // U circumflex
			/* case 0xF4: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC3\x99")),     // U grave
			/* case 0xF5: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC4\xB1")),     // dotless i ligature (not in Windows 1252)
			/* case 0xF6: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x86")),     // circumflex
			/* case 0xF7: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x9C")),     // tilde
			/* case 0xF8: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xAF")),     // macron
			/* case 0xF9: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x98")),     // breve (not in Windows 1252)
			/* case 0xFA: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x99")),     // raised dot (not in Windows 1252)
			/* case 0xFB: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x9A")),     // ring
			/* case 0xFC: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xC2\xB8")),     // cedilla
			/* case 0xFD: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x9D")),     // double acute accent (not in Windows 1252)
			/* case 0xFE: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x9B")),     // ogonek (not in Windows 1252)
			/* case 0xFF: return */ ConstUnicodeStringView(MAKE_UNICODE_LITERAL("\xCB\x87")),     // caron (not in Windows 1252)
		};

		if (c >= 0x80)
		{
			return UnicodeString(strings[c - 0x80]);
		}

		return UnicodeString(ConstUnicodeStringView{&static_cast<const UnicodeCharType&>(c), 1});
	}

	[[nodiscard]] UnicodeString ConvertMacRomanToUtf8(const ConstStringView inputString)
	{
		UnicodeString string(Memory::Reserve, inputString.GetSize() * 3);
		for (const char character : inputString)
		{
			string += ConvertMacRomanCharacterToUtf8(character);
		}
		return string;
	};

	Font Cache::LoadFont(const ConstByteView data) const
	{
		const size fontSize = data.GetDataSize();

		Font::MemoryType fontMemory(Memory::ConstructWithSize, Memory::Uninitialized, fontSize);
		if (UNLIKELY(!data.ReadIntoView(fontMemory.GetView())))
		{
			return {};
		}

		FT_Face face;
		const FT_Error result = FT_New_Memory_Face(m_pLibrary, fontMemory.GetData(), (FT_Long)fontSize, 0, &face);

		if (UNLIKELY(result != 0))
		{
			return {};
		}

		Font::VariationsContainer fontVariations;

		FT_MM_Var* pFontVariation;
		if (FT_Get_MM_Var(face, &pFontVariation) == 0)
		{
			const uint32 nameCount = FT_Get_Sfnt_Name_Count(face);

			auto getFontNameEntry = [face, nameCount](const uint32 nameID) -> String
			{
				FT_SfntName name;
				for (uint32 nameIndex = 0; nameIndex < nameCount; ++nameIndex)
				{
					if (FT_Get_Sfnt_Name(face, nameIndex, &name) != 0)
					{
						continue;
					}

					if (name.name_id != nameID)
					{
						continue;
					}

					static auto convertUTF16BigEndianToUTF8 = [](const ConstUTF16StringView string) -> String
					{
						if (Memory::IsLittleEndian())
						{
							UTF16String encodedNameSwapped(string);
							for (char16_t& character : encodedNameSwapped)
							{
								character = (char16_t)Memory::ByteSwap((uint16)character);
							}
							return String(encodedNameSwapped.GetView());
						}
						return String(string);
					};

					enum class Encoding : uint8
					{
						Unknown,
						UTF16BE,
						MacRoman
					};

					static auto getEncoding = [](const FT_UShort platformIdentifier, const FT_UShort encodingIdentifier)
					{
						switch (platformIdentifier)
						{
							case TT_PLATFORM_ISO:
							{
								switch (encodingIdentifier)
								{
									case TT_ISO_ID_10646:
										return Encoding::UTF16BE;
								}
							}
							break;
							case TT_PLATFORM_MICROSOFT:
							{
								switch (encodingIdentifier)
								{
									case TT_MS_ID_SYMBOL_CS:
									case TT_MS_ID_UNICODE_CS:
									case TT_MS_ID_UCS_4:
										return Encoding::UTF16BE;
								}
								break;
							}
							case TT_PLATFORM_APPLE_UNICODE:
								return Encoding::UTF16BE;
							case TT_PLATFORM_MACINTOSH:
							{
								switch (encodingIdentifier)
								{
									case TT_MAC_ID_ROMAN:
									case TT_MAC_ID_JAPANESE:
										return Encoding::MacRoman;
								}
								break;
							}
						}
						return Encoding::Unknown;
					};

					static auto decodeName = [](const ConstByteView encodedNameBuffer, const FT_SfntName& name) -> String
					{
						switch (getEncoding(name.platform_id, name.encoding_id))
						{
							case Encoding::UTF16BE:
							{
								ConstUTF16StringView encodedName(
									reinterpret_cast<const char16_t*>(encodedNameBuffer.GetData()),
									uint32(encodedNameBuffer.GetDataSize() / sizeof(char16_t))
								);
								return convertUTF16BigEndianToUTF8(encodedName);
							}
							case Encoding::MacRoman:
							{
								return ConvertMacRomanToUtf8(
									ConstStringView{reinterpret_cast<const char*>(encodedNameBuffer.GetData()), (uint32)encodedNameBuffer.GetDataSize()}
								);
							}
							case Encoding::Unknown:
								return {};
						}
						ExpectUnreachable();
					};
					return decodeName(ConstByteView{name.string, name.string_len}, name);
				}
				return {};
			};

			const ArrayView<const FT_Var_Named_Style, uint16> namedStyles{pFontVariation->namedstyle, (uint16)pFontVariation->num_namedstyles};
			fontVariations.Reserve(namedStyles.GetSize());

			for (const FT_Var_Named_Style& namedStyle : namedStyles)
			{
				const String styleName = getFontNameEntry(namedStyle.strid);
				const size styleNameHash = Math::Hash(styleName.GetView());

				const OptionalIterator<const FontVariationInfo> pFontVariationInfo = FontVariations.GetView().FindIf(
					[styleNameHash](const FontVariationInfo& __restrict fontVariationInfo)
					{
						return fontVariationInfo.m_nameHash == styleNameHash;
					}
				);
				if (pFontVariationInfo.IsValid())
				{
					const uint16 namedInstanceIndex = namedStyles.GetIteratorIndex(&namedStyle) + 1;
					fontVariations.EmplaceBack(Font::Variation{namedInstanceIndex, pFontVariationInfo->m_weight, pFontVariationInfo->m_modifiers});
				}
				else
				{
					Assert(false, "Encountered unsupported font style");
				}
			}

			FT_Done_MM_Var(m_pLibrary, pFontVariation);
		}

		return Font(face, Move(fontMemory), Move(fontVariations));
	}

#if DEVELOPMENT_BUILD
	void Cache::OnAssetModified([[maybe_unused]] const Asset::Guid assetGuid, const Identifier, [[maybe_unused]] const IO::PathView filePath)
	{
		// TODO: Handle font reloading
		// Needs to propagate to all font atlases
	}
#endif
}

namespace ngine
{
	template struct UnorderedMap<Font::InstanceProperties::Hash, Font::InstanceIdentifier>;
	template struct TSaltedIdentifierStorage<Font::InstanceIdentifier>;

	template struct Threading::AtomicIdentifierMask<Font::InstanceIdentifier>;
	template struct TIdentifierArray<UniquePtr<Font::Atlas>, Font::InstanceIdentifier>;
}
