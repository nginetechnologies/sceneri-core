#include "Assets/Material/RuntimeMaterial.h"

#include <Common/Serialization/Reader.h>

namespace ngine::Rendering
{
	bool RuntimeMaterial::Serialize(const Serialization::Reader serializer)
	{
		return serializer.SerializeInPlace(m_asset);
	}

	void RuntimeMaterial::OnLoadingFinished()
	{
		LoadingStatus expected = LoadingStatus::Loading;
		[[maybe_unused]] const bool wasSet = m_loadingStatus.CompareExchangeStrong(expected, LoadingStatus::Finished);
		Assert(wasSet);
	}

	void RuntimeMaterial::OnLoadingFailed()
	{
		LoadingStatus expected = LoadingStatus::Loading;
		[[maybe_unused]] const bool wasSet = m_loadingStatus.CompareExchangeStrong(expected, LoadingStatus::Failed);
		Assert(wasSet);
	}
}
