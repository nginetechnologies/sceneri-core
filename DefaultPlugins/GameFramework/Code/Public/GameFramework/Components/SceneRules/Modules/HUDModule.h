#pragma once

#include <GameFramework/Components/SceneRules/Modules/Module.h>

#include <Engine/Entity/Component3D.h>

#include <Common/Storage/IdentifierArray.h>
#include <Common/Function/Event.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct HUDModule final : SceneRulesModule
	{
		using BaseType = SceneRulesModule;

		Event<void(void*, SceneRules&, const ClientIdentifier), 24> OnPlayerHUDLoaded;

		struct Initializer : public SceneRulesModule::Initializer
		{
			using BaseType = SceneRulesModule::Initializer;
			using BaseType::BaseType;

			Initializer(BaseType&& initializer, Asset::Guid hudAsset = {})
				: BaseType(Forward<SceneRulesModule::Initializer>(initializer))
				, m_hudWidgetAsset(hudAsset)
			{
			}

			Asset::Guid m_hudWidgetAsset;
		};

		HUDModule(const HUDModule& templateComponent, const Cloner& cloner);
		HUDModule(const Deserializer& deserializer);
		HUDModule(Initializer&& initializer);

		void Notify(const Guid eventGuid, const ClientIdentifier);

		Optional<Widgets::Widget*> GetWidget(const ClientIdentifier playerIdentifier);

		virtual void OnGameplayStopped(SceneRules&) override;
		virtual void ClientOnLocalPlayerJoined(SceneRules&, const ClientIdentifier) override;
		virtual void ClientOnLocalPlayerLeft(SceneRules&, const ClientIdentifier) override;
	private:
		friend struct Reflection::ReflectedType<HUDModule>;

		Asset::Guid m_hudAsset;
		TIdentifierArray<Optional<Widgets::Widget*>, ClientIdentifier> m_playerWidgets;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::HUDModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::HUDModule>(
			"f1dad7f0-0053-4a05-af00-0cc2a88f1913"_guid,
			MAKE_UNICODE_LITERAL("HUD Game Rules Module"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Asset"),
				"asset",
				"{2E508F2A-0388-403B-9A95-5A7D846C385E}"_guid,
				MAKE_UNICODE_LITERAL("HUD"),
				&GameFramework::HUDModule::m_hudAsset
			)}
		);
	};
}
