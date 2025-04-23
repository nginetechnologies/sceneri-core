#pragma once

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Storage/IdentifierMask.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/ComponentMask.h>

namespace ngine
{
	struct Scene3D;

	namespace Entity
	{
		struct Component3D;
	}
}

namespace ngine::GameFramework
{
	struct ResetSystem
	{
		ResetSystem(Scene3D& scene);

		void Reset();
		void Capture();

		void ResumeSimulation();
		void PauseSimulation();

		void RegisterSimulatedComponent(Entity::Component3D& component);
		void DeregisterSimulatedComponent(Entity::Component3D& component);

		[[nodiscard]] bool IsSimulationActive() const
		{
			return m_isSimulationActive;
		}
	protected:
		void Clear();
		void ResetTransforms();
	protected:
		Scene3D& m_scene;
		Entity::ComponentMask m_storedComponents;
		Vector<Entity::ComponentSoftReference> m_simulatedComponents;
		Vector<Entity::ComponentSoftReference> m_tempSimulatedComponents;
		bool m_isSimulationActive{false};
	};
}
