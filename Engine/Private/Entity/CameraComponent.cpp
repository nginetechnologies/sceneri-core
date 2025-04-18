#include "Entity/CameraComponent.h"
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/CameraProperties.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Serialization/Reader.h>
#include <Common/Math/Radius.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	CameraComponent::CameraComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_fieldOfView(initializer.m_fieldOfView)
		, m_nearPlane(initializer.m_nearPlane)
		, m_farPlane(initializer.m_farPlane)
	{
	}

	CameraComponent::CameraComponent(const CameraComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_fieldOfView(templateComponent.m_fieldOfView)
		, m_nearPlane(templateComponent.m_nearPlane)
		, m_farPlane(templateComponent.m_farPlane)
	{
	}

	CameraProperties ReadCameraProperties(const Optional<Serialization::Reader> serializer)
	{
		CameraProperties properties;
		if (serializer)
		{
			Threading::JobBatch jobBatch;
			Reflection::GetType<CameraProperties>().SerializeTypePropertiesInline(*serializer, *serializer, properties, Invalid, jobBatch);
		}
		return properties;
	}

	CameraComponent::CameraComponent(const Deserializer& deserializer)
		: CameraComponent(
				deserializer,
				ReadCameraProperties(deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<CameraComponent>().ToString().GetView()))
			)
	{
	}

	CameraComponent::CameraComponent(const Deserializer& deserializer, const CameraProperties& properties)
		: Component3D(DeserializerWithBounds{deserializer} | Math::Radiusf::FromMeters(properties.m_farPlane - properties.m_nearPlane))
		, m_fieldOfView(properties.m_fieldOfView)
		, m_nearPlane(properties.m_nearPlane)
		, m_farPlane(properties.m_farPlane)
	{
	}

	CameraComponent::~CameraComponent()
	{
		OnDestroyedEvent();
	}

	void CameraComponent::OnEnable()
	{
		if (IsSimulationActive())
		{
			Controller* pCurrentController = m_pController;
			if (pCurrentController != nullptr)
			{
				pCurrentController->OnBecomeActive();
			}
		}
	}

	void CameraComponent::OnDisable()
	{
		if (IsSimulationActive())
		{
			Controller* pCurrentController = m_pController;
			if (pCurrentController != nullptr)
			{
				pCurrentController->OnBecomeInactive();
			}
		}
	}

	void CameraComponent::OnSimulationResumed()
	{
		if (IsEnabled())
		{
			Controller* pCurrentController = m_pController;
			if (pCurrentController != nullptr)
			{
				pCurrentController->OnBecomeActive();
			}
		}
	}

	void CameraComponent::OnSimulationPaused()
	{
		if (IsEnabled())
		{
			Controller* pCurrentController = m_pController;
			if (pCurrentController != nullptr)
			{
				pCurrentController->OnBecomeInactive();
			}
		}
	}

	void CameraComponent::SetController(const Optional<Controller*> pNewController)
	{
		Controller* pCurrentController = m_pController;
		Assert(pCurrentController != pNewController);
		if (pCurrentController != nullptr && IsEnabled() && IsSimulationActive())
		{
			pCurrentController->OnBecomeInactive();
		}
		m_pController = pNewController;
		if (pNewController != nullptr && IsEnabled() && IsSimulationActive())
		{
			pNewController->OnBecomeActive();
		}
	}

	void CameraComponent::RemoveController(Controller& controller)
	{
		Controller* pCurrentController = m_pController;
		if (&controller == pCurrentController)
		{
			m_pController = Invalid;
			if (pCurrentController != nullptr && IsEnabled() && IsSimulationActive())
			{
				pCurrentController->OnBecomeInactive();
			}
		}
	}

	void CameraComponent::RemoveCurrentController()
	{
		Controller* pCurrentController = m_pController;
		m_pController = Invalid;
		if (pCurrentController != nullptr && IsEnabled() && IsSimulationActive())
		{
			pCurrentController->OnBecomeInactive();
		}
	}

	void CameraComponent::SetProperties(const CameraProperties& properties)
	{
		m_fieldOfView = properties.m_fieldOfView;
		m_nearPlane = properties.m_nearPlane;
		m_farPlane = properties.m_farPlane;
		SetBoundingBox(Math::Radiusf::FromMeters(properties.m_farPlane - properties.m_nearPlane));

		OnFieldOfViewChanged();
		OnNearPlaneChanged();
		OnFarPlaneChanged();
	}

	CameraProperties CameraComponent::GetProperties() const
	{
		return {m_fieldOfView, m_nearPlane, m_farPlane};
	}

	void CameraComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags>)
	{
		OnTransfromChangedEvent();
	}

	[[maybe_unused]] const bool wasCameraRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<CameraComponent>>::Make());
	[[maybe_unused]] const bool wasCameraTypeRegistered = Reflection::Registry::RegisterType<CameraComponent>();
}
