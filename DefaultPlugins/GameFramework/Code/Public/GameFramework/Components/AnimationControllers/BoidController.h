#pragma once

#include <Animation/Components/Controllers/AnimationController.h>
#include <Animation/Blendspace1D.h>
#include <Common/Function/Event.h>
#include <Common/Reflection/EnumTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Common/Asset/Picker.h>

namespace ngine::Audio
{
	struct SoundSpotComponent;
}

namespace ngine::GameFramework::Animation::Controller
{
	struct Boid : public ngine::Animation::Controller
	{
		using BaseType = ngine::Animation::Controller;

		enum class State : uint8
		{
			None,
			Idle,
			Moving,
			Dying,
			Dead
		};

		Boid(Initializer&& initializer);
		Boid(const Boid& templateComponent, const Cloner&);
		Boid(const Deserializer& deserializer);

		void StartDeath();

		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::Component3D& parent);
		bool SerializeCustomData(Serialization::Writer, const Entity::Component3D& parent) const;

		void SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Picker asset);
		Asset::Picker GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const;

		void SetDeathAudioAsset(const Asset::Picker asset);
		Asset::Picker GetDeathAudioAsset() const;
	protected:
		void UpdateDeathAnimation(const float deltaTime);
		void SetStateFromBoid(Entity::Component3D& owner);

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
		friend struct Reflection::ReflectedType<GameFramework::Animation::Controller::Boid>;

		ngine::Animation::Blendspace1D m_movingBlendspace;
		ngine::Animation::Blendspace1D m_dyingBlendspace;
		ngine::Animation::Blendspace1D m_finalBlendspace;

		Entity::Component3D& m_owner;

		Optional<Audio::SoundSpotComponent*> m_pDeathSoundComponent;

		State m_state = State::Idle;
		float m_deathAnimationTime{0.f};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Animation::Controller::Boid>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Animation::Controller::Boid>(
			"c33d1cba-ad23-4248-ab1e-3c96d9715f18"_guid,
			MAKE_UNICODE_LITERAL("Boid Animation Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Death Sound"),
				"audio_death",
				"{5401FCD8-5B46-4C2D-87DC-324FC6B99498}"_guid,
				MAKE_UNICODE_LITERAL("Audio"),
				&GameFramework::Animation::Controller::Boid::SetDeathAudioAsset,
				&GameFramework::Animation::Controller::Boid::GetDeathAudioAsset
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::IndicatorTypeExtension{
				EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost}
			}}
		);
	};
}
