#include "PhysicsCore/Components/RigidbodyCharacter.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Plugin.h"
#include "PhysicsCore/Layer.h"
#include "PhysicsCore/Material.h"
#include "PhysicsCore/MaterialAsset.h"
#include "PhysicsCore/Contact.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <3rdparty/jolt/Physics/Collision/Shape/MutableCompoundShape.h>
#include <3rdparty/jolt/Physics/Collision/CollideShape.h>
#include <3rdparty/jolt/Physics/Collision/ShapeCast.h>
#include <3rdparty/jolt/Physics/Collision/Shape/CylinderShape.h>
#include <3rdparty/jolt/Physics/Collision/Shape/SphereShape.h>

#include <PhysicsCore/3rdparty/jolt/Physics/Constraints/SixDOFConstraint.h>

#include <NetworkingCore/Components/BoundComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/LinearInterpolate.h>

namespace ngine::Physics
{
	RigidbodyCharacter::RigidbodyCharacter(const RigidbodyCharacter& templateComponent, const Cloner& cloner)
		: CharacterBase(templateComponent, cloner)
		, m_maximumWalkableAngle(templateComponent.m_maximumWalkableAngle)
		, m_maximumGroundAngle(templateComponent.m_maximumGroundAngle)
		, m_accelerationStrength(templateComponent.m_accelerationStrength)
		, m_inAirControl(templateComponent.m_inAirControl)
		, m_stepUpHeight(templateComponent.m_stepUpHeight)
		, m_stepUpSpeed(templateComponent.m_stepUpSpeed)
		, m_stepDownSpeed(templateComponent.m_stepDownSpeed)
		, m_colliderHeightAboveGround(templateComponent.m_colliderHeightAboveGround)
	{
	}

	RigidbodyCharacter::RigidbodyCharacter(const Deserializer& deserializer)
		: CharacterBase(
				deserializer,
				Settings{
					BodyType::Dynamic,
					Layer::Dynamic,
					2700_degrees,
					80_kilograms,
					100_percent,
					BodyFlags::HasOverriddenMass | BodyFlags::DisableRotation
				}
			)
	{
	}

	RigidbodyCharacter::RigidbodyCharacter(Initializer&& initializer)
		: CharacterBase(Forward<Initializer>(initializer))
	{
	}

	RigidbodyCharacter::~RigidbodyCharacter()
	{
	}

	void RigidbodyCharacter::OnCreated()
	{
		Entity::ComponentTypeSceneData<RigidbodyCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RigidbodyCharacter>&>(*GetTypeSceneData());
		if (IsEnabled())
		{
			sceneData.EnableBeforePhysicsUpdate(*this);
			sceneData.EnableFixedPhysicsUpdate(*this);
		}

		JPH::SixDOFConstraintSettings* pSettings = new JPH::SixDOFConstraintSettings();

		// Add a strong "push up" spring motor and a pair of weaker ones for constraining character on x-y plane.
		pSettings->mMotorSettings[JPH::SixDOFConstraintSettings::EAxis::TranslationZ] = JPH::MotorSettings(10.0f, 1.0f, 1.0e5f, 0.0f);
		pSettings->mMotorSettings[JPH::SixDOFConstraintSettings::EAxis::TranslationY] = JPH::MotorSettings(2.0f, 1.0f, 1.0e5f, 0.0f);
		pSettings->mMotorSettings[JPH::SixDOFConstraintSettings::EAxis::TranslationX] = JPH::MotorSettings(2.0f, 1.0f, 1.0e5f, 0.0f);

		const Math::WorldCoordinate parentRelativeLocation = GetRelativeLocation();
		pSettings->mPosition1 = parentRelativeLocation;
		pSettings->mPosition2 = parentRelativeLocation;
		pSettings->mSpace = JPH::EConstraintSpace::LocalToBody;

		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		m_constraintIdentifier = physicsScene.RegisterConstraint();

		// Create a constraint between "world" and this body
		physicsScene.GetCommandStage().AddConstraint(m_constraintIdentifier, Array<JPH::BodyID, 2>{Invalid, GetBodyID()}, Move(pSettings));
	}

	void RigidbodyCharacter::OnDestroying()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const ConstraintIdentifier constraintIdentifier = m_constraintIdentifier;
		if (constraintIdentifier.IsValid())
		{
			physicsScene.GetCommandStage().RemoveConstraint(constraintIdentifier);
		}
	}

	void RigidbodyCharacter::OnEnable()
	{
		Entity::ComponentTypeSceneData<RigidbodyCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RigidbodyCharacter>&>(*GetTypeSceneData());
		sceneData.EnableBeforePhysicsUpdate(*this);
		sceneData.EnableFixedPhysicsUpdate(*this);
	}

	void RigidbodyCharacter::OnDisable()
	{
		Entity::ComponentTypeSceneData<RigidbodyCharacter>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<RigidbodyCharacter>&>(*GetTypeSceneData());
		sceneData.DisableBeforePhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
	}

	RigidbodyCharacter::GroundState RigidbodyCharacter::GetGroundState() const
	{
		if (const Optional<Physics::Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			const HistoryEntry entry = GetHistoryEntry(pPhysicsScene->GetTickTime());
			return entry.state.groundState;
		}
		return RigidbodyCharacter::GroundState::OnGround;
	}

	Math::Vector3f RigidbodyCharacter::GetGroundNormal() const
	{
		if (const Optional<Physics::Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			const HistoryEntry entry = GetHistoryEntry(pPhysicsScene->GetTickTime());
			Assert(entry.state.groundState == GroundState::OnGround);
			return entry.state.groundNormal;
		}
		return Math::Up;
	}

	Optional<Entity::Component3D*> RigidbodyCharacter::GetGroundComponent() const
	{
		if (const Optional<Physics::Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			const HistoryEntry entry = GetHistoryEntry(pPhysicsScene->GetTickTime());
			const JPH::BodyInterface& bodyInterface = pPhysicsScene->m_physicsSystem.GetBodyInterface();
			return reinterpret_cast<Entity::Component3D*>(bodyInterface.GetUserData(entry.state.groundBodyID));
		}
		return Invalid;
	}

	void RigidbodyCharacter::Jump(const Math::Vector3f acceleration)
	{
		AddImpulse(acceleration);
	}

	Math::Vector3f RigidbodyCharacter::GetHostPosition() const
	{
		return m_latestLocalState.m_position;
	}

	void RigidbodyCharacter::SetHostPosition(const Math::Vector3f position)
	{
		m_nextRemoteState.m_position = position;
	}

	Math::Vector3f RigidbodyCharacter::GetHostVelocity() const
	{
		return m_latestLocalState.m_velocity;
	}

	void RigidbodyCharacter::SetHostVelocity(const Math::Vector3f velocity)
	{
		m_nextRemoteState.m_velocity = velocity;
	}

	Time::Timestamp RigidbodyCharacter::GetHostStateTimestamp() const
	{
		return m_latestLocalState.m_timestamp;
	}

	void RigidbodyCharacter::SetHostStateTimestamp(const Time::Timestamp hostTimestamp)
	{
		const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>();
		if (pBoundComponent.IsInvalid())
		{
			return;
		}

		const Optional<Network::LocalClient*> pLocalClient = pBoundComponent->GetLocalClient(*this, GetSceneRegistry());
		Assert(pLocalClient.IsValid());
		if (UNLIKELY_ERROR(pLocalClient.IsInvalid()))
		{
			return;
		}

		const Time::Timestamp localTimestamp = pLocalClient->ConvertHostTimestampToLocal(hostTimestamp);
		// Assert(localTimestamp > m_hostStateTimestamp || !m_hostStateTimestamp.IsValid());

		m_nextRemoteState.m_timestamp = localTimestamp;
		Threading::UniqueLock lock(m_remoteStateMutex);
		m_queuedRemoteStates.EmplaceBack(Move(m_nextRemoteState));
	}

	RigidbodyCharacter::HistoryEntry RigidbodyCharacter::GetHistoryEntry(const Time::Timestamp timestamp) const
	{
		const ArrayView<const HistoryEntry, uint8> entries{m_history.GetView()};
		const uint8 startIndex = m_history.GetLastIndex();
		const Optional<const HistoryEntry*> pEntry = [timestamp, entries, startIndex]() -> Optional<const HistoryEntry*>
		{
			for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
			{
				const HistoryEntry* it = Math::Wrap(
					((const HistoryEntry*)entries.begin() + startIndex) - i,
					(const HistoryEntry*)entries.begin(),
					(const HistoryEntry*)entries.end() - 1
				);
				const HistoryEntry& entry = *it;
				if (entry.state.currentTimestamp <= timestamp)
				{
					return entry;
				}
			}
			return Invalid;
		}();
		if (pEntry.IsValid())
		{
			return *pEntry;
		}
		else
		{
			return HistoryEntry{m_movementRequest, m_latestState};
		}
	}

	void RigidbodyCharacter::FixedPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			if (physicsScene.IsRollingBack())
			{
				const HistoryEntry entry = GetHistoryEntry(physicsScene.GetTickTime());
				State state = entry.state;
				Assert(state.currentTimestamp.IsValid());
				PreTick(state, entry.movementRequest);
				m_latestState = state;
			}
			else
			{
				State state = m_latestState;
				state.currentTimestamp = physicsScene.GetTickTime();
				Assert(state.currentTimestamp.IsValid());
				state.gravity = body.GetGravity(
					physicsScene.m_physicsSystem,
					bodyLockInterface,
					JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Gravity)),
					static_cast<JPH::ObjectLayer>(Layer::Gravity)
				);
				state.remoteState = m_latestRemoteState;

				Assert(state.currentTimestamp.IsValid());
				DetectGroundBody(state, m_movementRequest);
				PreTick(state, m_movementRequest);
				PostTick(state.currentTimestamp, m_movementRequest);
				m_latestState = state;

				HistoryEntry& entry = m_history.Emplace();
				entry.movementRequest = m_movementRequest;
				entry.state = state;

				m_movementRequest = {};
			}
		}
	}

	void RigidbodyCharacter::PreTick(State& state, const MovementRequest& movementRequest)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const FrameTime frameTime{physicsScene.GetDeltaTime()};

			Math::Vector3f currentVelocity = body.GetLinearVelocity();
			Math::Vector3f resultingVelocity = currentVelocity;
			float combinedFriction = 0.f;
			float slideFactor = 0.f;
			float controlFactor = 1.f;
			const Math::Vector3f slideDirection = JPH::Vec3(state.groundNormal).GetNormalizedPerpendicular();
			const Math::Vector3f worldUp = -state.gravity.GetNormalizedSafe(Math::Down);

			if (state.groundState != GroundState::OnGround)
			{
				// We use same control factor both when sliding and being in-air.
				combinedFriction = 1.f;
				controlFactor = m_inAirControl;
			}

			{
				if (state.groundMaterial.IsValid() && state.groundState == GroundState::OnGround)
				{
					constexpr float feetFriction = 5.f;
					combinedFriction = Math::Sqrt(feetFriction * static_cast<const Material&>(*state.groundMaterial).GetFriction());
				}

				Math::Vector3f slideAcceleration = Math::Zero;
				Math::Vector3f acceleration = movementRequest.velocity * m_accelerationStrength * controlFactor * combinedFriction;

				if (state.groundState != GroundState::InAir)
				{
					slideFactor = state.gravity.Dot(slideDirection);
					slideAcceleration = slideDirection * slideFactor * combinedFriction;
					if (!movementRequest.velocity.IsZero() && !slideAcceleration.IsZero())
					{
						Math::Vector3f removedVel = slideAcceleration *
						                            Math::Min(0.f, acceleration.GetNormalized().Dot(slideAcceleration.GetNormalized()));
						acceleration -= removedVel;
					}
				}

				resultingVelocity += (acceleration + slideAcceleration) * frameTime;
			}

			currentVelocity =
				resultingVelocity.GetNormalizedSafe(Math::Forward) *
				Math::Min(resultingVelocity.GetLength(), Math::Max(currentVelocity.GetLength(), movementRequest.velocity.GetLength()));

			currentVelocity += movementRequest.impulse;

			if (slideFactor != 0.f)
			{
				currentVelocity += slideDirection * slideFactor * frameTime;
			}

			// Apply new velocity before determining friction and suspension.
			Assert(currentVelocity.GetLength() < body.GetMotionPropertiesUnchecked()->GetMaxLinearVelocity());
			body.SetLinearVelocity(currentVelocity);

			// Constraint character position
			JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_constraintIdentifier);
			if (constraint.IsValid())
			{
				JPH::SixDOFConstraint* pConstraint = static_cast<JPH::SixDOFConstraint*>(constraint.GetPtr());
				if (state.groundState == GroundState::OnGround && state.groundDetectionBypassTime <= 0.f)
				{
					// TODO: Check ground velocity;
					const JPH::Vec3 shapeExtent = body.GetShape()->GetLocalBounds().GetExtent();
					const JPH::Vec3 targetPosition = state.groundPosition +
					                                 (body.GetRotation() *
					                                  (JPH::Vec3(0.f, 0.f, (m_colliderHeightAboveGround * GetWorldScale().z) + shapeExtent.GetZ())));
					pConstraint->SetTargetPositionCS(targetPosition - pConstraint->GetConstraintToBody1Matrix().GetTranslation());
					pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationZ, JPH::EMotorState::Position);

					// If we are stepping up, constraint our position in x and y axis
					if (state.targetHeight > 0.f)
					{
						pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationY, JPH::EMotorState::Position);
						pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationX, JPH::EMotorState::Position);
					}
					else
					{
						pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationY, JPH::EMotorState::Off);
						pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationX, JPH::EMotorState::Off);
					}
				}
				else
				{
					pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationZ, JPH::EMotorState::Off);
					pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationY, JPH::EMotorState::Off);
					pConstraint->SetMotorState(JPH::SixDOFConstraintSettings::EAxis::TranslationX, JPH::EMotorState::Off);
				}
			}

			// When not in air, apply friction
			if (state.groundState != GroundState::InAir)
			{
				JPH::BodyLockWrite groundBodyLock(bodyLockInterface, state.groundBodyID);
				if (LIKELY(groundBodyLock.Succeeded()))
				{
					JPH::Body& groundBody = groundBodyLock.GetBody();
					const JPH::Vec3 r1PlusU = JPH::Vec3::sZero();
					const JPH::Vec3 r2 = state.groundPosition - groundBody.GetCenterOfMassPosition();
					const JPH::Vec3 forwardDirection = (body.GetRotation() * JPH::Vec3::sAxisY()).Normalized();
					const JPH::Vec3 rightDirection = (body.GetRotation() * JPH::Vec3::sAxisX()).Normalized();

					const JPH::Vec3 velocityDirection = currentVelocity.GetNormalizedSafe(body.GetRotation() * JPH::Vec3::sAxisY());

					// Calculate velocity perpendicular to ground normal
					const float normalVelocity = velocityDirection.Dot(slideDirection * worldUp.Dot(slideDirection));

					// Multiply friction by normal force acting at the body, in our case we assume just gravity.
					// More correct solution would be to use lambda used for penetration solving but we don't have access to it here.
					combinedFriction *= state.gravity.GetLength() * frameTime / body.GetMotionPropertiesUnchecked()->GetInverseMassUnchecked();

					// Calculate friction tangents.
					JPH::Vec3 tangent1 = velocityDirection.Cross(state.groundNormal);
					tangent1 = tangent1.NormalizedOr(forwardDirection);
					JPH::Vec3 tangent2 = state.groundNormal.Cross(tangent1) * Math::SignNonZero(-normalVelocity);
					tangent2 = tangent2.NormalizedOr(rightDirection);

					// Calculate and apply longitudinal and lateral friction.
					JPH::AxisConstraintPart friction1;
					friction1.CalculateConstraintProperties(frameTime, body, r1PlusU, groundBody, r2, tangent1, 0.0f);
					friction1.SolveVelocityConstraint(body, groundBody, tangent1, -combinedFriction, combinedFriction);

					JPH::AxisConstraintPart friction2;
					friction2.CalculateConstraintProperties(frameTime, body, r1PlusU, groundBody, r2, tangent2, 0.0f);
					friction2.SolveVelocityConstraint(body, groundBody, tangent2, -combinedFriction, combinedFriction);
				}
			}

#if ENABLE_CLIENT_PREDICTION
			JPH::Vec3 bodyPosition = body.GetPosition();
			JPH::Quat bodyRotation = body.GetRotation();

			// Interpolate towards host position before physics update
			if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
			{
				if (pBoundComponent->IsBound() && !pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
				{
					Math::Vector3f hostPosition = state.remoteState.m_position;
					const Time::Timestamp hostStateTimestamp = state.remoteState.m_timestamp;
					const Time::Timestamp currentTimestamp = state.currentTimestamp;

					const float hostDelayTime = LIKELY(currentTimestamp > hostStateTimestamp)
					                              ? (float)(currentTimestamp - hostStateTimestamp).GetDuration().GetSeconds()
					                              : 0.f;
					hostPosition += state.remoteState.m_velocity * hostDelayTime;

					constexpr float correctionStrength = 2.f;
					const float alpha = 1.f - Math::Min(Math::Exponential(-correctionStrength * frameTime), 1.f);

					const Math::Vector3f positionError = hostPosition - bodyPosition;
					Math::Vector3f newPosition = bodyPosition + positionError * alpha;
					if ((Math::Vector3f{bodyPosition} - hostPosition).GetLength() > 5.f)
					{
						newPosition = hostPosition;
					}
					bodyPosition = newPosition;

					JPH::BodyLockWrite newLock(bodyLockInterface, GetBodyID());
					if (LIKELY(newLock.Succeeded()))
					{
						body.SetPositionAndRotationInternal(bodyPosition, bodyRotation);
					}
				}
			}
#endif

			if (!body.IsActive())
			{
				lock.ReleaseLock();
				physicsScene.m_physicsSystem.GetBodyInterfaceNoLock().ActivateBody(GetBodyID());
			}
		}
	}

	void RigidbodyCharacter::BeforePhysicsUpdate()
	{
		// Compare local / predicted data to the host's
		/*if (m_history.HasElements())
		{
		  const Time::Timestamp lastResolvedHostStateTimestamp = m_hostStateTimestamp;
		  const ArrayView<const HistoryEntry, uint8> entries{m_history.GetView()};
		  // Start from the most recent item
		  const uint8 firstIndex = m_history.GetFirstIndex();

		  const Optional<const HistoryEntry*> pMostRecentUnresolvedHistoryEntry = [timestamp, lastResolvedHostStateTimestamp, entries,
		firstIndex]() -> Optional<const HistoryEntry*>
		  {
		    // Validate local / predicted state to what the server just notified us
		    for (uint8 i = 0, n = entries.GetSize(); i < n; ++i)
		    {
		      const HistoryEntry* it = Math::Wrap(
		        ((const HistoryEntry*)entries.begin() + firstIndex) - i,
		        (const HistoryEntry*)entries.begin(),
		        (const HistoryEntry*)entries.end() - 1
		      );
		      const HistoryEntry& entry = *it;
		      if (entry.state.currentTimestamp <= timestamp && entry.state.currentTimestamp > lastResolvedHostStateTimestamp)
		      {
		        return entry;
		      }
		    }
		    return Invalid;
		  }();
		  if (pMostRecentUnresolvedHistoryEntry.IsValid())
		  {
		    const Math::Vector3f positionError = m_hostPosition - pMostRecentUnresolvedHistoryEntry->m_position;
		    //const Math::EulerAnglesf rotationError =
		Math::Quaternionf{pMostRecentUnresolvedHistoryEntry->m_rotation}.InverseTransformRotation(m_hostPosition).GetEulerAngles(); constexpr
		float maximumPositionError = 0.01f; constexpr float maximumPositionErrorSquared = maximumPositionError * maximumPositionError; if
		(positionError.GetLengthSquared() > maximumPositionErrorSquared)
		    {
		      Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		      physicsScene.MarkBodyForRollback(GetBodyID());
		      [[maybe_unused]] const bool wasRolledBack = physicsScene.RollBackAndTick(timestamp);
		      Assert(wasRolledBack);
		    }
		  }
		}*/

		Threading::UniqueLock lock(m_remoteStateMutex);
		if (m_queuedRemoteStates.HasElements())
		{
			Vector<RemoteState> queuedRemoteStates = Move(m_queuedRemoteStates);
			lock.Unlock();

			[[maybe_unused]] Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			[[maybe_unused]] const JPH::BodyID bodyIdentifier = GetBodyID();

			Time::Timestamp oldestRemoteStateTimestamp{Time::Timestamp::FromNanoseconds(Math::NumericLimits<uint64>::Max)};
			Time::Timestamp newestRemoteStateTimestamp{Time::Timestamp::FromNanoseconds(0)};

			bool updatedStates = true;
			for (const RemoteState& remoteState : queuedRemoteStates)
			{
				if (remoteState.m_timestamp > m_lastResolvedHostStateTimestamp)
				{
					oldestRemoteStateTimestamp = Math::Min(oldestRemoteStateTimestamp, remoteState.m_timestamp);
					newestRemoteStateTimestamp = Math::Max(newestRemoteStateTimestamp, remoteState.m_timestamp);

#if ENABLE_ROLLBACK
					// Update the previous state in history with the host data
					const bool wasStateUpdated = physicsScene.RollbackAndVisit(
						remoteState.m_timestamp,
						[&remoteState, &physicsScene, bodyIdentifier]()
						{
							JPH::BodyInterface& bodyInterfaceNoLock = physicsScene.m_physicsSystem.GetBodyInterfaceNoLock();
							const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
							JPH::BodyLockWrite bodyLock(bodyLockInterface, bodyIdentifier);
							Assert(bodyLock.Succeeded());
							if (LIKELY(bodyLock.Succeeded()))
							{
								bodyInterfaceNoLock.SetPositionAndRotation(
									bodyIdentifier,
									remoteState.m_position,
									bodyLock.GetBody().GetRotation(),
									JPH::EActivation::DontActivate
								);
								bodyInterfaceNoLock.SetLinearVelocity(bodyIdentifier, remoteState.m_velocity);
							}
							// Indicate that the state was changed
							return bodyLock.Succeeded();
						}
					);
					updatedStates &= wasStateUpdated;
#endif
				}
			}

			if (updatedStates && newestRemoteStateTimestamp.IsValid())
			{
#if ENABLE_ROLLBACK
				physicsScene.MarkBodyForRollback(bodyIdentifier);

				const bool wasRolledBack = physicsScene.RollBackAndTick(oldestRemoteStateTimestamp);
				if (wasRolledBack)
				{
					m_lastResolvedHostStateTimestamp = newestRemoteStateTimestamp;
				}
#else
				m_lastResolvedHostStateTimestamp = newestRemoteStateTimestamp;
#endif
			}

			for (const RemoteState& remoteState : queuedRemoteStates)
			{
				if (remoteState.m_timestamp == newestRemoteStateTimestamp)
				{
					m_latestRemoteState = remoteState;
					break;
				}
			}
		};

		/*Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
		  State state{m_latestState};
		  PostTick(state);
		  m_latestState = state;
		}*/

		// Aways wake the body
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		Physics::Data::Body& body = *FindDataComponentOfType<Data::Body>(GetSceneRegistry());
		body.Wake(physicsScene);
	}

	void RigidbodyCharacter::DetectGroundBody(State& state, const MovementRequest& movementRequest)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const FrameTime frameTime{physicsScene.GetDeltaTime()};

		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const Math::Vector3f bodyPosition = body.GetPosition();
			const Math::Quaternionf bodyRotation = body.GetRotation();
			const Math::Vector3f bodyVelocity = body.GetLinearVelocity();

			const JPH::Vec3 shapeExtent = body.GetShape()->GetLocalBounds().GetExtent();
			lock.ReleaseLock();

			// We assume world up direction to be opposite of gravity (if there's no gravity we assume Z up)
			const JPH::Vec3 worldUp = -state.gravity.GetNormalizedSafe(Math::Down);

			if (state.groundDetectionBypassTime > 0.f)
			{
				state.groundBodyVelocity = Math::Zero;
				state.groundDetectionBypassTime -= frameTime;
				state.groundBodyID = JPH::BodyID{};
				state.targetHeight = 0.f;
			}
			else
			{
				const JPH::BodyInterface& bodyInterface = physicsScene.m_physicsSystem.GetBodyInterface();

				// TODO: Average out ground position and ground normal
				class SweepCollector : public JPH::CastShapeCollector
				{
				public:
					SweepCollector(JPH::Vec3Arg upDirection, float cosMaxSlopeAngle)
						: m_upDirection(upDirection)
						, m_cosMaxSlopeAngle(cosMaxSlopeAngle)
					{
					}

					virtual void AddHit(const JPH::ShapeCastResult& inResult) override
					{
						// Test if this collision is closer than the previous one
						if (inResult.mFraction < GetEarlyOutFraction())
						{
							JPH::Vec3 normal = -inResult.mPenetrationAxis.Normalized();
							float dot = normal.Dot(m_upDirection.Normalized());
							if (dot < 0.f)
							{
								if (dot <= m_bestDot && (m_cosMaxSlopeAngle == 0.f || dot < -m_cosMaxSlopeAngle))
								{
									// Update early out fraction to this hit
									UpdateEarlyOutFraction(inResult.mFraction);

									m_groundBodyID = inResult.mBodyID2;
									m_groundBodySubShapeID = inResult.mSubShapeID2;
									m_groundPosition = inResult.mContactPointOn2;
									m_groundNormal = normal;
									m_bestDot = dot;
								}
							}
						}
					}

					JPH::Vec3 m_upDirection;
					float m_cosMaxSlopeAngle;

					JPH::BodyID m_groundBodyID;
					JPH::SubShapeID m_groundBodySubShapeID;
					JPH::Vec3 m_groundPosition = JPH::Vec3::sZero();
					JPH::Vec3 m_groundNormal = JPH::Vec3::sZero();
					float m_bestDot = Math::NumericLimits<float>::Max;
				};

				class CollideCollector : public JPH::CollideShapeCollector
				{
				public:
					// Constructor
					explicit CollideCollector(JPH::Vec3Arg upDirection)
						: m_upDirection(upDirection)
					{
					}

					// See: CollectorType::AddHit
					virtual void AddHit(const JPH::CollideShapeResult& inResult) override
					{
						JPH::Vec3 normal = -inResult.mPenetrationAxis.Normalized();
						float dot = normal.Dot(m_upDirection);
						if (dot > m_bestDot)
						{
							m_groundBodyID = inResult.mBodyID2;
							m_groundBodySubShapeID = inResult.mSubShapeID2;
							m_groundPosition = inResult.mContactPointOn2;
							m_groundNormal = normal;
							m_bestDot = dot;
							m_hit = true;
						}
					}

					JPH::BodyID m_groundBodyID;
					JPH::SubShapeID m_groundBodySubShapeID;
					JPH::Vec3 m_groundPosition = JPH::Vec3::sZero();
					JPH::Vec3 m_groundNormal = JPH::Vec3::sZero();
					bool m_hit = false;
				private:
					float m_bestDot = Math::NumericLimits<float>::Min;
					JPH::Vec3 m_upDirection;
				};

				auto checkSweep = [&physicsScene,
				                   bodyID = body.GetID(),
				                   layer = body.GetObjectLayer(
													 )](const JPH::Shape& shape, SweepCollector& collector, const JPH::Vec3& origin, const JPH::Vec3& direction)
				{
					JPH::MultiBroadPhaseLayerFilter broadPhaseLayerFilter;
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Static)));
					broadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Dynamic)));

					JPH::MultiObjectLayerFilter objectLayerFilter;
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Static));
					objectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Dynamic));

					// Ignore ourselves
					JPH::IgnoreSingleBodyFilter bodyFilter(bodyID);

					JPH::ShapeCast shapeCast(&shape, JPH::Vec3::sReplicate(1.0f), JPH::Mat44::sTranslation(origin), direction);

					JPH::ShapeCastSettings settings;
					settings.mBackFaceModeTriangles = JPH::EBackFaceMode::IgnoreBackFaces;
					settings.mBackFaceModeConvex = JPH::EBackFaceMode::IgnoreBackFaces;
					settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;
					settings.mUseShrunkenShapeAndConvexRadius = true;
					settings.mReturnDeepestPoint = false;

					const JPH::NarrowPhaseQuery& narrowPhaseQuery = physicsScene.m_physicsSystem.GetNarrowPhaseQuery();
					narrowPhaseQuery.CastShape(shapeCast, settings, collector, broadPhaseLayerFilter, objectLayerFilter, bodyFilter);
				};

				auto checkCollide = [&physicsScene, bodyID = body.GetID(), layer = body.GetObjectLayer()](
															const JPH::Shape& shape,
															const JPH::Mat44& queryTransform,
															CollideCollector& collector,
															const JPH::MultiBroadPhaseLayerFilter& broadPhaseLayerFilter,
															const JPH::MultiObjectLayerFilter& objectLayerFilter
														)
				{
					// Ignore ourselves
					JPH::IgnoreSingleBodyFilter bodyFilter(bodyID);

					JPH::CollideShapeSettings settings;
					settings.mBackFaceMode = JPH::EBackFaceMode::IgnoreBackFaces;
					settings.mActiveEdgeMode = JPH::EActiveEdgeMode::CollideOnlyWithActive;
					settings.mMaxSeparationDistance = 0.01f;

					// Use a bit shrunken down shape in order to increase tolerance for accidental collisions
					const JPH::NarrowPhaseQuery& narrowPhaseQuery = physicsScene.m_physicsSystem.GetNarrowPhaseQuery();
					narrowPhaseQuery.CollideShape(
						&shape,
						JPH::Vec3::sReplicate(1.f),
						queryTransform,
						settings,
						collector,
						broadPhaseLayerFilter,
						objectLayerFilter,
						bodyFilter
					);
				};

				// Detect ground under character, test distance is half height of the character.
				// This ensures that character will stay "on ground" even if we lift off a tiny bit.
				const float castLength = Math::Max(0.0f, shapeExtent.GetZ());
				const JPH::Vec3 castDirection = state.gravity.GetNormalizedSafe(Math::Down);
				const JPH::Vec3 castDistance = castDirection * castLength;
				// Collide shape
				SweepCollector collector(-worldUp, m_maximumGroundAngle.Cos().GetRadians());
				checkSweep(*body.GetShape(), collector, bodyPosition, castDistance);

				// Copy results
				state.groundBodyID = collector.m_groundBodyID;

				if (state.groundBodyID.IsValid())
				{
					state.groundNormal = collector.m_groundNormal;
					state.groundPosition = collector.m_groundPosition;
				}

				const float scaledStepUpHeight = m_stepUpHeight * GetWorldScale().z;
				// If character is set up with step up and it's being moved via input then check if it should step up
				if (m_stepUpHeight > 0.f && !body.GetLinearVelocity().IsNearZero() && state.groundState != GroundState::InAir)
				{
					// Check if character is currently colliding with its shape, if so check if space above it is "free"
					CollideCollector collideCollector(worldUp);

					JPH::MultiBroadPhaseLayerFilter collideBroadPhaseLayerFilter;
					collideBroadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Static)));
					collideBroadPhaseLayerFilter.FilterLayer(JPH::BroadPhaseLayer(static_cast<uint8>(BroadPhaseLayer::Dynamic)));

					JPH::MultiObjectLayerFilter collideObjectLayerFilter;
					collideObjectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Static));
					collideObjectLayerFilter.FilterLayer(static_cast<JPH::ObjectLayer>(Layer::Dynamic));

					checkCollide(
						*body.GetShape(),
						JPH::Mat44::sTranslation(bodyPosition),
						collideCollector,
						collideBroadPhaseLayerFilter,
						collideObjectLayerFilter
					);

					if (collideCollector.m_hit)
					{
						// We check space above character that is "stepUpHeight" and we sweep entire character shape forward to see
						// if character can freely get up to that position without immediately colliding with any objects.
						const JPH::Vec3 castPosition =
							JPH::Vec3(bodyPosition.x, bodyPosition.y, state.groundPosition.z) +
							bodyRotation.TransformDirection(Math::Vector3f{0.f, 0.0f, scaledStepUpHeight + shapeExtent.GetZ()});
						const Math::Vector3f stepUpCastDirection = bodyRotation.TransformDirection(Math::Vector3f{0.f, shapeExtent.GetX() * 2.f, 0.f});

						SweepCollector stepUpCollector(stepUpCastDirection.GetNormalized(), 0.f);
						checkSweep(*body.GetShape(), stepUpCollector, castPosition, stepUpCastDirection);

						// If space above character is free, we move upwards
						if (stepUpCollector.m_groundBodyID.IsInvalid())
						{
							if (state.groundBodyID.IsInvalid())
							{
								// In case we wouldn't have a valid ground body, use the one that comes from the collision check.
								state.groundBodyID = collideCollector.m_groundBodyID;
							}

							// We assume ground normal to be world up in order to avoid any unwanted collision solving and velocity changes
							state.groundNormal = worldUp;

							// We use ground position as a "target" position
							state.groundPosition = state.groundPosition + bodyVelocity * frameTime +
							                       bodyRotation.TransformDirection(Math::Vector3f{0.f, shapeExtent.GetX() * 2.f, state.targetHeight});
							state.targetHeight += scaledStepUpHeight * m_stepUpSpeed * frameTime;
						}
					}
					else
					{
						state.targetHeight -= scaledStepUpHeight * m_stepDownSpeed * frameTime;
					}
				}
				else
				{
					state.targetHeight -= scaledStepUpHeight * m_stepDownSpeed * frameTime;
				}

				if (state.groundBodyID.IsValid())
				{
					state.groundMaterial = bodyInterface.GetMaterial(collector.m_groundBodyID, collector.m_groundBodySubShapeID);
					state.groundBodyVelocity = bodyInterface.GetLinearVelocity(collector.m_groundBodyID);

					// Get angular velocity
					const Math::Vector3f groundAngularVelocity = bodyInterface.GetAngularVelocity(collector.m_groundBodyID);
					if (!groundAngularVelocity.IsZero())
					{
						const float groundAngularVelocityLength = groundAngularVelocity.GetLength();
						JPH::Quat rotation =
							JPH::Quat::sRotation(groundAngularVelocity / groundAngularVelocityLength, groundAngularVelocityLength * frameTime);

						JPH::Vec3 centerOfMass = bodyInterface.GetCenterOfMassPosition(collector.m_groundBodyID);
						JPH::Vec3 newPosition = centerOfMass + rotation * (bodyPosition - centerOfMass);

						// Calculate the extra velocity
						state.groundBodyVelocity += (newPosition - bodyPosition) / frameTime;
					}
				}
				else
				{
					state.targetHeight = 0.f;
				}

				state.targetHeight = Math::Clamp(state.targetHeight, 0.f, scaledStepUpHeight);
			}

			if (state.groundBodyID.IsInvalid())
			{
				state.groundState = GroundState::InAir;
			}
			else
			{
				const float groundDot = state.groundNormal.Dot(worldUp);
				if (groundDot > m_maximumWalkableAngle.Cos().GetRadians())
				{
					state.groundState = GroundState::OnGround;
				}
				else
				{
					state.groundState = GroundState::Sliding;
				}
			}
		}

		if (!movementRequest.impulse.IsZero())
		{
			const Math::Vector3f worldUp = -state.gravity.GetNormalizedSafe(Math::Down);
			const float impulseDot = movementRequest.impulse.Dot(worldUp);
			if (impulseDot > 0.f)
			{
				// If we get an impulse that would send us flying, bypass ground detection for 200ms.
				state.groundDetectionBypassTime = 0.2f;
				state.groundState = GroundState::InAir;
			}
		}
	}

	void RigidbodyCharacter::PostTick(const Time::Timestamp currentTime, const MovementRequest& movementRequest)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();

		const JPH::BodyLockInterfaceLocking& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterface();
		JPH::BodyLockWrite lock(bodyLockInterface, GetBodyID());
		if (LIKELY(lock.Succeeded()))
		{
			JPH::Body& body = lock.GetBody();

			const Math::Vector3f bodyPosition = body.GetPosition();
			const Math::Vector3f bodyVelocity = body.GetLinearVelocity();

			lock.ReleaseLock();

			if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
			{
				if (pBoundComponent->HasAuthority(*this, GetSceneRegistry()))
				{
					const bool changedHostPosition = !bodyPosition.IsEquivalentTo(m_latestLocalState.m_position, 0.005f) ||
					                                 (bodyPosition.IsZeroExact() != Math::Vector3f{m_latestLocalState.m_position}.IsZeroExact());
					const bool changedHostVelocity = !bodyVelocity.IsEquivalentTo(m_latestLocalState.m_velocity, 0.005f) ||
					                                 (bodyVelocity.IsZeroExact() != Math::Vector3f{m_latestLocalState.m_velocity}.IsZeroExact());

					m_latestLocalState = RemoteState{bodyPosition, bodyVelocity, currentTime};

					if (changedHostPosition || changedHostVelocity)
					{
						pBoundComponent->InvalidateProperties<
							&RigidbodyCharacter::GetHostPosition,
							&RigidbodyCharacter::GetHostVelocity,
							&RigidbodyCharacter::GetHostStateTimestamp>(*this, GetSceneRegistry());
					}

					// Ensure immediate send of properties when immediate impulses (such as jumps) are applied
					if (!movementRequest.impulse.IsZero())
					{
						pBoundComponent->FlushProperties<
							&RigidbodyCharacter::GetHostPosition,
							&RigidbodyCharacter::GetHostVelocity,
							&RigidbodyCharacter::GetHostStateTimestamp>(*this, GetSceneRegistry());
					}
				}
			}
		}
	}

	[[maybe_unused]] const bool wasRigidbodyCharacterRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RigidbodyCharacter>>::Make());
	[[maybe_unused]] const bool wasRigidbodyCharacterTypeRegistered = Reflection::Registry::RegisterType<RigidbodyCharacter>();
}
