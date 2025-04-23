#include "Components/AnimationControllers/ThirdPersonAnimationController.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Asset/AssetManager.h>

#include <Animation/Plugin.h>
#include <Animation/Components/SkeletonComponent.h>

namespace ngine::GameFramework::Animation::Controller
{
	ThirdPerson::ThirdPerson(Initializer&& initializer)
		: Controller(Forward<Initializer>(initializer))
	{
	}

	ThirdPerson::ThirdPerson(const ThirdPerson& templateComponent, const Cloner& cloner)
		: Controller(templateComponent, cloner)
		, m_moveBlendspace(templateComponent.m_moveBlendspace)
		, m_jumpBlendspace(templateComponent.m_jumpBlendspace)
		, m_moveJumpBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_moveBlendspace, m_jumpBlendspace}, m_skeletonComponent
			)
	{
		m_moveBlendspace.LoadAnimations(m_skeletonComponent);
		m_jumpBlendspace.LoadAnimations(m_skeletonComponent);

		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	ThirdPerson::ThirdPerson(const Deserializer& deserializer)
		: Controller(deserializer)
		, m_moveBlendspace(
				deserializer.m_reader.FindSerializer("move_animations"), deserializer.GetParent().AsExpected<ngine::Animation::SkeletonComponent>()
			)
		, m_jumpBlendspace(
				deserializer.m_reader.FindSerializer("jump_animations"), deserializer.GetParent().AsExpected<ngine::Animation::SkeletonComponent>()
			)
		, m_moveJumpBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_moveBlendspace, m_jumpBlendspace},
				deserializer.GetParent().AsExpected<ngine::Animation::SkeletonComponent>()
			)
	{
		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton =
		      deserializer.GetParent().AsExpected<ngine::Animation::SkeletonComponent>().GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			deserializer.GetParent().AsExpected<ngine::Animation::SkeletonComponent>().TryEnableUpdate();
		}
	}

	void ThirdPerson::SetMoveBlendRatio(const float ratio)
	{
		m_moveBlendspace.SetBlendRatio(ratio);
	}

	void ThirdPerson::SetTimeInAir(Time::Durationf timeInAir)
	{
		// TODO: We should make sure that this is never called before animation finish loading!
		if (!m_jumpBlendspace.IsValid() || !m_jumpBlendspace.HasAnimation(0))
		{
			return;
		}

		const Optional<ngine::Animation::Animation*> pJumpStartAnimation = m_jumpBlendspace.GetEntryCount() > 0
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

	bool ThirdPerson::ShouldUpdate() const
	{
		return m_moveBlendspace.IsValid() && m_jumpBlendspace.IsValid() && m_moveJumpBlendspace.IsValid() &&
		       m_skeletonComponent.GetSkeletonInstance().IsValid();
	}

	void ThirdPerson::Update()
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
				m_moveJumpBlendspace.SetBlendRatio(0.f);
				break;
			case State::StartJump:
			{
				const float jumpRatio =
					Math::Clamp(m_timeInAir.GetSeconds() / m_jumpBlendspace.GetAnimation(0)->GetDuration().GetSeconds(), 0.f, 1.f);
				m_moveJumpBlendspace.SetBlendRatio(jumpRatio);
			}
			break;
			case State::InAir:
				break;
			case State::Landing:
			{
				const float landingRatio =
					Math::Min(Math::Abs(m_timeInAir.GetSeconds()) / m_jumpBlendspace.GetAnimation(2)->GetDuration().GetSeconds(), 1.f);
				m_moveJumpBlendspace.SetBlendRatio(1.f - landingRatio);
			}
			break;
			default:
				break;
		}

		m_moveJumpBlendspace.Advance(frameTime, jointBindPoses, sampledTransformCount);
		m_moveJumpBlendspace.Process(jointBindPoses, sampledTransforms);
	}

	void ThirdPerson::OnSkeletonChanged()
	{
		const uint16 jointCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize();
		m_moveBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_jumpBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);

		m_moveJumpBlendspace.Initialize(sampledTransformCount, 2);
	}

	void ThirdPerson::DeserializeCustomData(const Optional<Serialization::Reader> serializer, Entity::Component3D& parent)
	{
		if (serializer.IsValid())
		{
			ngine::Animation::SkeletonComponent& skeletonComponent = static_cast<ngine::Animation::SkeletonComponent&>(parent);

			serializer->Serialize("move_animations", m_moveBlendspace, skeletonComponent);
			serializer->Serialize("jump_animations", m_jumpBlendspace, skeletonComponent);

			m_moveBlendspace.LoadAnimations(skeletonComponent);
			m_jumpBlendspace.LoadAnimations(skeletonComponent);
		}
	}

	bool ThirdPerson::SerializeCustomData(Serialization::Writer serializer, const Entity::Component3D&) const
	{
		bool wroteAny = serializer.Serialize("move_animations", m_moveBlendspace);
		wroteAny |= serializer.Serialize("jump_animations", m_jumpBlendspace);
		return wroteAny;
	}

	[[maybe_unused]] const bool wasFirstAnimationControllerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ThirdPerson>>::Make());
	[[maybe_unused]] const bool wasFirstAnimationControllerTypeRegistered = Reflection::Registry::RegisterType<ThirdPerson>();
}
