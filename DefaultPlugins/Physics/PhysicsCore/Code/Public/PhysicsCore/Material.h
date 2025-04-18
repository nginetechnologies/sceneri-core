#pragma once

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Collision/PhysicsMaterial.h>

#include <Common/Asset/Guid.h>
#include <Common/Math/Density.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::Physics
{
	struct MaterialCache;

	struct Material final : public JPH::PhysicsMaterial
	{
		Material(const Asset::Guid guid, const float friction, const float restitution, const Math::Densityf density)
			: PhysicsMaterial{friction, restitution, density.GetKilogramsCubed()}
			, m_assetGuid(guid)
		{
		}
		Material(const Material&) = default;
		virtual ~Material() = default;

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;

		[[nodiscard]] Asset::Guid GetAssetGuid() const
		{
			return m_assetGuid;
		}

		void SetFriction(const float friction)
		{
			m_friction = friction;
		}
		void SetRestitution(const float restitution)
		{
			m_restitution = restitution;
		}
		void SetDensity(const Math::Densityf density)
		{
			m_density = density.GetKilogramsCubed();
		}
		[[nodiscard]] Math::Densityf GetDensity() const
		{
			return Math::Densityf::FromKilogramsCubed(m_density);
		}
	protected:
		friend MaterialCache;

		Asset::Guid m_assetGuid;
	};
}
