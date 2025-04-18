#pragma once

#include "MaterialAsset.h"
#include "MaterialInstanceIdentifier.h"
#include "MaterialIdentifier.h"

#include <Common/Function/Event.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Rendering
{
	struct MaterialCache;

	struct RuntimeMaterial
	{
		RuntimeMaterial(const MaterialIdentifier identifier)
			: m_identifier(identifier)
		{
		}

		[[nodiscard]] MaterialIdentifier GetIdentifier() const
		{
			return m_identifier;
		}

		enum class LoadingStatus : uint8
		{
			Loading,
			Finished,
			Failed
		};

		[[nodiscard]] bool HasFinishedLoading() const
		{
			return m_loadingStatus != LoadingStatus::Loading;
		}
		[[nodiscard]] bool IsValid() const
		{
			return m_loadingStatus == LoadingStatus::Finished;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Optional<const MaterialAsset*> GetAsset() const
		{
			return Optional<const MaterialAsset*>{&m_asset, IsValid()};
		}

		bool Serialize(const Serialization::Reader serializer);

		Event<void(void*, const MaterialInstanceIdentifier, const MaterialIdentifier previousParent, const MaterialIdentifier newParent), 24>
			OnMaterialInstanceParentChanged;
	protected:
		friend MaterialCache;

		void OnLoadingFinished();
		void OnLoadingFailed();
	protected:
		MaterialIdentifier m_identifier;
		Threading::Atomic<LoadingStatus> m_loadingStatus{LoadingStatus::Loading};
		MaterialAsset m_asset;
	};
}
