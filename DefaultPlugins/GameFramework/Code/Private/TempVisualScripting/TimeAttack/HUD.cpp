#include "TempVisualScripting/TimeAttack/HUD.h"
#include "Tags.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsSystem.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Vehicles/Vehicle.h>

#include <Engine/Entity/SpringComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Tag/TagRegistry.h>

#include "Engine/Entity/ComponentType.h"
#include <Common/Reflection/Registry.inl>
#include <Common/Math/LinearInterpolate.h>

#include <FontRendering/Components/TextComponent.h>
#include <FontRendering/Manager.h>

#include <VisualDebug/Plugin.h>

#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::GameFramework::TimeAttack
{
	HUD::HUD(const HUD& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
	{
		CreateTextComponent();
	}

	HUD::~HUD()
	{
		Entity::ComponentTypeSceneData<HUD>& sceneData = static_cast<Entity::ComponentTypeSceneData<HUD>&>(*GetTypeSceneData());
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void HUD::OnDeserialized(const Serialization::Reader, const Threading::JobBatch&)
	{
		Entity::ComponentTypeSceneData<HUD>& sceneData = static_cast<Entity::ComponentTypeSceneData<HUD>&>(*GetTypeSceneData());
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void HUD::CreateTextComponent()
	{
		Assert(m_velocityText.IsInvalid());

		using namespace Font::Literals;

		Entity::ComponentTypeSceneData<Font::TextComponent>& textSceneData =
			*GetSceneRegistry().GetOrCreateComponentTypeData<Font::TextComponent>();
		Font::Manager& fontManager = *System::FindPlugin<Font::Manager>();

		const Math::YawPitchRollf yawPitchRoll{70_degrees, -120_degrees, 180_degrees};
		const Math::Quaternionf targetRotation = Math::Quaternionf(yawPitchRoll);
		Math::WorldTransform transform{targetRotation, {0.f, 0.f, 1.f}};

		Optional<Font::TextComponent*> pVelocityText = textSceneData.CreateInstance(Font::TextComponent::Initializer{
			Entity::StaticMeshComponent::Initializer{Entity::RenderItemComponent::Initializer{Entity::Component3D::Initializer{*this, transform}}
		  },
			UnicodeString(MAKE_UNICODE_LITERAL("0km/h")),
			{1.f, 1.f, 1.f, 1.f},
			20_pt,
			fontManager.GetCache().FindOrRegisterAsset("73ebe3ce-2911-6273-46f5-122ca2c061f9"_asset)
		});

		Assert(pVelocityText.IsValid());
		if (LIKELY(pVelocityText.IsValid()))
		{
			m_velocityText = *pVelocityText;
		}
	}

	void HUD::AfterPhysicsUpdate()
	{
		if (m_vehicle.IsValid() && m_velocityText.IsValid())
		{
			Physics::Vehicle& vehicle = m_vehicle.Get();
			Font::TextComponent& velocityText = m_velocityText.Get();

			const Math::WorldTransform vehicleTransform = vehicle.GetWorldTransform();
			const Math::Vector3f direction = vehicleTransform.TransformDirection({1.0f, 1.0f, 0.1f});

			static const Math::YawPitchRollf yawPitchRoll{160_degrees, -120_degrees, 180_degrees};
			static const Math::Quaternionf targetRotation = Math::Quaternionf(yawPitchRoll);

			const Math::Quaternionf rotation = vehicleTransform.TransformRotation(targetRotation);

			velocityText.SetWorldLocationAndRotation(vehicleTransform.GetLocation() + direction, rotation);

			const float velocity = vehicle.GetVelocity().GetLength() * 3.6f;
			velocityText.SetText(UnicodeString(String().Format("{:.0f}km/h", velocity)));
		}
	}

	[[maybe_unused]] const bool wasHUDRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HUD>>::Make());
	[[maybe_unused]] const bool wasHUDTypeRegistered = Reflection::Registry::RegisterType<HUD>();
}
