#pragma once

#include "TagIdentifier.h"

#include <Engine/Asset/AssetType.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Common/System/SystemType.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Color.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Tag
{
	enum class Flags : uint16
	{
		VisibleToUI = 1 << 0,
		Removable = 1 << 1,
		Addable = 1 << 2,
		ComponentType = 1 << 3,
		ReflectedType = 1 << 4,
		//! Don't save to disk
		Transient = 1 << 5
	};

	ENUM_FLAG_OPERATORS(Flags);

	struct Info
	{
		Math::Color m_color;
		EnumFlags<Flags> m_flags;
	};

	struct Registry final : public Asset::Type<Identifier, Info>, public DataSource::Interface
	{
		inline static constexpr System::Type SystemType = System::Type::TagRegistry;
		inline static constexpr Guid DataSourceGuid = "3b4155c5-0c2b-4e0c-8dce-46f6e2a0788f"_guid;

		using BaseType = Asset::Type<Identifier, Info>;

		Registry();
		~Registry();

		Identifier Register(const Asset::Guid tagGuid, const EnumFlags<Flags> flags = {});
		Identifier FindOrRegister(const Asset::Guid tagGuid, const EnumFlags<Flags> flags = {});

		Identifier Register(
			const Asset::Guid tagGuid, const ConstUnicodeStringView name, const Math::Color color, const EnumFlags<Flags> flags = Flags::Removable
		);
		Identifier FindOrRegister(
			const Asset::Guid tagGuid, const ConstUnicodeStringView name, const Math::Color color, const EnumFlags<Flags> flags = Flags::Removable
		);

		//! Registers a tag from disk
		void RegisterAsset(const Asset::Guid tagGuid);

		template<typename RegisterCallback>
		Identifier RegisterAsset(
			const Guid guid, const ConstUnicodeStringView name, const Math::Color color, RegisterCallback&& registerCallback
		) = delete;
		template<typename RegisterCallback>
		Identifier FindOrRegisterAsset(
			const Guid guid, const ConstUnicodeStringView name, const Math::Color color, RegisterCallback&& registerCallback
		) = delete;

		// DataSource::Interface
		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override;
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
	private:
		void RegisterVirtualAsset(const Identifier identifier, const ConstUnicodeStringView name);
	protected:
		DataSource::PropertyIdentifier m_tagGenericIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_tagNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_tagGuidPropertyIdentifier;
		DataSource::PropertyIdentifier m_tagColorPropertyIdentifier;
	};
}
