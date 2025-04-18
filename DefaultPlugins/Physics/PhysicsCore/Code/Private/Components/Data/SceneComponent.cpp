#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/ColliderComponent.h"
#include "PhysicsCore/Components/CharacterComponent.h"
#include "PhysicsCore/Contact.h"
#include "PhysicsCore/Material.h"
#include "Plugin.h"

#include "Layer.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Engine/Entity/Data/Flags.h>

#include <Common/Math/Sqrt.h>
#include <Common/Memory/MemorySize.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Collision/Shape/BoxShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SphereShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Body/BodyCreationSettings.h>
#include <3rdparty/jolt/Physics/Collision/CollideShape.h>
#include <3rdparty/jolt/Physics/Collision/ShapeCast.h>
#include <3rdparty/jolt/Physics/Collision/RayCast.h>
#include <3rdparty/jolt/Physics/Collision/CastResult.h>
#include <3rdparty/jolt/Physics/Collision/Shape/CylinderShape.h>

#if ENABLE_JOLT_DEBUG_RENDERER
#include "TestFramework/TestFramework.h"
#include "TestFramework/Renderer/DebugRendererImp.h"
#endif

#include <Renderer/Scene/SceneView.h>
#include <Engine/Entity/CameraComponent.h>

namespace ngine::Physics::Data
{
	/* static */ Scene& Scene::Get(ngine::Scene3D& scene)
	{
		static Threading::SharedMutex mutex;
		Entity::Component3D& rootComponent = scene.GetRootComponent();
		{
			Threading::SharedLock lock(mutex);
			if (const Optional<Scene*> pScene = rootComponent.FindDataComponentOfType<Scene>())
			{
				return *pScene;
			}
		}
		Threading::UniqueLock lock(mutex);
		if (const Optional<Scene*> pScene = rootComponent.FindDataComponentOfType<Scene>())
		{
			return *pScene;
		}
		// TODO: Remove the need for this in our physics components
		// They should be able to assume that the physics scene already existed.
		return *scene.GetRootComponent().CreateDataComponent<Physics::Data::Scene>(
			scene.GetEntitySceneRegistry(),
			Physics::Data::Scene::DynamicInitializer{scene.GetRootComponent(), scene.GetEntitySceneRegistry()}
		);
	}

	// Function that determines if two object layers can collide
	bool FilterLayerCollision(JPH::ObjectLayer inObject1, JPH::ObjectLayer inObject2)
	{
		switch (static_cast<Layer>(inObject1))
		{
			case Layer::Static:
				return static_cast<Layer>(inObject2) == Layer::Dynamic; // Non moving only collides with moving
			case Layer::Dynamic:
				return true; // Moving collides with everything
			case Layer::Queries:
			case Layer::Triggers:
			case Layer::Gravity:
				return false;
			default:
				ExpectUnreachable();
		}
	};

	// Function that determines if two broadphase layers can collide
	bool FilterBroadPhaseLayerCollision(JPH::ObjectLayer inLayer1, JPH::BroadPhaseLayer inLayer2)
	{
		switch (static_cast<Layer>(inLayer1))
		{
			case Layer::Dynamic:
				return static_cast<BroadPhaseLayer>((uint8)inLayer2) != BroadPhaseLayer::Queries;
			case Layer::Queries:
				return static_cast<BroadPhaseLayer>((uint8)inLayer2) == BroadPhaseLayer::Queries;
			case Layer::Triggers:
			case Layer::Static:
				return static_cast<BroadPhaseLayer>((uint8)inLayer2) == BroadPhaseLayer::Dynamic;
			case Layer::Gravity:
				return false;
			default:
				ExpectUnreachable();
		}
	}

	Scene::Scene(Initializer&& initializer)
		: m_plugin(*System::FindPlugin<Plugin>())
		, m_engineScene(initializer.GetParent().GetRootScene())
		, m_bodyComponentTypeSceneData(*m_engineScene.GetEntitySceneRegistry().GetOrCreateComponentTypeData<Data::Body>())
		, m_physicsCommandStage(UniqueRef<PhysicsCommandStage>::Make(*this))
		, m_physicsSimulationStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					Step();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Physics,
				"Physics Simulation"
			))
		, m_tempAllocator(m_engineScene.IsTemplate() ? (uint32)(0_megabytes).ToBytes() : (uint32)(50_megabytes).ToBytes())
	{
		Threading::UniqueLock lock(m_initializationMutex);
		Initialize(initializer.GetParent());
	}

	Scene::Scene(const Deserializer& deserializer)
		: m_plugin(*System::FindPlugin<Plugin>())
		, m_engineScene(deserializer.GetParent().GetRootScene())
		, m_bodyComponentTypeSceneData(*m_engineScene.GetEntitySceneRegistry().GetOrCreateComponentTypeData<Data::Body>())
		, m_physicsCommandStage(UniqueRef<PhysicsCommandStage>::Make(*this))
		, m_physicsSimulationStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					Step();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Physics,
				"Physics Simulation"
			))
		, m_tempAllocator(m_engineScene.IsTemplate() ? (uint32)(0_megabytes).ToBytes() : (uint32)(50_megabytes).ToBytes())
	{
		Threading::UniqueLock lock(m_initializationMutex);
		Initialize(deserializer.GetParent());
	}

	Scene::Scene(const Scene& templateComponent, const Cloner& cloner)
		: m_plugin(templateComponent.m_plugin)
		, m_engineScene(cloner.GetParent().GetRootScene())
		, m_bodyComponentTypeSceneData(*m_engineScene.GetEntitySceneRegistry().GetOrCreateComponentTypeData<Data::Body>())
		, m_physicsCommandStage(UniqueRef<PhysicsCommandStage>::Make(*this))
		, m_physicsSimulationStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					Step();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Physics,
				"Physics Simulation"
			))
		, m_tempAllocator(m_engineScene.IsTemplate() ? (uint32)(0_megabytes).ToBytes() : (uint32)(50_megabytes).ToBytes())
	{
		Threading::UniqueLock lock(m_initializationMutex);
		Initialize(cloner.GetParent());
	}

	Scene::~Scene()
	{
	}

	void Scene::OnEnable()
	{
		m_engineScene.ModifyFrameGraph(
			[this]()
			{
				Threading::UniqueLock lock(m_initializationMutex);
				Threading::Job& physicsSimulationStage = GetPhysicsSimulationStage();
				if (!m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationStartStage().IsDirectlyFollowedBy(physicsSimulationStage))
				{
					m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationStartStage().AddSubsequentStage(physicsSimulationStage);
					physicsSimulationStage.AddSubsequentStage(m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationFinishedStage());

					m_physicsCommandStage->AddSubsequentStage(physicsSimulationStage);

					m_engineScene.GetStartFrameStage().AddSubsequentStage(m_physicsCommandStage);
				}

				m_nextTickTime = Time::Timestamp::GetCurrent();
			}
		);
	}

	void Scene::OnDisable()
	{
		m_engineScene.ModifyFrameGraph(
			[this]()
			{
				Threading::UniqueLock lock(m_initializationMutex);
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();

				Threading::Job& physicsSimulationStage = GetPhysicsSimulationStage();
				Assert(m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationStartStage().IsDirectlyFollowedBy(physicsSimulationStage));
				if (LIKELY(m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationStartStage().IsDirectlyFollowedBy(physicsSimulationStage)))
				{
					m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationStartStage().RemoveSubsequentStage(physicsSimulationStage, thread, {});
					physicsSimulationStage
						.RemoveSubsequentStage(m_engineScene.GetEntitySceneRegistry().GetPhysicsSimulationFinishedStage(), thread, {});

					m_physicsCommandStage->RemoveSubsequentStage(physicsSimulationStage, thread, {});

					m_engineScene.GetStartFrameStage().RemoveSubsequentStage(m_physicsCommandStage, thread, {});
				}
			}
		);
	}

	void Scene::Initialize([[maybe_unused]] ParentType& parent)
	{
		const bool isTemplateScene = m_engineScene.IsTemplate();

		// This is the max amount of rigid bodies that you can add to the physics system. If you try to add more you'll get an error.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const uint32 maxBodies = isTemplateScene ? 10000 : JPH::BodyIdentifier::MaximumCount;

		// This determines how many mutexes to allocate to protect rigid bodies from concurrent access. Set it to 0 for the default settings.
		const uint32 numBodyMutexes = 0;

		// This is the max amount of body pairs that can be queued at any time (the broad phase will detect overlapping
		// body pairs based on their bounding boxes and will insert them into a queue for the narrowphase). If you make this buffer
		// too small the queue will fill up and the broad phase jobs will start to do narrow phase work. This is slightly less efficient.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const uint32 maxBodyPairs = isTemplateScene ? 4 : 65536;

		// This is the maximum size of the contact constraint buffer. If more contacts (collisions between bodies) are detected than this
		// number then these contacts will be ignored and bodies will start interpenetrating / fall through the world.
		// Note: This value is low because this is a simple test. For a real project use something in the order of 65536.
		const uint32 maxContactConstraints = isTemplateScene ? 4 : 10240;

		m_physicsSystem.Init(
			maxBodies,
			numBodyMutexes,
			maxBodyPairs,
			maxContactConstraints,
			m_broadPhaseLayerInterface,
			FilterBroadPhaseLayerCollision,
			FilterLayerCollision
		);
		m_physicsSystem.SetBodyActivationListener(this);
		m_physicsSystem.SetContactListener(this);

		if (parent.IsEnabled())
		{
			OnEnable();
		}
	}

	void Scene::StepInternal()
	{
		Entity::SceneRegistry& sceneRegistry = m_engineScene.GetEntitySceneRegistry();
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		{
			struct AwaitUpdateJob final : public Threading::Job
			{
				AwaitUpdateJob()
					: Job(Threading::JobPriority::Physics)
				{
				}

				virtual Result OnExecute(Threading::JobRunnerThread&) override
				{
					return Result::AwaitExternalFinish;
				}

				virtual void OnAwaitExternalFinish(Threading::JobRunnerThread&) override
				{
					finished = true;
				}

				Threading::Atomic<bool> finished{false};
			};
			AwaitUpdateJob awaitUpdateJob;
			sceneRegistry.GetPhysicsStepFinishedStage().AddSubsequentStage(awaitUpdateJob);
			Assert(sceneRegistry.GetPhysicsStepStartStage().GetDependencyCount() == 0);
			sceneRegistry.GetPhysicsStepStartStage().SignalExecutionFinished(thread);

			while (!awaitUpdateJob.finished)
			{
				thread.DoRunNextJob();
			}
			sceneRegistry.GetPhysicsStepFinishedStage().RemoveSubsequentStage(awaitUpdateJob, Invalid, Threading::StageBase::RemovalFlags{});
		}

		// Ensure queued commands are sent to physics before the step
		m_physicsCommandStage->FlushCommandQueue();

		const Time::Durationd updateRate = m_updateRate.GetDuration();

		// If we take larger steps than 1 / 60th of a second we need to do multiple collision steps in order to keep the simulation stable. Do
		// 1 collision step per 1 / 60th of a second.
		const double collisionStepRatio = (updateRate / Time::Durationd::FromSeconds(1.0f / 60.0f)).GetSeconds();
		const uint32 collisionStepCount = (uint32)Math::Ceil(collisionStepRatio);

		// If we want more accurate step results we can do multiple sub steps within a collision step.
		const int integrationStepCount = 1; // JPH::PhysicsUpdateContext::cMaxSubSteps

		m_physicsSystem
			.Update((float)updateRate.GetSeconds(), collisionStepCount, integrationStepCount, &m_tempAllocator, &m_plugin.m_jobSystem);
	}

	void Scene::Step()
	{
#if ENABLE_JOLT_DEBUG_RENDERER
		m_plugin.m_pDebugRenderer->Clear();

		if (m_engineScene.GetRenderData().GetActiveViews().HasElements())
		{
			JPH::DebugRendering::CameraState camera;
			Rendering::SceneView& sceneView = m_engineScene.GetRenderData().GetActiveViews()[0];
			if (Entity::CameraComponent* pCamera = sceneView.GetActiveCameraComponentSafe())
			{
				const Math::WorldTransform cameraTransform = pCamera->GetWorldTransform();
				camera.mPos = {cameraTransform.GetLocation().x, cameraTransform.GetLocation().y, cameraTransform.GetLocation().z};
				camera
					.mForward = {cameraTransform.GetForwardColumn().x, cameraTransform.GetForwardColumn().y, cameraTransform.GetForwardColumn().z};
				camera.mUp = {cameraTransform.GetUpColumn().x, cameraTransform.GetUpColumn().y, cameraTransform.GetUpColumn().z};
				camera.mFOVY = pCamera->GetFieldOfView().GetRadians();
				camera.mFarPlane = pCamera->GetFarPlane().GetMeters();
			}

			m_plugin.m_pRenderer->BeginFrame(camera, 1.f);

			JPH::BodyManager::DrawSettings drawBodySettings;
			m_physicsSystem.DrawBodies(drawBodySettings, m_plugin.m_pDebugRenderer);
			m_physicsSystem.DrawConstraints(m_plugin.m_pDebugRenderer);
			m_physicsSystem.DrawConstraintLimits(m_plugin.m_pDebugRenderer);
			m_physicsSystem.DrawConstraintReferenceFrame(m_plugin.m_pDebugRenderer);

			JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_physicsSystem.GetBodyLockInterface();
			for (const JPH::BodyID bodyId : m_bodies)
			{
				JPH::BodyLockRead lock(bodyLockInterface, bodyId);
				Assert(lock.Succeeded());
				if (LIKELY(lock.Succeeded()))
				{
					Entity::Component3D* pComponent = reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(bodyId));
					lock.ReleaseLock();

					if (Optional<BodyComponent*> pBodyComponent = pComponent->As<BodyComponent>())
					{
						pBodyComponent->DebugDraw(m_plugin.m_pDebugRenderer);
					}
				}
			}

			m_plugin.m_pDebugRenderer->Draw();
			// m_physicsSystem->DrawConstraints(mDebugRenderer);

			m_plugin.m_pRenderer->EndFrame();
		}
#endif

		Threading::UniqueLock lock(m_stepMutex);

		const Time::Timestamp currentTime = Time::Timestamp::GetCurrent();

		constexpr uint8 maximumTicksPerFrame = 16;
		const Time::Timestamp updateRateTimestamp{m_updateRate};
		// Ensure we don't get stuck catching up
		m_nextTickTime = Math::Max(m_nextTickTime, currentTime - updateRateTimestamp * maximumTicksPerFrame);

		// Optional step: Before starting the physics simulation you can optimize the broad phase. This improves collision detection
		// performance (it's pointless here because we only have 2 bodies). You should definitely not call this every frame or when e.g.
		// streaming in a new level section as it is an expensive operation. Instead insert all new objects in batches instead of 1 at a time
		// to keep the broad phase efficient. m_physicsSystem.OptimizeBroadPhase();

		Time::Timestamp tickableTime = currentTime - m_nextTickTime;
		while (tickableTime >= updateRateTimestamp)
		{
			// Step the world

			// Store state of the world before each update
			// Contains dynamic data that can change
			HistoryEntry& entry = m_history.Emplace();
			entry.timestamp = m_nextTickTime;
			m_physicsSystem.SaveState(entry.stateRecorder);
			entry.stateRecorder.Rewind();

			if (m_physicsSystem.GetNumActiveBodies() > 0)
			{
				StepInternal();
			}

			m_nextTickTime = m_nextTickTime + updateRateTimestamp;
			tickableTime = tickableTime - updateRateTimestamp;
		}

		const Time::Timestamp previousTickTime = m_nextTickTime - updateRateTimestamp;
		const Time::Timestamp remainingTimestamp = currentTime - previousTickTime;
		const Time::Durationd remainingTime = remainingTimestamp.GetDuration();

		m_physicsSystem.GetActiveBodies(m_bodies);
		JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_physicsSystem.GetBodyLockInterface();

		Entity::SceneRegistry& sceneRegistry = m_engineScene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		Entity::ComponentTypeSceneData<Physics::Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;

		JPH::Vec3 bodyLocation;
		JPH::Quat bodyRotation;
		JPH::Vec3 bodyLinearVelocity;
		JPH::Vec3 bodyAngularVelocity;
		for (const JPH::BodyID bodyId : m_bodies)
		{
			JPH::BodyLockRead bodyLock(bodyLockInterface, bodyId);
			Assert(bodyLock.Succeeded());
			if (LIKELY(bodyLock.Succeeded()))
			{
				if (bodyLock.GetBody().IsKinematic())
				{
					continue;
				}

				bodyInterfaceNoLock.GetPositionAndRotation(bodyId, bodyLocation, bodyRotation);
				bodyInterfaceNoLock.GetLinearAndAngularVelocity(bodyId, bodyLinearVelocity, bodyAngularVelocity);

				// Interpolate location and rotation based on remaining simulation time
				bodyLocation += bodyLinearVelocity * (float)remainingTime.GetSeconds();
				bodyAngularVelocity *= (float)remainingTime.GetSeconds();
				const float angularVelocityLength = bodyAngularVelocity.Length();
				if (angularVelocityLength > 1.0e-6f)
				{
					bodyRotation =
						(JPH::Quat::sRotation(bodyAngularVelocity / angularVelocityLength, angularVelocityLength) * bodyRotation).Normalized();
					JPH_ASSERT(!bodyRotation.IsNaN());
				}

				Entity::Component3D* pComponent = reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(bodyId));
				bodyLock.ReleaseLock();

				Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr
				                                                  ? pComponent->FindDataComponentOfType<Physics::Data::Body>(bodySceneData)
				                                                  : nullptr;

				if (pBodyComponent != nullptr && pComponent->HasParent())
				{
					pBodyComponent->SetWorldLocationAndRotationFromPhysics(
						*pComponent,
						Math::WorldCoordinate{bodyLocation.GetX(), bodyLocation.GetY(), bodyLocation.GetZ()},
						bodyRotation,
						worldTransformSceneData,
						localTransformSceneData,
						flagsSceneData
					);
				}
			}
		}
	}

	bool Scene::RollBackAndTick(const Time::Timestamp timestamp)
	{
		Threading::UniqueLock lock(m_stepMutex);

		const Time::Timestamp updateRateTimestamp{m_updateRate};
		const Time::Timestamp previousNextTickTime = m_nextTickTime;
		Time::Timestamp previousTickTime = m_nextTickTime - updateRateTimestamp;

		// Start at the most recent, step back until we find the desired timestamp
		const ArrayView<HistoryEntry, uint8> entries{m_history.GetView()};
		const uint8 startIndex = m_history.GetLastIndex();
		const Optional<HistoryEntry*> pLastHistoryEntry = entries[startIndex];
		Assert(pLastHistoryEntry->timestamp == previousTickTime);
		Optional<HistoryEntry*> pHistoryEntry = [timestamp, entries, startIndex]() -> Optional<HistoryEntry*>
		{
			for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
			{
				HistoryEntry* it =
					Math::Wrap(((HistoryEntry*)entries.begin() + startIndex) - i, (HistoryEntry*)entries.begin(), (HistoryEntry*)entries.end() - 1);
				HistoryEntry& entry = *it;
				if (entry.timestamp <= timestamp)
				{
					return entry;
				}
			}
			return Invalid;
		}();
		if (pHistoryEntry.IsInvalid() || pHistoryEntry->timestamp >= previousTickTime)
		{
			return false;
		}

		m_flags.Set(SceneFlags::IsRollingBack);

		// Save state before rolling back
		m_physicsSystem.SaveState(m_rollbackStateRecorder);
		m_rollbackStateRecorder.Rewind();

		// Deactivate all bodies, only simulating what's being rolled back
		JPH::BodyInterface& bodyInterface = m_physicsSystem.GetBodyInterface();
		bodyInterface.DeactivateAllBodies();

		for (const JPH::BodyIdentifier::IndexType bodyIdentifierIndex : m_rolledBackBodies.GetSetBitsIterator())
		{
			bodyInterface.ActivateBody(JPH::BodyIdentifier::MakeFromValidIndex(bodyIdentifierIndex));
		}

		m_nextTickTime = pHistoryEntry->timestamp;

		// Restore state to the initial entry
		if (!m_physicsSystem.RestoreState(pHistoryEntry->stateRecorder))
		{
			m_flags.Clear(SceneFlags::IsRollingBack);
			pHistoryEntry->stateRecorder.Rewind();

			m_physicsSystem.RestoreState(m_rollbackStateRecorder);
			m_rollbackStateRecorder.Rewind();
			return false;
		}
		pHistoryEntry->stateRecorder.Rewind();

		Assert(pLastHistoryEntry->timestamp == previousTickTime);
		for (; pHistoryEntry != pLastHistoryEntry;
		     pHistoryEntry = Math::Wrap(pHistoryEntry.Get() + 1, (HistoryEntry*)entries.begin(), (HistoryEntry*)entries.end() - 1))
		{
			if (pHistoryEntry->timestamp == m_nextTickTime)
			{
				StepInternal();
				m_nextTickTime = m_nextTickTime + updateRateTimestamp;
			}
			else
			{
				m_flags.Clear(SceneFlags::IsRollingBack);
				m_physicsSystem.RestoreState(m_rollbackStateRecorder);
				m_rollbackStateRecorder.Rewind();
				return false;
			}
		}

		// Repeat the last real step
		m_flags.Clear(SceneFlags::IsRollingBack);

		Assert(pHistoryEntry == pLastHistoryEntry);
		Assert(pHistoryEntry->timestamp == m_nextTickTime);
		Assert(previousTickTime == m_nextTickTime);
		StepInternal();

		Assert(previousNextTickTime == m_nextTickTime + updateRateTimestamp);
		m_nextTickTime = m_nextTickTime + updateRateTimestamp;

		// Store state of bodies that were rolled back
		m_physicsSystem.GetActiveBodies(m_bodies);
		m_rolledBackBodyStates.Reserve((uint32)m_bodies.size());
		Assert(m_rolledBackBodyStates.IsEmpty());

		JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_physicsSystem.GetBodyLockInterface();
		JPH::Vec3 bodyLocation;
		JPH::Quat bodyRotation;
		JPH::Vec3 bodyLinearVelocity;
		JPH::Vec3 bodyAngularVelocity;
		for (const JPH::BodyID bodyId : m_bodies)
		{
			JPH::BodyLockRead bodyLock(bodyLockInterface, bodyId);
			Assert(bodyLock.Succeeded());
			if (LIKELY(bodyLock.Succeeded()))
			{
				if (bodyLock.GetBody().IsKinematic())
				{
					continue;
				}

				bodyInterfaceNoLock.GetPositionAndRotation(bodyId, bodyLocation, bodyRotation);
				bodyInterfaceNoLock.GetLinearAndAngularVelocity(bodyId, bodyLinearVelocity, bodyAngularVelocity);

				m_rolledBackBodyStates.EmplaceBack(RolledBackState{bodyId, bodyLocation, bodyLinearVelocity, bodyAngularVelocity, bodyRotation});
			}
		}

		m_physicsSystem.RestoreState(m_rollbackStateRecorder);
		m_rollbackStateRecorder.Rewind();

		previousTickTime = m_nextTickTime - updateRateTimestamp;
		const Time::Timestamp remainingTimestamp = Time::Timestamp::GetCurrent() - previousTickTime;
		const Time::Durationd remainingTime = remainingTimestamp.GetDuration();

		Entity::SceneRegistry& sceneRegistry = m_engineScene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		Entity::ComponentTypeSceneData<Physics::Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;
		for (RolledBackState& rolledBackState : m_rolledBackBodyStates)
		{
			JPH::BodyLockWrite bodyLock(bodyLockInterface, rolledBackState.bodyIdentifier);
			Assert(bodyLock.Succeeded());
			if (LIKELY(bodyLock.Succeeded()))
			{
				bodyLocation = rolledBackState.position;
				bodyRotation = rolledBackState.rotation;
				bodyLinearVelocity = rolledBackState.velocity;
				bodyAngularVelocity = rolledBackState.angularVelocity;

				bodyInterfaceNoLock
					.SetPositionAndRotation(rolledBackState.bodyIdentifier, bodyLocation, bodyRotation, JPH::EActivation::DontActivate);
				bodyInterfaceNoLock.SetLinearAndAngularVelocity(rolledBackState.bodyIdentifier, bodyLinearVelocity, bodyAngularVelocity);

				Entity::Component3D* pComponent =
					reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(rolledBackState.bodyIdentifier));
				bodyLock.ReleaseLock();

				// Interpolate location and rotation based on remaining simulation time
				bodyLocation += bodyLinearVelocity * (float)remainingTime.GetSeconds();
				bodyAngularVelocity *= (float)remainingTime.GetSeconds();
				const float angularVelocityLength = bodyAngularVelocity.Length();
				if (angularVelocityLength > 1.0e-6f)
				{
					bodyRotation =
						(JPH::Quat::sRotation(bodyAngularVelocity / angularVelocityLength, angularVelocityLength) * bodyRotation).Normalized();
					JPH_ASSERT(!bodyRotation.IsNaN());
				}

				Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr
				                                                  ? pComponent->FindDataComponentOfType<Physics::Data::Body>(bodySceneData)
				                                                  : nullptr;

				if (LIKELY(pBodyComponent != nullptr))
				{
					Assert(pComponent->HasParent());
					if (LIKELY(pComponent->HasParent()))
					{
						pBodyComponent->SetWorldLocationAndRotationFromPhysics(
							*pComponent,
							Math::WorldCoordinate{bodyLocation.GetX(), bodyLocation.GetY(), bodyLocation.GetZ()},
							bodyRotation,
							worldTransformSceneData,
							localTransformSceneData,
							flagsSceneData
						);
					}
				}
			}
		}

		m_rolledBackBodies.Clear();
		m_rolledBackBodyStates.Clear();

		return true;
	}

	bool Scene::RollbackAndVisit(const Time::Timestamp timestamp, RollbackVisitCallback&& callback)
	{
		Threading::UniqueLock lock(m_stepMutex);

		// Start at the most recent, step back until we find the desired timestamp
		const ArrayView<HistoryEntry, uint8> entries{m_history.GetView()};
		const uint8 startIndex = m_history.GetLastIndex();
		Optional<HistoryEntry*> pHistoryEntry = [timestamp, entries, startIndex]() -> Optional<HistoryEntry*>
		{
			for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
			{
				HistoryEntry* it =
					Math::Wrap(((HistoryEntry*)entries.begin() + startIndex) - i, (HistoryEntry*)entries.begin(), (HistoryEntry*)entries.end() - 1);
				HistoryEntry& entry = *it;
				if (entry.timestamp <= timestamp)
				{
					return entry;
				}
			}
			return Invalid;
		}();
		const Time::Timestamp updateRateTimestamp{m_updateRate};
		const Time::Timestamp previousTickTime = m_nextTickTime - updateRateTimestamp;
		if (pHistoryEntry.IsInvalid() || pHistoryEntry->timestamp >= previousTickTime)
		{
			return false;
		}

		m_flags.Set(SceneFlags::IsRollingBack);

		// Save state before rolling back
		m_physicsSystem.SaveState(m_rollbackStateRecorder);
		m_rollbackStateRecorder.Rewind();

		if (m_physicsSystem.RestoreState(pHistoryEntry->stateRecorder))
		{
			if (callback())
			{
				// Callback changed body state, save the new variant
				pHistoryEntry->stateRecorder.Rewind();
				m_physicsSystem.SaveState(pHistoryEntry->stateRecorder);
			}

			pHistoryEntry->stateRecorder.Rewind();

			m_physicsSystem.RestoreState(m_rollbackStateRecorder);
			m_rollbackStateRecorder.Rewind();
			m_flags.Clear(SceneFlags::IsRollingBack);
			return true;
		}
		else
		{
			pHistoryEntry->stateRecorder.Rewind();
			m_flags.Clear(SceneFlags::IsRollingBack);
		}
		return false;
	}

	JPH::ValidateResult Scene::OnContactValidate(
		[[maybe_unused]] const JPH::Body& inBody1,
		[[maybe_unused]] const JPH::Body& inBody2,
		[[maybe_unused]] const JPH::CollideShapeResult& inCollisionResult
	)
	{
		return JPH::ValidateResult::AcceptAllContactsForThisBodyPair;
	}

	void Scene::OnContactAdded(
		const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings
	)
	{
		Entity::SceneRegistry& sceneRegistry = m_engineScene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();

		Optional<Entity::Component3D*> pComponent1 = reinterpret_cast<Entity::Component3D*>(inBody1.GetUserData());
		const Entity::ComponentIdentifier component1Identifier = pComponent1.IsValid() ? pComponent1->GetIdentifier()
		                                                                               : Entity::ComponentIdentifier{};
		const EnumFlags<Entity::ComponentFlags> component1Flags =
			pComponent1.IsValid() ? (EnumFlags<Entity::ComponentFlags>)flagsSceneData.GetComponentImplementationUnchecked(component1Identifier)
														: EnumFlags<Entity::ComponentFlags>{};

		if (pComponent1.IsValid() && component1Flags.AreAnySet(Entity::ComponentFlags::IsDestroying | Entity::ComponentFlags::IsDisabledFromAnySource))
		{
			pComponent1 = Invalid;
		}

		Optional<Entity::Component3D*> pComponent2 = reinterpret_cast<Entity::Component3D*>(inBody2.GetUserData());
		const Entity::ComponentIdentifier component2Identifier = pComponent2.IsValid() ? pComponent2->GetIdentifier()
		                                                                               : Entity::ComponentIdentifier{};
		const EnumFlags<Entity::ComponentFlags> component2Flags =
			pComponent2.IsValid() ? (EnumFlags<Entity::ComponentFlags>)flagsSceneData.GetComponentImplementationUnchecked(component2Identifier)
														: EnumFlags<Entity::ComponentFlags>{};
		if (pComponent2.IsValid() && component2Flags.AreAnySet(Entity::ComponentFlags::IsDestroying | Entity::ComponentFlags::IsDisabledFromAnySource))
		{
			pComponent2 = Invalid;
		}

		Entity::ComponentTypeSceneData<Physics::Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;
		Optional<Data::Body*> pBody1 = pComponent1.IsValid() ? pComponent1->FindDataComponentOfType<Data::Body>(bodySceneData) : nullptr;
		Optional<Data::Body*> pBody2 = pComponent2.IsValid() ? pComponent2->FindDataComponentOfType<Data::Body>(bodySceneData) : nullptr;

		Optional<Physics::ColliderComponent*> pCollider1;
		if (pBody1.IsValid())
		{
			const uint64 colliderUserData = inBody1.GetShape()->GetSubShapeUserData(inManifold.mSubShapeID1);
			if (colliderUserData != 0)
			{
				pCollider1 = pBody1->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
			}
		}

		Optional<Physics::ColliderComponent*> pCollider2;
		if (pBody2.IsValid())
		{
			const uint64 colliderUserData = inBody2.GetShape()->GetSubShapeUserData(inManifold.mSubShapeID2);
			if (colliderUserData != 0)
			{
				pCollider2 = pBody2->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
			}
		}

		if (pBody1 != nullptr)
		{
			static_assert(sizeof(Math::WorldCoordinate) == sizeof(JPH::Vec3));
			const Contact contact{
				pComponent1,
				pBody1,
				pCollider1,
				ArrayView<const Math::WorldCoordinate, uint8>{
					reinterpret_cast<const Math::WorldCoordinate*>(inManifold.mWorldSpaceContactPointsOn1.data()),
					(uint8)inManifold.mWorldSpaceContactPointsOn1.size()
				},
				pComponent2,
				pBody2,
				pCollider2,
				ArrayView<const Math::WorldCoordinate, uint8>{
					reinterpret_cast<const Math::WorldCoordinate*>(inManifold.mWorldSpaceContactPointsOn2.data()),
					(uint8)inManifold.mWorldSpaceContactPointsOn2.size()
				},
				Math::Vector3f{inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ()}
			};

			pBody1->OnContactFound(contact);
		}

		if (pBody2 != nullptr)
		{
			static_assert(sizeof(Math::WorldCoordinate) == sizeof(JPH::Vec3));
			const Contact contact{
				pComponent2,
				pBody2,
				pCollider2,
				ArrayView<const Math::WorldCoordinate, uint8>{
					reinterpret_cast<const Math::WorldCoordinate*>(inManifold.mWorldSpaceContactPointsOn2.data()),
					(uint8)inManifold.mWorldSpaceContactPointsOn2.size()
				},
				pComponent1,
				pBody1,
				pCollider1,
				ArrayView<const Math::WorldCoordinate, uint8>{
					reinterpret_cast<const Math::WorldCoordinate*>(inManifold.mWorldSpaceContactPointsOn1.data()),
					(uint8)inManifold.mWorldSpaceContactPointsOn1.size()
				},
				Math::Vector3f{inManifold.mWorldSpaceNormal.GetX(), inManifold.mWorldSpaceNormal.GetY(), inManifold.mWorldSpaceNormal.GetZ()}
			};

			pBody2->OnContactFound(contact);
		}

		const Material& __restrict shape1Material = static_cast<const Material&>(*inBody1.GetShape()->GetMaterial(inManifold.mSubShapeID1));
		const Material& __restrict shape2Material = static_cast<const Material&>(*inBody2.GetShape()->GetMaterial(inManifold.mSubShapeID2));

		// Apply material friction and restitution
		const float shape1Friction = shape1Material.GetFriction();
		const float shape1Restitution = shape1Material.GetRestitution();

		const float shape2Friction = shape2Material.GetFriction();
		const float shape2Restitution = shape2Material.GetRestitution();

		ioSettings.mCombinedFriction = Math::Sqrt(shape1Friction * shape2Friction);
		ioSettings.mCombinedRestitution = Math::Max(shape1Restitution, shape2Restitution);
	}

	void Scene::OnContactPersisted(
		const JPH::Body& inBody1, const JPH::Body& inBody2, const JPH::ContactManifold& inManifold, JPH::ContactSettings& ioSettings
	)
	{
		const Material& __restrict shape1Material = static_cast<const Material&>(*inBody1.GetShape()->GetMaterial(inManifold.mSubShapeID1));
		const Material& __restrict shape2Material = static_cast<const Material&>(*inBody2.GetShape()->GetMaterial(inManifold.mSubShapeID2));

		// Apply material friction and restitution
		const float shape1Friction = shape1Material.GetFriction();
		const float shape1Restitution = shape1Material.GetRestitution();

		const float shape2Friction = shape2Material.GetFriction();
		const float shape2Restitution = shape2Material.GetRestitution();

		ioSettings.mCombinedFriction = Math::Sqrt(shape1Friction * shape2Friction);
		ioSettings.mCombinedRestitution = Math::Max(shape1Restitution, shape2Restitution);
	}

	void Scene::OnContactRemoved(const JPH::SubShapeIDPair& inSubShapePair)
	{
		Entity::SceneRegistry& sceneRegistry = m_engineScene.GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();

		const JPH::BodyID bodyID1 = inSubShapePair.GetBody1ID();
		const JPH::BodyID bodyID2 = inSubShapePair.GetBody2ID();

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockRead lock1(bodyLockInterface, bodyID1);
		JPH::BodyLockRead lock2(bodyLockInterface, bodyID2);

		const uint64 bodyUserData1 = lock1.Succeeded() ? lock1.GetBody().GetUserData() : 0ull;
		const uint64 bodyUserData2 = lock2.Succeeded() ? lock2.GetBody().GetUserData() : 0ull;

		Optional<Entity::Component3D*> pComponent1 = reinterpret_cast<Entity::Component3D*>(bodyUserData1);
		const Entity::ComponentIdentifier component1Identifier = pComponent1.IsValid() ? pComponent1->GetIdentifier()
		                                                                               : Entity::ComponentIdentifier{};
		const EnumFlags<Entity::ComponentFlags> component1Flags =
			pComponent1.IsValid() ? (EnumFlags<Entity::ComponentFlags>)flagsSceneData.GetComponentImplementationUnchecked(component1Identifier)
														: EnumFlags<Entity::ComponentFlags>{};

		if (pComponent1.IsValid() && component1Flags.AreAnySet(Entity::ComponentFlags::IsDestroying | Entity::ComponentFlags::IsDisabledFromAnySource))
		{
			pComponent1 = Invalid;
		}

		Optional<Entity::Component3D*> pComponent2 = reinterpret_cast<Entity::Component3D*>(bodyUserData2);
		const Entity::ComponentIdentifier component2Identifier = pComponent2.IsValid() ? pComponent2->GetIdentifier()
		                                                                               : Entity::ComponentIdentifier{};
		const EnumFlags<Entity::ComponentFlags> component2Flags =
			pComponent2.IsValid() ? (EnumFlags<Entity::ComponentFlags>)flagsSceneData.GetComponentImplementationUnchecked(component2Identifier)
														: EnumFlags<Entity::ComponentFlags>{};
		if (pComponent2.IsValid() && component2Flags.AreAnySet(Entity::ComponentFlags::IsDestroying | Entity::ComponentFlags::IsDisabledFromAnySource))
		{
			pComponent2 = Invalid;
		}

		Entity::ComponentTypeSceneData<Physics::Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;
		Optional<Data::Body*> pBodyComponent1 = pComponent1.IsValid() ? pComponent1->FindDataComponentOfType<Data::Body>(bodySceneData)
		                                                              : nullptr;
		Optional<Data::Body*> pBodyComponent2 = pComponent2.IsValid() ? pComponent2->FindDataComponentOfType<Data::Body>(bodySceneData)
		                                                              : nullptr;

		Optional<Physics::ColliderComponent*> pCollider1;
		if (pBodyComponent1 != nullptr && lock1.GetBody().GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound)
		{
			JPH::MutableCompoundShape& bodyCompoundShape =
				static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*lock1.GetBody().GetShape()));
			if (bodyCompoundShape.IsSubShapeIDValid(inSubShapePair.GetSubShapeID1()))
			{
				const uint64 colliderUserData = bodyCompoundShape.GetSubShapeUserData(inSubShapePair.GetSubShapeID1());
				if (colliderUserData != 0)
				{
					pCollider1 = pBodyComponent1->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
				}
			}
		}

		Optional<Physics::ColliderComponent*> pCollider2;
		if (pBodyComponent2 != nullptr && lock2.GetBody().GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound)
		{
			JPH::MutableCompoundShape& bodyCompoundShape =
				static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*lock2.GetBody().GetShape()));
			if (bodyCompoundShape.IsSubShapeIDValid(inSubShapePair.GetSubShapeID2()))
			{
				const uint64 colliderUserData = bodyCompoundShape.GetSubShapeUserData(inSubShapePair.GetSubShapeID2());
				if (colliderUserData != 0)
				{
					pCollider2 = pBodyComponent2->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
				}
			}
		}

		const Contact contact{pComponent1, pBodyComponent1, pCollider1, {}, pComponent2, pBodyComponent2, pCollider2, {}, Math::Zero};
		if (pBodyComponent1.IsValid())
		{
			pBodyComponent1->OnContactLost(contact);
		}

		if (pBodyComponent2.IsValid())
		{
			pBodyComponent2->OnContactLost(contact.GetInverted());
		}
	}

	void Scene::OnBodyActivated([[maybe_unused]] const JPH::BodyID& inBodyID, [[maybe_unused]] JPH::uint64 inBodyUserData)
	{
	}

	void Scene::OnBodyDeactivated([[maybe_unused]] const JPH::BodyID& inBodyID, [[maybe_unused]] JPH::uint64 inBodyUserData)
	{
	}

	JPH::EActivation Scene::GetDefaultBodyWakeState()
	{
		if (ShouldBodiesSleepByDefault())
		{
			return JPH::EActivation::DontActivate;
		}

		return JPH::EActivation::Activate;
	}

	void Scene::PutAllBodiesToSleep()
	{
		Threading::SharedLock lock(m_initializationMutex);
		m_physicsCommandStage->PutAllBodiesToSleep();
	}

	void Scene::WakeAllBodiesFromSleep()
	{
		Threading::SharedLock lock(m_initializationMutex);
		m_physicsCommandStage->WakeAllBodiesFromSleep();
	}

	void Scene::WakeBodiesFromSleep()
	{
		Threading::SharedLock lock(m_initializationMutex);

		auto setBitIterator = m_dynamicBodies.GetSetBitsIterator(0, m_physicsSystem.GetBodyInterface().GetMaximumUsedBodyCount());
		JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
		Entity::ComponentTypeSceneData<Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;

		for (JPH::BodyID::IndexType index : setBitIterator)
		{
			const JPH::BodyID bodyId{JPH::BodyID::MakeFromValidIndex(index)};
			const Entity::Component3D* pComponent = reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(bodyId));
			Optional<Data::Body*> pBodyComponent = pComponent ? pComponent->FindDataComponentOfType<Data::Body>(bodySceneData) : nullptr;
			if (pBodyComponent)
			{
				m_physicsCommandStage->WakeBodyFromSleep(bodyId);
			}
		}
	}

	JPH::BodyID Scene::RegisterBody()
	{
		Threading::SharedLock lock(m_initializationMutex);
		JPH::BodyInterface& bodyInterface = m_physicsSystem.GetBodyInterface();
		return bodyInterface.AcquireBodyIdentifier();
	}

	void Scene::DeregisterBody(JPH::BodyID bodyIdentifier)
	{
		JPH::BodyInterface& bodyInterface = m_physicsSystem.GetBodyInterface();
		return bodyInterface.ReturnBodyIdentifier(bodyIdentifier);
	}

	ConstraintIdentifier Scene::RegisterConstraint()
	{
		return m_constraintIdentifiers.AcquireIdentifier();
	}

	JPH::Ref<JPH::Constraint> Scene::GetConstraint(const ConstraintIdentifier identifier)
	{
		Assert(identifier.IsValid());
		return m_constraints[identifier];
	}
	struct Collector : public JPH::CastShapeCollector
	{
		Collector(JPH::PhysicsSystem& physicsSystem, Entity::ComponentTypeSceneData<Data::Body>& bodyComponentTypeSceneData)
			: m_physicsSystem(physicsSystem)
			, m_bodyComponentTypeSceneData(bodyComponentTypeSceneData)
		{
		}

		virtual void AddHit([[maybe_unused]] const JPH::ShapeCastResult& inResult) override
		{
			Optional<Physics::Data::Body*> pBody;
			Optional<Entity::Component3D*> pComponent;
			Optional<Physics::ColliderComponent*> pCollider;

			const JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_physicsSystem.GetBodyLockInterface();

			JPH::BodyLockRead lock(bodyLockInterface, inResult.mBodyID2);
			if (lock.Succeeded())
			{
				Entity::ComponentTypeSceneData<Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;
				pComponent = reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(inResult.mBodyID2));
				pBody = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>(bodySceneData) : nullptr;

				if (pBody != nullptr && lock.GetBody().GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound)
				{
					JPH::MutableCompoundShape& bodyCompoundShape =
						static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*lock.GetBody().GetShape()));
					if (bodyCompoundShape.IsSubShapeIDValid(inResult.mSubShapeID2))
					{
						const uint64 colliderUserData = bodyCompoundShape.GetSubShapeUserData(inResult.mSubShapeID2);
						if (colliderUserData != 0)
						{
							pCollider = pBody->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
						}
					}
				}
			}

			m_results.EmplaceBack(Scene::ShapeCastResult(
				inResult.mPenetrationAxis,
				inResult.mContactPointOn2,
				inResult.mPenetrationDepth,
				pBody,
				pComponent,
				pCollider
			));
		}

		JPH::PhysicsSystem& m_physicsSystem;
		Entity::ComponentTypeSceneData<Data::Body>& m_bodyComponentTypeSceneData;
		Scene::ShapeCastResults m_results;
	};

	Scene::ShapeCastResults Scene::SphereCast(
		const Math::WorldCoordinate& origin,
		const Math::Vector3f direction,
		const Math::Radiusf radius,
		const EnumFlags<BroadPhaseLayerMask> broadphaseLayers,
		const EnumFlags<LayerMask> objectLayers,
		const ArrayView<const JPH::BodyID> bodiesFilter
	)
	{
		JPH::SphereShapeSettings sphereShapeSettings{radius.GetUnits()};
		JPH::ShapeSettings::ShapeResult shapeResult = sphereShapeSettings.Create();

		return ShapeCast(origin, direction, shapeResult.Get(), broadphaseLayers, objectLayers, bodiesFilter);
	}

	[[nodiscard]] Scene::ShapeCastResults Scene::BoxCast(
		const Math::WorldCoordinate& origin,
		const Math::Vector3f direction,
		const Math::Vector3f halfExtends,
		const EnumFlags<BroadPhaseLayerMask> broadphaseLayers,
		const EnumFlags<LayerMask> objectLayers,
		const ArrayView<const JPH::BodyID> bodiesFilter
	)
	{
		JPH::BoxShapeSettings sphereShapeSettings{halfExtends};
		JPH::ShapeSettings::ShapeResult shapeResult = sphereShapeSettings.Create();

		return ShapeCast(origin, direction, shapeResult.Get(), broadphaseLayers, objectLayers, bodiesFilter);
	}

	[[nodiscard]] JPH::MultiBroadPhaseLayerFilter GetBroadphaseLayerFilter(const EnumFlags<BroadPhaseLayerMask> broadphaseLayers)
	{
		JPH::MultiBroadPhaseLayerFilter broadPhaseLayerFilter;
		for (const BroadPhaseLayerMask broadphaseLayer : broadphaseLayers)
		{
			switch (broadphaseLayer)
			{
				case BroadPhaseLayerMask::Static:
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Static)));
					break;
				case BroadPhaseLayerMask::Dynamic:
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Dynamic)));
					break;
				case BroadPhaseLayerMask::Queries:
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Queries)));
					break;
				case BroadPhaseLayerMask::Triggers:
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Triggers)));
					break;
				case BroadPhaseLayerMask::Gravity:
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Gravity)));
					break;
				case BroadPhaseLayerMask::None:
					ExpectUnreachable();
			}
		}
		return Move(broadPhaseLayerFilter);
	}

	[[nodiscard]] JPH::MultiObjectLayerFilter GetObjectLayerFilter(const EnumFlags<LayerMask> objectLayers)
	{
		JPH::MultiObjectLayerFilter objectLayerFilter;
		for (const LayerMask objectLayer : objectLayers)
		{
			switch (objectLayer)
			{
				case LayerMask::Static:
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Static));
					break;
				case LayerMask::Dynamic:
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Dynamic));
					break;
				case LayerMask::Queries:
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Queries));
					break;
				case LayerMask::Triggers:
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Triggers));
					break;
				case LayerMask::Gravity:
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Gravity));
					break;
				case LayerMask::None:
					ExpectUnreachable();
			}
		}
		return Move(objectLayerFilter);
	}

	[[nodiscard]] Scene::ShapeCastResults Scene::ShapeCast(
		const Math::WorldCoordinate& origin,
		const Math::Vector3f direction,
		JPH::Shape* pShape,
		const EnumFlags<BroadPhaseLayerMask> broadphaseLayers,
		const EnumFlags<LayerMask> objectLayers,
		const ArrayView<const JPH::BodyID> bodiesFilter
	)
	{
		Collector collector(m_physicsSystem, m_bodyComponentTypeSceneData);

		const JPH::MultiBroadPhaseLayerFilter broadPhaseLayerFilter = GetBroadphaseLayerFilter(broadphaseLayers);
		const JPH::MultiObjectLayerFilter objectLayerFilter = GetObjectLayerFilter(objectLayers);

		JPH::IgnoreMultipleBodiesFilter ignoreBodiesFilter;
		ignoreBodiesFilter.Reserve(bodiesFilter.GetSize());

		for (const JPH::BodyID& bodyID : bodiesFilter)
		{
			ignoreBodiesFilter.IgnoreBody(bodyID);
		}

		JPH::ShapeCast shapeCast(pShape, JPH::Vec3::sReplicate(1.0f), JPH::Mat44::sTranslation(origin), direction);

		JPH::ShapeCastSettings settings;
		settings.mBackFaceModeTriangles = JPH::EBackFaceMode::CollideWithBackFaces;
		settings.mBackFaceModeConvex = JPH::EBackFaceMode::CollideWithBackFaces;
		settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideWithAll;
		settings.mUseShrunkenShapeAndConvexRadius = true;
		settings.mReturnDeepestPoint = false;

		const JPH::NarrowPhaseQuery& narrowPhaseQuery = m_physicsSystem.GetNarrowPhaseQuery();
		narrowPhaseQuery.CastShape(shapeCast, settings, collector, broadPhaseLayerFilter, objectLayerFilter, ignoreBodiesFilter);

		return Move(collector.m_results);
	}

	Scene::RayCastResult Scene::RayCast(
		const Math::WorldLine worldLine,
		const EnumFlags<BroadPhaseLayerMask> broadphaseLayers,
		const EnumFlags<LayerMask> objectLayers,
		const ArrayView<const JPH::BodyID> bodiesFilter
	) const
	{
		JPH::RayCastResult result;

		JPH::RayCast ray;
		ray.mOrigin = worldLine.GetStart();
		ray.mDirection = worldLine.GetEnd() - worldLine.GetStart();

		const JPH::MultiBroadPhaseLayerFilter broadPhaseLayerFilter = GetBroadphaseLayerFilter(broadphaseLayers);
		const JPH::MultiObjectLayerFilter objectLayerFilter = GetObjectLayerFilter(objectLayers);

		JPH::IgnoreMultipleBodiesFilter ignoreBodiesFilter;
		ignoreBodiesFilter.Reserve(bodiesFilter.GetSize());

		for (const JPH::BodyID& bodyID : bodiesFilter)
		{
			ignoreBodiesFilter.IgnoreBody(bodyID);
		}

		const JPH::NarrowPhaseQuery& narrowPhaseQuery = m_physicsSystem.GetNarrowPhaseQuery();
		narrowPhaseQuery.CastRay(ray, result, broadPhaseLayerFilter, objectLayerFilter, ignoreBodiesFilter);

		Math::Vector3f contactNormal{Math::Zero};
		Optional<Entity::Component3D*> pComponent;
		Optional<Physics::Data::Body*> pBodyComponent;
		Optional<Physics::ColliderComponent*> pCollider;

		const JPH::BodyInterface& bodyInterfaceNoLock = m_physicsSystem.GetBodyInterfaceNoLock();
		const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_physicsSystem.GetBodyLockInterface();

		JPH::BodyLockRead contactBodyLock(bodyLockInterface, result.mBodyID);
		if (contactBodyLock.Succeeded())
		{
			const JPH::Body& body = contactBodyLock.GetBody();
			const JPH::Shape* pShape = body.GetShape();
			Entity::ComponentTypeSceneData<Data::Body>& bodySceneData = m_bodyComponentTypeSceneData;

			contactNormal = body.GetWorldSpaceSurfaceNormal(result.mSubShapeID2, ray.GetPointOnRay(result.mFraction));

			pComponent = reinterpret_cast<Entity::Component3D*>(bodyInterfaceNoLock.GetUserData(result.mBodyID));
			pBodyComponent = pComponent ? pComponent->FindDataComponentOfType<Physics::Data::Body>(bodySceneData) : nullptr;

			if (pBodyComponent && pShape && pShape->GetSubType() == JPH::EShapeSubType::MutableCompound)
			{
				const JPH::MutableCompoundShape& bodyCompoundShape = static_cast<const JPH::MutableCompoundShape&>(*pShape);
				if (bodyCompoundShape.IsSubShapeIDValid(result.mSubShapeID2))
				{
					const uint64 colliderUserData = bodyCompoundShape.GetSubShapeUserData(result.mSubShapeID2);
					if (colliderUserData != 0)
					{
						pCollider = pBodyComponent->GetCollider(ColliderIdentifier::MakeFromValue((uint32)colliderUserData));
					}
				}
			}
		}

		return RayCastResult(result.mFraction, worldLine, contactNormal, pBodyComponent, pComponent, pCollider);
	}

	[[maybe_unused]] const bool wasSceneDataComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Scene>>::Make());
	[[maybe_unused]] const bool wasSceneDataComponentTypeRegistered = Reflection::Registry::RegisterType<Scene>();

	namespace Internal
	{
		void StateRecorder::WriteBytes(const void* inData, size_t inNumBytes)
		{
			if (m_data.GetSize() < m_position + inNumBytes)
			{
				m_data.Resize(m_position + inNumBytes, Memory::Uninitialized);
			}
			const ArrayView<const ByteType, size> source{reinterpret_cast<const ByteType*>(inData), inNumBytes};
			const ArrayView<ByteType, size> target = m_data.GetView().GetSubViewFrom(m_position);
			target.CopyFrom(source);
			m_position += inNumBytes;
		}

		void StateRecorder::ReadBytes(void* outData, size_t inNumBytes)
		{
			const ArrayView<const ByteType, size> source = m_data.GetView().GetSubViewFrom(m_position);
			const ArrayView<ByteType, size> target{reinterpret_cast<ByteType*>(outData), inNumBytes};
			target.CopyFrom(source);
			m_position += inNumBytes;
		}

		bool StateRecorder::IsEOF() const
		{
			return m_position == m_data.GetSize();
		}

		bool StateRecorder::IsFailed() const
		{
			return false;
		}
	}
}
