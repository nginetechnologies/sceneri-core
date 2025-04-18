#pragma once

#include <Common/Asset/Asset.h>
#include <Common/Math/Density.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Physics
{
	struct MaterialAsset : public Asset::Asset
	{
		MaterialAsset(const Serialization::Data& assetData, IO::Path&& assetMetaFilePath);

		float m_friction = 0.2f;
		float m_restitution = 0.0f;
		Math::Densityf m_density{Math::Densityf::FromKilogramsCubed(1000.f)};

		bool Serialize(const Serialization::Reader reader);
	};
}
