#include "Components/AnimationControllers/FirstPersonAnimationController.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Asset/AssetManager.h>

#include <Animation/Plugin.h>
#include <Animation/Components/SkeletonComponent.h>

namespace ngine::GameFramework::Animation::Controller
{
	FirstPerson::FirstPerson(Initializer&& initializer)
		: Controller(Forward<Initializer>(initializer))
	{
	}

	FirstPerson::FirstPerson(const FirstPerson& templateComponent, const Cloner& cloner)
		: Controller(templateComponent, cloner)
		, m_standardMoveBlendspace(templateComponent.m_standardMoveBlendspace)
		, m_aimMoveBlendspace(templateComponent.m_aimMoveBlendspace)
		, m_finalAimBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 3>{
					*templateComponent.m_finalAimBlendspace.GetSampleableAnimation(0),
					m_aimMoveBlendspace,
					*templateComponent.m_finalAimBlendspace.GetSampleableAnimation(2)
				},
				m_skeletonComponent
			)
		, m_finalMoveBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_standardMoveBlendspace, m_finalAimBlendspace},
				m_skeletonComponent
			)
		, m_jumpBlendspace(templateComponent.m_jumpBlendspace)
		, m_finalLoopedBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_finalMoveBlendspace, m_jumpBlendspace}, m_skeletonComponent
			)
		, m_oneshotBlendspace(templateComponent.m_oneshotBlendspace)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_finalLoopedBlendspace, m_oneshotBlendspace}, m_skeletonComponent
			)
	{
		m_standardMoveBlendspace.LoadAnimations(m_skeletonComponent);
		m_aimMoveBlendspace.LoadAnimations(m_skeletonComponent);
		m_finalAimBlendspace.LoadAnimations(m_skeletonComponent);
		m_jumpBlendspace.LoadAnimations(m_skeletonComponent);
		m_oneshotBlendspace.LoadAnimations(m_skeletonComponent);

		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	// Standard move blendspace -> Idle = 0, walk = 0.5, run = 1
	// Aim move blendspace -> idle = 0, walk = 0.5, run = 1
	// Final aim blendspace -> aim in = 0, aim move (blendspace) = 0.5, aim out = 1
	// Final move blendspace -> Default Move (blendspace) = 0, Final aim (blendspace) = 1
	// Jump blendspace -> Jump = 0, loop = 0.5, land = 1
	// Final looped blendspace -> Final move (blendspace) = 0, jump (blendspace) = 1
	// Final blendspace -> Final looped (blendspace) = 0, one shot = 1

	// Fire (one shot)
	// Fire empty (one shot)
	// Reload (one shot)
	// Reload empty (one shot)

	// Aim fire (one shot)
	// Aim fire empty (one shot)
	// Aim reload (one shot)
	// Aim reload empty (one shot)

	FirstPerson::FirstPerson(const Deserializer& deserializer)
		: Controller(deserializer)
		, m_standardMoveBlendspace(deserializer.m_reader.FindSerializer("move_animations"), m_skeletonComponent)
		, m_aimMoveBlendspace(deserializer.m_reader.FindSerializer("aim_move_animations"), m_skeletonComponent)
		, m_finalAimBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 3>{
					deserializer.m_reader.GetSerializer("aim_in_animation"),
					m_aimMoveBlendspace,
					deserializer.m_reader.GetSerializer("aim_out_animation")
				},
				m_skeletonComponent
			)
		, m_finalMoveBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_standardMoveBlendspace, m_finalAimBlendspace},
				m_skeletonComponent
			)
		, m_jumpBlendspace(deserializer.m_reader.FindSerializer("jump_animations"), m_skeletonComponent)
		, m_finalLoopedBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_finalMoveBlendspace, m_jumpBlendspace}, m_skeletonComponent
			)
		, m_oneshotBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 8>{
					deserializer.m_reader.GetSerializer("fire_animation"),
					deserializer.m_reader.GetSerializer("fire_empty_animation"),
					deserializer.m_reader.GetSerializer("aim_fire_animation"),
					deserializer.m_reader.GetSerializer("aim_fire_empty_animation"),
					deserializer.m_reader.GetSerializer("reload_animation"),
					deserializer.m_reader.GetSerializer("reload_empty_animation"),
					deserializer.m_reader.GetSerializer("aim_reload_animation"),
					deserializer.m_reader.GetSerializer("aim_reload_empty_animation"),
				},
				m_skeletonComponent
			)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_finalLoopedBlendspace, m_oneshotBlendspace}, m_skeletonComponent
			)
	{
		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	void FirstPerson::SetMoveBlendRatio(const float ratio)
	{
		m_standardMoveBlendspace.SetBlendRatio(ratio);
		m_aimMoveBlendspace.SetBlendRatio(ratio);
	}

	void FirstPerson::SetTimeInAir(Time::Durationf timeInAir)
	{
		// TODO: We should make sure that this is never called before animation finish loading!
		if (!m_jumpBlendspace.IsValid() || !m_jumpBlendspace.HasAnimation(0))
		{
			return;
		}

		const Optional<ngine::Animation::Animation*> pJumpStartAnimation = m_jumpBlendspace.GetEntryCount() > 1
		                                                                     ? m_jumpBlendspace.GetAnimation(0)
		                                                                     : Optional<ngine::Animation::Animation*>{};
		const Time::Durationf jumpStartDuration = pJumpStartAnimation.IsValid() ? pJumpStartAnimation->GetDuration()
		                                                                        : Time::Durationf{0_seconds};
		if (timeInAir <= 0_seconds)
		{
			const Optional<ngine::Animation::Animation*> pJumpEndAnimation = m_jumpBlendspace.GetAnimation(2);
			const Time::Durationf jumpEndDuration = pJumpEndAnimation.IsValid() ? pJumpEndAnimation->GetDuration() : Time::Durationf{0_seconds};
			const float jumpEndRatio = Math::Abs(timeInAir.GetSeconds()) / jumpEndDuration.GetSeconds();
			if (jumpEndRatio < 1.f && m_state != State::OnGround)
			{
				if (m_state != State::Landing)
				{
					const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
						m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
					m_jumpBlendspace
						.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());
					m_state = State::Landing;
				}

				// Landing
				m_jumpBlendspace.SetBlendRatio(0.5f + jumpEndRatio * 0.5f);
			}
			else if (m_state != State::OnGround)
			{
				m_state = State::OnGround;
			}
		}
		else if (timeInAir <= jumpStartDuration)
		{
			if (m_state != State::StartJump)
			{
				const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
					m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
				m_jumpBlendspace
					.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());
				m_state = State::StartJump;
			}

			const float jumpStartRatio = timeInAir.GetSeconds() / jumpStartDuration.GetSeconds();
			m_jumpBlendspace.SetBlendRatio(jumpStartRatio * 0.5f);
		}
		else
		{
			m_jumpBlendspace.SetBlendRatio(0.5f);

			if (m_state != State::InAir)
			{
				m_state = State::InAir;
			}
		}
		m_timeInAir = timeInAir;
	}

	void FirstPerson::UpdateAimingTime(const float deltaTime)
	{
		switch (m_aimState)
		{
			case AimState::None:
				m_finalAimBlendspace.SetBlendRatio(0.5f);
				m_finalMoveBlendspace.SetBlendRatio(0.f);
				break;
			case AimState::AimIn:
			{
				const float aimInSpeed = 2.f;
				m_aimingTime += deltaTime * aimInSpeed;

				const Optional<ngine::Animation::Animation*> pAimInAnimation = m_finalAimBlendspace.GetAnimation(0);
				const Time::Durationf aimInDuration = pAimInAnimation.IsValid() ? pAimInAnimation->GetDuration() : Time::Durationf{0_seconds};

				const float aimInDurationSeconds = aimInDuration.GetSeconds();
				const float aimInRatio = Math::Min(m_aimingTime / aimInDurationSeconds, 1.f);
				m_finalAimBlendspace.GetSampleableAnimation(0)->SetTimeRatio(aimInRatio);
				m_finalMoveBlendspace.SetBlendRatio(aimInRatio);
				m_finalAimBlendspace.SetBlendRatio(aimInRatio * 0.5f);
				if (m_aimingTime >= aimInDurationSeconds)
				{
					m_aimState = AimState::Aiming;
				}
			}
			break;
			case AimState::Aiming:
				break;
			case AimState::AimOut:
			{
				const float aimOutSpeed = 2.f;
				m_aimingTime += deltaTime * aimOutSpeed;

				const Optional<ngine::Animation::Animation*> pAimOutAnimation = m_finalAimBlendspace.GetAnimation(2);
				const Time::Durationf aimOutDuration = pAimOutAnimation.IsValid() ? pAimOutAnimation->GetDuration() : Time::Durationf{0_seconds};

				const float aimOutDurationSeconds = aimOutDuration.GetSeconds();
				const float aimOutRatio = 1.f - Math::Min(m_aimingTime / aimOutDurationSeconds, 1.f);
				// m_finalAimBlendspace.GetSampleableAnimation(2)->SetTimeRatio(aimOutRatio);
				m_finalMoveBlendspace.SetBlendRatio(aimOutRatio);
				// m_finalAimBlendspace.SetBlendRatio(0.5f + aimOutRatio * 0.5f);
				if (m_aimingTime >= aimOutDurationSeconds)
				{
					m_aimState = AimState::None;
				}
			}
			break;
		}
	}

	void FirstPerson::UpdateOneShot(const float deltaTime)
	{
		if (m_oneShotAnimation == OneShotAnimation::None)
		{
			m_finalBlendspace.SetBlendRatio(0.f);
			return;
		}

		const float blendOutOneshotTime = 0.5f;
		if (Math::SignNonZero(m_oneShotAnimationTime) > 0.f)
		{
			const float blendToOneshotTime = 0.1f;
			m_oneShotAnimationTime += deltaTime;
			const float blendToOneshotRatio = Math::Min(m_oneShotAnimationTime / blendToOneshotTime, 1.f);
			m_finalBlendspace.SetBlendRatio(blendToOneshotRatio);

			const uint8 animationIndex = uint8((uint8)m_oneShotAnimation - 1u);
			const Optional<ngine::Animation::Animation*> pOneShotAnimation = m_oneshotBlendspace.GetAnimation(animationIndex);
			const Time::Durationf oneshotDuration = pOneShotAnimation.IsValid() ? pOneShotAnimation->GetDuration() : Time::Durationf{0_seconds};

			if (m_oneShotAnimationTime >= (oneshotDuration.GetSeconds() - blendOutOneshotTime))
			{
				m_oneShotAnimationTime -= oneshotDuration.GetSeconds() - blendOutOneshotTime;
				if (m_oneShotAnimationTime > 0.f)
				{
					m_oneShotAnimationTime *= -1.f;
				}

				const float blendFromOneshotRatio = Math::Min(Math::Abs(m_oneShotAnimationTime) / blendOutOneshotTime, 1.f);

				m_finalBlendspace.SetBlendRatio(1.f - blendFromOneshotRatio);
				if (blendFromOneshotRatio == 1.f)
				{
					m_oneShotAnimation = OneShotAnimation::None;
				}
			}
		}
		else
		{
			m_oneShotAnimationTime -= deltaTime;
			const float blendFromOneshotRatio = Math::Min(Math::Abs(m_oneShotAnimationTime) / blendOutOneshotTime, 1.f);

			m_finalBlendspace.SetBlendRatio(1.f - blendFromOneshotRatio);
			if (blendFromOneshotRatio == 1.f)
			{
				OnOneShotAnimationCompleted(m_oneShotAnimation);
				m_oneShotAnimation = OneShotAnimation::None;
			}
		}
	}

	void FirstPerson::SetAiming(const bool isAiming)
	{
		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
			m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
		if (isAiming)
		{
			m_aimState = AimState::AimIn;

			m_finalAimBlendspace
				.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());
			// Start aiming in animation
			m_finalAimBlendspace.SetBlendRatio(0.f);
			m_aimingTime = 0.f;
		}
		else
		{
			m_aimState = AimState::AimOut;
			m_finalAimBlendspace
				.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());
			// Start aiming out animation
			m_finalAimBlendspace.SetBlendRatio(0.5f);
			m_aimingTime = 0.f;
		}
	}

	void FirstPerson::StartOneShot(const OneShotAnimation oneShot)
	{
		m_oneShotAnimation = oneShot;
		m_oneShotAnimationTime = 0.f;

		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
			m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
		m_oneshotBlendspace
			.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());

		const float animationBlendRatio = (float)oneShot / (float)OneShotAnimation::Count;
		m_oneshotBlendspace.SetBlendRatio(animationBlendRatio);
	}

	bool FirstPerson::ShouldUpdate() const
	{
		return m_standardMoveBlendspace.IsValid() && m_aimMoveBlendspace.IsValid() && m_jumpBlendspace.IsValid() &&
		       m_finalAimBlendspace.IsValid() && m_finalMoveBlendspace.IsValid() && m_finalLoopedBlendspace.IsValid() &&
		       m_finalBlendspace.IsValid() && m_oneshotBlendspace.IsValid() && m_skeletonComponent.GetSkeletonInstance().IsValid();
	}

	void FirstPerson::Update()
	{
		const FrameTime frameTime = m_skeletonComponent.GetCurrentFrameTime();

		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
			m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount();
		const ArrayView<ozz::math::SoaTransform, uint16> sampledTransforms = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms();

		switch (m_state)
		{
			case State::None:
			case State::OnGround:
				m_finalLoopedBlendspace.SetBlendRatio(0.f);
				break;
			case State::StartJump:
			{
				const float jumpRatio = Math::Min(m_timeInAir.GetSeconds() / m_jumpBlendspace.GetAnimation(0)->GetDuration().GetSeconds(), 1.f);
				m_finalLoopedBlendspace.SetBlendRatio(jumpRatio);
			}
			break;
			case State::InAir:
				break;
			case State::Landing:
			{
				const float landingRatio =
					Math::Min(Math::Abs(m_timeInAir.GetSeconds()) / m_jumpBlendspace.GetAnimation(2)->GetDuration().GetSeconds(), 1.f);
				m_finalLoopedBlendspace.SetBlendRatio(1.f - landingRatio);
			}
			break;
			default:
				break;
		}

		UpdateAimingTime(frameTime);
		UpdateOneShot(frameTime);

		m_finalBlendspace.Advance(frameTime, jointBindPoses, sampledTransformCount);
		m_finalBlendspace.Process(jointBindPoses, sampledTransforms);
	}

	void FirstPerson::OnSkeletonChanged()
	{
		const uint16 jointCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize();
		m_standardMoveBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_aimMoveBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_finalAimBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_finalMoveBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_jumpBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_oneshotBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_finalBlendspace.Initialize(sampledTransformCount, 2);
		m_finalLoopedBlendspace.Initialize(sampledTransformCount, 2);
	}

	void FirstPerson::DeserializeCustomData(const Optional<Serialization::Reader> serializer, Entity::Component3D& parent)
	{
		if (serializer.IsValid())
		{
			ngine::Animation::SkeletonComponent& skeletonComponent = static_cast<ngine::Animation::SkeletonComponent&>(parent);

			serializer->Serialize("move_animations", m_standardMoveBlendspace, skeletonComponent);
			serializer->Serialize("aim_move_animations", m_aimMoveBlendspace, skeletonComponent);
			serializer->Serialize("jump_animations", m_jumpBlendspace, skeletonComponent);

			m_finalAimBlendspace.Assign(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 3>{
					serializer->GetSerializer("aim_in_animation"),
					m_aimMoveBlendspace,
					serializer->GetSerializer("aim_out_animation")
				},
				m_skeletonComponent
			);

			m_oneshotBlendspace.Assign(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 8>{
					serializer->GetSerializer("fire_animation"),
					serializer->GetSerializer("fire_empty_animation"),
					serializer->GetSerializer("aim_fire_animation"),
					serializer->GetSerializer("aim_fire_empty_animation"),
					serializer->GetSerializer("reload_animation"),
					serializer->GetSerializer("reload_empty_animation"),
					serializer->GetSerializer("aim_reload_animation"),
					serializer->GetSerializer("aim_reload_empty_animation"),
				},
				m_skeletonComponent
			);

			m_standardMoveBlendspace.LoadAnimations(skeletonComponent);
			m_aimMoveBlendspace.LoadAnimations(skeletonComponent);
			m_finalAimBlendspace.LoadAnimations(skeletonComponent);
			m_jumpBlendspace.LoadAnimations(skeletonComponent);
			m_oneshotBlendspace.LoadAnimations(skeletonComponent);
		}
	}

	bool FirstPerson::SerializeCustomData(Serialization::Writer serializer, const Entity::Component3D&) const
	{
		bool wroteAny = serializer.Serialize("move_animations", m_standardMoveBlendspace);
		wroteAny |= serializer.Serialize("aim_move_animations", m_aimMoveBlendspace);
		wroteAny |= serializer.Serialize("jump_animations", m_jumpBlendspace);
		return wroteAny;
	}

	[[maybe_unused]] const bool wasAnimationControllerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FirstPerson>>::Make());
	[[maybe_unused]] const bool wasAnimationControllerTypeRegistered = Reflection::Registry::RegisterType<FirstPerson>();
}
