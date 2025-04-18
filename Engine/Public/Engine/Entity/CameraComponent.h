#pragma once

#include "Component3D.h"
#include "Data/Component.h"
#include "CameraController.h"

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Renderer/Constants.h>

#include <Common/Math/Vector3.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Length.h>
#include <Common/Math/ClampedValue.h>
#include <Common/ForwardDeclarations/EnumFlags.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Threading/Mutexes/SharedLock.h>
#include <Common/Function/Event.h>

namespace ngine
{
	struct Scene3D;
	struct SceneOctreeNode;
	struct RootSceneOctreeNode;
}

namespace ngine::Entity
{
	struct CameraProperties;

	//! Represents a camera in the scene
	struct CameraComponent : public Component3D
	{
		using Controller = CameraController;
	public:
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 10>;

		struct Initializer : public Component3D::Initializer
		{
			using BaseType = Component3D::Initializer;

			Initializer(
				BaseType&& initializer,
				Math::Anglef fieldOfView = 60.0_degrees,
				Math::ClampedValuef nearPlane = {0.1f, 0.0001f, 4096},
				Math::ClampedValuef farPlane = {128.f, 1.f, 65536}
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_fieldOfView(fieldOfView)
				, m_nearPlane(nearPlane)
				, m_farPlane(farPlane)
			{
			}

			Math::Anglef m_fieldOfView = 60.0_degrees;
			Math::ClampedValuef m_nearPlane = {0.1f, 0.0001f, 4096};
			Math::ClampedValuef m_farPlane = {128.f, 1.f, 65536};
		};

		CameraComponent(Initializer&& initializer);
		CameraComponent(const CameraComponent& templateComponent, const Cloner& cloner);
		CameraComponent(const Deserializer& deserializer);
		virtual ~CameraComponent();

		void OnEnable();
		void OnDisable();
		void OnSimulationResumed();
		void OnSimulationPaused();

		[[nodiscard]] Math::Anglef GetFieldOfView() const
		{
			return m_fieldOfView;
		}
		[[nodiscard]] Math::Lengthf GetNearPlane() const
		{
			return Math::Lengthf::FromMeters(m_nearPlane);
		}
		[[nodiscard]] Math::Lengthf GetFarPlane() const
		{
			return Math::Lengthf::FromMeters(m_farPlane);
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Math::Lengthf GetDepthRange() const
		{
			return GetFarPlane() - GetNearPlane();
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Math::Lengthf GetMaximumDepth() const
		{
			return GetNearPlane() + GetDepthRange();
		}

		void SetProperties(const CameraProperties& properties);
		CameraProperties GetProperties() const;

		// Component3D
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;
		// ~Component3D

		[[nodiscard]] Optional<Controller*> GetController() const
		{
			return m_pController;
		}
		void SetController(const Optional<Controller*> pNewController);
		void RemoveController(Controller& controller);
		void RemoveCurrentController();

		void OnFieldOfViewChanged()
		{
			OnFieldOfViewChangedEvent();
		}
		void OnFarPlaneChanged()
		{
			OnFarPlaneChangedEvent();
		}
		void OnNearPlaneChanged()
		{
			OnNearPlaneChangedEvent();
		}

		Event<void(void*), 24> OnFieldOfViewChangedEvent;
		Event<void(void*), 24> OnNearPlaneChangedEvent;
		Event<void(void*), 24> OnFarPlaneChangedEvent;
		Event<void(void*), 24> OnTransfromChangedEvent;
		Event<void(void*), 24> OnDestroyedEvent;
	private:
		CameraComponent(const Deserializer& deserializer, const CameraProperties& cameraProperties);

		virtual void OnBecomeActive()
		{
		}
		virtual void OnBecomeInactive()
		{
		}
	private:
		friend struct Reflection::ReflectedType<Entity::CameraComponent>;
		friend Scene3D;
		friend RootSceneOctreeNode;

		Math::Anglef m_fieldOfView;
		Math::ClampedValuef m_nearPlane = {0.1f, 0.0001f, 4096};
		Math::ClampedValuef m_farPlane = {128.f, 1.f, 65536};
	protected:
		Optional<Controller*> m_pController;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::CameraComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::CameraComponent>(
			"b0ec7111-5de9-4039-bee2-d915f63f929e"_guid,
			MAKE_UNICODE_LITERAL("Camera"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Field of View"),
					"fieldOfView",
					"{BCE8CA1B-BE81-42C4-B557-B462825CBFDF}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraComponent::m_fieldOfView,
					&Entity::CameraComponent::OnFieldOfViewChanged
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Near Plane"),
					"nearPlane",
					"{CE320C7D-6CE1-4B25-896E-FABD26AA6180}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraComponent::m_nearPlane,
					&Entity::CameraComponent::OnNearPlaneChanged
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Far Plane"),
					"farPlane",
					"{1B620A62-6439-4C19-8247-C5162CEDA0E3}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Entity::CameraComponent::m_farPlane,
					&Entity::CameraComponent::OnFarPlaneChanged
				}
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "5b417da8-d03a-5493-9990-4cf7954aeae9"_asset, "3ca2d65d-1e8b-472d-b419-34982867841f"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "436142e1-0ca0-5c98-4006-81905c1a1946"_asset}
			}
		);
	};
}
