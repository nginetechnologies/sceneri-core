#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Math/Primitives/ForwardDeclarations/Sphere.h>
#include <Common/Math/Mass.h>

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>

#include <PhysicsCore/Components/ColliderIdentifier.h>
#include <PhysicsCore/Components/Data/BodyFlags.h>
#include <PhysicsCore/Components/Data/BodyType.h>
#include <PhysicsCore/Components/Data/BodySettings.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>

namespace JPH::DebugRendering
{
	class Renderer;
	class DebugRendererImp;
}

namespace JPH
{
	class Body;
}

namespace ngine::Physics
{
	namespace Data
	{
		struct Scene;
		struct PhysicsCommandStage;
	}

	namespace Components
	{
		struct PlaneConstraint;
	}

	struct Contact;
	struct ColliderComponent;
	struct CharacterComponent;
	struct HandleComponent;
	struct RopeBodyComponent;
	struct TriangleMeshLoadJob;

	//! Legacy 3D physics body component.
	//! Use the data component instead
	struct BodyComponent : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "ac11f104-0d79-4ecf-bae5-dbbbb344ad19"_guid;

		using BaseType = Component3D;
		using Flags = BodyFlags;
		using Type = BodyType;
		using Settings = BodySettings;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Component3D::Initializer;
			Initializer(BaseType&& initializer, Settings&& bodySettings = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_bodySettings(Forward<Settings>(bodySettings))
			{
			}

			Settings m_bodySettings;
		};

		BodyComponent(const BodyComponent& templateComponent, const Cloner& cloner);
		BodyComponent(const Deserializer& deserializer, Settings&& defaultSettings = {});
		BodyComponent(Initializer&& initializer);
		virtual ~BodyComponent() = default;

		[[nodiscard]] bool WasCreated() const;

		virtual void DebugDraw(JPH::DebugRendering::DebugRendererImp*)
		{
		}
	protected:
		[[nodiscard]] JPH::BodyID GetBodyID() const;
	protected:
		friend struct Reflection::ReflectedType<Physics::BodyComponent>;
		friend Data::Scene;
		friend Data::PhysicsCommandStage;
		friend ColliderComponent;
		friend HandleComponent;
		friend RopeBodyComponent;
		friend TriangleMeshLoadJob;

		// Constraints
		friend Components::PlaneConstraint;
		// ~Constraints
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::BodyComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::BodyComponent>(
			Physics::BodyComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Body Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}
