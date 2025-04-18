#pragma once

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>

#include <Engine/Asset/AssetType.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Common/Memory/Containers/String.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	struct Renderer;

	enum class StageFlags : uint8
	{
		Hidden = 1 << 0,
		Transient = 1 << 1
	};

	ENUM_FLAG_OPERATORS(StageFlags);

	struct StageInfo
	{
		StageInfo(const EnumFlags<StageFlags> flags = {})
			: m_flags(flags)
		{
		}

		EnumFlags<StageFlags> m_flags;
	};

	struct StageCache final : public Asset::Type<SceneRenderStageIdentifier, StageInfo>, public DataSource::Interface
	{
		using BaseType = Asset::Type<SceneRenderStageIdentifier, StageInfo>;

		inline static constexpr Guid DataSourceGuid = "{36676E21-BD7D-4489-991F-542C22F07D8C}"_guid;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& GetRenderer();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& GetRenderer() const;

		StageCache();
		virtual ~StageCache();

		template<typename RegisterCallback>
		SceneRenderStageIdentifier RegisterAsset(const Guid guid, RegisterCallback&& registerCallback) = delete;

		SceneRenderStageIdentifier RegisterAsset(const Guid guid, const EnumFlags<StageFlags> flags = StageFlags::Hidden);
		SceneRenderStageIdentifier FindOrRegisterAsset(const Guid guid, const EnumFlags<StageFlags> flags = StageFlags::Hidden);
		SceneRenderStageIdentifier
		FindOrRegisterAsset(const Guid guid, const ConstUnicodeStringView name, const EnumFlags<StageFlags> flags = {});

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
		void RegisterVirtualAsset(const SceneRenderStageIdentifier identifier, const ConstUnicodeStringView name);
	protected:
		DataSource::PropertyIdentifier m_stageGenericIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_stageNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_stageGuidPropertyIdentifier;
	};
}
