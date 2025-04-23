#include "Components/Camera/ThirdPersonCameraController.h"

#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/RootSceneComponent.h>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/RigidbodyCharacter.h>
#include <PhysicsCore/BroadPhaseLayer.h>
#include <Physics/Collision/RayCast.h>
#include "PhysicsCore/Layer.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/CameraProperties.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Common/System/Query.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/IO/Log.h>

namespace ngine::GameFramework::Camera
{
	ThirdPerson::ThirdPerson(const ThirdPerson& templateComponent, const Cloner& cloner)
		: m_verticalStep(templateComponent.m_verticalStep)
		, m_flags(templateComponent.m_flags)
	{
		LogWarningIf(
			!cloner.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a none-camera component!"
		);

		m_pCameraComponent = &static_cast<Entity::CameraComponent&>(cloner.GetParent());
		m_pCameraComponent->SetController(*this);
		if (!m_pCameraComponent->IsDetachedFromTree())
		{
			UpdateCameraLocationAndRotation(Math::NumericLimits<float>::Max);
		}
	}

	ThirdPerson::ThirdPerson(const Deserializer& deserializer)
	{
		LogWarningIf(
			!deserializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a none-camera component!"
		);

		m_pCameraComponent = &static_cast<Entity::CameraComponent&>(deserializer.GetParent());
		m_pCameraComponent->SetController(*this);
		if (!m_pCameraComponent->IsDetachedFromTree())
		{
			UpdateCameraLocationAndRotation(Math::NumericLimits<float>::Max);
		}
	}

	ThirdPerson::ThirdPerson(Initializer&& initializer)
	{
		LogWarningIf(
			!initializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a none-camera component!"
		);

		m_pCameraComponent = &static_cast<Entity::CameraComponent&>(initializer.GetParent());
		m_pCameraComponent->SetController(*this);
		if (!m_pCameraComponent->IsDetachedFromTree())
		{
			UpdateCameraLocationAndRotation(Math::NumericLimits<float>::Max);
		}
	}

	void ThirdPerson::OnDestroying(Entity::Component3D&)
	{
		if (m_pCameraComponent != nullptr)
		{
			m_pCameraComponent->RemoveController(*this);
		}
	}

	void ThirdPerson::OnBecomeActive()
	{
		Entity::ComponentTypeSceneData<ThirdPerson>& sceneData = *m_pCameraComponent->GetSceneRegistry().FindComponentTypeData<ThirdPerson>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void ThirdPerson::OnBecomeInactive()
	{
		Entity::ComponentTypeSceneData<ThirdPerson>& sceneData = *m_pCameraComponent->GetSceneRegistry().FindComponentTypeData<ThirdPerson>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void ThirdPerson::AfterPhysicsUpdate()
	{
		const FrameTime deltaTime = m_pCameraComponent->GetCurrentFrameTime();
		UpdateCameraLocationAndRotation(deltaTime);
	}

	void ThirdPerson::SetTrackedComponent(Entity::Component3D& component)
	{
		m_pTrackedComponent = &component;

		const Math::WorldCoordinate trackedTargetLocation = component.GetWorldTransform().GetLocation();
		m_interpolatedTrackedTargetLocation = m_lastTrackedPosition = trackedTargetLocation;

		const Math::LocalTransform initialCameraTransform = m_pCameraComponent->GetRelativeTransform();
		m_cameraOffset = initialCameraTransform.GetLocation();
		m_initialOrbitAngles = initialCameraTransform.GetRotationQuaternion().GetEulerAngles();

		SetInitialCameraLocationAndRotation();
	}

	void ThirdPerson::SetInitialCameraLocationAndRotation()
	{
		if (m_pTrackedComponent.IsInvalid())
		{
			return;
		}

		// Collect some useful values
		const Math::WorldTransform initialCameraTransform = m_pCameraComponent->GetWorldTransform();
		const Math::Quaternionf initialCameraRotation = initialCameraTransform.GetRotationQuaternion();
		const Math::Vector3f initialCameraForward = initialCameraRotation.GetForwardColumn().GetNormalized();

		const Entity::Component3D& trackedComponent = *m_pTrackedComponent;
		const Math::WorldTransform& trackedComponentTransform = trackedComponent.GetWorldTransform();
		const Math::WorldCoordinate trackedTargetLocation = trackedComponentTransform.GetLocation();

		const Math::Vector3f projectedCameraForward = initialCameraForward.Project(Math::Up).GetNormalizedSafe(Math::Forward);

		const float dotProduct = projectedCameraForward.Dot(Math::Forward);
		const Math::Vector3f crossProduct = projectedCameraForward.Cross(Math::Forward).GetNormalizedSafe(Math::Forward);
		const float acos = Math::Acos(Math::Clamp(dotProduct, -1.f, 1.f)
		); // / (projectedTrackedForward.GetLength() * projectedCameraForward.GetLength())); // Lengths should be 1 if they're normalised
		const float dotProduct2 = crossProduct.Dot(Math::Up);

		Math::Anglef angleBetweenForwards = Math::Anglef::FromRadians(acos * Math::SignNonZero(dotProduct2));
		const Math::Vector2f resetPosition{angleBetweenForwards.GetRadians(), m_initialOrbitAngles.y.GetRadians()};

		m_orbitPosition = resetPosition;
		m_lastTrackedPosition = trackedTargetLocation;

		Math::Anglef yawAngle = Math::Anglef::FromRadians(-m_orbitPosition.x);
		const Math::Quaternionf yawRotation = {Math::CreateRotationAroundAxis, yawAngle, Math::Up};

		const Math::Anglef pitchAngle = Math::Clamp<Math::Anglef>(
			Math::Anglef::FromRadians(m_orbitPosition.y),
			Math::Anglef::FromDegrees(m_minPitchAngle),
			Math::Anglef::FromDegrees(m_maxPitchAngle)
		);
		m_orbitPosition.y = pitchAngle.GetRadians();
		const Math::Quaternionf pitchRotation = {Math::CreateRotationAroundAxis, pitchAngle, yawRotation.TransformDirection(Math::Right)};
		const Math::Quaternionf cameraOffsetRotation = pitchRotation.TransformRotation(yawRotation);

		// Update the rotation and offset of the camera
		Math::WorldCoordinate modifiedTargetLocation = trackedTargetLocation;
		modifiedTargetLocation.z = m_verticalLocation;

		m_interpolatedTrackedTargetLocation = modifiedTargetLocation;

		const Math::WorldCoordinate targetLookAtLocation = m_interpolatedTrackedTargetLocation;
		const Math::WorldCoordinate targetCameraLocation = targetLookAtLocation + cameraOffsetRotation.TransformDirection(m_cameraOffset);

		FinaliseCameraLocationAndRotation(targetCameraLocation, targetLookAtLocation);
	}

	void ThirdPerson::UpdateCameraLocationAndRotation(float deltaTime)
	{
		if (m_pTrackedComponent.IsInvalid())
		{
			return;
		}

		// Collect some useful values
		const Math::WorldTransform initialCameraTransform = m_pCameraComponent->GetWorldTransform();
		const Math::Quaternionf initialCameraRotation = initialCameraTransform.GetRotationQuaternion();
		const Math::Vector3f initialCameraForward = initialCameraRotation.GetForwardColumn().GetNormalized();

		const Entity::Component3D& trackedComponent = *m_pTrackedComponent;
		const Math::WorldTransform& trackedComponentTransform = trackedComponent.GetWorldTransform();
		const Math::WorldCoordinate trackedTargetLocation = trackedComponentTransform.GetLocation();
		// const Math::Quaternionf trackedTargetRotation = trackedComponentTransform.GetRotationQuaternion();
		// const Math::Vector3f trackedTargetForward = trackedTargetRotation.GetForwardColumn().GetNormalized();

		// Joystick input
		const Math::Vector2f orbitDelta = m_rotationalDirection * m_rotationSpeed * deltaTime;

		// Slowly reset view while moving
		/*const Math::WorldLine trackedTargetMovementDelta = {m_lastTrackedPosition, trackedTargetLocation};
		if (m_flags.IsNotSet(Flags::IgnoreTrackedComponentOrientation) && m_rotationalDirection.GetLengthSquared() < 0.1f)
		{
		  const Math::Vector3f projectedTrackedForward = trackedTargetForward.Project(Math::Up).GetNormalizedSafe(Math::Forward);
		  const Math::Vector3f projectedCameraForward = initialCameraForward.Project(Math::Up).GetNormalizedSafe(Math::Forward);

		  const float dotProduct = projectedTrackedForward.Dot(projectedCameraForward);
		  const Math::Vector3f crossProduct = projectedTrackedForward.Cross(projectedCameraForward).GetNormalizedSafe(Math::Right);
		  const float acos = Math::Acos(Math::Clamp(dotProduct, -1.f, 1.f)
		  ); // / (projectedTrackedForward.GetLength() * projectedCameraForward.GetLength())); // Lengths should be 1 if they're normalised
		  const float dotProduct2 = crossProduct.Dot(Math::Up);

		  Math::Anglef angleBetweenForwards = Math::Anglef::FromRadians(acos * Math::SignNonZero(dotProduct2));

		  const float resetRotationBlend = Math::Clamp((trackedTargetMovementDelta.GetLength() - m_minCatchupDistance), 0.0f, 1.0f) * deltaTime;
		  const Math::Vector2f resetPosition{m_orbitPosition.x + angleBetweenForwards.GetRadians(), m_initialOrbitAngles.x.GetRadians()};
		  m_orbitPosition = Math::LinearInterpolate(m_orbitPosition, resetPosition, Math::Min(resetRotationBlend, 1.f));
		}
		else
		{
		  m_lastTrackedPosition = trackedTargetLocation;
		}*/

		// Add a slight yaw deadzone
		if (Math::Abs(m_rotationalDirection.x) > m_yawDeadzone)
		{
			m_orbitPosition.x -= orbitDelta.x;
		}

		// Add a slight pitch deadzone
		if (Math::Abs(m_rotationalDirection.y) > m_pitchDeadzone)
		{
			m_orbitPosition.y -= orbitDelta.y;
		}

		Math::Anglef yawAngle = Math::Anglef::FromRadians(-m_orbitPosition.x);
		yawAngle.Wrap();

		Math::Anglef pitchAngle = Math::Clamp<Math::Anglef>(
			Math::Anglef::FromRadians(m_orbitPosition.y),
			Math::Anglef::FromDegrees(m_minPitchAngle),
			Math::Anglef::FromDegrees(m_maxPitchAngle)
		);

		// Slowly rotate towards velocity
		if (const Optional<Physics::Data::Body*> pCharacterBody = m_pTrackedComponent->FindDataComponentOfType<Physics::Data::Body>())
		{
			if (const Optional<Physics::Data::Scene*> pPhysicsScene = m_pTrackedComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
			{
				const Math::Vector3f velocity = pCharacterBody->GetVelocity(*pPhysicsScene);
				const Optional<Physics::RigidbodyCharacter*> pTrackedRigidbodyCharacter = m_pTrackedComponent->As<Physics::RigidbodyCharacter>();

				if (velocity.GetLength() > 0.1f && (initialCameraForward.Dot(velocity.GetNormalized()) > 0 || pTrackedRigidbodyCharacter.IsInvalid()))
				{
					const Math::Quaternionf velocityRotation{velocity.GetNormalizedSafe(initialCameraForward), Math::Up};

					const float rotationTime = 0.5f;

					Math::Quaternionf yawRotation = {Math::CreateRotationAroundAxis, yawAngle, Math::Up};
					yawRotation = Math::Quaternionf::SphericalInterpolation(yawRotation, velocityRotation, Math::Min(deltaTime * rotationTime, 1.f));

					yawAngle = yawRotation.GetYaw();
					// pitchAngle = Math::LinearInterpolate(pitchAngle, yawPitchRoll.y, deltaTime);
				}
			}
		}

		m_orbitPosition.x = -yawAngle.GetRadians();
		m_orbitPosition.y = pitchAngle.GetRadians();

		const Math::Quaternionf yawRotation = {Math::CreateRotationAroundAxis, yawAngle, Math::Up};
		const Math::Quaternionf pitchRotation = {Math::CreateRotationAroundAxis, pitchAngle, yawRotation.TransformDirection(Math::Right)};
		const Math::Quaternionf cameraOffsetRotation = pitchRotation.TransformRotation(yawRotation);

		// Update the rotation and offset of the camera
		Math::WorldCoordinate modifiedTargetLocation = trackedTargetLocation;
		modifiedTargetLocation.z = m_verticalLocation;

		m_interpolatedTrackedTargetLocation =
			Math::LinearInterpolate(m_interpolatedTrackedTargetLocation, modifiedTargetLocation, Math::Min(m_positionSpeed * deltaTime, 1.f));

		const Math::WorldCoordinate targetLookAtLocation = m_interpolatedTrackedTargetLocation;
		const Math::WorldCoordinate targetCameraLocation = m_interpolatedTrackedTargetLocation +
		                                                   cameraOffsetRotation.TransformDirection(m_cameraOffset);

		FinaliseCameraLocationAndRotation(targetCameraLocation, targetLookAtLocation);
	}

	void ThirdPerson::FinaliseCameraLocationAndRotation(
		Math::WorldCoordinate targetCameraLocation, const Math::WorldCoordinate targetLookAtLocation
	)
	{
		// Check the camera against the scene to prevent it clipping through stuff
		const Optional<Physics::Data::Body*> pCharacterBody = m_pTrackedComponent->FindDataComponentOfType<Physics::Data::Body>();

		InlineVector<JPH::BodyID, 2> bodiesFilter;
		if (pCharacterBody.IsValid())
		{
			bodiesFilter.EmplaceBack(pCharacterBody->GetIdentifier());
		}

		if (const Optional<Physics::RigidbodyCharacter*> pTrackedRigidbodyCharacter = m_pTrackedComponent->As<Physics::RigidbodyCharacter>())
		{
			if (pTrackedRigidbodyCharacter->GetGroundState() == Physics::RigidbodyCharacter::GroundState::OnGround)
			{
				bodiesFilter.EmplaceBack(pTrackedRigidbodyCharacter->GetGroundBodyID());
			}
		}

		// Disabled for now as it's glitchy
		constexpr bool sphereCast = false;
		if constexpr (sphereCast)
		{
			const Math::WorldLine raycastLine{targetLookAtLocation, targetCameraLocation};
			Physics::Data::Scene& physicsScene = *m_pTrackedComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
			Physics::Data::Scene::ShapeCastResults results = physicsScene.SphereCast(
				raycastLine.GetStart(),
				raycastLine.GetDistance(),
				0.05_meters,
				Physics::BroadPhaseLayerMask::Static | Physics::BroadPhaseLayerMask::Dynamic,
				Physics::LayerMask::Static | Physics::LayerMask::Dynamic,
				bodiesFilter
			);
			if (results.HasElements())
			{
				const float originalDistance = raycastLine.GetLength();
				Math::WorldCoordinate closestContact = results[0].GetContactLocation();
				float bestDistance = Math::Abs(raycastLine.GetDirection().Dot(closestContact - raycastLine.GetStart()));
				for (const Physics::Data::Scene::ShapeCastResult& result : (results.GetView() + 1))
				{
					const float distance = Math::Abs(raycastLine.GetDirection().Dot(result.GetContactLocation() - raycastLine.GetStart()));
					if (distance < bestDistance)
					{
						closestContact = result.GetContactLocation();
						bestDistance = distance;
					}
				}

				const float nearPlane = m_pCameraComponent->GetProperties().m_nearPlane;
				const float distanceToHit = Math::Max(bestDistance, nearPlane * 2.f);

				targetCameraLocation -= raycastLine.GetDirection() * (originalDistance - distanceToHit);
			}
		}

		// Calculate look-at rotation
		const Math::WorldLine cameraLine(targetCameraLocation, targetLookAtLocation);
		const Math::Vector3f cameraForward = cameraLine.GetDirection().GetNormalizedSafe(Math::Forward);
		const Math::Vector3f cameraRight = cameraForward.Cross(Math::Up).GetNormalizedSafe(Math::Right);
		const Math::Vector3f cameraUp = cameraRight.Cross(cameraForward).GetNormalizedSafe(Math::Up);
		const Math::Quaternionf targetCameraRotation(cameraForward, cameraUp);

		const Math::WorldTransform currentTransform = m_pCameraComponent->GetWorldTransform();
		if (!currentTransform.GetLocation().IsEquivalentTo(targetCameraLocation, 0.005f) || !currentTransform.GetRotationQuaternion().IsEquivalentTo(targetCameraRotation, 0.005f))
		{
			m_pCameraComponent->SetWorldLocationAndRotation(targetCameraLocation, targetCameraRotation);
		}
	}

	[[maybe_unused]] const bool wasCameraControllerTypeRegistered = Reflection::Registry::RegisterType<ThirdPerson>();
	[[maybe_unused]] const bool wasCameraControllerComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ThirdPerson>>::Make());
}
