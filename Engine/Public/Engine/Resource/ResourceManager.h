#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/System/SystemType.h>
#include <Common/Threading/AtomicInteger.h>

namespace ngine::Resource
{
	// Allow identifiers to reference others
	// I.e. when a scene unloads it'll release references to all underneath it
	using Identifier = TIdentifier<uint32, 18>;

	struct Manager
	{
		inline static constexpr System::Type SystemType = System::Type::ResourceManager;

		using Priority = uint32;

		struct Resource
		{
			size m_dataSize;
		};

		Manager();

		[[nodiscard]] Identifier Register() const;

		void Acquire(const Identifier);
		void Release(const Identifier);

		void OnMemoryRunningLow();
	protected:
		TIdentifierArray<Threading::Atomic<uint32>, Identifier> m_resourceUseCounts;
		// TIdentifierArray<Event<void(void*, const Identifier), 24>, Identifier> m_resourceReleaseCallbacks;
	};
}
