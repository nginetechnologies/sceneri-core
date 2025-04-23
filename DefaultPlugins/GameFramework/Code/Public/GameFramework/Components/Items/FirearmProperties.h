#pragma once

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>

#include <Common/Asset/Guid.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/Radius.h>
#include <Common/Math/Speed.h>
#include <Common/Time/ForwardDeclarations/Duration.h>
#include <Common/Time/FrameTime.h>
#include <Common/Time/Timestamp.h>
#include <Common/Math/Frequency.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>

namespace ngine::GameFramework::Data
{
	struct ProjectileProperties
	{
		// TODO: make this more data/asset driven
		[[nodiscard]] static ProjectileProperties MakeAutomaticRifle();

		Math::Speedf m_speed{915_mps};
		float m_damage{1.f};
		Math::Ratiof m_impulseFactor{100_percent};

		Math::Radiusf m_radius{0.022_meters};
		Rendering::StaticMeshIdentifier m_staticMeshIdentifier;
		Rendering::MaterialInstanceIdentifier m_materialInstanceIdentifier;

		Asset::Guid m_impactSoundAssetGuid;
		Math::Radiusf m_impactSoundRadius{10_meters};
		Asset::Guid m_impactDecalMaterialAssetGuid;
	};

	struct FirearmProperties
	{
		// TODO: make this more data/asset driven
		[[nodiscard]] static FirearmProperties MakeAutomaticRifle();

		int32 m_burstSize{1};
		Math::Frequencyf m_fireRate{1_hz};
		int32 m_magazineCapacity{12};

		Math::Ratiof m_verticalRecoilFactor{2_percent};
		Math::Ratiof m_horizontalRecoilFactor{1_percent};
	};
}
