#include "Widgets/Style/StylesheetCache.h"
#include "Widgets/Style/ComputedStylesheet.h"

#include <Engine/Asset/AssetType.inl>
#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/System/Query.h>

namespace ngine::Widgets::Style
{
	StylesheetData::StylesheetData() = default;
	StylesheetData::StylesheetData(StylesheetData&&) = default;
	StylesheetData::~StylesheetData() = default;

	StylesheetCache::StylesheetCache(Asset::Manager& assetManager)
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	StylesheetCache::~StylesheetCache() = default;

	StylesheetIdentifier StylesheetCache::FindOrRegister(const Asset::Guid assetGuid)
	{
		return BaseType::FindOrRegisterAsset(
			assetGuid,
			[](const StylesheetIdentifier, const Asset::Guid) -> StylesheetData
			{
				return StylesheetData{};
			}
		);
	}

	Threading::Job* StylesheetCache::FindOrLoad(const StylesheetIdentifier identifier, StylesheetRequesters::ListenerData&& listenerData)
	{
		StylesheetRequesters* pRequesters = nullptr;

		{
			Threading::SharedLock lock(m_stylesheetRequesterMutex);
			decltype(m_stylesheetRequesterMap)::iterator it = m_stylesheetRequesterMap.Find(identifier);
			if (it != m_stylesheetRequesterMap.end())
			{
				pRequesters = it->second;
			}
		}
		if (pRequesters == nullptr)
			;
		{
			Threading::UniqueLock lock(m_stylesheetRequesterMutex);
			decltype(m_stylesheetRequesterMap)::iterator it = m_stylesheetRequesterMap.Find(identifier);
			if (it != m_stylesheetRequesterMap.end())
			{
				pRequesters = it->second;
			}
			else
			{
				it = m_stylesheetRequesterMap.Emplace(StylesheetIdentifier(identifier), UniqueRef<StylesheetRequesters>::Make());
				pRequesters = it->second;
			}
		}

		pRequesters->Emplace(Forward<StylesheetRequesters::ListenerData>(listenerData));

		StylesheetData& stylesheetData = GetAssetData(identifier);
		if (stylesheetData.pStylesheet.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = pRequesters->Execute(listenerData.m_identifier, *stylesheetData.pStylesheet);
			Assert(wasExecuted);
			return nullptr;
		}
		else if (m_loadingStylesheets.Set(identifier))
		{
			if (stylesheetData.pStylesheet.IsValid())
			{
				m_loadingStylesheets.Clear(identifier);
				[[maybe_unused]] const bool wasExecuted = pRequesters->Execute(listenerData.m_identifier, *stylesheetData.pStylesheet);
				Assert(wasExecuted);
				return nullptr;
			}
			else
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				const Asset::Guid guid = GetAssetGuid(identifier);
				const Guid assetTypeGuid = assetManager.GetAssetTypeGuid(guid);
				if (assetTypeGuid == ComputedStylesheet::AssetFormat.assetTypeGuid)
				{
					return assetManager.RequestAsyncLoadAssetMetadata(
						guid,
						Threading::JobPriority::LoadShader,
						[this, identifier](const ConstByteView data)
						{
							Optional<const ComputedStylesheet*> pComputedStylesheet;

							Assert(data.HasElements());
							if (LIKELY(data.HasElements()))
							{
								UniquePtr<ComputedStylesheet> pStylesheet = UniquePtr<ComputedStylesheet>::Make();

								Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
									ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
								);
								Assert(rootReader.GetData().IsValid());
								if (LIKELY(rootReader.GetData().IsValid()))
								{
									Serialization::Reader reader{rootReader};

									const bool readStylesheet = reader.Serialize("style", *pStylesheet);
									if (LIKELY(readStylesheet))
									{
										pComputedStylesheet = pStylesheet.Get();
										GetAssetData(identifier).pStylesheet = Move(pStylesheet);
									}
								}
							}

							{
								Threading::SharedLock lock(m_stylesheetRequesterMutex);
								auto it = m_stylesheetRequesterMap.Find(identifier);
								Assert(it != m_stylesheetRequesterMap.end());
								if (LIKELY(it != m_stylesheetRequesterMap.end()))
								{
									(*it->second)(pComputedStylesheet);
								}

								[[maybe_unused]] const bool wasCleared = m_loadingStylesheets.Clear(identifier);
								Assert(wasCleared);
							}
						}
					);
				}
				else if (assetTypeGuid == ComputedStylesheet::CSSAssetFormat.assetTypeGuid)
				{
					return assetManager.RequestAsyncLoadAssetBinary(
						guid,
						Threading::JobPriority::LoadShader,
						[this, identifier](const ConstByteView data)
						{
							Assert(data.HasElements());
							Optional<const ComputedStylesheet*> pComputedStylesheet;

							if (LIKELY(data.HasElements()))
							{
								UniquePtr<ComputedStylesheet> pStylesheet = UniquePtr<ComputedStylesheet>::Make();

								const ConstStringView stylesheetData{
									reinterpret_cast<const char*>(data.GetData()),
									uint32(data.GetDataSize() / sizeof(char))
								};

								pStylesheet->ParseFromCSS(stylesheetData);
								pComputedStylesheet = pStylesheet.Get();
								GetAssetData(identifier).pStylesheet = Move(pStylesheet);
							}

							{
								Threading::SharedLock lock(m_stylesheetRequesterMutex);
								auto it = m_stylesheetRequesterMap.Find(identifier);
								Assert(it != m_stylesheetRequesterMap.end());
								if (LIKELY(it != m_stylesheetRequesterMap.end()))
								{
									(*it->second)(pComputedStylesheet);
								}

								[[maybe_unused]] const bool wasCleared = m_loadingStylesheets.Clear(identifier);
								Assert(wasCleared);
							}
						}
					);
				}

				return nullptr;
			}
		}
		else
		{
			return nullptr;
		}
	}

#if DEVELOPMENT_BUILD
	void StylesheetCache::OnAssetModified(
		const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		// TODO
		UNUSED(assetGuid);
		UNUSED(identifier);
	}
#endif
}
