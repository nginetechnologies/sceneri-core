#pragma once

#include <Engine/Entity/CameraController.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Common/Asset/Picker.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/WorldCoordinate.h>

#include <Common/Math/Primitives/RectangleEdges.h>
#include <Common/Math/Primitives/Serialization/RectangleEdges.h>

namespace ngine::GameFramework
{
	struct Player;
}

namespace ngine::GameFramework::Camera
{
	struct Planar : public Entity::CameraController
	{
		inline static constexpr Guid TypeGuid = "4955475A-D592-4DB6-B78F-54EE8F520C01"_guid;

		using BaseType = Entity::CameraController;
		using InstanceIdentifier = TIdentifier<uint32, 3>;
		using Initializer = BaseType::DynamicInitializer;

		Planar(const Deserializer& deserializer);
		Planar(const Planar& templateComponent, const Cloner& cloner);
		Planar(Initializer&& initializer);

		virtual ~Planar() = default;

		void SetTarget(Entity::Component3D& owner, Entity::Component3DPicker target);
		Entity::Component3DPicker GetTarget(Entity::Component3D& owner) const;

		void OnCreated();
		void OnDestroying();
		void OnEnable();
		void OnDisable();

		void AfterPhysicsUpdate();
	private:
		friend struct Reflection::ReflectedType<Planar>;

		Entity::CameraComponent* m_pCameraComponent{nullptr};

		Optional<Player*> m_pPlayer;

		Entity::ComponentSoftReference m_followTarget;
		Math::RectangleEdgesf m_innerFollowBounds{0.25f, 0.25f, 0.25f, 0.f};
		Math::RectangleEdgesf m_outerFollowBounds{0.5f, 0.5f, 0.5f, 0.5f};
		Math::RectangleEdgesf m_followSpeeds{1.f, 1.f, 1.f, 1.f};
	};

}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Camera::Planar>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Camera::Planar>(
			GameFramework::Camera::Planar::TypeGuid,
			MAKE_UNICODE_LITERAL("Planar Camera Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Follow Target"),
					"followTarget",
					"{FBF0D7F0-54E9-4103-845D-67EA7C46A9BA}"_guid,
					MAKE_UNICODE_LITERAL("Planar Camera Controller"),
					&GameFramework::Camera::Planar::SetTarget,
					&GameFramework::Camera::Planar::GetTarget
				),
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Inner Follow Bounds"),
					"innerFollowBounds",
					"{02491CDE-266C-410A-BE70-D6BCE1A8A445}"_guid,
					MAKE_UNICODE_LITERAL("Planar Camera Controller"),
					Reflection::PropertyFlags{},
					&GameFramework::Camera::Planar::m_innerFollowBounds
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Outer Follow Bounds"),
					"outerFollowBounds",
					"{52980AE6-72E3-4868-BDAE-B6E16A5D8129}"_guid,
					MAKE_UNICODE_LITERAL("Planar Camera Controller"),
					Reflection::PropertyFlags{},
					&GameFramework::Camera::Planar::m_outerFollowBounds
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Follow Speeds"),
					"followSpeeds",
					"{39E7F812-3E7F-43DE-928E-B2FC8100C3CE}"_guid,
					MAKE_UNICODE_LITERAL("Planar Camera Controller"),
					Reflection::PropertyFlags{},
					&GameFramework::Camera::Planar::m_followSpeeds
				},
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "5b417da8-d03a-5493-9990-4cf7954aeae9"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
			}}
		);
	};
}
