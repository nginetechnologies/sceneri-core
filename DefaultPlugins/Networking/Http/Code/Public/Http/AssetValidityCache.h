#pragma once

#include <Engine/Engine.h>

#include <Http/FileCacheState.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/System/Query.h>

namespace ngine::Networking::HTTP
{
	struct AssetValidityCache
	{
		inline static constexpr IO::PathView FileName = MAKE_PATH("AssetValidityCache.json");

		[[nodiscard]] static IO::Path GetDefaultFilePath()
		{
			return IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), FileName);
		}

		AssetValidityCache()
		{
			[[maybe_unused]] const bool wasRead = Serialization::DeserializeFromDisk(GetDefaultFilePath(), *this);
		}

		[[nodiscard]] bool HasEntries() const
		{
			return m_map.HasElements();
		}

		void SaveIfNecessary()
		{
			bool expected = true;
			if (m_isDirty.CompareExchangeStrong(expected, false))
			{
				[[maybe_unused]] const bool wasSaved = Serialization::SerializeToDisk(GetDefaultFilePath(), *this, Serialization::SavingFlags{});
				Assert(wasSaved);
			}
		}
	protected:
		Threading::Atomic<bool> m_isDirty{false};
		bool m_registeredSaveCallback{false};
	};
}
