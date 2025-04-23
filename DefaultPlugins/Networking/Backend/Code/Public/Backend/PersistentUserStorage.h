#pragma once

#include <Backend/PersistentUserIdentifier.h>

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Function/Event.h>

namespace ngine::Networking::Backend
{
	struct Game;

	struct PersistentUserStorage
	{
		PersistentUserStorage(Game& game);

		[[nodiscard]] ConstUnicodeStringView Find(const PersistentUserIdentifier playerIdentifier, const ConstStringView key) const;
		[[nodiscard]] ConstUnicodeStringView Find(const ConstStringView key) const;
		[[nodiscard]] bool Contains(const PersistentUserIdentifier playerIdentifier, const ConstStringView key) const;
		[[nodiscard]] bool Contains(const ConstStringView key) const;
		void Emplace(const PersistentUserIdentifier playerIdentifier, const ConstStringView key, UnicodeString&& value);
		void Emplace(const ConstStringView key, UnicodeString&& value);

		void Clear();

		struct UserEntries
		{
			bool Serialize(const Serialization::Reader reader);
			bool Serialize(Serialization::Writer reader) const;

			mutable Threading::SharedMutex m_mutex;
			UnorderedMap<String, UnicodeString, String::Hash> m_map;
		};
		Event<void(void*, ConstStringView, ConstUnicodeStringView), 24> OnDataChanged;
	protected:
		void Save() const;
	private:
		Game& m_game;

		mutable Threading::SharedMutex m_mutex;
		using UserStorage = UnorderedMap<PersistentUserIdentifier::Type, UniquePtr<UserEntries>>;
		UserStorage m_userStorage;
	};
}
