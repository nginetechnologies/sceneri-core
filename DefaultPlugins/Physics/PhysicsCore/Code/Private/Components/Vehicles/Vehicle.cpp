#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>

#include "PhysicsCore/Components/Vehicles/Vehicle.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "Plugin.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Engine/Entity/Data/Flags.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#if ENABLE_JOLT_DEBUG_RENDERER
#include "TestFramework/TestFramework.h"
#include "TestFramework/Renderer/DebugRendererImp.h"
#endif

#include <PhysicsCore/Components/Vehicles/Wheel.h>
#include <PhysicsCore/Components/Vehicles/Axle.h>
#include <PhysicsCore/Components/Vehicles/Engine.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

namespace ngine::Physics
{
	Vehicle::Vehicle(Initializer&& initializer)
		: BodyComponent(BodyComponent::Initializer{
				Component3D::Initializer(initializer),
				BodyComponent::Settings(
					Type::Dynamic,
					Layer::Dynamic,
					BodyComponent::Settings().m_maximumAngularVelocity,
					BodyComponent::Settings().m_overriddenMass,
					BodyComponent::Settings().m_gravityScale,
					BodyComponent::Settings().m_flags
				)
			})
	{
	}

	Vehicle::Vehicle(const Vehicle& templateComponent, const Cloner& cloner)
		: BodyComponent(templateComponent, cloner)
	{
	}
	Vehicle::Vehicle(const Deserializer& deserializer)
		: BodyComponent(deserializer)
	{
	}
	Vehicle::~Vehicle()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		if (m_vehicleConstraintIdentifier.IsValid())
		{
			physicsScene.GetCommandStage().RemoveConstraint(m_vehicleConstraintIdentifier);
		}
	}

	void Vehicle::OnCreated()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		m_vehicleConstraintIdentifier = physicsScene.RegisterConstraint();

		// TODO: Use some sane values
		m_collisionTester = new JPH::VehicleCollisionTesterCastSphere(static_cast<uint8>(Layer::Dynamic), 0.1f, Math::Vector3f(Math::Up));

		// Create vehicle constraint
		JPH::VehicleConstraintSettings* pVehicle = new JPH::VehicleConstraintSettings();
		pVehicle->mDrawConstraintSize = 0.1f;
		pVehicle->mUp = Math::Vector3f(Math::Up);
		pVehicle->mForward = Math::Vector3f(Math::Forward);

		JPH::WheeledVehicleControllerSettings* controller = new JPH::WheeledVehicleControllerSettings;
		controller->mEngine.mNormalizedTorque.Clear();
		controller->mEngine.mNormalizedTorque.AddPoint(0.f, 0.5f);
		controller->mEngine.mNormalizedTorque.AddPoint(0.65f, 1.0f);
		controller->mEngine.mNormalizedTorque.AddPoint(1.0f, 0.7f);
		pVehicle->mController = controller;

		physicsScene.GetCommandStage().AddVehicleConstraint(m_vehicleConstraintIdentifier, GetBodyID(), Move(pVehicle));

		Entity::ComponentTypeSceneData<Vehicle>& sceneData = static_cast<Entity::ComponentTypeSceneData<Vehicle>&>(*GetTypeSceneData());
		if (IsEnabled() && IsSimulationActive())
		{
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void Vehicle::OnEnable()
	{
		if (IsSimulationActive())
		{
			Entity::ComponentTypeSceneData<Vehicle>& sceneData = static_cast<Entity::ComponentTypeSceneData<Vehicle>&>(*GetTypeSceneData());
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void Vehicle::OnDisable()
	{
		if (IsSimulationActive())
		{
			Entity::ComponentTypeSceneData<Vehicle>& sceneData = static_cast<Entity::ComponentTypeSceneData<Vehicle>&>(*GetTypeSceneData());
			sceneData.DisableFixedPhysicsUpdate(*this);
			sceneData.DisableAfterPhysicsUpdate(*this);
		}
	}

	void Vehicle::OnSimulationResumed()
	{
		if (IsEnabled())
		{
			Entity::ComponentTypeSceneData<Vehicle>& sceneData = static_cast<Entity::ComponentTypeSceneData<Vehicle>&>(*GetTypeSceneData());
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);
		}
	}

	void Vehicle::OnSimulationPaused()
	{
		if (IsEnabled())
		{
			Entity::ComponentTypeSceneData<Vehicle>& sceneData = static_cast<Entity::ComponentTypeSceneData<Vehicle>&>(*GetTypeSceneData());
			sceneData.DisableFixedPhysicsUpdate(*this);
			sceneData.DisableAfterPhysicsUpdate(*this);
		}
	}

	const Math::Vector3f Vehicle::GetVelocity() const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		Data::Body& bodyComponent = *FindDataComponentOfType<Data::Body>();
		Data::BodyLockRead readLock = bodyComponent.LockRead(physicsScene);
		return bodyComponent.GetVelocity(physicsScene);
	}

	void Vehicle::SetMaximumPitchRollAngle(const Math::Anglef value)
	{
		m_maximumPitchRollAngle = value;
	}

	void Vehicle::AddAxle(Axle& axle)
	{
		const uint16 axleIndex = m_axles.GetNextAvailableIndex();
		m_axles.EmplaceBack(axle);
		m_queuedAxleAdditions.EmplaceBack(axle);
		axle.m_vehicleAxleIndex = axleIndex;
	}

	void Vehicle::AddAxleInternal(JPH::VehicleConstraint& vehicleConstraint, Axle& axle)
	{
		Assert(axle.m_vehicleAxleIndex != Axle::InvalidAxleIndex);

		JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(vehicleConstraint.GetController());
		Assert(pVehicleController != nullptr);

		JPH::VehicleAntiRollBar antiRollBarSettings;
		antiRollBarSettings.mStiffness = axle.GetAntiRollBarStiffness().GetNewtonMeters();

		JPH::VehicleDifferentialSettings differentialSettings;
		differentialSettings.mDifferentialRatio = axle.GetDifferentialRatio();
		differentialSettings.mEngineTorqueRatio = axle.GetEnginePowerRatio();
		differentialSettings.mLeftRightSplit = axle.GetLeftRightPowerRatio();

		if (Optional<Wheel*> leftWheel = axle.GetLeftWheel())
		{
			differentialSettings.mLeftWheel = leftWheel->m_wheelIndex;
			antiRollBarSettings.mLeftWheel = leftWheel->m_wheelIndex;
		}
		if (Optional<Wheel*> rightWheel = axle.GetRightWheel())
		{
			differentialSettings.mRightWheel = rightWheel->m_wheelIndex;
			antiRollBarSettings.mRightWheel = rightWheel->m_wheelIndex;
		}

		Assert(pVehicleController->GetDifferentials().size() == vehicleConstraint.GetAntiRollBars().size());
		Assert(axle.m_vehicleAxleIndex == static_cast<uint16>(pVehicleController->GetDifferentials().size()));

		pVehicleController->GetDifferentials().push_back(differentialSettings);
		vehicleConstraint.GetAntiRollBars().push_back(antiRollBarSettings);
	}

	void Vehicle::RemoveAxle(const uint16 axleIndex)
	{
		m_queuedAxleRemovals.EmplaceBack(axleIndex);
	}

	void Vehicle::RemoveAxleInternal(JPH::VehicleConstraint& vehicleConstraint, const uint16 axleIndex)
	{
		JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(vehicleConstraint.GetController());
		Assert(pVehicleController->GetDifferentials().size() == vehicleConstraint.GetAntiRollBars().size());
		Assert(pVehicleController->GetDifferentials().size() == m_axles.GetSize());

		const uint16 previousAxleCount = (uint16)m_axles.GetSize();
		Assert(axleIndex < previousAxleCount);

		for (uint16 otherAxleIndex = axleIndex + 1; otherAxleIndex < previousAxleCount; ++otherAxleIndex)
		{
			Axle& otherAxle = m_axles[otherAxleIndex];
			otherAxle.m_vehicleAxleIndex--;
		}

		m_axles.Remove(m_axles.begin() + axleIndex);
		pVehicleController->GetDifferentials().erase(pVehicleController->GetDifferentials().begin() + axleIndex);
		vehicleConstraint.GetAntiRollBars().erase(vehicleConstraint.GetAntiRollBars().begin() + axleIndex);
	}

	void Vehicle::AddWheel(Wheel& wheel, const Optional<Axle*> pAxle)
	{
		const uint16 wheelIndex = m_wheels.GetNextAvailableIndex();
		m_wheels.EmplaceBack(wheel);
		m_queuedWheelAdditions.EmplaceBack(QueuedWheel{wheel, pAxle});
		wheel.m_wheelIndex = wheelIndex;
	}

	void Vehicle::AddWheelInternal(JPH::VehicleConstraint& vehicleConstraint, Wheel& wheel, const Optional<Axle*> pAxle)
	{
		Assert(wheel.m_pVehicle.IsInvalid(), "A wheel can't be attached to two vehicles at the same time!");
		Assert(wheel.m_wheelIndex != Wheel::InvalidWheelIndex);

		const Math::WorldTransform wheelTransform = GetWorldTransform().GetTransformRelativeTo(wheel.GetWorldTransform());

		JPH::WheelSettingsWV* pWheelSettings = new JPH::WheelSettingsWV;
		pWheelSettings->mPosition = wheelTransform.GetLocation();
		pWheelSettings->mDirection = -wheelTransform.GetUpColumn();
		pWheelSettings->mDirection = pWheelSettings->mDirection.Normalized();
		pWheelSettings->mRadius = wheel.GetRadius().GetMeters();
		pWheelSettings->mWidth = wheel.GetWidth().GetMeters();
		pWheelSettings->mSuspensionMinLength = wheel.GetSuspensionMinimumLength().GetMeters();
		pWheelSettings->mSuspensionMaxLength = wheel.GetSuspensionMaximumLength().GetMeters();
		pWheelSettings->mMaxSteerAngle = wheel.GetMaximumSteeringAngle().GetRadians();
		pWheelSettings->mMaxHandBrakeTorque = wheel.GetMaximumHandbrakeTorque().GetNewtonMeters();
		pWheelSettings->mSuspensionDamping = wheel.GetSuspensionDamping();
		pWheelSettings->mSuspensionFrequency = wheel.GetSuspensionStiffness();

		// TODO: Move to wheel?
		pWheelSettings->mInertia = 0.9f;
		pWheelSettings->mAngularDamping = 0.2f;

		// TODO: Inject in the force application instead
		const float sidewaysFriction = wheel.GetSidewaysGrip();
		const float forwardFriction = wheel.GetForwardGrip();

		// TODO: Expose per wheel?
		// TODO: Turn into property once we have line curve UI
		pWheelSettings->mLateralFriction.Clear();
		pWheelSettings->mLateralFriction.AddPoint(0.f, 0.0f * sidewaysFriction);
		pWheelSettings->mLateralFriction.AddPoint(3.f, 1.1f * sidewaysFriction);
		pWheelSettings->mLateralFriction.AddPoint(20.f, 1.0f * sidewaysFriction);
		pWheelSettings->mLateralFriction.AddPoint(90.f, 0.6f * sidewaysFriction);

		pWheelSettings->mLongitudinalFriction.Clear();
		pWheelSettings->mLongitudinalFriction.AddPoint(-1.0f, 0.9f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(-0.2f, 1.0f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(-0.04f, 1.1f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(0.f, 0.f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(0.04f, 1.1f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(0.2f, 1.0f * forwardFriction);
		pWheelSettings->mLongitudinalFriction.AddPoint(1.0f, 0.9f * forwardFriction);

		JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(vehicleConstraint.GetController());
		Assert(pVehicleController != nullptr);

		JPH::Wheels& wheels = vehicleConstraint.GetWheels();
		const uint16 wheelIndex = wheel.m_wheelIndex;
		Assert(wheelIndex == static_cast<uint16>(wheels.size()));
		wheel.m_pVehicle = this;
		wheels.push_back(pVehicleController->ConstructWheel(*pWheelSettings));

		if (pAxle.IsValid())
		{
			Assert(pAxle->GetLeftWheel() == &wheel || pAxle->GetRightWheel() == &wheel);
			const bool isLeftWheel = pAxle->GetLeftWheel() == &wheel;

			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
			Assert(constraint.IsValid());
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());

			JPH::VehicleAntiRollBar& antiRollBarSettings = pVehicleConstraint->GetAntiRollBars()[pAxle->m_vehicleAxleIndex];
			JPH::VehicleDifferentialSettings& differentialSettings = pVehicleController->GetDifferentials()[pAxle->m_vehicleAxleIndex];

			if (isLeftWheel)
			{
				differentialSettings.mLeftWheel = wheelIndex;
				antiRollBarSettings.mLeftWheel = wheelIndex;
			}
			else
			{
				differentialSettings.mRightWheel = wheelIndex;
				antiRollBarSettings.mRightWheel = wheelIndex;
			}
		}
	}

	void Vehicle::RemoveWheel(const uint16 wheelIndex, const uint16 axleIndex)
	{
		m_queuedWheelRemovals.EmplaceBack(QueuedWheelRemoval{wheelIndex, axleIndex});
	}

	void Vehicle::RemoveWheelInternal(JPH::VehicleConstraint& vehicleConstraint, const uint16 wheelIndex, const uint16 axleIndex)
	{
		const uint16 previousWheelCount = (uint16)m_wheels.GetSize();
		Assert(wheelIndex < previousWheelCount);

		for (uint16 otherWheelIndex = wheelIndex + 1; otherWheelIndex < previousWheelCount; ++otherWheelIndex)
		{
			Wheel& otherWheel = m_wheels[otherWheelIndex];
			otherWheel.m_wheelIndex--;
		}

		JPH::Wheels& wheels = vehicleConstraint.GetWheels();

		m_wheels.Remove(m_wheels.begin() + wheelIndex);
		wheels.erase(wheels.begin() + wheelIndex);

		if (axleIndex != Math::NumericLimits<uint16>::Max)
		{
			JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(vehicleConstraint.GetController());
			Axle& axle = m_axles[axleIndex];
			Assert(
				(axle.GetLeftWheel().IsValid() && axle.GetLeftWheel()->m_wheelIndex == wheelIndex) ||
				(axle.GetRightWheel().IsValid() && axle.GetRightWheel()->m_wheelIndex == wheelIndex)
			);
			const bool isLeftWheel = axle.GetLeftWheel()->m_wheelIndex == wheelIndex;

			Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
			Assert(constraint.IsValid());

			JPH::VehicleAntiRollBar& antiRollBarSettings = vehicleConstraint.GetAntiRollBars()[axle.m_vehicleAxleIndex];
			JPH::VehicleDifferentialSettings& differentialSettings = pVehicleController->GetDifferentials()[axle.m_vehicleAxleIndex];

			if (isLeftWheel)
			{
				differentialSettings.mLeftWheel = 0;
				antiRollBarSettings.mLeftWheel = 0;
			}
			else
			{
				differentialSettings.mRightWheel = 1;
				antiRollBarSettings.mRightWheel = 1;
			}
		}
	}

	void Vehicle::SetEngine(Engine& engine)
	{
		m_pEngine = engine;
		m_flags |= Flags::UpdateEngineProperties;
	}

	void Vehicle::RemoveEngine([[maybe_unused]] Engine& engine)
	{
		Assert(m_pEngine == &engine);
		m_pEngine = Invalid;
		m_flags |= Flags::UpdateEngineProperties;
	}

	void Vehicle::SetEngineInternal(JPH::VehicleConstraint& vehicleConstraint, Engine& engine)
	{
		m_pEngine = engine;

		JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(vehicleConstraint.GetController());
		Assert(pVehicleController != nullptr);

		JPH::VehicleEngine& vehicleEngine = pVehicleController->GetEngine();
		vehicleEngine.mMinRPM = engine.GetMinimumRPM().GetRevolutionsPerMinute();
		vehicleEngine.mMaxRPM = engine.GetMaximumRPM().GetRevolutionsPerMinute();
		vehicleEngine.mMaxTorque = engine.GetMaximumTorque().GetNewtonMeters();

		JPH::VehicleTransmission& vehicleTransmission = pVehicleController->GetTransmission();
		vehicleTransmission.mGearRatios.clear();
		for (float ratio : engine.GetGearRatios())
		{
			vehicleTransmission.mGearRatios.push_back(ratio);
		}
		vehicleTransmission.mReverseGearRatios.clear();
		vehicleTransmission.mReverseGearRatios.push_back(engine.GetReverseGearRatio());
		vehicleTransmission.mSwitchTime = engine.GetGearSwitchTime().GetSeconds();
		vehicleTransmission.mClutchReleaseTime = engine.GetClutchReleaseTime().GetSeconds();
		vehicleTransmission.mShiftUpRPM = engine.GetShiftUpRPM().GetRevolutionsPerMinute();
		vehicleTransmission.mShiftDownRPM = engine.GetShiftDownRPM().GetRevolutionsPerMinute();
	}

	void Vehicle::FixedPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		if (constraint.IsValid())
		{
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());
			Assert(pVehicleConstraint != nullptr);

			if (m_queuedWheelRemovals.HasElements())
			{
				std::sort(
					(QueuedWheelRemoval*)m_queuedWheelRemovals.begin(),
					(QueuedWheelRemoval*)m_queuedWheelRemovals.end(),
					[](const QueuedWheelRemoval& left, const QueuedWheelRemoval& right)
					{
						return left.wheelIndex < right.wheelIndex;
					}
				);
				for (const QueuedWheelRemoval& queuedWheelRemoval : m_queuedWheelRemovals)
				{
					RemoveWheelInternal(*pVehicleConstraint, queuedWheelRemoval.wheelIndex, queuedWheelRemoval.axleIndex);
				}
				m_queuedWheelRemovals.Clear();
			}

			if (m_queuedAxleRemovals.HasElements())
			{
				std::sort((uint16*)m_queuedAxleRemovals.begin(), (uint16*)m_queuedAxleRemovals.end());
				for (const uint16 axleIndex : m_queuedAxleRemovals)
				{
					RemoveAxleInternal(*pVehicleConstraint, axleIndex);
				}
				m_queuedAxleRemovals.Clear();
			}

			if (m_queuedAxleAdditions.HasElements())
			{
				for (Axle& axle : m_queuedAxleAdditions)
				{
					AddAxleInternal(*pVehicleConstraint, axle);
				}
				m_queuedAxleAdditions.Clear();
			}

			if (m_queuedWheelAdditions.HasElements())
			{
				float newTesterRadius = m_collisionTester->GetRadius();
				for (QueuedWheel& queuedWheel : m_queuedWheelAdditions)
				{
					AddWheelInternal(*pVehicleConstraint, queuedWheel.wheel, queuedWheel.pAxle);
					newTesterRadius = Math::Max(newTesterRadius, queuedWheel.wheel.GetWidth().GetMeters() * 0.5f);
				}

				m_collisionTester->SetRadius(newTesterRadius);
				m_queuedWheelAdditions.Clear();
			}

			if (m_flags.IsSet(Flags::UpdateEngineProperties))
			{
				if (m_pEngine.IsValid())
				{
					SetEngineInternal(*pVehicleConstraint, *m_pEngine);
				}
				m_flags &= ~Flags::UpdateEngineProperties;
			}

			JPH::WheeledVehicleController* pVehicleController = static_cast<JPH::WheeledVehicleController*>(pVehicleConstraint->GetController());
			Assert(pVehicleController != nullptr);
			pVehicleController->SetDriverInput(m_accelerationPedalPosition, m_steeringWheelPosition, m_brakePedalPosition, m_handbrakePosition);
			pVehicleConstraint->SetVehicleCollisionTester(m_collisionTester);
			pVehicleConstraint->SetMaxPitchRollAngle(m_maximumPitchRollAngle.GetRadians());
		}

		if (m_accelerationPedalPosition != 0.f || m_steeringWheelPosition != 0.f || m_brakePedalPosition != 0.f || m_handbrakePosition != 0.f)
		{
			const JPH::BodyLockInterfaceNoLock& bodyLockInterface = physicsScene.m_physicsSystem.GetBodyLockInterfaceNoLock();
			const JPH::BodyID bodyId = GetBodyID();
			JPH::BodyLockWrite lock(bodyLockInterface, bodyId);
			if (LIKELY(lock.Succeeded()))
			{
				JPH::Body& body = lock.GetBody();
				if (!body.IsActive())
				{
					lock.ReleaseLock();
					physicsScene.m_physicsSystem.GetBodyInterfaceNoLock().ActivateBody(bodyId);
				}
			}
		}
	}

	void Vehicle::AfterPhysicsUpdate()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform>();
		Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::LocalTransform3D>();
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = *sceneRegistry.FindComponentTypeData<Entity::Data::Flags>();

		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		Assert(constraint.IsValid());
		if (LIKELY(constraint.IsValid()))
		{
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());
			const Math::WorldTransform vehicleWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(GetIdentifier());

			const JPH::Wheels& wheels = pVehicleConstraint->GetWheels();
			for (Wheel& wheel : m_wheels)
			{
				if (LIKELY(wheel.m_wheelIndex >= 0 && wheel.m_wheelIndex < (int32)wheels.size()))
				{
					Assert(wheels[wheel.m_wheelIndex] != nullptr);
					const JPH::Wheel& physicsWheel = *wheels[wheel.m_wheelIndex];
					const Math::Anglef steerAngle = Math::Anglef::FromRadians(physicsWheel.GetSteerAngle());
					const Math::Anglef rotationAngle = Math::Anglef::FromRadians(-physicsWheel.GetRotationAngle());
					const Math::Quaternionf localWheelRotation =
						wheel.m_defaultRotation.TransformRotation(Math::Quaternionf{Math::CreateRotationAroundZAxis, steerAngle})
							.TransformRotation(Math::Quaternionf{Math::CreateRotationAroundXAxis, rotationAngle});

					const JPH::WheelSettings& wheelSettings = *physicsWheel.GetSettings();
					const Math::WorldCoordinate worldWheelPosition = vehicleWorldTransform.TransformLocation(
						wheelSettings.mPosition + wheelSettings.mDirection * physicsWheel.GetSuspensionLength()
					);

					const Math::WorldTransform parentWorldTransform =
						*worldTransformSceneData.GetComponentImplementation(wheel.GetParent().GetIdentifier());

					const Math::WorldRotation worldWheelRotation = parentWorldTransform.GetRotationQuaternion().TransformRotation(localWheelRotation);

					Entity::Data::WorldTransform& wheelWorldTransform =
						worldTransformSceneData.GetComponentImplementationUnchecked(wheel.GetIdentifier());
					wheelWorldTransform.SetLocation(worldWheelPosition);
					wheelWorldTransform.SetRotation(worldWheelRotation);

					const Math::LocalTransform newLocalTransform = parentWorldTransform.GetTransformRelativeToAsLocal(wheelWorldTransform);
					Entity::Data::LocalTransform3D& localTransformComponent = *localTransformSceneData.GetComponentImplementation(wheel.GetIdentifier(
					));
					localTransformComponent = newLocalTransform;

					wheel.OnWorldTransformChanged(Entity::TransformChangeFlags::ChangedByPhysics);
					wheel.OnWorldTransformChangedEvent(Entity::TransformChangeFlags::ChangedByPhysics);

					for (Entity::Component3D& child : wheel.GetChildren())
					{
						child.OnParentWorldTransformChanged(
							wheelWorldTransform,
							worldTransformSceneData,
							localTransformSceneData,
							flagsSceneData,
							Entity::TransformChangeFlags::ChangedByPhysics
						);
					}

					Assert(wheel.IsRegisteredInTree());
					if (LIKELY(wheel.IsRegisteredInTree()))
					{
						wheel.GetRootSceneComponent().OnComponentWorldLocationOrBoundsChanged(wheel, sceneRegistry);
					}
				}
			}
		}
	}

	Math::WorldTransform Vehicle::GetWheelTransform(const WheelIndex wheelIndex) const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		if (constraint.IsValid())
		{
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());

			const JPH::Wheels& wheels = pVehicleConstraint->GetWheels();
			if (LIKELY(wheelIndex < (int32)wheels.size()))
			{
				const Wheel& wheel = m_wheels[wheelIndex];

				Assert(wheels[wheel.m_wheelIndex] != nullptr);
				const JPH::Wheel& physicsWheel = *wheels[wheel.m_wheelIndex];
				const Math::Anglef steerAngle = Math::Anglef::FromRadians(physicsWheel.GetSteerAngle());
				const Math::Anglef rotationAngle = Math::Anglef::FromRadians(-physicsWheel.GetRotationAngle());
				const Math::Quaternionf localWheelRotation = wheel.m_defaultRotation
				                                               .TransformRotation(Math::Quaternionf{Math::CreateRotationAroundZAxis, steerAngle})
				                                               .TransformRotation(Math::Quaternionf{Math::CreateRotationAroundXAxis, rotationAngle}
				                                               );

				const JPH::WheelSettings& wheelSettings = *physicsWheel.GetSettings();
				const Math::WorldRotation worldWheelRotation = wheel.GetParent().GetWorldRotation().TransformRotation(localWheelRotation);

				return Math::WorldTransform{
					worldWheelRotation,
					GetWorldTransform().TransformLocation(wheelSettings.mPosition + wheelSettings.mDirection * physicsWheel.GetSuspensionLength()),
					wheel.GetWorldScale()
				};
			}
		}

		return Math::Identity;
	}

	Math::WorldTransform Vehicle::GetDefaultWheelTransform(const WheelIndex wheelIndex) const
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		if (constraint.IsValid())
		{
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());

			const JPH::Wheels& wheels = pVehicleConstraint->GetWheels();
			if (LIKELY(wheelIndex < (int32)wheels.size()))
			{
				const Wheel& wheel = m_wheels[wheelIndex];

				Assert(wheels[wheel.m_wheelIndex] != nullptr);
				const JPH::Wheel& physicsWheel = *wheels[wheel.m_wheelIndex];
				const Math::Quaternionf localWheelRotation = wheel.m_defaultRotation;

				const JPH::WheelSettings& wheelSettings = *physicsWheel.GetSettings();
				const Math::Vector3f localWheelPosition = wheelSettings.mPosition;

				return Math::WorldTransform{
					wheel.GetParent().GetWorldRotation().TransformRotation(localWheelRotation),
					GetWorldTransform().TransformLocation(localWheelPosition),
					wheel.GetWorldScale()
				};
			}
		}

		return Math::Identity;
	}

	void Vehicle::OnWheelTransformChanged(const Wheel& wheel)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		if (constraint.IsValid())
		{
			if (JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr()))
			{
				JPH::Wheels& physicsWheels = pVehicleConstraint->GetWheels();
				if (wheel.m_wheelIndex < physicsWheels.size())
				{
					if (const Optional<JPH::Wheel*> pPhysicsWheel = physicsWheels[wheel.m_wheelIndex])
					{
						JPH::WheelSettings& wheelSettings = *pPhysicsWheel->GetSettings();
						const Math::WorldTransform wheelTransform = GetWorldTransform().GetTransformRelativeTo(wheel.GetWorldTransform());

						wheelSettings.mDirection = -wheelTransform.GetUpColumn();
						wheelSettings.mDirection = wheelSettings.mDirection.Normalized();
						wheelSettings.mPosition = wheelTransform.GetLocation() - wheelSettings.mDirection * pPhysicsWheel->GetSuspensionLength();
					}
				}
			}
		}
	}

#if ENABLE_JOLT_DEBUG_RENDERER
	void Vehicle::DebugDraw(JPH::DebugRendering::DebugRendererImp* pDebugRenderer)
	{
		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		JPH::Ref<JPH::Constraint> constraint = physicsScene.GetConstraint(m_vehicleConstraintIdentifier);
		if (constraint.IsValid())
		{
			JPH::VehicleConstraint* pVehicleConstraint = static_cast<JPH::VehicleConstraint*>(constraint.GetPtr());
			// Draw our wheels (this needs to be done in the pre update since we draw the bodies too in the state before the step)
			uint32 wheelIndex = 0;
			for (const JPH::Wheel* pWheel : pVehicleConstraint->GetWheels())
			{
				const JPH::WheelSettings* settings = pWheel->GetSettings();
				JPH::Mat44 wheel_transform = pVehicleConstraint->GetWheelWorldTransform(
					wheelIndex,
					JPH::Vec3::sAxisY(),
					JPH::Vec3::sAxisX()
				); // The cyclinder we draw is aligned with Y so we specify that as rotational axis
				pDebugRenderer->DrawCylinder(wheel_transform, 0.5f * settings->mWidth, settings->mRadius, JPH::Color::sGreen);
				wheelIndex++;
			}

			pVehicleConstraint->DrawConstraint(pDebugRenderer);
			pVehicleConstraint->DrawConstraintLimits(pDebugRenderer);
			pVehicleConstraint->DrawConstraintReferenceFrame(pDebugRenderer);
		}
	}
#else
	void Vehicle::DebugDraw(JPH::DebugRendering::DebugRendererImp*)
	{
	}
#endif

	[[maybe_unused]] const bool wasVehicleRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Vehicle>>::Make());
	[[maybe_unused]] const bool wasVehicleTypeRegistered = Reflection::Registry::RegisterType<Vehicle>();
}
