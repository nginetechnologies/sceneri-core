#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Guid.h>
#include <Common/Asset/Picker.h>
#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::GameFramework
{
	struct SpawnPointComponent final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "88a0cdbb-2826-46e3-b372-2a5154eacb61"_guid;
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		SpawnPointComponent(const Deserializer& deserializer);
		SpawnPointComponent(const SpawnPointComponent& templateComponent, const Cloner& cloner);
		SpawnPointComponent(Initializer&& initializer);

		using Component3D::Serialize;

		[[nodiscard]] Asset::Guid GetSpawnedAssetGuid() const
		{
			return m_assetGuid;
		}

		ThreadSafe::Event<void(void*), 24> OnChanged;
	protected:
		friend struct Reflection::ReflectedType<GameFramework::SpawnPointComponent>;

		void SetSpawnedAsset(const Asset::Picker asset);
		[[nodiscard]] Asset::Picker GetSpawnedAsset() const;
	private:
		Asset::Guid m_assetGuid;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SpawnPointComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SpawnPointComponent>(
			GameFramework::SpawnPointComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Spawn Point"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Asset"),
				"asset",
				"{3D6242F6-F32A-42E4-A788-CB90D2934E59}"_guid,
				MAKE_UNICODE_LITERAL("Spawn Point"),
				&GameFramework::SpawnPointComponent::SetSpawnedAsset,
				&GameFramework::SpawnPointComponent::GetSpawnedAsset
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(),
					"45f77367-73b4-0984-573a-52cf388e0344"_asset,
					"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
					"7a7e41a5-3e9c-4300-9e14-d1c0b5e1e4a9"_asset
				},
				Entity::IndicatorTypeExtension{
					"1aab6ce6-8cb6-46a1-9d4c-8e04a189eb41"_guid,
				}
			}
		);
	};
}
