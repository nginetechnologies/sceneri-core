#include "Material.h"

#include <Common/Serialization/Deserialize.h>

namespace ngine::Physics
{
	bool Material::Serialize(const Serialization::Reader reader)
	{
		reader.Serialize("friction", m_friction);
		reader.Serialize("restitution", m_restitution);
		reader.Serialize("density", m_density);
		return true;
	}

	bool Material::Serialize(Serialization::Writer writer) const
	{
		writer.Serialize("friction", m_friction);
		writer.Serialize("restitution", m_restitution);
		writer.Serialize("density", m_density);
		return true;
	}
}
