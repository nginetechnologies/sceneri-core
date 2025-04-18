#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

#include <NetworkingCore/Components/BoundObjectIdentifier.h>
#include <NetworkingCore/Host/LocalHost.h>

#include <Common/Storage/Identifier.h>
#include <Common/Network/Address.h>

namespace ngine::Entity
{
	template<typename DataComponentType>
	struct DataComponentResult;
}

namespace ngine::Network::Session
{
	struct BoundComponent;
	struct LocalClient;
	struct Host;

	struct LocalHost final : public Entity::Data::HierarchyComponent
	{
		using InstanceIdentifier = TIdentifier<uint8, 1>;
		using BaseType = Entity::Data::HierarchyComponent;
		using ParentType = Host;

		struct Initializer : public DynamicInitializer
		{
			Initializer(DynamicInitializer&& baseObject, const Network::Address address, const uint32 maximumClientCount)
				: DynamicInitializer(Forward<DynamicInitializer>(baseObject))
				, m_address(address)
				, m_maximumClientCount(maximumClientCount)
			{
			}

			Network::Address m_address;
			uint32 m_maximumClientCount;
		};
		LocalHost(Initializer&& initializer);

		void Restart(const Network::Address address, const uint32 maximumClientCount);
		void Stop(ParentType& owner);

		[[nodiscard]] bool IsLocalClient(const ClientIdentifier clientIdentifier) const
		{
			return m_localHost.IsLocalClient(clientIdentifier);
		}

		[[nodiscard]] bool IsStarted() const
		{
			return m_localHost.IsStarted();
		}

		[[nodiscard]] Address GetLocalAddress() const
		{
			return m_localHost.GetLocalAddress();
		}
		[[nodiscard]] Address GetPublicAddress() const
		{
			return m_localHost.GetPublicAddress();
		}

		[[nodiscard]] static Entity::DataComponentResult<LocalHost>
		Find(const Entity::HierarchyComponentBase& rootParent, Entity::SceneRegistry& sceneRegistry);

		template<typename Type>
		[[nodiscard]] Optional<Type*> GetBoundObject(const BoundObjectIdentifier boundObjectIdentifier) const
		{
			return m_localHost.GetBoundObject<Type>(boundObjectIdentifier);
		}

		[[nodiscard]] Network::LocalHost& GetLocalHost()
		{
			return m_localHost;
		}
		[[nodiscard]] const Network::LocalHost& GetLocalHost() const
		{
			return m_localHost;
		}

		void OnDestroying();
	protected:
		friend BoundComponent;
		friend LocalClient;
	private:
		Network::LocalHost m_localHost;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::LocalHost>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::LocalHost>(
			"a51c59cf-1f97-415e-84f8-c15359fcc1c0"_guid,
			MAKE_UNICODE_LITERAL("Local Host Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::DisableDeletionFromUserInterface |
				Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}
