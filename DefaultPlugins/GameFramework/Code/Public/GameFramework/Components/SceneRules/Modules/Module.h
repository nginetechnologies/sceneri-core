#pragma once

#include <Engine/Entity/Data/Component3D.h>

#include <GameFramework/Components/SceneRules/FinishResult.h>
#include <GameFramework/Components/SceneRules/SceneRules.h>

#include <NetworkingCore/Client/ClientIdentifier.h>
#include <Common/Storage/Identifier.h>

namespace ngine::GameFramework
{
	struct SceneRules;
}

namespace ngine::GameFramework
{
	using ClientIdentifier = Network::ClientIdentifier;

	struct SceneRulesModule : Entity::Data::Component3D
	{
		using InstanceIdentifier = TIdentifier<uint32, 3>;
		using BaseType = Entity::Data::Component3D;
		using ParentType = SceneRules;

		using BaseType::BaseType;

		struct DynamicInitializer : public Entity::Data::Component3D::DynamicInitializer
		{
			using BaseType = Entity::Data::Component3D::DynamicInitializer;
			using BaseType::BaseType;

			[[nodiscard]] SceneRules& GetParent() const
			{
				return BaseType::GetParent().AsExpected<SceneRules>();
			}
		};
		using Initializer = DynamicInitializer;

		struct Deserializer final : public Entity::Data::Component3D::Deserializer
		{
			using BaseType = Entity::Data::Component3D::Deserializer;
			using BaseType::BaseType;

			[[nodiscard]] SceneRules& GetParent() const
			{
				return BaseType::GetParent().AsExpected<SceneRules>();
			}
		};

		struct Cloner : public Entity::Data::Component3D::Cloner
		{
			using BaseType = Entity::Data::Component3D::Cloner;
			using BaseType::BaseType;

			[[nodiscard]] SceneRules& GetParent() const
			{
				return BaseType::GetParent().AsExpected<SceneRules>();
			}
			[[nodiscard]] const SceneRules& GetTemplateParent() const
			{
				return BaseType::GetParent().AsExpected<SceneRules>();
			}
		};

		virtual ~SceneRulesModule() = default;

		//! Called when the local scene has finished loading
		virtual void OnLocalSceneLoaded(SceneRules&, const ClientIdentifier)
		{
		}

		//! Called when gameplay starts
		virtual void OnGameplayStarted(SceneRules&)
		{
		}
		//! Called when gameplay is paused
		virtual void OnGameplayPaused(SceneRules&)
		{
		}
		//! Called when gameplay is resumed
		virtual void OnGameplayResumed(SceneRules&)
		{
		}
		//! Called when gameplay is stopped
		virtual void OnGameplayStopped(SceneRules&)
		{
		}

		virtual void OnLocalPlayerPaused(SceneRules&, const ClientIdentifier)
		{
		}
		virtual void OnLocalPlayerResumed(SceneRules&, const ClientIdentifier)
		{
		}

		//! Called on the host when a player joins
		virtual void HostOnPlayerJoined(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the host when a player has finished loading the main scene
		virtual void HostOnPlayerSceneLoaded(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the host when a player left
		virtual void HostOnPlayerLeft(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the host when a player requests restart
		virtual void HostOnPlayerRequestRestart(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the host when a player requests to pause
		virtual void HostOnPlayerRequestPause(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the host when a player requests to pause
		virtual void HostOnPlayerRequestResume(SceneRules&, const ClientIdentifier)
		{
		}

		//! Called on the local client when the player joins
		virtual void ClientOnLocalPlayerJoined(SceneRules&, const ClientIdentifier)
		{
		}
		//! Called on the local client when the player left
		virtual void ClientOnLocalPlayerLeft(SceneRules&, const ClientIdentifier)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SceneRulesModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SceneRulesModule>(
			"8ab2cf66-3d30-46ff-bea4-f6a6a1192389"_guid, MAKE_UNICODE_LITERAL("Game Rules Module Base"), TypeFlags::IsAbstract
		);
	};
}
