
#pragma once

#include "Components/SensorComponent.h"

#include <GameFramework/Components/SceneRules/FinishResult.h>

#include <Engine/Entity/Scene/ComponentTemplateIdentifier.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Guid.h>
#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::GameFramework
{
	struct GameRulesBase;

	struct FinishGameRulesSensor final : public SensorComponent
	{
		using BaseType = SensorComponent;
		using BaseType::BaseType;

		static constexpr Guid TypeGuid = "683e117b-9758-4ba8-85d3-75114f58589e"_guid;

		FinishGameRulesSensor(Initializer&& initializer);
		FinishGameRulesSensor(const FinishGameRulesSensor& templateComponent, const Cloner& cloner);

		void SetAudioAsset(const Asset::Picker asset);
		Asset::Picker GetAudioAsset() const;

		void OnCreated();
	protected:
		void OnPlayerReachedFinish(SensorComponent&, Optional<Entity::Component3D*> pComponent);

		using ScenePicker = Asset::Picker;
		void SetSceneAsset(const ScenePicker asset);
		ScenePicker GetSceneAsset() const;

		void SpawnMesh();
	private:
		friend struct Reflection::ReflectedType<GameFramework::FinishGameRulesSensor>;

		GameRulesFinishResult m_finishResult = GameRulesFinishResult::Success;
		Optional<Audio::SoundSpotComponent*> m_pSoundSpotComponent;
		Entity::ComponentTemplateIdentifier m_sceneTemplateIdentifier;
		Optional<Entity::Component3D*> m_pMeshComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::FinishGameRulesSensor>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::FinishGameRulesSensor>(
			GameFramework::FinishGameRulesSensor::TypeGuid,
			MAKE_UNICODE_LITERAL("Finish Game Sensor"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Result"),
					"result",
					"{184ABA9A-849B-4781-AB7A-3BA988A97D25}"_guid,
					MAKE_UNICODE_LITERAL("Finish Sensor"),
					Reflection::PropertyFlags::HideFromUI,
					&GameFramework::FinishGameRulesSensor::m_finishResult
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Sound"),
					"audio_asset",
					"{0F04CE69-382E-4629-AA55-23CFEAB21836}"_guid,
					MAKE_UNICODE_LITERAL("Finish Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::FinishGameRulesSensor::SetAudioAsset,
					&GameFramework::FinishGameRulesSensor::GetAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh"),
					"mesh",
					"{76FFD47C-28F1-4F02-B223-A8849174770C}"_guid,
					MAKE_UNICODE_LITERAL("Finish Sensor"),
					Reflection::PropertyFlags::VisibleToParentScope,
					&GameFramework::FinishGameRulesSensor::SetSceneAsset,
					&GameFramework::FinishGameRulesSensor::GetSceneAsset
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"eebed34e-7cff-7a0b-742a-f92bef66a445"_asset,
				"ef85043b-303d-43ac-84da-8b920b61fb2b"_guid,
				"9f0ee7de-1af4-4d68-ada9-a4c63f9905aa"_asset
			}}
		);
	};
}
