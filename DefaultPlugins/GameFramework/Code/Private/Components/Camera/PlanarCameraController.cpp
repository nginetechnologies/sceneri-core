#include <GameFramework/Components/Camera/PlanarCameraController.h>

#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentSoftReference.inl>

#include <Engine/Tag/TagRegistry.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <GameFramework/Components/Player/Player.h>
#include <Renderer/Scene/SceneView.h>

#include <Engine/Entity/Data/Tags.h>
#include <Engine/Tag/TagIdentifier.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Math/Quaternion.h>
#include <Common/Math/Primitives/WorldLine.h>
#include <Common/Math/Power.h>
#include <Common/Math/NumericLimits.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/IsNearlyZero.h>
#include <Common/Math/SignNonZero.h>
#include <Common/Math/Damping.h>
#include <Common/Math/Abs.h>
#include <Common/IO/Log.h>

namespace ngine::GameFramework::Camera
{
	namespace
	{
		constexpr Guid CameraTargetTag{"5A874881-8E50-4F54-9726-CDA4A757FD9B"_guid};

		void CheckBounds(Math::RectangleEdgesf& bounds)
		{
			LogWarningIf(bounds.m_left < 0.f, "Left follow bound cannot be negative");
			LogWarningIf(bounds.m_top < 0.f, "Top follow bound cannot be negative");
			LogWarningIf(bounds.m_right < 0.f, "Right follow bound cannot be negative");
			LogWarningIf(bounds.m_bottom < 0.f, "Bottom follow bound cannot be negative");

			bounds.m_left = Math::Abs(bounds.m_left);
			bounds.m_top = Math::Abs(bounds.m_top);
			bounds.m_right = Math::Abs(bounds.m_right);
			bounds.m_bottom = Math::Abs(bounds.m_bottom);
		}

		Math::Vector3f GetBoundsMovement(Math::Vector3f axis, float offset, float innerBound, float outerBound, float speed, float frameTime)
		{
			if (offset >= innerBound)
			{
				const float boundsDifference = outerBound - innerBound;

				if (!Math::IsNearlyZero(innerBound) && !Math::IsNearlyZero(boundsDifference))
				{
					const float offsetInBounds = (offset - innerBound) / boundsDifference;
					const float horizontalSpeed = Math::Min(speed * offsetInBounds * frameTime, 1.f);
					return axis * horizontalSpeed;
				}
				else if (Math::IsNearlyZero(boundsDifference))
				{
					const float horizontalSpeed = Math::Min(speed * outerBound * frameTime, 1.f);
					return axis * horizontalSpeed;
				}
				else
				{
					return axis * offset;
				}
			}

			return Math::Zero;
		}
	}

	Planar::Planar(const Deserializer& deserializer)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(deserializer.GetParent()))
		, m_followTarget(
				deserializer.m_reader.HasSerializer("followTarget")
					? *deserializer.m_reader.Read<Entity::ComponentSoftReference>("followTarget", deserializer.GetSceneRegistry())
					: Entity::ComponentSoftReference{}
			)
	{
		LogWarningIf(
			!deserializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		LogWarningIf(m_followSpeeds.m_left == 0.f, "Left follow speed cannot be zero");
		LogWarningIf(m_followSpeeds.m_right == 0.f, "Right follow speed cannot be zero");
		LogWarningIf(m_followSpeeds.m_bottom == 0.f, "Bottom follow speed cannot be zero");
		LogWarningIf(m_followSpeeds.m_top == 0.f, "Top follow speed cannot be zero");

		m_followSpeeds.m_left = m_followSpeeds.m_left != 0.f ? m_followSpeeds.m_left : 1.f;
		m_followSpeeds.m_right = m_followSpeeds.m_right != 0.f ? m_followSpeeds.m_right : 1.f;
		m_followSpeeds.m_bottom = m_followSpeeds.m_bottom != 0.f ? m_followSpeeds.m_bottom : 1.f;
		m_followSpeeds.m_top = m_followSpeeds.m_top != 0.f ? m_followSpeeds.m_top : 1.f;

		CheckBounds(m_innerFollowBounds);
		CheckBounds(m_outerFollowBounds);

		LogWarningIf(m_outerFollowBounds.m_left < m_innerFollowBounds.m_left, "Left outer bound cannot be less than the inner bound");
		if (m_outerFollowBounds.m_left < m_innerFollowBounds.m_left)
		{
			m_outerFollowBounds.m_left = m_innerFollowBounds.m_left;
		}

		LogWarningIf(m_outerFollowBounds.m_right < m_innerFollowBounds.m_right, "Right outer bound cannot be less than the inner bound");
		if (m_outerFollowBounds.m_right < m_innerFollowBounds.m_right)
		{
			m_outerFollowBounds.m_right = m_innerFollowBounds.m_right;
		}

		LogWarningIf(m_outerFollowBounds.m_top < m_innerFollowBounds.m_top, "Top outer bound cannot be less than the inner bound");
		if (m_outerFollowBounds.m_top < m_innerFollowBounds.m_top)
		{
			m_outerFollowBounds.m_top = m_innerFollowBounds.m_top;
		}

		LogWarningIf(m_outerFollowBounds.m_bottom < m_innerFollowBounds.m_bottom, "Bottom outer bound cannot be less than the inner bound");
		if (m_outerFollowBounds.m_bottom < m_innerFollowBounds.m_bottom)
		{
			m_outerFollowBounds.m_bottom = m_innerFollowBounds.m_bottom;
		}

		m_pCameraComponent->SetController(*this);
	}

	Planar::Planar(const Planar& templateComponent, const Cloner& cloner)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(cloner.GetParent())), m_followTarget{
				templateComponent.m_followTarget,
				Entity::ComponentSoftReference::Cloner{
					cloner.GetTemplateParent().GetSceneRegistry(), cloner.GetSceneRegistry()}},
     m_innerFollowBounds{templateComponent.m_innerFollowBounds},
     m_outerFollowBounds{templateComponent.m_outerFollowBounds},
    m_followSpeeds{templateComponent.m_followSpeeds}
	{
		LogWarningIf(
			!cloner.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		m_pCameraComponent->SetController(*this);
	}

	Planar::Planar(Initializer&& initializer)
		: m_pCameraComponent(&static_cast<Entity::CameraComponent&>(initializer.GetParent()))
	{
		LogWarningIf(
			!initializer.GetParent().Is<Entity::CameraComponent>(),
			"Camera controller component can not be initialzed by a non-camera component!"
		);

		m_pCameraComponent->SetController(*this);
	}
	void Planar::SetTarget(Entity::Component3D&, const Entity::Component3DPicker target)
	{
		m_followTarget = target;
	}

	Entity::Component3DPicker Planar::GetTarget(Entity::Component3D& owner) const
	{
		Entity::Component3DPicker picker{m_followTarget, owner.GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<Entity::Component3D>()});
		return picker;
	}

	void Planar::OnCreated()
	{
		Entity::ComponentTypeSceneData<Planar>& sceneData = *m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<Planar>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void Planar::OnDestroying()
	{
		Entity::ComponentTypeSceneData<Planar>& sceneData = *m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<Planar>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void Planar::OnEnable()
	{
		Entity::ComponentTypeSceneData<Planar>& sceneData = *m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<Planar>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void Planar::OnDisable()
	{
		Entity::ComponentTypeSceneData<Planar>& sceneData = *m_pCameraComponent->GetSceneRegistry().GetOrCreateComponentTypeData<Planar>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void Planar::AfterPhysicsUpdate()
	{
		Entity::SceneRegistry& sceneRegistry = m_pCameraComponent->GetSceneRegistry();
		if (UNLIKELY(m_pPlayer.IsInvalid()))
		{
			m_pPlayer = m_pCameraComponent->FindFirstDataComponentOfTypeInParents<Player>(sceneRegistry);
			if (m_pPlayer.IsInvalid())
			{
				return;
			}
		}

		// TODO: Remove this when ComponentSoftReference can be used in spawned scenes as it is a temporary workaround
		if (m_followTarget.GetInstanceIdentifier().IsInvalid())
		{
			Tag::Identifier cameraIdentifierTag = System::Get<Tag::Registry>().FindOrRegister(CameraTargetTag);
			m_pCameraComponent->GetParentSceneComponent()->IterateDataComponentsOfTypeInChildrenRecursive<Entity::Data::Tags>(
				sceneRegistry,
				[this,
			   cameraIdentifierTag,
			   &sceneRegistry](Entity::HierarchyComponentBase& component, Entity::Data::Tags& tags) -> Memory::CallbackResult
				{
					if (tags.HasTag(cameraIdentifierTag))
					{
						m_followTarget = Entity::ComponentSoftReference{component, sceneRegistry};
						return Memory::CallbackResult::Break;
					}
					return Memory::CallbackResult::Continue;
				}
			);
		}

		if (Optional<Entity::Component3D*> pComponent = m_followTarget.Find<Entity::Component3D>(sceneRegistry))
		{
			const FrameTime frameTime = Math::Min(m_pCameraComponent->GetCurrentFrameTime(), FrameTime{1_seconds});

			const Math::WorldCoordinate trackedComponentCoordinate = pComponent->GetWorldLocation(sceneRegistry);
			const Math::WorldTransform cameraTransform = m_pCameraComponent->GetWorldTransform(sceneRegistry);
			const Math::Vector3f cameraLocation = cameraTransform.GetLocation();
			const Math::Quaternionf cameraRotation = cameraTransform.GetRotationQuaternion();
			const Math::Vector3f cameraUp = cameraRotation.GetUpColumn();
			const Math::Vector3f cameraRight = cameraRotation.GetRightColumn();

			const Math::WorldLine lineToTarget{cameraLocation, trackedComponentCoordinate};
			const Math::Vector3f direction = lineToTarget.GetDistance();

			const float horizontalOffset = cameraRight.Dot(direction);
			const float verticalOffset = -cameraUp.Dot(direction);

			const Optional<const Rendering::SceneView*> sceneView = m_pPlayer->GetSceneView();
			if (LIKELY(sceneView.IsValid()))
			{
				const float fieldOfView = m_pCameraComponent->GetFieldOfView().GetRadians();
				const Math::Vector2ui renderResolution = sceneView->GetRenderResolution();
				const float aspectRatio = static_cast<float>(renderResolution.x) / static_cast<float>(renderResolution.y);
				const float halfVerticalHeight = Math::Tan(fieldOfView * 0.5f) * lineToTarget.GetLength();
				const float halfHorizontalHeight = halfVerticalHeight * aspectRatio;

				const Math::TRectangleEdges<float> intersectionSlice{
					-halfHorizontalHeight,
					+halfVerticalHeight,
					+halfHorizontalHeight,
					-halfVerticalHeight,
				};

				const float leftOffset = horizontalOffset / intersectionSlice.m_left;
				const float rightOffset = horizontalOffset / intersectionSlice.m_right;
				const float topOffset = verticalOffset / intersectionSlice.m_top;
				const float bottomOffset = verticalOffset / intersectionSlice.m_bottom;

				const Math::Vector3f leftMovement = GetBoundsMovement(
					-cameraRight,
					leftOffset,
					m_innerFollowBounds.m_left,
					m_outerFollowBounds.m_left,
					m_followSpeeds.m_left,
					frameTime
				);

				const Math::Vector3f rightMovement = GetBoundsMovement(
					cameraRight,
					rightOffset,
					m_innerFollowBounds.m_right,
					m_outerFollowBounds.m_right,
					m_followSpeeds.m_right,
					frameTime
				);

				const Math::Vector3f topMovement =
					GetBoundsMovement(cameraUp, topOffset, m_innerFollowBounds.m_top, m_outerFollowBounds.m_top, m_followSpeeds.m_top, frameTime);

				const Math::Vector3f bottomMovement = GetBoundsMovement(
					-cameraUp,
					bottomOffset,
					m_innerFollowBounds.m_bottom,
					m_outerFollowBounds.m_bottom,
					m_followSpeeds.m_bottom,
					frameTime
				);

				const Math::Vector3f combinedMovement = leftMovement + rightMovement + topMovement + bottomMovement;
				m_pCameraComponent->SetWorldLocation(cameraLocation + combinedMovement);
			}
		}
	}

	[[maybe_unused]] const bool wasPlanarCameraControllerTypeRegistered = Reflection::Registry::RegisterType<Planar>();
	[[maybe_unused]] const bool wasPlanarCameraControllerComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Planar>>::Make());
}
