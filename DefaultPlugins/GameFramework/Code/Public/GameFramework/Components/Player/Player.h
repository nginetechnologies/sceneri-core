#pragma once

#include <Engine/Entity/Data/Component.h>

#include <NetworkingCore/Client/ClientIdentifier.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Entity
{
	struct CameraComponent;
	struct InputComponent;
}

namespace ngine::Rendering
{
	struct SceneView;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct Player final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using InstanceIdentifier = ClientIdentifier;

		struct Initializer : public Entity::Data::Component::Initializer
		{
			using BaseType = Entity::Data::Component::Initializer;

			Initializer(BaseType&& initializer, const ClientIdentifier playerIdentifier)
				: BaseType(Forward<BaseType>(initializer))
				, m_clientIdentifier(playerIdentifier)
			{
			}

			ClientIdentifier m_clientIdentifier;
		};

		Player(Initializer&& initializer);

		[[nodiscard]] bool IsValid() const
		{
			return m_clientIdentifier.IsValid();
		}

		[[nodiscard]] Optional<const Rendering::SceneView*> GetSceneView() const;

		[[nodiscard]] ClientIdentifier GetClientIdentifier() const
		{
			return m_clientIdentifier;
		}

		void ChangeCamera(Entity::CameraComponent& camera);
		void AssignInput(Entity::InputComponent& input);
		void UnassignInput(Entity::InputComponent& input);
	private:
		ClientIdentifier m_clientIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Player>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Player>(
			"cd4df50a-d0aa-4811-9362-ea0e4519c926"_guid,
			MAKE_UNICODE_LITERAL("Player Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::DisableDynamicCloning |
				Reflection::TypeFlags::DisableDynamicDeserialization | Reflection::TypeFlags::DisableWriteToDisk
		);
	};
}
