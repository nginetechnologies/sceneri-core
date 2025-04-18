#pragma once

#include <Engine/Entity/ComponentTypeMask.h>
#include <Engine/Tag/TagContainer.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/ForwardDeclarations/Optional.h>
#include <Common/Memory/Containers/UnorderedMap.h>

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Threading/AtomicPtr.h>
#include <Common/Reflection/GetType.h>
#include <Common/Guid.h>

namespace ngine
{
	extern template struct UnorderedMap<Guid, Entity::ComponentTypeIdentifier, Guid::Hash>;
}

namespace ngine::Entity
{
	struct ComponentTypeInterface;

	struct ComponentRegistry final : public DataSource::Interface
	{
		static constexpr Guid DataSourceGuid = "9776a777-7c23-4ac7-b0bb-1a04536c5206"_guid;

		ComponentRegistry();
		~ComponentRegistry();

		static bool Register(UniquePtr<ComponentTypeInterface>&& type);
		void Register(const Optional<ComponentTypeInterface*> pType);
		void Deregister(const Guid typeGuid);

		[[nodiscard]] Optional<const ComponentTypeInterface*> Get(const ComponentTypeIdentifier identifier) const
		{
			return m_componentTypes[identifier];
		}
		[[nodiscard]] Optional<ComponentTypeInterface*> Get(const ComponentTypeIdentifier identifier)
		{
			return m_componentTypes[identifier];
		}
		[[nodiscard]] ComponentTypeIdentifier FindIdentifier(const Guid guid) const
		{
			const typename decltype(m_lookupMap)::const_iterator it = m_lookupMap.Find(guid);
			if (it != m_lookupMap.end())
			{
				return it->second;
			}

			return ComponentTypeIdentifier();
		}
		template<typename Type>
		[[nodiscard]] ComponentTypeIdentifier FindIdentifier() const
		{
			return FindIdentifier(Reflection::GetTypeGuid<Type>());
		}
		[[nodiscard]] Guid GetGuid(const ComponentTypeIdentifier identifier) const;

		template<typename Type>
		[[nodiscard]] Optional<const ComponentTypeInterface*> Get() const
		{
			return Get(FindIdentifier(Reflection::GetTypeGuid<Type>()));
		}
		template<typename Type>
		[[nodiscard]] Optional<ComponentTypeInterface*> Get()
		{
			return Get(FindIdentifier(Reflection::GetTypeGuid<Type>()));
		}

		// DataSource::Interface
		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override
		{
			return m_componentTypeCount;
		}
		virtual void IterateData(
			const CachedQuery& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual void IterateData(
			const SortedQueryIndices& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual PropertyValue GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const override;
		// ~DataSource::Interface

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, ComponentTypeIdentifier>
		GetValidElementView(IdentifierArrayView<ElementType, ComponentTypeIdentifier> view) const
		{
			return m_componentTypeIdentifiers.GetValidElementView(view);
		}

		template<typename ElementType>
		[[nodiscard]] IdentifierArrayView<ElementType, ComponentTypeIdentifier>
		GetValidElementView(FixedIdentifierArrayView<ElementType, ComponentTypeIdentifier> view) const
		{
			return m_componentTypeIdentifiers.GetValidElementView(view);
		}
	protected:
		TSaltedIdentifierStorage<ComponentTypeIdentifier> m_componentTypeIdentifiers;
		TIdentifierArray<Optional<ComponentTypeInterface*>, ComponentTypeIdentifier> m_componentTypes;
		UnorderedMap<Guid, ComponentTypeIdentifier, Guid::Hash> m_lookupMap;
		Threading::Atomic<GenericDataIndex> m_componentTypeCount{0};
		ComponentTypeMask m_componentMask;
		Tag::AtomicMaskContainer<ComponentTypeIdentifier> m_tags;
		DataSource::PropertyIdentifier m_typeIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeGenericIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_typeGuidPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeAssetGuidPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeAssetIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeThumbnailPropertyIdentifier;
	};
}
