#include "PersistentUserStorage.h"
#include "Game.h"

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Memory/Serialization/UniquePtr.h>
#include <Common/IO/Path.h>

namespace ngine::Networking::Backend
{
	inline static constexpr IO::PathView LocalStorageFileName{MAKE_PATH("PersistentUserStorage.json")};

	bool PersistentUserStorage::UserEntries::Serialize(const Serialization::Reader reader)
	{
		Threading::UniqueLock lock(m_mutex);
		return reader.SerializeInPlace(m_map);
	}

	bool PersistentUserStorage::UserEntries::Serialize(Serialization::Writer writer) const
	{
		Threading::SharedLock lock(m_mutex);
		return writer.SerializeInPlace(m_map);
	}

	PersistentUserStorage::PersistentUserStorage(Game& game)
		: m_game(game)
	{
		const IO::Path localStorageFilePath = IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), LocalStorageFileName);
		[[maybe_unused]] const bool wasRead = Serialization::DeserializeFromDisk(localStorageFilePath.GetZeroTerminated(), m_userStorage);
	}

	ConstUnicodeStringView PersistentUserStorage::Find(const PersistentUserIdentifier playerIdentifier, const ConstStringView key) const
	{
		Threading::SharedLock lock(m_mutex);
		auto userIt = m_userStorage.Find(playerIdentifier.Get());
		if (userIt != m_userStorage.end())
		{
			const UserEntries& userEntries = *userIt->second;
			lock.Unlock();

			Threading::SharedLock userLock(userEntries.m_mutex);
			auto it = userEntries.m_map.Find(key);
			if (it != userEntries.m_map.end())
			{
				return it->second;
			}
		}

		return {};
	}

	ConstUnicodeStringView PersistentUserStorage::Find(const ConstStringView key) const
	{
		return Find(m_game.GetLocalPlayerInternalIdentifier(), key);
	}

	bool PersistentUserStorage::Contains(const PersistentUserIdentifier playerIdentifier, const ConstStringView key) const
	{
		Threading::SharedLock lock(m_mutex);
		auto userIt = m_userStorage.Find(playerIdentifier.Get());
		if (userIt != m_userStorage.end())
		{
			const UserEntries& userEntries = *userIt->second;
			lock.Unlock();

			Threading::SharedLock userLock(userEntries.m_mutex);
			return userEntries.m_map.Contains(key);
		}

		return false;
	}

	bool PersistentUserStorage::Contains(const ConstStringView key) const
	{
		return Contains(m_game.GetLocalPlayerInternalIdentifier(), key);
	}

	void PersistentUserStorage::Emplace(PersistentUserIdentifier playerIdentifier, ConstStringView key, UnicodeString&& value)
	{
		{
			Threading::SharedLock lock(m_mutex);
			auto userIt = m_userStorage.Find(playerIdentifier.Get());
			if (userIt != m_userStorage.end())
			{
				UserEntries& userEntries = *userIt->second;
				lock.Unlock();

				Threading::UniqueLock userLock(userEntries.m_mutex);
				auto entryIt = userEntries.m_map.Find(key);
				if (entryIt != userEntries.m_map.end())
				{
					entryIt->second = Move(value);
					OnDataChanged(entryIt->first, entryIt->second);
				}
				else
				{
					auto newEntryIt = userEntries.m_map.Emplace(key, Forward<UnicodeString>(value));
					OnDataChanged(newEntryIt->first, newEntryIt->second);
				}

				userLock.Unlock();
				Save();
				return;
			}
		}

		{
			Threading::UniqueLock lock(m_mutex);
			auto userIt = m_userStorage.Find(playerIdentifier.Get());
			if (userIt != m_userStorage.end())
			{
				UserEntries& userEntries = *userIt->second;
				lock.Unlock();

				Threading::UniqueLock userLock(userEntries.m_mutex);
				userEntries.m_map.Emplace(key, Forward<UnicodeString>(value));
			}
			else
			{
				UniquePtr<UserEntries> pUserEntries = UniquePtr<UserEntries>::Make();
				UserEntries& userEntries = *pUserEntries;
				m_userStorage.Emplace(playerIdentifier.Get(), Move(pUserEntries));

				lock.Unlock();

				Threading::UniqueLock userLock(userEntries.m_mutex);
				auto entryIt = userEntries.m_map.Find(key);
				if (entryIt != userEntries.m_map.end())
				{
					entryIt->second = Move(value);
					OnDataChanged(entryIt->first, entryIt->second);
				}
				else
				{
					auto newEntryIt = userEntries.m_map.Emplace(key, Forward<UnicodeString>(value));
					OnDataChanged(newEntryIt->first, newEntryIt->second);
				}
			}
		}

		Save();
	}

	void PersistentUserStorage::Emplace(const ConstStringView key, UnicodeString&& value)
	{
		return Emplace(m_game.GetLocalPlayerInternalIdentifier(), key, Forward<UnicodeString>(value));
	}

	void PersistentUserStorage::Clear()
	{
		{
			Threading::UniqueLock lock(m_mutex);
			m_userStorage.Clear();
		}

		Save();
	}

	void PersistentUserStorage::Save() const
	{
		const IO::Path localStorageFilePath = IO::Path::Combine(IO::Path::GetApplicationCacheDirectory(), LocalStorageFileName);

		Threading::SharedLock lock(m_mutex);
		[[maybe_unused]] const bool wasWritten =
			Serialization::SerializeToDisk(localStorageFilePath.GetZeroTerminated(), m_userStorage, Serialization::SavingFlags{});
		Assert(wasWritten);
	}

	// TODO: Implement backend persistent player storage
	// For now we only handle the local player
}
