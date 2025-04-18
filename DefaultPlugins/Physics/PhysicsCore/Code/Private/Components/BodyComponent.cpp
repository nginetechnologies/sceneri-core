#include "PhysicsCore/Components/BodyComponent.h"
#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/ColliderComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/PhysicsCommandStage.h"
#include "PhysicsCore/Contact.h"
#include "PhysicsCore/Plugin.h"

#include "Layer.h"

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentType.h>

#include <Common/Serialization/Reader.h>
#include <Common/Math/Primitives/WorldLine.h>
#include <Common/Math/Primitives/Sphere.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Math/ClampedValue.h>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/Body/BodyCreationSettings.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>

namespace ngine::Physics
{
	BodyComponent::BodyComponent(const BodyComponent& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
	}

	BodyComponent::BodyComponent(const Deserializer& deserializer, Settings&& defaultSettings)
		: BaseType(deserializer)
	{
		// Body as a 3D component is deprecated
		// If we notice this having been saved with a data component, skip creation of a body and let the data component deserialize itself
		// TODO: this old 3D BodyComponent as a Component3D
		if (const Optional<Serialization::Reader> dataComponentsReader = deserializer.m_reader.FindSerializer("data_components"))
		{
			for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
			{
				if (*dataComponentReader.Read<Guid>("typeGuid") == Reflection::GetTypeGuid<Data::Body>())
				{
					return;
				}
			}
		}

		if (Optional<Serialization::Reader> reader = deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<BodyComponent>().ToString().GetView()))
		{
			const Type type = reader->ReadWithDefaultValue<Type>("type", Type::Static);
			const Layer layer = reader->ReadWithDefaultValue<Layer>("layer", Layer::Static);
			const Math::Anglef maximumAngularVelocity =
				reader->ReadWithDefaultValue<Math::Anglef>("maximumAngularVelocity", Math::Anglef(defaultSettings.m_maximumAngularVelocity));
			const Math::Massf overriddenMass =
				reader->ReadWithDefaultValue<Math::Massf>("overriddenMass", Math::Massf(defaultSettings.m_overriddenMass));
			const Math::Ratiof gravityScale = reader->ReadWithDefaultValue<Math::Ratiof>("gravityScale", Math::Ratiof{100_percent});
			const EnumFlags<Flags> flags = reader->ReadWithDefaultValue<Flags>("body_flags", Flags());

			[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
				deserializer.GetSceneRegistry(),
				Data::Body::Initializer{
					Entity::Data::Component3D::DynamicInitializer{*this, deserializer.GetSceneRegistry()},
					Settings{type, layer, maximumAngularVelocity, overriddenMass, gravityScale, flags}
				}
			);
			Assert(pBody.IsValid());
		}
		else
		{
			[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
				deserializer.GetSceneRegistry(),
				Data::Body::Initializer{
					Entity::Data::Component3D::DynamicInitializer{*this, deserializer.GetSceneRegistry()},
					Forward<Settings>(defaultSettings)
				}
			);
			Assert(pBody.IsValid());
		}
	}

	BodyComponent::BodyComponent(Initializer&& initializer)
		: BaseType(Forward<Initializer>(initializer))
	{
		[[maybe_unused]] Optional<Data::Body*> pBody = CreateDataComponent<Data::Body>(
			initializer.GetSceneRegistry(),
			Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*this, initializer.GetSceneRegistry()},
				Settings(initializer.m_bodySettings)
			}
		);
		Assert(pBody.IsValid());
	}

	JPH::BodyID BodyComponent::GetBodyID() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<Data::Body*> pBody = FindDataComponentOfType<Data::Body>(sceneRegistry);
		if (LIKELY(pBody.IsValid()))
		{
			return pBody->GetIdentifier();
		}
		else
		{
			return {};
		}
	}

	bool BodyComponent::WasCreated() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Optional<Data::Body*> pBody = FindDataComponentOfType<Data::Body>(sceneRegistry);
		if (Ensure(pBody.IsValid()))
		{
			const Optional<Data::Scene*> pPhysicsScene = GetRootSceneComponent().FindDataComponentOfType<Data::Scene>(sceneRegistry);
			Assert(pPhysicsScene.IsValid());
			if (LIKELY(pPhysicsScene.IsValid()))
			{
				return pBody->WasCreated(*pPhysicsScene);
			}
		}
		return false;
	}

	[[maybe_unused]] const bool wasBodyRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<BodyComponent>>::Make(
	));
	[[maybe_unused]] const bool wasBodyTypeRegistered = Reflection::Registry::RegisterType<BodyComponent>();
}
