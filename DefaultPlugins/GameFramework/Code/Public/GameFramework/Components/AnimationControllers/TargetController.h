#pragma once

#include <Animation/Components/Controllers/AnimationController.h>
#include <Animation/Blendspace1D.h>
#include <Common/Asset/Picker.h>
#include <Common/Function/Event.h>
#include <Common/Reflection/EnumTypeExtension.h>
#include <Common/Reflection/CoreTypes.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::GameFramework::Animation::Controller
{
	struct Target : public ngine::Animation::Controller
	{
		using BaseType = ngine::Animation::Controller;

		enum class State : uint8
		{
			None,
			Up,
			GoingDown,
			GoingUp,
			Down,
		};

		Target(Initializer&& initializer);
		Target(const Target& templateComponent, const Cloner&);
		Target(const Deserializer& deserializer);

		void SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Guid asset);
		Asset::Picker GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const;

		void StartHit();
		void StartReerect();

		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::Component3D& parent);
		bool SerializeCustomData(Serialization::Writer, const Entity::Component3D& parent) const;

		void SetUpAudioAsset(const Asset::Picker asset);
		Asset::Picker GetUpAudioAsset() const;

		void SetDownAudioAsset(const Asset::Picker asset);
		Asset::Picker GetDownAudioAsset() const;
	protected:
		void UpdateAnimation(const float deltaTime);

		void SubscribeToProjectileTarget(Entity::Component3D& owner);

		virtual bool ShouldUpdate() const override;
		virtual void Update() override;

		virtual void OnSkeletonChanged() override;

		virtual void ApplyAnimation(const Asset::Guid) override
		{
		}

		virtual void IterateAnimations(const Function<Memory::CallbackResult(ConstAnyView), 36>&) override
		{
		}
	protected:
		friend struct Reflection::ReflectedType<GameFramework::Animation::Controller::Target>;

		ngine::Animation::Blendspace1D m_upBlendspace;
		ngine::Animation::Blendspace1D m_downBlendspace;
		ngine::Animation::Blendspace1D m_finalBlendspace;
		Entity::Component3D& m_owner;

		Optional<Audio::SoundSpotComponent*> m_pUpSoundComponent;
		Optional<Audio::SoundSpotComponent*> m_pDownSoundComponent;
		State m_state = State::Up;
		float m_animationTime{0.f};
		float m_resetTime = 0.0f;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Animation::Controller::Target>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Animation::Controller::Target>(
			"16324a86-fa45-40e3-a839-32ede39df20a"_guid,
			MAKE_UNICODE_LITERAL("Target Animation Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Up Sound"),
					"audio_up",
					"{AFD8E4AA-469C-4677-8CD1-59E44E9F7EE6}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Animation::Controller::Target::SetUpAudioAsset,
					&GameFramework::Animation::Controller::Target::GetUpAudioAsset
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Down Sound"),
					"audio_down",
					"{A96DD112-4760-4923-8BCC-1E93CF2DD55C}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Animation::Controller::Target::SetDownAudioAsset,
					&GameFramework::Animation::Controller::Target::GetDownAudioAsset
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Reset Time"),
					"resetTime",
					"{5328CF45-EF4C-43E1-933A-E97EEAA2D35D}"_guid,
					MAKE_UNICODE_LITERAL("Audio"),
					&GameFramework::Animation::Controller::Target::m_resetTime
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::IndicatorTypeExtension{
				EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost}
			}}
		);
	};
}
