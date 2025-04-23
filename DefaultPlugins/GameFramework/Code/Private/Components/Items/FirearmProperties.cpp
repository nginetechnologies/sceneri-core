#include <GameFramework/Components/Items/FirearmProperties.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Defaults.h>

#include <Common/System/Query.h>

namespace ngine::GameFramework::Data
{
	ProjectileProperties ProjectileProperties::MakeAutomaticRifle()
	{
		ProjectileProperties properties;
		properties.m_damage = 10;
		properties.m_speed = 915_mps;
		properties.m_impulseFactor = 8.f;

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		properties.m_staticMeshIdentifier = renderer.GetMeshCache().FindOrRegisterAsset(ngine::Primitives::SphereMeshAssetGuid);
		properties.m_materialInstanceIdentifier =
			renderer.GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(Rendering::Constants::DefaultMaterialInstanceAssetGuid);

		return properties;
	}

	FirearmProperties FirearmProperties::MakeAutomaticRifle()
	{
		FirearmProperties properties;
		properties.m_burstSize = 30;
		properties.m_fireRate = 5_hz;
		properties.m_magazineCapacity = 30;
		properties.m_verticalRecoilFactor = 0.02f;
		properties.m_horizontalRecoilFactor = 0.02f;

		return properties;
	}
}
