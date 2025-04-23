#include "Components/CinematicComponent.h"
#include "Components/SceneRules/SceneRules.h"

#include <GameFramework/PlayerManager.h>
#include <GameFramework/Plugin.h>
#include <GameFramework/Components/Controllers/SplineMovementComponent.h>
#include <GameFramework/Reset/ResetComponent.h>

#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>

#include <Renderer/Scene/SceneView.h>

#include <Engine/Threading/JobManager.h>

#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Reflection/DynamicTypeDefinition.h>

namespace ngine::GameFramework
{
	CinematicComponent::CinematicComponent(const CinematicComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_cameraDurations(templateComponent.m_cameraDurations)
	{
		const Entity::SceneRegistry& templateSceneRegistry = cloner.GetTemplateSceneRegistry();
		Entity::SceneRegistry& sceneRegistry = cloner.GetParent()->GetSceneRegistry();
		for (uint8 index = 0; index < MaximumSlotCount; ++index)
		{
			m_cameras[index] = Entity::ComponentSoftReference(
				templateComponent.m_cameras[index],
				Entity::ComponentSoftReference::Cloner{templateSceneRegistry, sceneRegistry}
			);
		}
	}

	CinematicComponent::CinematicComponent(const Deserializer& deserializer)
		: Component3D(deserializer)
	{
	}

	CinematicComponent::CinematicComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
	{
	}

	CinematicComponent::~CinematicComponent()
	{
	}

	void CinematicComponent::OnCreated()
	{
		if (IsEnabled() && IsSimulationActive() && m_isRunning == false)
		{
			StartSequence();
		}
	}

	void CinematicComponent::OnSimulationResumed()
	{
		if (IsEnabled() && m_currentIndex == 0 && m_isRunning == false)
		{
			StartSequence();
		}
	}

	void CinematicComponent::OnEnabled()
	{
		if (IsSimulationActive() && m_currentIndex == 0 && m_isRunning == false)
		{
			StartSequence();
		}
	}

	void CinematicComponent::StartSequence()
	{
		StartCameraSequence(m_currentIndex);

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (!HasAnyDataComponentsImplementingType<Data::Reset>(sceneRegistry))
		{
			CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, sceneRegistry},
				[this](Entity::Component3D&)
				{
					m_currentIndex = 0;
					m_isRunning = false;
				},
				*this
			});
		}
	}

	void CinematicComponent::OnSimulationPaused()
	{
		m_isRunning = false;
	}

	void CinematicComponent::OnDisabled()
	{
		m_isRunning = false;
	}

	void CinematicComponent::OnSequenceFinished()
	{
		if (m_isRunning)
		{
			// If the next sequence can't be started the cinematic is over.
			m_isRunning = StartCameraSequence(++m_currentIndex);
			if (!m_isRunning && m_shouldLoop)
			{
				RestartSequence();
			}
		}
	}

	void CinematicComponent::RestartSequence()
	{
		// For now we trigger a full game restart to also properly reset other elements in the scene
		Entity::HierarchyComponentBase& rootSceneComponent = GetRootSceneComponent();
		if (Optional<SceneRules*> pSceneRules = SceneRules::Find(rootSceneComponent))
		{
			const Optional<Network::Session::Client*> pClientComponent = rootSceneComponent.FindFirstParentOfType<Network::Session::Client>();
			Assert(pClientComponent.IsValid());
			if (LIKELY(pClientComponent.IsValid()))
			{
				if (pClientComponent->HasDataComponentOfType<Network::Session::LocalClient>(GetSceneRegistry()))
				{
					pSceneRules->OnLocalPlayerRequestRestart(pClientComponent->GetClientIdentifier());
				}
			}
		}
	}

	void CinematicComponent::RegisterFinishCondition(Time::Durationf delay)
	{
		// TODO: This probably does not respect pauses
		System::Get<Threading::JobManager>().ScheduleAsync(
			delay,
			[this](Threading::JobRunnerThread&)
			{
				OnSequenceFinished();
			},
			Threading::JobPriority::QueuedComponentDestructions
		);
	}

	void CinematicComponent::SwitchToCamera(Entity::CameraComponent& camera)
	{
		PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		playerManager.IterateActivePlayers(
			[&camera](PlayerInfo* pPlayerInfo)
			{
				// TODO: This assumes that we just have one player for cutscenes
				if (Optional<Rendering::SceneView*> pSceneView = pPlayerInfo->GetSceneView())
				{
					pSceneView->AssignCamera(camera);
				}

				return Memory::CallbackResult::Break;
			}
		);
	}

	bool CinematicComponent::StartCameraSequence(uint8 index)
	{
		const Optional<Entity::CameraComponent*> pCamera = FindCamera(index);
		if (pCamera.IsInvalid())
		{
			return false;
		}

		const Time::Durationf duration = m_cameraDurations[index];

		RegisterFinishCondition(duration);
		SwitchToCamera(*pCamera);

		m_isRunning = true;
		return true;
	}

	Optional<Entity::CameraComponent*> CinematicComponent::FindCamera(uint8 index)
	{
		const Entity::ComponentSoftReference cameraReference = m_cameras[index];

		if (Optional<Entity::Component3D*> pCameraRoot = cameraReference.Find<Entity::Component3D>(GetSceneRegistry()))
		{
			Optional<Entity::CameraComponent*> pCamera = pCameraRoot->As<Entity::CameraComponent>();
			if (pCamera.IsInvalid())
			{
				pCamera = pCameraRoot->FindFirstChildOfTypeRecursive<Entity::CameraComponent>();
			}
			return pCamera;
		}
		return {};
	}

	void CinematicComponent::SetCameraPicker(uint8 cameraIndex, const Entity::Component3DPicker cameraPicker)
	{
		Assert(cameraIndex < MaximumSlotCount, "camera index is out of bounds");
		m_cameras[cameraIndex] = static_cast<Entity::ComponentSoftReference>(cameraPicker);
	}

	Entity::Component3DPicker CinematicComponent::GetCameraPicker(uint8 cameraIndex) const
	{
		Assert(cameraIndex < MaximumSlotCount, "camera index is out of bounds");
		Entity::Component3DPicker cameraPicker{m_cameras[cameraIndex], GetSceneRegistry()};
		cameraPicker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Entity::CameraComponent>()});
		return cameraPicker;
	}

	void CinematicComponent::SetCameraDuration(uint8 cameraIndex, Time::Durationf time)
	{
		Assert(cameraIndex < MaximumSlotCount, "camera index is out of bounds");
		m_cameraDurations[cameraIndex] = time;
	}

	Time::Durationf CinematicComponent::GetCameraDuration(uint8 cameraIndex) const
	{
		Assert(cameraIndex < MaximumSlotCount, "camera index is out of bounds");
		return m_cameraDurations[cameraIndex];
	}

#define CameraGetterAndSetter(index) \
	void CinematicComponent::SetCameraPicker##index(const Entity::Component3DPicker cameraPicker) \
	{ \
		SetCameraPicker(index, cameraPicker); \
	} \
	Entity::Component3DPicker CinematicComponent::GetCameraPicker##index() const \
	{ \
		return GetCameraPicker(index); \
	} \
	void CinematicComponent::SetCameraDuration##index(float duration) \
	{ \
		SetCameraDuration(index, Time::Durationf::FromSeconds(duration)); \
	} \
	float CinematicComponent::GetCameraDuration##index() const \
	{ \
		return GetCameraDuration(index).GetSeconds(); \
	}

	CameraGetterAndSetter(0);
	CameraGetterAndSetter(1);
	CameraGetterAndSetter(2);
	CameraGetterAndSetter(3);
	CameraGetterAndSetter(4);
	CameraGetterAndSetter(5);
	CameraGetterAndSetter(6);
	CameraGetterAndSetter(7);
	CameraGetterAndSetter(8);
	CameraGetterAndSetter(9);
	CameraGetterAndSetter(10);
	CameraGetterAndSetter(11);
	CameraGetterAndSetter(12);
	CameraGetterAndSetter(13);
	CameraGetterAndSetter(14);
	CameraGetterAndSetter(15);

#undef CameraGetterAndSetter

	[[maybe_unused]] const bool wasCinematicRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CinematicComponent>>::Make());
	[[maybe_unused]] const bool wasCinematicTypeRegistered = Reflection::Registry::RegisterType<CinematicComponent>();
}
