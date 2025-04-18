#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/ColliderComponent.h"
#include "PhysicsCore/Components/CharacterComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Memory/Optional.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Body/Body.h>
#include <3rdparty/jolt/Physics/Body/BodyLockMulti.h>
#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/RotatedTranslatedShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/ScaledShape.h>

namespace ngine::Physics::Data
{
	PhysicsCommandStage::PhysicsCommandStage(Scene& scene)
		: Threading::Job(Threading::JobPriority::Physics)
		, m_scene(scene)
	{
		m_queuedCommands.Reserve(25000);

		for (uint8 i = 0; i < 2; ++i)
		{
			m_doubleBufferedData[i].m_wakeBodiesQueue.Reserve(25000);
			m_doubleBufferedData[i].m_sleepBodiesQueue.Reserve(25000);
		}
	}

	bool PhysicsCommandStage::ShouldQueueCommands()
	{
		// Disabled queueing logic as we ran into unordered triggering of commands
		// Problem was that a component's disable event would be queued, but then remove later would be executed immediately before the queue
		// was processed
		return !m_scene.m_engineScene.IsTemplate(); // m_physicsSimulationStage.IsExecuting();
	}

	Threading::Job::Result PhysicsCommandStage::OnExecute(Threading::JobRunnerThread&)
	{
		FlushCommandQueue();
		return Result::Finished;
	}

	void PhysicsCommandStage::FlushCommandQueue()
	{
		JPH::BodyInterface& bodyInterface = m_scene.m_physicsSystem.GetBodyInterfaceNoLock();
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();

		Threading::UniqueLock flushQueueLock(m_flushQueueMutex);

		// Process the generalized queue
		{
			Threading::UniqueLock lock(m_queuedCommandsMutex);
			Execute(m_queuedCommands, bodyInterface, bodyLockInterface);
		}

		// Process the put bodies to sleep queue
		{
			m_flags |= Flags::IsProcessingSleepQueue0;
			Threading::UniqueLock lock(m_doubleBufferedData[0].m_sleepBodiesQueueMutex);
			ProcessPutBodiesToSleep(m_doubleBufferedData[0], bodyInterface);
			[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingSleepQueue0);
			Assert(wasCleared);
		}
		{
			m_flags |= Flags::IsProcessingSleepQueue1;
			Threading::UniqueLock lock(m_doubleBufferedData[1].m_sleepBodiesQueueMutex);
			ProcessPutBodiesToSleep(m_doubleBufferedData[1], bodyInterface);
			[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingSleepQueue1);
			Assert(wasCleared);
		}
		if (m_flags.TryClearFlags(Flags::PutAllBodiesToSleep))
		{
			bodyInterface.DeactivateAllBodies();
		}

		// Process the wake bodies queue
		{
			m_flags |= Flags::IsProcessingWakeQueue0;
			Threading::UniqueLock lock(m_doubleBufferedData[0].m_wakeBodiesQueueMutex);
			ProcessWakeBodiesFromSleep(m_doubleBufferedData[0], bodyInterface);
			[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingWakeQueue0);
			Assert(wasCleared);
		}
		{
			m_flags |= Flags::IsProcessingWakeQueue1;
			Threading::UniqueLock lock(m_doubleBufferedData[1].m_wakeBodiesQueueMutex);
			ProcessWakeBodiesFromSleep(m_doubleBufferedData[1], bodyInterface);
			[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingWakeQueue1);
			Assert(wasCleared);
		}
		if (m_flags.TryClearFlags(Flags::WakeAllBodiesFromSleep))
		{
			bodyInterface.ActivateAllBodies();
		}
	}

	void PhysicsCommandStage::Execute(
		CommandQueue<Command>& queue, JPH::BodyInterface& bodyInterface, const JPH::BodyLockInterface& bodyLockInterface
	)
	{
		for (Command& __restrict command : queue)
		{
			command.Visit(
				[&bodyInterface](const CreateBodyCommand& command)
				{
					JPH::Body& body = bodyInterface.CreateBody(command.m_bodyId, command.m_creationSettings);
					body.SetUserData(command.m_pUserData);
				},
				[this, &bodyInterface](const CloneBodyCommand& command)
				{
					CloneBodyInternal(
						bodyInterface,
						command.m_bodyId,
						command.m_clonedBodyScene,
						command.m_clonedBodyId,
						command.m_pUserData,
						command.m_pTemplateUserData
					);
				},
				[&bodyInterface](const DestroyBodyCommand& command)
				{
					if (Ensure(bodyInterface.IsBodyValid(command.m_bodyId)))
					{
						FlatVector<JPH::BodyID, 1> bodyIdentifiers;
						bodyIdentifiers.EmplaceBack(command.m_bodyId);

						bodyInterface.DestroyBodies(bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
					}
				},
				[this, &bodyInterface](const AddBodyCommand& command)
				{
					// TODO: Re-batch this if multiple add body commands follow one another
					if (Ensure(bodyInterface.IsBodyValid(command.m_bodyId)))
					{
						FlatVector<JPH::BodyID, 1> bodyIdentifiers;
						bodyIdentifiers.EmplaceBack(command.m_bodyId);

						JPH::BodyInterface::AddState addState = bodyInterface.AddBodiesPrepare(bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
						bodyInterface
							.AddBodiesFinalize(bodyIdentifiers.GetData(), bodyIdentifiers.GetSize(), addState, m_scene.GetDefaultBodyWakeState());
					}
				},
				[this, &bodyLockInterface](const RemoveBodyCommand& command)
				{
					FlatVector<JPH::BodyID, 1> bodyIdentifiers;

					{
						JPH::BodyLockWrite lock(bodyLockInterface, command.m_bodyId);
						Assert(lock.Succeeded());
						if (UNLIKELY(lock.Succeeded()))
						{
							if (lock.GetBody().IsInBroadPhase())
							{
								bodyIdentifiers.EmplaceBack(command.m_bodyId);
							}
						}
					}

					if (bodyIdentifiers.GetSize() > 0)
					{
						JPH::BodyInterface& bodyInterfaceLock = m_scene.m_physicsSystem.GetBodyInterface();
						bodyInterfaceLock.RemoveBodies(bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
					}
				},
				[&bodyInterface](const SetBodyLocationCommand& command)
				{
					bodyInterface.SetPosition(command.m_bodyId, command.m_position, JPH::EActivation::DontActivate);
				},
				[&bodyInterface](const SetBodyRotationCommand& command)
				{
					bodyInterface.SetRotation(command.m_bodyId, command.m_rotation, JPH::EActivation::DontActivate);
				},
				[&bodyInterface](const SetBodyTransformCommand& command)
				{
					bodyInterface
						.SetPositionAndRotationWhenChanged(command.m_bodyId, command.m_position, command.m_rotation, JPH::EActivation::DontActivate);
				},
				[this, &bodyInterface](const MoveKinematicBodyLocationCommand& command)
				{
					bodyInterface.MoveKinematic(
						command.m_bodyId,
						command.m_position,
						Math::Max(m_scene.m_engineScene.GetCurrentFrameTime(), (float)m_scene.GetDeltaTime())

					);
				},
				[this, &bodyInterface](const MoveKinematicBodyRotationCommand& command)
				{
					bodyInterface.MoveKinematic(
						command.m_bodyId,
						command.m_rotation,
						Math::Max(m_scene.m_engineScene.GetCurrentFrameTime(), (float)m_scene.GetDeltaTime())

					);
				},
				[this, &bodyInterface](const MoveKinematicBodyTransformCommand& command)
				{
					bodyInterface.MoveKinematic(
						command.m_bodyId,
						command.m_position,
						command.m_rotation,
						Math::Max(m_scene.m_engineScene.GetCurrentFrameTime(), (float)m_scene.GetDeltaTime())

					);
				},
				[&bodyInterface](const SetBodyVelocityCommand& command)
				{
					bodyInterface.SetLinearVelocity(command.m_bodyId, command.m_velocity);
				},
				[&bodyInterface](const SetBodyMotionTypeCommand& command)
				{
					bodyInterface.SetMotionType(command.m_bodyId, command.m_motionType, command.m_activationMode);
				},
				[&bodyInterface](const SetBodyLayerCommand& command)
				{
					bodyInterface.SetObjectLayer(command.m_bodyId, command.m_objectLayer);
				},
				[this, &bodyLockInterface](AddColliderCommand& command)
				{
					AddColliderInternal(bodyLockInterface, command.m_bodyId, Move(command.m_pShape), command.m_colliderIdentifier);
				},
				[this, &bodyLockInterface](ReplaceColliderCommand& command)
				{
					ReplaceColliderInternal(bodyLockInterface, command.m_bodyId, Move(command.m_pShape), command.m_colliderIdentifier);
				},
				[this, &bodyLockInterface](const RemoveColliderCommand& command)
				{
					RemoveColliderInternal(bodyLockInterface, command.m_bodyId, command.m_colliderIdentifier);
				},
				[this, &bodyLockInterface](SetColliderTransformCommand& command)
				{
					SetColliderTransformInternal(bodyLockInterface, command.m_bodyId, command.m_colliderIdentifier, command.m_newTransform);
				},
				[this](AddSixDegreesOfFreedomConstraintCommand& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddSwingTwistConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddPointConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddSliderConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddDistanceConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddHingeConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddFixedConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddConeConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddPathConstraint& command)
				{
					AddConstraintInternal(command.m_constraintIdentifier, command.m_bodies, Move(command.m_settings));
				},
				[this](AddVehicleConstraintCommand& command)
				{
					AddVehicleConstraintInternal(command.m_constraintIdentifier, command.m_bodyId, Move(command.m_settings));
				},
				[this](const RemoveConstraintCommand& command)
				{
					// TODO: Use the batched RemoveConstraints function
					Assert(m_scene.m_constraints[command.m_constraintIdentifier].IsValid());
					if (m_scene.m_constraints[command.m_constraintIdentifier]->GetSubType() == JPH::EConstraintSubType::Vehicle)
					{
						m_scene.m_physicsSystem.RemoveStepListener(
							static_cast<JPH::VehicleConstraint*>(m_scene.m_constraints[command.m_constraintIdentifier].GetPtr())
						);
					}
					m_scene.m_physicsSystem.RemoveConstraint(m_scene.m_constraints[command.m_constraintIdentifier]);
					m_scene.m_constraints[command.m_constraintIdentifier] = nullptr;
					m_scene.m_constraintIdentifiers.ReturnIdentifier(command.m_constraintIdentifier);
				},
				[&bodyInterface](const AddImpulseCommand& command)
				{
					bodyInterface.AddImpulse(command.m_bodyId, command.m_impulse);
				},
				[&bodyInterface](const AddImpulseAtLocationCommand& command)
				{
					bodyInterface.AddImpulse(command.m_bodyId, command.m_impulse, command.m_location);
				},
				[&bodyInterface](const AddForceCommand& command)
				{
					bodyInterface.AddForce(command.m_bodyId, command.m_impulse);
				},
				[&bodyInterface](const AddForceAtLocationCommand& command)
				{
					bodyInterface.AddForce(command.m_bodyId, command.m_impulse, command.m_location);
				},
				[&bodyInterface](const AddTorqueCommand& command)
				{
					bodyInterface.AddTorque(command.m_bodyId, command.m_impulse);
				},
				[&bodyInterface](const AddAngularImpulseCommand& command)
				{
					bodyInterface.AddAngularImpulse(command.m_bodyId, command.m_impulse);
				},
				[]()
				{
				}
			);
		}
		queue.Clear();
	}

	template<typename... Commands>
	void PhysicsCommandStage::QueueCommands(Commands&&... command)
	{
		Threading::UniqueLock lock(m_queuedCommandsMutex);
		(m_queuedCommands.EmplaceBack(Forward<Commands>(command)), ...);
	}

	void
	PhysicsCommandStage::CreateBody(const JPH::BodyID bodyIdentifier, const JPH::BodyCreationSettings& bodyCreationSettings, uint64 pUserData)
	{
		Assert(bodyIdentifier.IsValid());

		// TODO: Always queue here.
		// Need to fix scene template cache to process the physics command stage
		// The reasoning is that then we can correctly utilize the batch functions which should be faster.
		if (ShouldQueueCommands())
		{
			QueueCommands(CreateBodyCommand{bodyIdentifier, bodyCreationSettings, pUserData});
		}
		else
		{
			JPH::BodyInterface& bodyInterface = m_scene.m_physicsSystem.GetBodyInterface();
			JPH::Body& body = bodyInterface.CreateBody(bodyIdentifier, bodyCreationSettings);
			body.SetUserData(pUserData);
		}
	}

	void PhysicsCommandStage::CloneBody(
		const JPH::BodyID bodyIdentifier,
		const Scene& otherBodyScene,
		const JPH::BodyID otherBodyIdentifier,
		const uint64 pUserData,
		uint64 pTemplateUserData
	)
	{
		Assert(bodyIdentifier.IsValid());

		// TODO: Always queue here.
		// Need to fix scene template cache to process the physics command stage
		// The reasoning is that then we can correctly utilize the batch functions which should be faster.
		if (ShouldQueueCommands())
		{
			QueueCommands(CloneBodyCommand{bodyIdentifier, otherBodyScene, otherBodyIdentifier, pUserData, pTemplateUserData});
		}
		else
		{
			JPH::BodyInterface& bodyInterface = m_scene.m_physicsSystem.GetBodyInterface();
			CloneBodyInternal(bodyInterface, bodyIdentifier, otherBodyScene, otherBodyIdentifier, pUserData, pTemplateUserData);
		}
	}

	void PhysicsCommandStage::CloneBodyInternal(
		JPH::BodyInterface& bodyInterface,
		const JPH::BodyID bodyIdentifier,
		[[maybe_unused]] const Scene& otherBodyScene,
		[[maybe_unused]] const JPH::BodyID otherBodyIdentifier,
		const uint64 pUserData,
		[[maybe_unused]] const uint64 pTemplateUserData
	)
	{
		const Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(pUserData);

		if constexpr (ENABLE_ASSERTS)
		{
			Entity::SceneRegistry& otherSceneRegistry = otherBodyScene.m_engineScene.GetEntitySceneRegistry();

			[[maybe_unused]] const Optional<const Entity::Component3D*> pTemplateComponent =
				reinterpret_cast<const Entity::Component3D*>(pTemplateUserData);
			[[maybe_unused]] const Optional<Physics::Data::Body*> pTemplateBodyComponent =
				pTemplateComponent != nullptr ? pTemplateComponent->FindDataComponentOfType<Physics::Data::Body>(otherSceneRegistry) : nullptr;
			if (pTemplateBodyComponent.IsValid())
			{
				Assert(pTemplateBodyComponent->GetBodyIdentifier() == otherBodyIdentifier);
			}

			const JPH::BodyLockInterfaceNoLock& otherBodyLockInterface = otherBodyScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
			JPH::BodyLockRead otherLock(otherBodyLockInterface, otherBodyIdentifier);
			if (otherLock.Succeeded())
			{
				const JPH::Body& templateBody = otherLock.GetBody();
				Assert(templateBody.GetUserData() == pTemplateUserData);
			}
		}

		Entity::SceneRegistry& sceneRegistry = m_scene.m_engineScene.GetEntitySceneRegistry();
		Assert(!pComponent->IsDestroying(sceneRegistry));
		if (LIKELY(!pComponent->IsDestroying(sceneRegistry)))
		{
			const Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr
			                                                        ? pComponent->FindDataComponentOfType<Physics::Data::Body>(sceneRegistry)
			                                                        : nullptr;

			if (pBodyComponent.IsValid())
			{
				const Optional<Math::Massf> massOverride = pBodyComponent->GetMassOverride();

				JPH::BodyCreationSettings bodyCreationSettings = Data::Body::GetJoltBodyCreationSettings(
					pComponent->GetWorldTransform(sceneRegistry),
					Physics::Data::Body::Settings{
						pBodyComponent->GetType(),
						pBodyComponent->GetLayer(),
						pBodyComponent->GetMaximumAngularVelocity(),
						massOverride.IsValid() ? *massOverride : Math::Massf{0_kilograms},
						pBodyComponent->GetGravityScale(),
						pBodyComponent->GetFlags()
					}
				);

				JPH::MutableCompoundShapeSettings compoundShapeSettings;
				JPH::ShapeSettings::ShapeResult compoundShapeResult = compoundShapeSettings.Create();
				JPH::ShapeRefC compoundShape = compoundShapeResult.Get();
				bodyCreationSettings.SetShape(compoundShape);

				JPH::Body& body = bodyInterface.CreateBody(bodyIdentifier, bodyCreationSettings);
				body.SetUserData(pUserData);
			}
		}
	}

	void PhysicsCommandStage::AddBody(const JPH::BodyID bodyIdentifier)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddBodyCommand{bodyIdentifier});
	}

	void PhysicsCommandStage::RemoveBody(const JPH::BodyID bodyIdentifier)
	{
		Assert(bodyIdentifier.IsValid());

		// First ensure we won't invoke other commands on the body
		for (uint8 queueIndex = 0; queueIndex < 2; ++queueIndex)
		{
			DoubleBufferedData& __restrict doubleBufferedData = m_doubleBufferedData[queueIndex];

			{
				Threading::UniqueLock lock(doubleBufferedData.m_sleepBodiesQueueMutex);
				doubleBufferedData.m_sleepBodiesQueue.RemoveAllOccurrences(bodyIdentifier);
			}
			{
				Threading::UniqueLock lock(doubleBufferedData.m_wakeBodiesQueueMutex);
				doubleBufferedData.m_wakeBodiesQueue.RemoveAllOccurrences(bodyIdentifier);
			}
		}

		QueueCommands(RemoveBodyCommand{bodyIdentifier});
	}

	void PhysicsCommandStage::DestroyBody(const JPH::BodyID bodyIdentifier)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(DestroyBodyCommand{bodyIdentifier});
	}

	bool PhysicsCommandStage::IsBodyValid(const JPH::BodyID bodyIdentifier) const
	{
		JPH::BodyInterface& bodyInterfaceNoLock = m_scene.m_physicsSystem.GetBodyInterfaceNoLock();
		return bodyInterfaceNoLock.IsBodyValid(bodyIdentifier);
	}

	void PhysicsCommandStage::SetBodyLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(SetBodyLocationCommand{bodyIdentifier, position});
	}

	void PhysicsCommandStage::SetBodyRotation(const JPH::BodyID bodyIdentifier, const JPH::Quat rotation)
	{
		Assert(bodyIdentifier.IsValid());

		Assert(rotation.IsNormalized(1.0e-5f));
		QueueCommands(SetBodyRotationCommand{bodyIdentifier, rotation});
	}

	void PhysicsCommandStage::SetBodyTransform(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position, const JPH::Quat rotation)
	{
		Assert(bodyIdentifier.IsValid());

		Assert(rotation.IsNormalized(1.0e-5f));
		QueueCommands(SetBodyTransformCommand{bodyIdentifier, position, rotation});
	}

	void PhysicsCommandStage::MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(MoveKinematicBodyLocationCommand{bodyIdentifier, position});
	}

	void PhysicsCommandStage::MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Quat rotation)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(MoveKinematicBodyRotationCommand{bodyIdentifier, rotation});
	}

	void PhysicsCommandStage::MoveKinematicBody(const JPH::BodyID bodyIdentifier, const JPH::Vec3 position, const JPH::Quat rotation)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(MoveKinematicBodyTransformCommand{bodyIdentifier, position, rotation});
	}

	void PhysicsCommandStage::SetBodyVelocity(const JPH::BodyID bodyIdentifier, const JPH::Vec3 velocity)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(SetBodyVelocityCommand{bodyIdentifier, velocity});
	}

	void PhysicsCommandStage::SetBodyMotionType(
		const JPH::BodyID bodyIdentifier, const JPH::EMotionType motionType, const JPH::EActivation activationMode
	)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(SetBodyMotionTypeCommand{bodyIdentifier, motionType, activationMode});
	}

	void PhysicsCommandStage::SetBodyLayer(const JPH::BodyID bodyIdentifier, const JPH::ObjectLayer objectLayer)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(SetBodyLayerCommand{bodyIdentifier, objectLayer});
	}

	void PhysicsCommandStage::AddCollider(
		const JPH::BodyID bodyIdentifier, JPH::RefConst<JPH::Shape>&& pShape, const ColliderIdentifier colliderIdentifier
	)
	{
		Assert(bodyIdentifier.IsValid());
		Assert(colliderIdentifier.IsValid());
		Assert(pShape.IsValid());

		Assert(pShape->GetSubType() == JPH::EShapeSubType::RotatedTranslated);

		if (ShouldQueueCommands() || !IsBodyValid(bodyIdentifier))
		{
			QueueCommands(AddColliderCommand{bodyIdentifier, Forward<JPH::RefConst<JPH::Shape>>(pShape), colliderIdentifier});
		}
		else
		{
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterface();
			AddColliderInternal(bodyLockInterface, bodyIdentifier, Forward<JPH::RefConst<JPH::Shape>>(pShape), colliderIdentifier);
		}
	}

	void PhysicsCommandStage::AddColliderInternal(
		const JPH::BodyLockInterface& bodyLockInterface,
		const JPH::BodyID bodyIdentifier,
		JPH::RefConst<JPH::Shape>&& pShape,
		const ColliderIdentifier colliderIdentifier
	)
	{
		JPH::BodyLockWrite lock(bodyLockInterface, bodyIdentifier);
		Assert(lock.Succeeded());
		if (UNLIKELY(!lock.Succeeded()))
		{
			return;
		}

		JPH::Body& body = lock.GetBody();
		Assert(body.GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound);
		JPH::MutableCompoundShape& bodyCompoundShape = static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*body.GetShape()));

		const JPH::Vec3 oldCenterOfMass = bodyCompoundShape.GetCenterOfMass();

		Assert(pShape.IsValid());
		// Update relative transform
		const uint32 subShapeIndex = bodyCompoundShape.AddShape(
			JPH::Vec3::sZero(),
			JPH::Quat::sIdentity(),
			Forward<JPH::RefConst<JPH::Shape>>(pShape),
			colliderIdentifier.GetValue()
		);

		Assert(bodyCompoundShape.IsSubShapeIDValid(bodyCompoundShape.GetSubShapeIDFromIndex(subShapeIndex, JPH::SubShapeIDCreator()).GetID()));

		Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(body.GetUserData());
		Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>()
		                                                                      : nullptr;
		if (pBodyComponent != nullptr)
		{
			if (Optional<ColliderComponent*> pColliderComponent = pBodyComponent->GetCollider(colliderIdentifier))
			{
				pColliderComponent->m_subShapeIndex = subShapeIndex;
			}
		}

		bodyCompoundShape.AdjustCenterOfMass();
		bodyCompoundShape.CalculateSubShapeBounds(subShapeIndex, 1);

		m_scene.m_physicsSystem.GetBodyInterfaceNoLock().NotifyShapeChanged(
			bodyIdentifier,
			oldCenterOfMass,
			true,
			body.IsInBroadPhase() ? m_scene.GetDefaultBodyWakeState() : JPH::EActivation::DontActivate
		);

		if (pBodyComponent != nullptr)
		{
			pBodyComponent->RecalculateShapeMass(body);
		}

		if (pBodyComponent != nullptr)
		{
			UpdateBodyMotionProperties(*pBodyComponent, body);
		}
	}

	void PhysicsCommandStage::ReplaceCollider(
		const JPH::BodyID bodyIdentifier, JPH::RefConst<JPH::Shape>&& pShape, const ColliderIdentifier colliderIdentifier
	)
	{
		Assert(bodyIdentifier.IsValid());
		Assert(colliderIdentifier.IsValid());
		Assert(pShape.IsValid());
		Assert(pShape->GetSubType() == JPH::EShapeSubType::RotatedTranslated);

		if (ShouldQueueCommands() || !IsBodyValid(bodyIdentifier))
		{
			QueueCommands(ReplaceColliderCommand{bodyIdentifier, Forward<JPH::RefConst<JPH::Shape>>(pShape), colliderIdentifier});
		}
		else
		{
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterface();
			ReplaceColliderInternal(bodyLockInterface, bodyIdentifier, Forward<JPH::RefConst<JPH::Shape>>(pShape), colliderIdentifier);
		}
	}

	void PhysicsCommandStage::ReplaceColliderInternal(
		const JPH::BodyLockInterface& bodyLockInterface,
		const JPH::BodyID bodyIdentifier,
		JPH::RefConst<JPH::Shape>&& pShape,
		const ColliderIdentifier colliderIdentifier
	)
	{
		JPH::BodyLockWrite lock(bodyLockInterface, bodyIdentifier);
		Assert(lock.Succeeded());
		if (UNLIKELY(!lock.Succeeded()))
		{
			return;
		}

		JPH::Body& body = lock.GetBody();
		Assert(body.GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound);
		JPH::MutableCompoundShape& bodyCompoundShape = static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*body.GetShape()));

		const JPH::Vec3 oldCenterOfMass = bodyCompoundShape.GetCenterOfMass();

		// Remove the previous shape
		{
			Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(body.GetUserData());
			Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>()
			                                                                      : nullptr;
			const Optional<ColliderComponent*> pColliderComponent = pBodyComponent != nullptr ? pBodyComponent->GetCollider(colliderIdentifier)
			                                                                                  : nullptr;
			Assert(pColliderComponent.IsValid());
			if (LIKELY(pColliderComponent.IsValid()))
			{
				const uint32 subShapeIndex = pColliderComponent->m_subShapeIndex;

				if (subShapeIndex != bodyCompoundShape.GetNumSubShapes() - 1)
				{
					Threading::SharedLock colliderLock(pBodyComponent->m_colliderMutex);
					// Update the subshape ids of all previous shapes
					for (const Optional<ColliderComponent*> pExistingCollider : pBodyComponent->m_colliders.GetView())
					{
						if (pExistingCollider.IsValid() && pExistingCollider->m_subShapeIndex > subShapeIndex)
						{
							pExistingCollider->m_subShapeIndex--;
						}
					}
				}

				Assert(subShapeIndex < bodyCompoundShape.GetNumSubShapes());
				if (subShapeIndex < bodyCompoundShape.GetNumSubShapes())
				{
					bodyCompoundShape.RemoveShape(subShapeIndex);
				}
			}
		}

		Assert(pShape.IsValid());
		// Update relative transform
		const uint32 subShapeIndex = bodyCompoundShape.AddShape(
			JPH::Vec3::sZero(),
			JPH::Quat::sIdentity(),
			Forward<JPH::RefConst<JPH::Shape>>(pShape),
			colliderIdentifier.GetValue()
		);

		Assert(bodyCompoundShape.IsSubShapeIDValid(bodyCompoundShape.GetSubShapeIDFromIndex(subShapeIndex, JPH::SubShapeIDCreator()).GetID()));

		Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(body.GetUserData());
		Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>()
		                                                                      : nullptr;
		Assert(pBodyComponent != nullptr);
		if (pBodyComponent != nullptr)
		{
			if (Optional<ColliderComponent*> pColliderComponent = pBodyComponent->GetCollider(colliderIdentifier))
			{
				pColliderComponent->m_subShapeIndex = subShapeIndex;
			}
		}

		bodyCompoundShape.AdjustCenterOfMass();
		bodyCompoundShape.CalculateSubShapeBounds(subShapeIndex, 1);

		if (pBodyComponent != nullptr)
		{
			pBodyComponent->RecalculateShapeMass(body);
		}

		m_scene.m_physicsSystem.GetBodyInterfaceNoLock().NotifyShapeChanged(
			bodyIdentifier,
			oldCenterOfMass,
			true,
			body.IsInBroadPhase() ? m_scene.GetDefaultBodyWakeState() : JPH::EActivation::DontActivate
		);

		if (pBodyComponent != nullptr)
		{
			UpdateBodyMotionProperties(*pBodyComponent, body);
		}
	}

	void PhysicsCommandStage::SetColliderTransform(
		const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier, const Math::WorldTransform transform
	)
	{
		Assert(bodyIdentifier.IsValid());
		Assert(colliderIdentifier.IsValid());

		if (ShouldQueueCommands() || !IsBodyValid(bodyIdentifier))
		{
			QueueCommands(SetColliderTransformCommand{bodyIdentifier, colliderIdentifier, transform});
		}
		else
		{
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterface();
			SetColliderTransformInternal(bodyLockInterface, bodyIdentifier, colliderIdentifier, transform);
		}
	}

	void PhysicsCommandStage::SetColliderTransformInternal(
		const JPH::BodyLockInterface& bodyLockInterface,
		const JPH::BodyID bodyIdentifier,
		const ColliderIdentifier colliderIdentifier,
		const Math::WorldTransform transform
	)
	{
		JPH::BodyLockWrite lock(bodyLockInterface, bodyIdentifier);
		Assert(lock.Succeeded());
		if (UNLIKELY(!lock.Succeeded()))
		{
			return;
		}

		JPH::Body& body = lock.GetBody();

		Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(body.GetUserData());
		Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>()
		                                                                      : nullptr;

		Assert(body.GetShape()->GetSubType() == JPH::EShapeSubType::MutableCompound);
		JPH::MutableCompoundShape& bodyCompoundShape = static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*body.GetShape()));
		const JPH::Vec3 oldCenterOfMass = bodyCompoundShape.GetCenterOfMass();

		if (LIKELY(pBodyComponent.IsValid()))
		{
			Optional<ColliderComponent*> pColliderComponent = pBodyComponent->GetCollider(colliderIdentifier);
			if (LIKELY(pColliderComponent.IsValid()))
			{
				const uint32 subShapeIndex = pColliderComponent->m_subShapeIndex;
				Assert(subShapeIndex < bodyCompoundShape.GetNumSubShapes());
				if (LIKELY(subShapeIndex < bodyCompoundShape.GetNumSubShapes()))
				{
					JPH::Shape& shape = const_cast<JPH::Shape&>(*bodyCompoundShape.GetSubShape(subShapeIndex).mShape);

					Assert(shape.GetSubType() == JPH::EShapeSubType::RotatedTranslated);
					JPH::RotatedTranslatedShape& rotatedTranslatedShape = static_cast<JPH::RotatedTranslatedShape&>(shape);

					Assert(rotatedTranslatedShape.GetInnerShape()->GetSubType() == JPH::EShapeSubType::Scaled);
					JPH::ScaledShape& scaledShape = static_cast<JPH::ScaledShape&>(*const_cast<JPH::Shape*>(rotatedTranslatedShape.GetInnerShape()));

					const Math::Vector3f scale = transform.GetScale();
					scaledShape.SetScale({scale.x, scale.y, scale.z});

					const Math::Vector3f relativeLocation = transform.GetLocation();
					const Math::Quaternionf rotation = transform.GetRotationQuaternion();

					JPH::Quat jphQuat = {rotation.x, rotation.y, rotation.z, rotation.w};
					jphQuat = jphQuat.Normalized();

					rotatedTranslatedShape.SetPositionAndRotation({relativeLocation.x, relativeLocation.y, relativeLocation.z}, jphQuat);

					bodyCompoundShape.CalculateSubShapeBounds(subShapeIndex, 1);
				}
			}
		}

		bodyCompoundShape.AdjustCenterOfMass();

		// TODO: Investigate making NotifyShapeChanged batched
		//       NotifyBodiesAABBChanged is batchable
		//       ActivateBodies is batchable
		m_scene.m_physicsSystem.GetBodyInterfaceNoLock().NotifyShapeChanged(
			bodyIdentifier,
			oldCenterOfMass,
			true,
			body.IsInBroadPhase() ? m_scene.GetDefaultBodyWakeState() : JPH::EActivation::DontActivate
		);

		if (LIKELY(pBodyComponent != nullptr))
		{
			UpdateBodyMotionProperties(*pBodyComponent, body);
		}
	}

	void PhysicsCommandStage::RemoveCollider(const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier)
	{
		Assert(bodyIdentifier.IsValid());
		Assert(colliderIdentifier.IsValid());

		if (ShouldQueueCommands() || !IsBodyValid(bodyIdentifier))
		{
			QueueCommands(RemoveColliderCommand{bodyIdentifier, colliderIdentifier});
		}
		else
		{
			const JPH::BodyLockInterfaceLocking& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterface();
			RemoveColliderInternal(bodyLockInterface, bodyIdentifier, colliderIdentifier);
		}
	}

	void PhysicsCommandStage::RemoveColliderInternal(
		const JPH::BodyLockInterface& bodyLockInterface, const JPH::BodyID bodyIdentifier, const ColliderIdentifier colliderIdentifier
	)
	{
		JPH::BodyLockWrite lock(bodyLockInterface, bodyIdentifier);
		Assert(lock.Succeeded());
		if (UNLIKELY(!lock.Succeeded()))
		{
			return;
		}

		JPH::Body& body = lock.GetBody();
		JPH::MutableCompoundShape& bodyCompoundShape = static_cast<JPH::MutableCompoundShape&>(const_cast<JPH::Shape&>(*body.GetShape()));
		const JPH::Vec3 oldCenterOfMass = bodyCompoundShape.GetCenterOfMass();

		Optional<Entity::Component3D*> pComponent = reinterpret_cast<Entity::Component3D*>(body.GetUserData());

		Optional<Physics::Data::Body*> pBodyComponent = pComponent != nullptr ? pComponent->FindDataComponentOfType<Physics::Data::Body>()
		                                                                      : nullptr;
		Optional<ColliderComponent*> pColliderComponent = pBodyComponent != nullptr ? pBodyComponent->GetCollider(colliderIdentifier) : nullptr;
		if (pColliderComponent != nullptr)
		{
			const uint32 subShapeIndex = pColliderComponent->m_subShapeIndex;

			if (subShapeIndex != bodyCompoundShape.GetNumSubShapes() - 1)
			{
				Threading::SharedLock colliderLock(pBodyComponent->m_colliderMutex);
				// Update the subshape ids of all previous shapes
				for (const Optional<ColliderComponent*> pExistingCollider : pBodyComponent->m_colliders.GetView())
				{
					if (pExistingCollider.IsValid() && pExistingCollider->m_subShapeIndex > subShapeIndex)
					{
						pExistingCollider->m_subShapeIndex--;
					}
				}
			}

			Assert(subShapeIndex < bodyCompoundShape.GetNumSubShapes());
			if (LIKELY(subShapeIndex < bodyCompoundShape.GetNumSubShapes()))
			{
				bodyCompoundShape.RemoveShape(subShapeIndex);
			}
		}
		bodyCompoundShape.AdjustCenterOfMass();

		// TODO: Investigate making NotifyShapeChanged batched
		//       NotifyBodiesAABBChanged is batchable
		//       ActivateBodies is batchable
		m_scene.m_physicsSystem.GetBodyInterfaceNoLock().NotifyShapeChanged(
			bodyIdentifier,
			oldCenterOfMass,
			true,
			body.IsInBroadPhase() ? m_scene.GetDefaultBodyWakeState() : JPH::EActivation::DontActivate
		);

		if (pBodyComponent != nullptr)
		{
			{
				Threading::SharedLock colliderLock(pBodyComponent->m_colliderMutex);
				if (colliderIdentifier.GetFirstValidIndex() < pBodyComponent->m_colliders.GetSize())
				{
					pBodyComponent->m_colliders[colliderIdentifier.GetFirstValidIndex()] = nullptr;
				}
			}
			pBodyComponent->m_colliderIdentifiers.ReturnIdentifier(colliderIdentifier);
			UpdateBodyMotionProperties(*pBodyComponent, body);
		}
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SixDOFConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddSixDegreesOfFreedomConstraintCommand{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::SixDOFConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::SixDOFConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SixDOFConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
				if (UNLIKELY_ERROR(bodies[i] == nullptr))
				{
					return;
				}
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SwingTwistConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddSwingTwistConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::SwingTwistConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::SwingTwistConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SwingTwistConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
				Assert(bodies[i] != nullptr);
				if (bodies[i] == nullptr)
				{
					bodies[i] = &JPH::Body::sFixedToWorld;
				}
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::PointConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddPointConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::PointConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::PointConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::PointConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SliderConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddSliderConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::SliderConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::SliderConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::SliderConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::DistanceConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddDistanceConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::DistanceConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::DistanceConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::DistanceConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::HingeConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddHingeConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::HingeConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::HingeConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::HingeConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::FixedConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddFixedConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::FixedConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::FixedConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::FixedConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::ConeConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddConeConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::ConeConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::ConeConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::ConeConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddConstraint(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::PathConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddPathConstraint{
				constraintIdentifier,
				Array<JPH::BodyID, 2>{bodyIdentifiers},
				Forward<JPH::Ref<JPH::PathConstraintSettings>>(settings)
			});
		}
		else
		{
			AddConstraintInternal(constraintIdentifier, bodyIdentifiers, Forward<JPH::Ref<JPH::PathConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddConstraintInternal(
		const ConstraintIdentifier constraintIdentifier,
		const FixedArrayView<const JPH::BodyID, 2> bodyIdentifiers,
		JPH::Ref<JPH::PathConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockMultiWrite lock(bodyLockInterface, bodyIdentifiers.GetData(), bodyIdentifiers.GetSize());
		Array<JPH::Body*, 2> bodies{};

		for (uint8 i = 0; i < 2; ++i)
		{
			if (bodyIdentifiers[i].IsInvalid())
			{
				bodies[i] = &JPH::Body::sFixedToWorld;
			}
			else
			{
				bodies[i] = lock.GetBody(i);
			}
		}

		m_scene.m_constraints[constraintIdentifier] = settings->Create(*bodies[0], *bodies[1]);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
	}

	void PhysicsCommandStage::AddVehicleConstraint(
		const ConstraintIdentifier constraintIdentifier, const JPH::BodyID bodyId, JPH::Ref<JPH::VehicleConstraintSettings>&& settings
	)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(AddVehicleConstraintCommand{constraintIdentifier, bodyId, Forward<JPH::Ref<JPH::VehicleConstraintSettings>>(settings)});
		}
		else
		{
			AddVehicleConstraintInternal(constraintIdentifier, bodyId, Forward<JPH::Ref<JPH::VehicleConstraintSettings>>(settings));
		}
	}

	void PhysicsCommandStage::AddVehicleConstraintInternal(
		const ConstraintIdentifier constraintIdentifier, const JPH::BodyID bodyId, JPH::Ref<JPH::VehicleConstraintSettings>&& settings
	)
	{
		Assert(!m_scene.m_constraints[constraintIdentifier].IsValid());
		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = m_scene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, bodyId);

		m_scene.m_constraints[constraintIdentifier] = new JPH::VehicleConstraint(lock.GetBody(), *settings);
		m_scene.m_physicsSystem.AddConstraint(m_scene.m_constraints[constraintIdentifier]);
		m_scene.m_physicsSystem.AddStepListener(static_cast<JPH::VehicleConstraint*>(m_scene.m_constraints[constraintIdentifier].GetPtr()));
	}

	void PhysicsCommandStage::RemoveConstraint(const ConstraintIdentifier constraintIdentifier)
	{
		if (ShouldQueueCommands())
		{
			QueueCommands(RemoveConstraintCommand{constraintIdentifier});
		}
		else
		{
			Assert(m_scene.m_constraints[constraintIdentifier].IsValid());
			if (m_scene.m_constraints[constraintIdentifier]->GetSubType() == JPH::EConstraintSubType::Vehicle)
			{
				m_scene.m_physicsSystem.RemoveStepListener(static_cast<JPH::VehicleConstraint*>(m_scene.m_constraints[constraintIdentifier].GetPtr()
				));
			}

			m_scene.m_physicsSystem.RemoveConstraint(m_scene.m_constraints[constraintIdentifier]);
			m_scene.m_constraintIdentifiers.ReturnIdentifier(constraintIdentifier);
		}
	}

	void PhysicsCommandStage::UpdateBodyMotionProperties(Data::Body& bodyComponent, JPH::Body& body)
	{
		if (JPH::MotionProperties* pMotionProperties = body.GetMotionPropertiesUnchecked())
		{
			const EnumFlags<Body::Flags> bodyFlags = bodyComponent.GetActiveFlags(body);
			if (bodyFlags.IsSet(BodyComponent::Flags::HasOverriddenMass))
			{
				pMotionProperties->SetInverseMass(1.f / bodyComponent.GetMassOverride()->GetKilograms());
			}
			if (bodyFlags.IsSet(BodyComponent::Flags::DisableRotation))
			{
				pMotionProperties->SetInverseInertia(JPH::Vec3::sZero(), JPH::Quat::sIdentity());
			}
		}
	}

	void PhysicsCommandStage::WakeBodyFromSleep(const JPH::BodyID bodyIdentifier)
	{
		Assert(bodyIdentifier.IsValid());

		const uint8 queueIndex = m_flags.IsSet(Flags::IsProcessingWakeQueue0);

		DoubleBufferedData& __restrict doubleBufferedData = m_doubleBufferedData[queueIndex];
		Threading::UniqueLock lock(doubleBufferedData.m_wakeBodiesQueueMutex);

		doubleBufferedData.m_wakeBodiesQueue.EmplaceBack(bodyIdentifier);
	}

	void PhysicsCommandStage::WakeAllBodiesFromSleep()
	{
		m_flags |= Flags::WakeAllBodiesFromSleep;
	}

	void PhysicsCommandStage::ProcessWakeBodiesFromSleep(DoubleBufferedData& queue, JPH::BodyInterface& bodyInterface)
	{
		if (queue.m_wakeBodiesQueue.HasElements())
		{
			bodyInterface.ActivateBodies(queue.m_wakeBodiesQueue.GetData(), static_cast<int>(queue.m_wakeBodiesQueue.GetSize()));
			queue.m_wakeBodiesQueue.Clear();
		}
	}

	void PhysicsCommandStage::PutBodyToSleep(const JPH::BodyID bodyIdentifier)
	{
		Assert(bodyIdentifier.IsValid());

		const uint8 queueIndex = m_flags.IsSet(Flags::IsProcessingSleepQueue0);

		DoubleBufferedData& __restrict doubleBufferedData = m_doubleBufferedData[queueIndex];
		Threading::UniqueLock lock(doubleBufferedData.m_sleepBodiesQueueMutex);

		doubleBufferedData.m_sleepBodiesQueue.EmplaceBack(bodyIdentifier);
	}

	void PhysicsCommandStage::PutAllBodiesToSleep()
	{
		m_flags |= Flags::PutAllBodiesToSleep;
	}

	void PhysicsCommandStage::ProcessPutBodiesToSleep(DoubleBufferedData& queue, JPH::BodyInterface& bodyInterface)
	{
		if (queue.m_sleepBodiesQueue.HasElements())
		{
			bodyInterface.DeactivateBodies(queue.m_sleepBodiesQueue.GetData(), static_cast<int>(queue.m_sleepBodiesQueue.GetSize()));
			queue.m_sleepBodiesQueue.Clear();
		}
	}

	void PhysicsCommandStage::AddImpulse(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddImpulseCommand{bodyIdentifier, impulse});
	}

	void PhysicsCommandStage::AddImpulseAtLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse, const JPH::Vec3 location)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddImpulseAtLocationCommand{bodyIdentifier, impulse, location});
	}

	void PhysicsCommandStage::AddForce(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddForceCommand{bodyIdentifier, impulse});
	}

	void PhysicsCommandStage::AddForceAtLocation(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse, const JPH::Vec3 location)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddForceAtLocationCommand{bodyIdentifier, impulse, location});
	}

	void PhysicsCommandStage::AddTorque(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddTorqueCommand{bodyIdentifier, impulse});
	}

	void PhysicsCommandStage::AddAngularImpulse(const JPH::BodyID bodyIdentifier, const JPH::Vec3 impulse)
	{
		Assert(bodyIdentifier.IsValid());

		QueueCommands(AddAngularImpulseCommand{bodyIdentifier, impulse});
	}
}
