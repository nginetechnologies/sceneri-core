#pragma once

#include <Animation/Components/Controllers/AnimationController.h>
#include <Animation/Blendspace1D.h>
#include <Common/Function/Event.h>

namespace ngine::GameFramework::Animation::Controller
{
	struct FirstPerson : public ngine::Animation::Controller
	{
		using BaseType = ngine::Animation::Controller;

		enum class State : uint8
		{
			None,
			StartJump,
			InAir,
			Landing,
			OnGround
		};

		enum class AimState : uint8
		{
			None,
			AimIn,
			Aiming,
			AimOut
		};

		enum class OneShotAnimation : uint8
		{
			None,
			Fire,
			FireEmpty,
			AimFire,
			AimFireEmpty,
			Reload,
			ReloadEmpty,
			AimReload,
			AimReloadEmpty,
			Count = AimReloadEmpty
		};

		Event<void(void*, OneShotAnimation), 24> OnOneShotAnimationCompleted;

		FirstPerson(Initializer&& initializer);
		FirstPerson(const FirstPerson& templateComponent, const Cloner&);
		FirstPerson(const Deserializer& deserializer);

		void SetMoveBlendRatio(const float ratio);
		[[nodiscard]] uint8 GetMoveBlendEntryCount() const
		{
			return m_standardMoveBlendspace.GetEntryCount();
		}
		void SetTimeInAir(const Time::Durationf timeInAir);
		void SetAiming(const bool isAiming);
		void StartOneShot(const OneShotAnimation oneShot);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::Component3D& parent);
		bool SerializeCustomData(Serialization::Writer, const Entity::Component3D& parent) const;
	protected:
		void UpdateAimingTime(const float deltaTime);
		void UpdateOneShot(const float deltaTime);

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
		ngine::Animation::Blendspace1D m_standardMoveBlendspace;
		ngine::Animation::Blendspace1D m_aimMoveBlendspace;
		ngine::Animation::Blendspace1D m_finalAimBlendspace;
		ngine::Animation::Blendspace1D m_finalMoveBlendspace;
		ngine::Animation::Blendspace1D m_jumpBlendspace;
		ngine::Animation::Blendspace1D m_finalLoopedBlendspace;
		ngine::Animation::Blendspace1D m_oneshotBlendspace;
		ngine::Animation::Blendspace1D m_finalBlendspace;
		// Positive if we're in air, negative if on ground
		Time::Durationf m_timeInAir = 0_seconds;
		State m_state = State::None;
		float m_aimingTime{0.f};
		AimState m_aimState = AimState::None;
		OneShotAnimation m_oneShotAnimation;
		float m_oneShotAnimationTime{0.f};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Animation::Controller::FirstPerson>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Animation::Controller::FirstPerson>(
			"{AD206980-8F37-4D45-971E-DD3A4F78791F}"_guid, MAKE_UNICODE_LITERAL("First Person Animation Controller")
		);
	};
}
