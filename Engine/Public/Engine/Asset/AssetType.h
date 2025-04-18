#pragma once

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Asset/Guid.h>
#include <Common/IO/PathView.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Asset
{
	struct Manager;

	template<typename IdentifierType_, typename InstanceDataType_>
	struct Type
	{
		using IdentifierType = IdentifierType_;
		using InstanceDataType = InstanceDataType_;
	public:
		Type() = default;
		Type(const Type&) = delete;
		Type& operator=(const Type&) = delete;
		Type(Type&&) = delete;
		Type& operator=(Type&&) = delete;
		virtual ~Type();

		void RegisterAssetModifiedCallback(Manager& manager);

		template<typename RegisterCallback>
		inline IdentifierType RegisterAsset(const Guid guid, RegisterCallback&& registerCallback)
		{
			Assert(guid.IsValid());
			Threading::UniqueLock lock(m_identifierLookupMapMutex);
			const IdentifierType identifier = m_identifierStorage.AcquireIdentifier();
			Assert(identifier.IsValid());
			if (LIKELY(identifier.IsValid()))
			{
				{
					Assert(!m_identifierLookupMap.Contains(guid));
					m_identifierLookupMap.Emplace(Guid(guid), IdentifierType(identifier));
				}
				m_assetGuids.Construct(identifier, guid);
				m_storedData.Construct(identifier, registerCallback(identifier, guid));
			}
			return identifier;
		}

		void DeregisterAsset(const IdentifierType identifier)
		{
			const Guid guid = m_assetGuids[identifier];
			{
				Threading::UniqueLock lock(m_identifierLookupMapMutex);
				typename decltype(m_identifierLookupMap)::const_iterator it = m_identifierLookupMap.Find(guid);
				Assert(it != m_identifierLookupMap.end());
				if (LIKELY(it != m_identifierLookupMap.end()))
				{
					m_identifierLookupMap.Remove(it);
				}
			}
			m_storedData.Destroy(identifier);
			m_identifierStorage.ReturnIdentifier(identifier);
		}

		template<typename RegisterCallback>
		[[nodiscard]] IdentifierType RegisterProceduralAsset(RegisterCallback&& registerCallback, const Guid guid = Guid::Generate())
		{
			return RegisterAsset(guid, Forward<RegisterCallback>(registerCallback));
		}

		[[nodiscard]] inline IdentifierType FindIdentifier(const Guid guid) const
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			typename decltype(m_identifierLookupMap)::const_iterator it = m_identifierLookupMap.Find(guid);
			if (it == m_identifierLookupMap.end())
			{
				return IdentifierType();
			}

			return it->second;
		}

		template<typename RegisterCallback>
		[[nodiscard]] IdentifierType FindOrRegisterAsset(const Guid guid, RegisterCallback&& registerCallback)
		{
			Assert(guid.IsValid());
			{
				Threading::SharedLock lock(m_identifierLookupMapMutex);
				typename decltype(m_identifierLookupMap)::const_iterator it = m_identifierLookupMap.Find(guid);
				if (it != m_identifierLookupMap.end())
				{
					return it->second;
				}
			}

			Threading::UniqueLock lock(m_identifierLookupMapMutex);
			{
				typename decltype(m_identifierLookupMap)::const_iterator it = m_identifierLookupMap.Find(guid);
				if (it != m_identifierLookupMap.end())
				{
					return it->second;
				}
			}

			const IdentifierType identifier = m_identifierStorage.AcquireIdentifier();
			Assert(identifier.IsValid());
			if (LIKELY(identifier.IsValid()))
			{
				Assert(!m_identifierLookupMap.Contains(guid));
				m_identifierLookupMap.Emplace(Guid(guid), IdentifierType(identifier));

				m_assetGuids.Construct(identifier, guid);
				m_storedData.Construct(identifier, registerCallback(identifier, guid));
			}
			return identifier;
		}

		[[nodiscard]] bool IsAssetAvailable(const Guid guid) const
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			return m_identifierLookupMap.Contains(guid);
		}

		[[nodiscard]] bool IsAssetAvailable(const IdentifierType identifier) const
		{
			return m_identifierStorage.IsIdentifierActive(identifier);
		}

		[[nodiscard]] InstanceDataType& GetAssetData(const IdentifierType identifier)
		{
			Assert(IsAssetAvailable(identifier));
			return m_storedData[identifier];
		}

		[[nodiscard]] const InstanceDataType& GetAssetData(const IdentifierType identifier) const
		{
			Assert(IsAssetAvailable(identifier));
			return m_storedData[identifier];
		}

		[[nodiscard]] const Guid GetAssetGuid(const IdentifierType identifier) const
		{
			return m_assetGuids[identifier];
		}

		template<typename Callback, typename ElementType>
		void IterateElements(FixedIdentifierArrayView<ElementType, IdentifierType> view, Callback&& callback) const
		{
			for (ElementType& element : m_identifierStorage.GetValidElementView(view))
			{
				callback(element);
			}
		}

		template<typename Callback>
		void IterateElements(Callback&& callback) const
		{
			for (const InstanceDataType& instanceData : m_identifierStorage.GetValidElementView(m_storedData.GetView()))
			{
				callback(instanceData);
			}
		}

		template<typename Callback>
		void IterateElements(Callback&& callback)
		{
			for (InstanceDataType& instanceData : m_identifierStorage.GetValidElementView(m_storedData.GetView()))
			{
				callback(instanceData);
			}
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, IdentifierType> GetValidElementView(IdentifierArrayView<ElementType, IdentifierType> view
		) const
		{
			return m_identifierStorage.GetValidElementView(view);
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, IdentifierType>
		GetValidElementView(FixedIdentifierArrayView<ElementType, IdentifierType> view) const
		{
			return m_identifierStorage.GetValidElementView(view);
		}

		[[nodiscard]] IdentifierArrayView<InstanceDataType, IdentifierType> GetValidElementView()
		{
			return m_identifierStorage.GetValidElementView(m_storedData.GetView());
		}
		[[nodiscard]] IdentifierArrayView<const InstanceDataType, IdentifierType> GetValidElementView() const
		{
			return m_identifierStorage.GetValidElementView(m_storedData.GetView());
		}

		[[nodiscard]] typename IdentifierType::IndexType GetMaximumUsedIdentifierCount() const
		{
			return m_identifierStorage.GetMaximumUsedElementCount();
		}
	protected:
		virtual void OnAssetModified(
			[[maybe_unused]] const Guid assetGuid, [[maybe_unused]] const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
		)
		{
		}

		Threading::AtomicIdentifierMask<IdentifierType> m_reloadingAssets;
	private:
		TSaltedIdentifierStorage<IdentifierType> m_identifierStorage;
		mutable Threading::SharedMutex m_identifierLookupMapMutex;
		UnorderedMap<Guid, IdentifierType, Guid::Hash> m_identifierLookupMap;

		TIdentifierArray<InstanceDataType, IdentifierType> m_storedData;
		TIdentifierArray<Guid, IdentifierType> m_assetGuids{Memory::Zeroed};
	};
}
